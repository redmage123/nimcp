/**
 * @file nimcp_mesh_types.h
 * @brief Core Types for NIMCP Unified Mesh Network Architecture
 *
 * WHAT: Fundamental types, enums, and structures for the mesh network
 * WHY:  Enable Hyperledger-inspired distributed consensus across brain modules
 * HOW:  Channels, coordinators, transactions, endorsements, and participant IDs
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                      MESH NETWORK TYPE HIERARCHY                        │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │  Identifiers:  mesh_participant_id_t, mesh_channel_id_t, mesh_tx_id_t  │
 * │  Enums:        coordinator_role, tx_status, endorsement_result         │
 * │  Structures:   mesh_belief_t, mesh_consensus_t, credential_t           │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe type definitions
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_TYPES_H
#define NIMCP_MESH_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Magic number for mesh structure validation */
#define NIMCP_MESH_MAGIC                    0x4D455348  /* "MESH" */

/** @brief Maximum participants per channel */
#define MESH_MAX_PARTICIPANTS_PER_CHANNEL   256

/** @brief Maximum channels in the mesh */
#define MESH_MAX_CHANNELS                   16

/** @brief Maximum coordinators per pool */
#define MESH_MAX_COORDINATORS_PER_POOL      8

/** @brief Maximum endorsers per policy */
#define MESH_MAX_ENDORSERS_PER_POLICY       16

/** @brief Maximum pending transactions per channel */
#define MESH_MAX_PENDING_TRANSACTIONS       1024

/** @brief Maximum transaction batch size */
#define MESH_MAX_BATCH_SIZE                 64

/** @brief Transaction payload maximum size */
#define MESH_MAX_PAYLOAD_SIZE               4096

/** @brief Maximum belief vector dimension */
#define MESH_BELIEF_VECTOR_DIM              64

/** @brief Maximum module name length */
#define MESH_MAX_NAME_LEN                   64

/** @brief Endorsement signature size (SHA256-based) */
#define MESH_SIGNATURE_SIZE                 64

/** @brief Credential ID size */
#define MESH_CREDENTIAL_ID_SIZE             32

/** @brief Default transaction timeout (ms) */
#define MESH_DEFAULT_TX_TIMEOUT_MS          5000

/** @brief Default endorsement timeout (ms) */
#define MESH_DEFAULT_ENDORSEMENT_TIMEOUT_MS 100

/** @brief Default ordering batch timeout (ms) */
#define MESH_DEFAULT_BATCH_TIMEOUT_MS       50

/* ============================================================================
 * Identifier Types
 * ============================================================================ */

/**
 * @brief Unique participant identifier in the mesh
 *
 * WHAT: 64-bit unique ID for any module participating in mesh
 * WHY:  Identify modules across channels and coordinator pools
 * HOW:  Upper 16 bits = channel, next 16 bits = type, lower 32 bits = local ID
 *
 * ENCODING: (channel_id << 48) | (type << 32) | local_id
 */
typedef uint64_t mesh_participant_id_t;

/**
 * @brief Channel identifier
 *
 * WHAT: Identifies isolated ledger domains
 * WHY:  Channels have separate world state and coordinator pools
 */
typedef uint16_t mesh_channel_id_t;

/**
 * @brief Transaction identifier
 *
 * WHAT: Globally unique transaction ID
 * WHY:  Track transactions across endorsement, ordering, and commit phases
 * HOW:  Combination of channel, proposer, and sequence number
 */
typedef struct mesh_tx_id {
    mesh_channel_id_t channel;          /**< Source channel */
    mesh_participant_id_t proposer;     /**< Proposing participant */
    uint64_t sequence;                  /**< Monotonic sequence number */
    uint64_t timestamp_ns;              /**< Creation timestamp (nanoseconds) */
} mesh_tx_id_t;

/**
 * @brief Coordinator pool identifier
 */
typedef uint32_t mesh_pool_id_t;

/* ============================================================================
 * Well-Known Channel IDs
 * ============================================================================ */

#define MESH_CHANNEL_SYSTEM             0   /**< System-wide orchestration */
#define MESH_CHANNEL_LEFT_HEMISPHERE    1   /**< Left hemisphere (analytical) */
#define MESH_CHANNEL_RIGHT_HEMISPHERE   2   /**< Right hemisphere (holistic) */
#define MESH_CHANNEL_SUBCORTICAL        3   /**< Subcortical (emotion, motor) */
#define MESH_CHANNEL_GPU_COMPUTE        4   /**< GPU accelerated processing */

/* ============================================================================
 * Participant Types
 * ============================================================================ */

