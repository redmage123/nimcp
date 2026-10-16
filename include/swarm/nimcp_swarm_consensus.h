/**
 * @file nimcp_swarm_consensus.h
 * @brief NIMCP Swarm Consensus Voting for Collective Decision-Making
 *
 * WHAT: Byzantine fault-tolerant voting mechanism for swarm coordination
 * WHY:  Enable decentralized decision-making with resilience to faulty drones
 * HOW:  Weighted voting with quorum requirements and confidence scoring
 *
 * ARCHITECTURE:
 * ┌──────────────────────────────────────────────────────────────────┐
 * │                    SWARM CONSENSUS VOTING                        │
 * ├──────────────────────────────────────────────────────────────────┤
 * │  Proposer     │      Active Votes      │      Voters             │
 * │  (Drone N)    │                        │    (All Drones)         │
 * │      │        │  ┌──────────────────┐  │        │                │
 * │      │        │  │ Proposal 1       │  │        │                │
 * │      ├───────►│  │ - Topic          │◄─┼────────┤                │
 * │      │        │  │ - Deadline       │  │  Vote(Agree/0.9)        │
 * │      │        │  │ - Quorum         │  │        │                │
 * │      │        │  │ - Threshold      │  │        │                │
 * │      │        │  │ - Votes: [...]   │  │  Vote(Disagree/0.7)     │
 * │      │        │  └──────────────────┘  │        │                │
 * │      │        │          │             │        │                │
 * │      │        │          ▼             │        ▼                │
 * │      │        │  Byzantine FT Logic    │  Callback Invoked       │
 * │      │        │  (≤1/3 faulty OK)      │  on Result              │
 * │      │        │          │             │                         │
 * │      │        │          ▼             │                         │
 * │      │        │  ┌──────────────────┐  │                         │
 * │      │        │  │ Vote Result      │  │                         │
 * │      │        │  │ - Passed/Failed  │  │                         │
 * │      │        │  │ - Vote Counts    │  │                         │
 * │      │        │  │ - Weighted Score │  │                         │
 * │      │        │  └──────────────────┘  │                         │
 * └──────────────────────────────────────────────────────────────────┘
 *
 * BYZANTINE FAULT TOLERANCE:
 * - Tolerates up to 1/3 malicious or faulty drones
 * - Weighted voting by voter confidence
 * - Quorum requirements prevent minority attacks
 * - Confidence thresholding filters unreliable votes
 *
 * FEATURES:
 * - Multiple vote topics (target priority, formation, retreat, etc.)
 * - Async callback notification on vote completion
 * - Time-based proposal expiration
 * - Confidence-weighted voting
 * - Thread-safe operations
 * - Comprehensive statistics tracking
 * - Bio-async integration ready
 *
 * USAGE:
 * 1. Create context: swarm_consensus_create()
 * 2. Propose vote: swarm_consensus_propose()
 * 3. Submit votes: swarm_consensus_vote() / swarm_consensus_receive_vote()
 * 4. Check results: swarm_consensus_check_result()
 * 5. Cleanup: swarm_consensus_destroy()
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_SWARM_CONSENSUS_H
#define NIMCP_SWARM_CONSENSUS_H

#include "utils/error/nimcp_error_codes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Magic number for structure validation */
#define NIMCP_SWARM_CONSENSUS_MAGIC 0x53574D56  /* "SWMV" */

/** @brief Maximum number of concurrent active votes */
#define SWARM_MAX_ACTIVE_VOTES 32

/** @brief Maximum number of votes per proposal */
#define SWARM_MAX_VOTES_PER_PROPOSAL 256

/** @brief Byzantine fault tolerance threshold (1/3) */
#define SWARM_BFT_THRESHOLD 0.333333f

/** @brief Default agreement threshold (2/3 majority) */
#define SWARM_DEFAULT_THRESHOLD 0.666667f

/* ============================================================================
 * Byzantine Fault Detection Thresholds
 * ============================================================================ */

/** @brief Upper bound for extreme confidence (votes >= this are suspicious) */
#define BYZANTINE_EXTREME_CONFIDENCE_HIGH   0.99f

/** @brief Lower bound for extreme confidence (votes <= this are suspicious) */
#define BYZANTINE_EXTREME_CONFIDENCE_LOW    0.01f

/** @brief High confidence threshold for pattern detection (suspicious if too many) */
#define BYZANTINE_HIGH_CONFIDENCE_THRESHOLD 0.95f

/** @brief Number of vote rejections before drone is quarantined */
#define BYZANTINE_EXCESSIVE_VOTE_THRESHOLD  100