/**
 * @brief Type of mesh participant
 *
 * WHAT: Categorizes participants by their role
 * WHY:  Different types have different capabilities and endorsement weights
 */
typedef enum mesh_participant_type {
    MESH_PARTICIPANT_NONE = 0,          /**< Invalid/uninitialized */
    MESH_PARTICIPANT_MODULE,            /**< Standard NIMCP module */
    MESH_PARTICIPANT_COORDINATOR,       /**< Coordinator (can lead pools) */
    MESH_PARTICIPANT_ORDERER,           /**< Ordering service node */
    MESH_PARTICIPANT_GATEWAY,           /**< Cross-channel gateway */
    MESH_PARTICIPANT_OBSERVER           /**< Read-only observer */
} mesh_participant_type_t;

/* ============================================================================
 * Coordinator Roles and States
 * ============================================================================ */

/**
 * @brief Role within a coordinator pool
 *
 * WHAT: Current role of coordinator in its pool
 * WHY:  Leader election and load balancing
 */
typedef enum coordinator_role {
    COORD_ROLE_NONE = 0,                /**< Not in a pool */
    COORD_ROLE_LEADER,                  /**< Pool leader, orchestrates */
    COORD_ROLE_WORKER,                  /**< Active worker, handles participants */
    COORD_ROLE_STANDBY,                 /**< Standby for failover */
    COORD_ROLE_FOLLOWER                 /**< Raft follower (ordering service) */
} coordinator_role_t;

/**
 * @brief Coordinator state machine
 */
typedef enum coordinator_state {
    COORD_STATE_INIT = 0,               /**< Initializing */
    COORD_STATE_JOINING,                /**< Joining pool */
    COORD_STATE_ACTIVE,                 /**< Active in pool */
    COORD_STATE_ELECTION,               /**< Participating in election */
    COORD_STATE_SYNCING,                /**< Syncing state */
    COORD_STATE_FAILED,                 /**< Failed, needs recovery */
    COORD_STATE_SHUTDOWN                /**< Shutting down */
} coordinator_state_t;

/* ============================================================================
 * Transaction Types and Status
 * ============================================================================ */

/**
 * @brief Transaction type
 *
 * WHAT: Category of transaction for routing and endorsement policy
 */
typedef enum mesh_tx_type {
    MESH_TX_NONE = 0,                   /**< Invalid / wildcard for default policy */
    MESH_TX_BELIEF_UPDATE,              /**< Update belief state */
    MESH_TX_STATE_CHANGE,               /**< World state modification */
    MESH_TX_CONSENSUS_VOTE,             /**< Consensus voting */
    MESH_TX_COORDINATOR_ELECTION,       /**< Leader election */
    MESH_TX_CHANNEL_JOIN,               /**< Join channel request */
    MESH_TX_CHANNEL_LEAVE,              /**< Leave channel */
    MESH_TX_CROSS_CHANNEL,              /**< Cross-channel operation */
    MESH_TX_EMERGENCY_OVERRIDE,         /**< Emergency (bypass normal endorsement) */
    MESH_TX_GPU_BATCH,                  /**< GPU batch processing */
    MESH_TX_IMMUNE_RESPONSE,            /**< Immune system action */
    MESH_TX_CONFIG_UPDATE,              /**< Configuration change */

    /* Extensible transaction types - modules can define custom types in this range */
    MESH_TX_CUSTOM_BASE = 0x1000,       /**< Base for custom transaction types */

    /* Cognitive custom types (0x1000 - 0x10FF) */
    MESH_TX_REASONING = 0x1001,         /**< Reasoning/inference operation */
    MESH_TX_PLANNING = 0x1002,          /**< Planning/goal decomposition */
    MESH_TX_ATTENTION_SHIFT = 0x1003,   /**< Attention focus change */
    MESH_TX_METACOGNITION = 0x1004,     /**< Metacognitive monitoring */
    MESH_TX_INNER_DIALOGUE = 0x1005,    /**< Internal speech/thought */

    /* Memory custom types (0x1100 - 0x11FF) */
    MESH_TX_MEMORY_ENCODE = 0x1100,     /**< Memory encoding */
    MESH_TX_MEMORY_CONSOLIDATE = 0x1101,/**< Memory consolidation */
    MESH_TX_MEMORY_RECALL = 0x1102,     /**< Memory recall */
    MESH_TX_MEMORY_FORGET = 0x1103,     /**< Intentional forgetting */

    /* Motor custom types (0x1200 - 0x12FF) */
    MESH_TX_MOTOR_PLAN = 0x1200,        /**< Motor planning */
    MESH_TX_MOTOR_EXECUTE = 0x1201,     /**< Motor execution */
    MESH_TX_MOTOR_INHIBIT = 0x1202,     /**< Motor inhibition */
    MESH_TX_MOTOR_SEQUENCE = 0x1203,    /**< Motor sequence learning */

    /* Sensory custom types (0x1300 - 0x13FF) */
    MESH_TX_VISUAL_PROCESS = 0x1300,    /**< Visual processing */
    MESH_TX_AUDITORY_PROCESS = 0x1301,  /**< Auditory processing */
    MESH_TX_SENSORY_FUSION = 0x1302,    /**< Cross-modal integration */
    MESH_TX_PATTERN_MATCH = 0x1303,     /**< Pattern recognition */

    /* Learning custom types (0x1400 - 0x14FF) */
    MESH_TX_LEARNING_UPDATE = 0x1400,   /**< Synaptic weight update */
    MESH_TX_PLASTICITY_TRIGGER = 0x1401,/**< Plasticity mechanism trigger */
    MESH_TX_REWARD_SIGNAL = 0x1402,     /**< Reward/dopamine signal */
    MESH_TX_ERROR_SIGNAL = 0x1403,      /**< Error/correction signal */

    /* User-defined custom types start here (0x2000+) */
    MESH_TX_USER_DEFINED = 0x2000       /**< Base for user-defined types */
} mesh_tx_type_t;