//=============================================================================
// Vote Topics and Choices
//=============================================================================

/**
 * @brief Vote topic types
 *
 * WHAT: Categories of decisions requiring consensus
 * WHY:  Different topics have different semantic meanings
 * HOW:  Enum-based categorization
 */
#ifndef SWARM_VOTE_TOPIC_DEFINED
#define SWARM_VOTE_TOPIC_DEFINED
typedef enum {
    VOTE_TOPIC_TARGET_PRIORITY,      /**< Prioritize attack target */
    VOTE_TOPIC_FORMATION_CHANGE,     /**< Change swarm formation */
    VOTE_TOPIC_RETREAT,              /**< Initiate retreat */
    VOTE_TOPIC_RESOURCE_ALLOCATION,  /**< Allocate swarm resources */
    VOTE_TOPIC_LEADER_ELECTION,      /**< Elect new swarm leader */
    VOTE_TOPIC_CUSTOM                /**< Custom user-defined topic */
} swarm_vote_topic_t;
#endif

/**
 * @brief Vote choice
 *
 * WHAT: Voter's decision on proposal
 * WHY:  Standard yes/no/abstain voting model
 * HOW:  Three-state enum
 */
typedef enum {
    VOTE_CHOICE_AGREE,               /**< Support proposal */
    VOTE_CHOICE_DISAGREE,            /**< Oppose proposal */
    VOTE_CHOICE_ABSTAIN              /**< No opinion */
} swarm_vote_choice_t;

/**
 * @brief Vote status
 *
 * WHAT: Current state of vote proposal
 * WHY:  Track lifecycle of votes
 * HOW:  State machine enum
 */
typedef enum {
    VOTE_STATUS_PENDING,             /**< Vote in progress */
    VOTE_STATUS_PASSED,              /**< Vote succeeded */
    VOTE_STATUS_FAILED,              /**< Vote failed */
    VOTE_STATUS_EXPIRED,             /**< Vote timed out */
    VOTE_STATUS_CANCELLED            /**< Vote cancelled by proposer */
} swarm_vote_status_t;

//=============================================================================
// Vote Structures
//=============================================================================

/**
 * @brief Vote proposal
 *
 * WHAT: Proposed decision requiring consensus
 * WHY:  Encapsulates all information needed for voting
 * HOW:  Structure with topic, values, deadline, quorum
 */
typedef struct {
    uint32_t proposal_id;            /**< Unique proposal identifier */
    uint16_t proposer_drone;         /**< ID of drone that proposed */
    swarm_vote_topic_t topic;        /**< Topic category */
    float proposal_value[4];         /**< Topic-specific values */
    uint64_t deadline_ms;            /**< Deadline timestamp (epoch ms) */
    uint32_t quorum_required;        /**< Minimum voters needed */
    float threshold;                 /**< Agreement threshold (0.0-1.0) */
} swarm_vote_proposal_t;

/**
 * @brief Vote response
 *
 * WHAT: Individual drone's vote on proposal
 * WHY:  Records each voter's choice and confidence
 * HOW:  Structure with choice, confidence, drone ID
 */
typedef struct {
    uint32_t proposal_id;            /**< Which proposal this votes on */
    uint16_t voter_drone;            /**< ID of voting drone */
    swarm_vote_choice_t choice;      /**< Agree/Disagree/Abstain */
    float confidence;                /**< Voter confidence [0.0-1.0] */
} swarm_vote_response_t;

/**
 * @brief Vote result
 *
 * WHAT: Final outcome of completed vote
 * WHY:  Communicates decision to all participants
 * HOW:  Structure with counts, weighted scores, pass/fail
 */
typedef struct {
    uint32_t proposal_id;            /**< Which proposal completed */
    bool passed;                     /**< Did vote pass threshold */
    uint32_t agree_count;            /**< Number of agree votes */
    uint32_t disagree_count;         /**< Number of disagree votes */
    uint32_t abstain_count;          /**< Number of abstain votes */
    float weighted_agreement;        /**< Confidence-weighted agreement */
    swarm_vote_status_t status;      /**< Final status */
} swarm_vote_result_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Vote completion callback
 *
 * WHAT: Notification when vote completes
 * WHY:  Async notification of results
 * HOW:  Function pointer called on completion
 *
 * @param result Final vote result
 * @param user_ctx User-provided context
 */
typedef void (*swarm_vote_callback_t)(
    const swarm_vote_result_t* result,
    void* user_ctx
);

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Consensus configuration
 *
 * WHAT: Configuration for consensus system
 * WHY:  Customize behavior per swarm
 * HOW:  Structure with limits and parameters
 */