/** @brief Check if transaction type is a custom/extended type */
#define MESH_TX_IS_CUSTOM(type) ((type) >= MESH_TX_CUSTOM_BASE)

/** @brief Get custom type category (cognitive=0x10, memory=0x11, motor=0x12, etc.) */
#define MESH_TX_CUSTOM_CATEGORY(type) (((type) >> 8) & 0xFF)

/**
 * @brief Transaction lifecycle status
 *
 * Execute-Order-Validate flow:
 * PROPOSED -> ENDORSING -> ORDERED -> VALIDATING -> COMMITTED/FAILED
 */
typedef enum mesh_tx_status {
    MESH_TX_STATUS_NONE = 0,            /**< Invalid */
    MESH_TX_STATUS_PROPOSED,            /**< Transaction proposed */
    MESH_TX_STATUS_ENDORSING,           /**< Collecting endorsements */
    MESH_TX_STATUS_ENDORSED,            /**< Endorsement policy satisfied */
    MESH_TX_STATUS_ORDERING,            /**< Submitted to ordering service */
    MESH_TX_STATUS_ORDERED,             /**< Sequenced by ordering service */
    MESH_TX_STATUS_VALIDATING,          /**< Peers validating */
    MESH_TX_STATUS_COMMITTED,           /**< Successfully committed */
    MESH_TX_STATUS_FAILED,              /**< Transaction failed */
    MESH_TX_STATUS_EXPIRED,             /**< Transaction timed out */
    MESH_TX_STATUS_REJECTED             /**< Rejected by endorsement */
} mesh_tx_status_t;

/* ============================================================================
 * Endorsement Types
 * ============================================================================ */

/**
 * @brief Endorsement result from a single endorser
 */
typedef enum endorsement_result {
    ENDORSEMENT_NONE = 0,               /**< No response yet */
    ENDORSEMENT_APPROVED,               /**< Endorser approves */
    ENDORSEMENT_REJECTED,               /**< Endorser rejects */
    ENDORSEMENT_ABSTAIN,                /**< Endorser abstains */
    ENDORSEMENT_ERROR,                  /**< Error during endorsement */
    ENDORSEMENT_TIMEOUT                 /**< Endorser timed out */
} endorsement_result_t;

/**
 * @brief Single endorsement from a participant
 */
typedef struct mesh_endorsement {
    mesh_participant_id_t endorser_id;  /**< Who endorsed */
    endorsement_result_t result;        /**< Endorsement decision */
    uint8_t signature[MESH_SIGNATURE_SIZE]; /**< Cryptographic signature */
    uint64_t timestamp_ns;              /**< When endorsed */
    uint8_t simulation_hash[32];        /**< Hash of simulated execution */
} mesh_endorsement_t;

/**
 * @brief Endorsement set for a transaction
 */
typedef struct endorsement_set {
    mesh_endorsement_t* endorsements;   /**< Array of endorsements */
    size_t count;                       /**< Number of endorsements */
    size_t capacity;                    /**< Array capacity */
    bool policy_satisfied;              /**< Whether policy is met */
} endorsement_set_t;

/* ============================================================================
 * Credential and Identity
 * ============================================================================ */

/**
 * @brief Credential state
 */
typedef enum credential_state {
    CREDENTIAL_STATE_NONE = 0,          /**< No credential */
    CREDENTIAL_STATE_PENDING,           /**< Awaiting verification */
    CREDENTIAL_STATE_VALID,             /**< Valid and active */
    CREDENTIAL_STATE_SUSPENDED,         /**< Temporarily suspended */
    CREDENTIAL_STATE_REVOKED,           /**< Permanently revoked */
    CREDENTIAL_STATE_EXPIRED            /**< Expired */
} credential_state_t;

/**
 * @brief Participant credential issued by MSP
 *
 * WHAT: Authentication and authorization token
 * WHY:  MSP uses credentials for access control
 */
typedef struct credential {
    uint8_t id[MESH_CREDENTIAL_ID_SIZE];    /**< Unique credential ID */
    mesh_participant_id_t participant_id;   /**< Owner participant */
    credential_state_t state;               /**< Current state */
    uint32_t privilege_level;               /**< Privilege level (0=lowest) */
    uint64_t issued_at_ns;                  /**< Issue timestamp */
    uint64_t expires_at_ns;                 /**< Expiration timestamp */
    uint64_t capabilities;                  /**< Capability bitmask */
    uint8_t public_key[64];                 /**< Public key for signatures */
    uint8_t msp_signature[MESH_SIGNATURE_SIZE]; /**< MSP signature */
} credential_t;

/* ============================================================================
 * Belief Types (FEP Integration)
 * ============================================================================ */

/**
 * @brief Mesh belief for gossip propagation
 *
 * WHAT: Belief with certainty for FEP-driven consensus
 * WHY:  Gossip beliefs across channel for distributed cognition
 */
typedef struct mesh_belief {
    uint32_t belief_id;                     /**< Unique belief ID */
    mesh_participant_id_t source;           /**< Originating participant */
    mesh_channel_id_t channel;              /**< Home channel */
    float certainty;                        /**< Belief certainty [0, 1] */
    float belief_vector[MESH_BELIEF_VECTOR_DIM]; /**< Neural encoding */
    uint32_t vector_dim;                    /**< Actual vector dimension used */
    uint64_t timestamp_ns;                  /**< Creation time */
    uint32_t propagation_count;             /**< Times propagated */
} mesh_belief_t;

/**
 * @brief Set of beliefs
 */
typedef struct mesh_belief_set {
    mesh_belief_t* beliefs;                 /**< Array of beliefs */
    size_t count;                           /**< Number of beliefs */
    size_t capacity;                        /**< Array capacity */
} mesh_belief_set_t;

/**
 * @brief Consensus result from channel
 */
typedef struct mesh_consensus {
    mesh_channel_id_t channel;              /**< Channel that reached consensus */
    mesh_belief_t consensus_belief;         /**< Agreed belief */
    float consensus_strength;               /**< Strength of consensus [0, 1] */
    uint32_t participating_count;           /**< Participants in consensus */
    uint64_t convergence_time_ns;           /**< Time to converge */
    float free_energy;                      /**< Final free energy */
} mesh_consensus_t;

/* ============================================================================
 * World State Delta
 * ============================================================================ */

/**
 * @brief World state change contributed by participant
 */
typedef struct world_state_delta {
    mesh_participant_id_t contributor;      /**< Who contributed */
    uint8_t* key;                           /**< State key */
    size_t key_len;                         /**< Key length */
    uint8_t* value;                         /**< New value */
    size_t value_len;                       /**< Value length */
    uint64_t version;                       /**< Vector clock version */
    bool is_delete;                         /**< True if this is a deletion */
} world_state_delta_t;

/* ============================================================================
 * Health Metrics
 * ============================================================================ */

/**
 * @brief Health metrics reported by participants
 */
typedef struct health_metrics {
    mesh_participant_id_t participant;      /**< Reporting participant */
    float cpu_utilization;                  /**< CPU usage [0, 1] */
    float memory_utilization;               /**< Memory usage [0, 1] */
    uint64_t transactions_processed;        /**< Total transactions */
    uint64_t transactions_failed;           /**< Failed transactions */
    float avg_latency_ms;                   /**< Average response latency */
    uint64_t last_heartbeat_ns;             /**< Last heartbeat timestamp */
    bool is_healthy;                        /**< Overall health status */
} health_metrics_t;

/* ============================================================================
 * Timing Configuration
 * ============================================================================ */

/**
 * @brief Timing configuration for a hierarchy level
 *
 * WHAT: Pink noise timing parameters
 * WHY:  Bio-plausible jitter prevents synchronized failures
 */