typedef struct {
    uint16_t drone_id;               /**< This drone's ID */
    uint32_t max_active_votes;       /**< Max concurrent votes (≤32) */
    uint32_t max_votes_per_proposal; /**< Max votes per proposal (≤256) */
    float min_confidence;            /**< Minimum confidence to count (0.0-1.0) */
    bool enable_byzantine_ft;        /**< Enable Byzantine fault tolerance */
    bool enable_logging;             /**< Enable detailed logging */
    void* user_data;                 /**< User context for callbacks */
    bool enable_quantum_consensus;   /**< Enable quantum-accelerated consensus */
} swarm_consensus_config_t;

/**
 * @brief Consensus statistics
 *
 * WHAT: Runtime statistics for monitoring
 * WHY:  Track consensus health and performance
 * HOW:  Counter structure
 */
typedef struct {
    uint64_t proposals_created;      /**< Total proposals created */
    uint64_t proposals_passed;       /**< Proposals that passed */
    uint64_t proposals_failed;       /**< Proposals that failed */
    uint64_t proposals_expired;      /**< Proposals that timed out */
    uint64_t proposals_cancelled;    /**< Proposals cancelled */
    uint64_t votes_cast;             /**< Total votes cast */
    uint64_t votes_received;         /**< Total votes received */
    uint64_t byzantine_faults_detected; /**< Suspected faulty votes */
    uint32_t active_votes;           /**< Currently active votes */
} swarm_consensus_stats_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/**
 * @brief Opaque consensus context handle
 *
 * WHAT: Handle to consensus voting system
 * WHY:  Encapsulation and thread safety
 * HOW:  Opaque pointer to internal structure
 */
typedef struct swarm_consensus_context* swarm_consensus_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible default config
 * WHY:  Simplifies initialization
 * HOW:  Static defaults with Byzantine FT enabled
 *
 * @param drone_id This drone's identifier
 * @return Default configuration
 */
swarm_consensus_config_t swarm_consensus_default_config(uint16_t drone_id);

/**
 * @brief Create consensus context
 *
 * WHAT: Allocates and initializes consensus system
 * WHY:  Begin participating in swarm voting
 * HOW:  Allocates state, initializes locks, sets config
 *
 * @param config Configuration (NULL for defaults with drone_id=0)
 * @return Consensus handle or NULL on failure
 */
swarm_consensus_t swarm_consensus_create(const swarm_consensus_config_t* config);

/**
 * @brief Destroy consensus context
 *
 * WHAT: Frees all consensus resources
 * WHY:  Clean shutdown
 * HOW:  Cancels active votes, invokes callbacks, frees memory
 *
 * @param ctx Consensus context
 */
void swarm_consensus_destroy(swarm_consensus_t ctx);

//=============================================================================
// Voting API
//=============================================================================

/**
 * @brief Propose a vote
 *
 * WHAT: Submit new proposal for consensus
 * WHY:  Initiate collective decision-making
 * HOW:  Creates proposal, assigns ID, starts tracking
 *
 * @param ctx Consensus context
 * @param topic Vote topic
 * @param proposal_values Topic-specific values (can be NULL)
 * @param deadline_ms Deadline timestamp (epoch ms, 0=no deadline)
 * @param quorum_required Minimum voters (0=all drones)
 * @param threshold Agreement threshold (0.0=use default)
 * @param callback Completion callback (can be NULL)
 * @param callback_ctx Context for callback (can be NULL)
 * @param proposal_id_out Output: assigned proposal ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_consensus_propose(
    swarm_consensus_t ctx,
    swarm_vote_topic_t topic,
    const float* proposal_values,
    uint64_t deadline_ms,
    uint32_t quorum_required,
    float threshold,
    swarm_vote_callback_t callback,
    void* callback_ctx,
    uint32_t* proposal_id_out
);

/**
 * @brief Cast vote on own proposal
 *
 * WHAT: Submit this drone's vote
 * WHY:  Participate in consensus decision
 * HOW:  Creates vote response, processes locally
 *
 * @param ctx Consensus context
 * @param proposal_id Proposal to vote on
 * @param choice Agree/Disagree/Abstain
 * @param confidence Confidence level [0.0-1.0]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_consensus_vote(
    swarm_consensus_t ctx,
    uint32_t proposal_id,
    swarm_vote_choice_t choice,
    float confidence
);

/**
 * @brief Receive vote from another drone
 *
 * WHAT: Process incoming vote from network
 * WHY:  Aggregate votes from swarm members
 * HOW:  Validates vote, updates counts, checks completion
 *
 * @param ctx Consensus context
 * @param vote Vote response from remote drone
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_consensus_receive_vote(
    swarm_consensus_t ctx,
    const swarm_vote_response_t* vote
);

/**
 * @brief Check if vote is complete
 *
 * WHAT: Determine if vote has reached conclusion
 * WHY:  Poll for completion without callback
 * HOW:  Checks quorum, deadline, threshold
 *
 * @param ctx Consensus context
 * @param proposal_id Proposal to check
 * @param is_complete_out Output: true if complete
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_consensus_check_result(
    swarm_consensus_t ctx,
    uint32_t proposal_id,
    bool* is_complete_out
);

/**
 * @brief Get vote result
 *
 * WHAT: Retrieve final or current result
 * WHY:  Access vote outcome details
 * HOW:  Copies result structure
 *
 * @param ctx Consensus context
 * @param proposal_id Proposal to query
 * @param result_out Output: vote result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_consensus_get_result(
    swarm_consensus_t ctx,
    uint32_t proposal_id,
    swarm_vote_result_t* result_out
);

/**
 * @brief Cancel active proposal
 *
 * WHAT: Cancel voting on proposal
 * WHY:  Abort obsolete or erroneous votes
 * HOW:  Marks cancelled, invokes callback with status
 *
 * @param ctx Consensus context
 * @param proposal_id Proposal to cancel
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_consensus_cancel(
    swarm_consensus_t ctx,
    uint32_t proposal_id
);

/**
 * @brief Cleanup expired votes
 *
 * WHAT: Remove timed-out votes
 * WHY:  Prevent memory leaks from abandoned votes
 * HOW:  Checks deadlines, marks expired, frees slots
 *
 * @param ctx Consensus context
 * @param current_time_ms Current timestamp (epoch ms)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_consensus_cleanup_expired(
    swarm_consensus_t ctx,
    uint64_t current_time_ms
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get statistics
 *
 * WHAT: Retrieve consensus statistics
 * WHY:  Monitor system health
 * HOW:  Copies stats structure
 *
 * @param ctx Consensus context
 * @param stats_out Output: statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_consensus_get_stats(
    swarm_consensus_t ctx,
    swarm_consensus_stats_t* stats_out
);

/**
 * @brief Get proposal info
 *
 * WHAT: Retrieve proposal details
 * WHY:  Inspect active or completed proposals
 * HOW:  Copies proposal structure
 *
 * @param ctx Consensus context
 * @param proposal_id Proposal to query
 * @param proposal_out Output: proposal details
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_consensus_get_proposal(
    swarm_consensus_t ctx,
    uint32_t proposal_id,
    swarm_vote_proposal_t* proposal_out
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get topic name
 *
 * WHAT: Convert topic enum to string
 * WHY:  Human-readable logging
 * HOW:  Static string table
 *
 * @param topic Vote topic
 * @return String name
 */
const char* swarm_vote_topic_name(swarm_vote_topic_t topic);

/**
 * @brief Get choice name
 *
 * WHAT: Convert choice enum to string
 * WHY:  Human-readable logging
 * HOW:  Static string table
 *
 * @param choice Vote choice
 * @return String name
 */
const char* swarm_vote_choice_name(swarm_vote_choice_t choice);

/**
 * @brief Get status name
 *
 * WHAT: Convert status enum to string
 * WHY:  Human-readable logging
 * HOW:  Static string table
 *
 * @param status Vote status
 * @return String name
 */
const char* swarm_vote_status_name(swarm_vote_status_t status);

//=============================================================================
// Bio-async Integration API
//=============================================================================

/**
 * @brief Connect consensus context to bio-async router
 *
 * WHAT: Register consensus module with bio-async messaging system
 * WHY:  Enable inter-module messaging for consensus coordination
 * HOW:  Register with BIO_MODULE_SWARM_CONSENSUS ID
 *
 * @param ctx Consensus context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_consensus_connect_bio_async(swarm_consensus_t ctx);

/**
 * @brief Disconnect consensus context from bio-async router
 *
 * WHAT: Unregister from bio-async messaging system
 * WHY:  Clean shutdown of messaging
 * HOW:  Deregister module and cleanup
 *
 * @param ctx Consensus context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_consensus_disconnect_bio_async(swarm_consensus_t ctx);

/**
 * @brief Check if consensus is connected to bio-async
 *
 * WHAT: Query bio-async connection status
 * WHY:  Verify messaging availability
 * HOW:  Check bio_async_enabled flag
 *
 * @param ctx Consensus context
 * @return true if connected, false otherwise
 */
bool swarm_consensus_is_bio_async_connected(const swarm_consensus_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_CONSENSUS_H */