typedef struct mesh_timing {
    float base_interval_ms;                 /**< Base timing interval */
    float jitter_amplitude_ms;              /**< Pink noise amplitude */
    float min_interval_ms;                  /**< Minimum interval */
    float max_interval_ms;                  /**< Maximum interval */
} mesh_timing_t;

/* ============================================================================
 * Transaction Result
 * ============================================================================ */

/**
 * @brief Result of transaction processing
 */
typedef struct mesh_result {
    mesh_tx_id_t tx_id;                     /**< Transaction ID */
    mesh_tx_status_t status;                /**< Final status */
    nimcp_error_t error;                    /**< Error code if failed */
    char error_msg[128];                    /**< Error message */
    uint64_t commit_timestamp_ns;           /**< When committed */
    uint8_t result_hash[32];                /**< Hash of result */
} mesh_result_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Transaction completion callback
 */
typedef void (*mesh_tx_callback_t)(
    const mesh_result_t* result,
    void* user_ctx
);

/**
 * @brief Belief received callback
 */
typedef void (*mesh_belief_callback_t)(
    const mesh_belief_t* belief,
    void* user_ctx
);

/**
 * @brief Consensus reached callback
 */
typedef void (*mesh_consensus_callback_t)(
    const mesh_consensus_t* consensus,
    void* user_ctx
);

/**
 * @brief Coordinator election callback
 */
typedef void (*mesh_election_callback_t)(
    mesh_pool_id_t pool_id,
    mesh_participant_id_t new_leader,
    uint64_t term,
    void* user_ctx
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Create participant ID from components
 *
 * @param channel Channel ID
 * @param type Participant type
 * @param local_id Local identifier within channel
 * @return Combined participant ID
 */
static inline mesh_participant_id_t mesh_make_participant_id(
    mesh_channel_id_t channel,
    mesh_participant_type_t type,
    uint32_t local_id
) {
    return ((uint64_t)channel << 48) | ((uint64_t)type << 32) | local_id;
}

/**
 * @brief Extract channel from participant ID
 */
static inline mesh_channel_id_t mesh_get_channel(mesh_participant_id_t id) {
    return (mesh_channel_id_t)(id >> 48);
}

/**
 * @brief Extract type from participant ID
 */
static inline mesh_participant_type_t mesh_get_participant_type(mesh_participant_id_t id) {
    return (mesh_participant_type_t)((id >> 32) & 0xFFFF);
}

/**
 * @brief Extract local ID from participant ID
 */
static inline uint32_t mesh_get_local_id(mesh_participant_id_t id) {
    return (uint32_t)(id & 0xFFFFFFFF);
}

/**
 * @brief Compare two transaction IDs
 *
 * @return <0 if a < b, 0 if equal, >0 if a > b
 */
static inline int mesh_tx_id_compare(const mesh_tx_id_t* a, const mesh_tx_id_t* b) {
    if (!a || !b) return a ? 1 : (b ? -1 : 0);
    if (a->channel != b->channel) return (int)a->channel - (int)b->channel;
    if (a->proposer != b->proposer) return (a->proposer < b->proposer) ? -1 : 1;
    if (a->sequence != b->sequence) return (a->sequence < b->sequence) ? -1 : 1;
    return 0;
}

/**
 * @brief Check if transaction ID is valid
 */
static inline bool mesh_tx_id_is_valid(const mesh_tx_id_t* id) {
    return id && id->proposer != 0 && id->timestamp_ns != 0;
}

/* ============================================================================
 * String Conversion
 * ============================================================================ */

/**
 * @brief Get participant type name
 */
const char* mesh_participant_type_to_string(mesh_participant_type_t type);

/**
 * @brief Get coordinator role name
 */
const char* mesh_coordinator_role_to_string(coordinator_role_t role);

/**
 * @brief Get coordinator state name
 */
const char* mesh_coordinator_state_to_string(coordinator_state_t state);

/**
 * @brief Get transaction type name
 */
const char* mesh_tx_type_to_string(mesh_tx_type_t type);

/**
 * @brief Get transaction status name
 */
const char* mesh_tx_status_to_string(mesh_tx_status_t status);

/**
 * @brief Get endorsement result name
 */
const char* mesh_endorsement_result_to_string(endorsement_result_t result);

/**
 * @brief Get credential state name
 */
const char* mesh_credential_state_to_string(credential_state_t state);

/**
 * @brief Get channel name for well-known channels
 */
const char* mesh_channel_name(mesh_channel_id_t channel);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_TYPES_H */
