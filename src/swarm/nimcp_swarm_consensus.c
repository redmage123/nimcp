/**
 * @file nimcp_swarm_consensus.c
 * @brief NIMCP Swarm Consensus Voting Implementation
 *
 * WHAT: Implementation of Byzantine fault-tolerant voting for swarm coordination
 * WHY:  Enable decentralized decision-making resilient to faulty drones
 * HOW:  Weighted voting with confidence, quorum, and threshold checks
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe operations
 * - Comprehensive logging
 * - Bio-async integration ready
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include <stddef.h>  /* for NULL */
//=============================================================================
// Includes
//=============================================================================

#include "swarm/nimcp_swarm_consensus.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "security/nimcp_bbb_helpers.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include <string.h>
#include <time.h>
#include <math.h>

#include "utils/exception/nimcp_exception_macros.h"

/* Quantum bridge integration */
#define NIMCP_SWARM_QUANTUM_BRIDGE_IMPLEMENTATION
#include "swarm/nimcp_swarm_consensus_quantum_bridge.h"

#define LOG_MODULE "swarm_consensus"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_atomic.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_consensus)

/* Maximum drone ID for vote tracking (to limit memory footprint) */
#define MAX_TRACKED_DRONE_ID 1024

/* Thread-safe BBB registration using atomic compare-exchange */
static nimcp_atomic_bool_t g_bbb_registered = {0};
static uint32_t g_vote_counts_by_drone[MAX_TRACKED_DRONE_ID] = {0};  // Track votes per drone
static nimcp_mutex_t g_vote_tracking_mutex = NIMCP_MUTEX_INITIALIZER;  // Protect vote tracking

/**
 * @brief Initialize BBB security for consensus (thread-safe)
 *
 * Uses atomic compare-exchange to ensure exactly one thread
 * performs the registration, preventing TOCTOU race conditions.
 */
static void consensus_init_bbb(void)
{
    bool expected = false;
    if (nimcp_atomic_compare_exchange_bool(&g_bbb_registered, &expected, true,
                                           NIMCP_MEMORY_ORDER_ACQ_REL)) {
        bbb_register_module("swarm_consensus", BBB_MODULE_TYPE_SWARM);
        bbb_audit_log(BBB_AUDIT_INFO, "swarm_consensus", "init", "Module registered with BBB");
    }
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Active vote tracking
 *
 * WHAT: Tracks votes for an active proposal
 * WHY:  Store all votes until decision reached
 * HOW:  Dynamic vote array with counts and metadata
 */
typedef struct {
    swarm_vote_proposal_t proposal;          /**< Proposal details */
    swarm_vote_response_t votes[SWARM_MAX_VOTES_PER_PROPOSAL];
    uint32_t vote_count;                     /**< Number of votes received */
    swarm_vote_result_t result;              /**< Current/final result */
    swarm_vote_callback_t callback;          /**< Completion callback */
    void* callback_ctx;                      /**< Callback context */
    bool active;                             /**< Is slot active */
    bool callback_invoked;                   /**< Callback already called */
} active_vote_t;

/**
 * @brief Consensus context structure
 *
 * WHAT: Main consensus system state
 * WHY:  Encapsulate all state with thread safety
 * HOW:  Configuration, active votes, stats, mutex
 */
struct swarm_consensus_context {
    uint32_t magic;                          /**< Validation magic */
    swarm_consensus_config_t config;         /**< Configuration */
    active_vote_t active_votes[SWARM_MAX_ACTIVE_VOTES];
    uint32_t next_proposal_id;               /**< Next proposal ID to assign */
    swarm_consensus_stats_t stats;           /**< Statistics */
    nimcp_mutex_t mutex;                     /**< Thread safety */
    bool mutex_initialized;                  /**< Mutex init flag */

    // Bio-async integration
    bio_module_context_t bio_ctx;            /**< Bio-async module context */
    bool bio_async_enabled;                  /**< Whether bio-async is active */

    // Quantum consensus
    swarm_quantum_bridge_t* quantum_bridge;  /**< Quantum-accelerated consensus */
    uint64_t quantum_decisions;              /**< Number of quantum-accelerated decisions */
};

//=============================================================================
// Forward Declarations
//=============================================================================

static nimcp_error_t evaluate_vote_result(
    swarm_consensus_t ctx,
    active_vote_t* vote,
    uint64_t current_time_ms
);

static void invoke_callback(active_vote_t* vote);

static bool is_byzantine_fault(
    const active_vote_t* vote,
    const swarm_vote_response_t* new_vote
);

//=============================================================================
// Utility Functions - Name Conversions
//=============================================================================

/**
 * @brief Get human-readable topic name
 */
const char* swarm_vote_topic_name(swarm_vote_topic_t topic)
{
    static const char* names[] = {
        "TARGET_PRIORITY",
        "FORMATION_CHANGE",
        "RETREAT",
        "RESOURCE_ALLOCATION",
        "LEADER_ELECTION",
        "CUSTOM"
    };

    if (topic < 0 || topic > VOTE_TOPIC_CUSTOM) {
        return "INVALID";
    }

    return names[topic];
}

/**
 * @brief Get human-readable choice name
 */
const char* swarm_vote_choice_name(swarm_vote_choice_t choice)
{
    static const char* names[] = {
        "AGREE",
        "DISAGREE",
        "ABSTAIN"
    };

    if (choice < 0 || choice > VOTE_CHOICE_ABSTAIN) {
        return "INVALID";
    }

    return names[choice];
}

/**
 * @brief Get human-readable status name
 */
const char* swarm_vote_status_name(swarm_vote_status_t status)
{
    static const char* names[] = {
        "PENDING",
        "PASSED",
        "FAILED",
        "EXPIRED",
        "CANCELLED"
    };

    if (status < 0 || status > VOTE_STATUS_CANCELLED) {
        return "INVALID";
    }

    return names[status];
}

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults
 * WHY:  Simplify initialization
 * HOW:  Static defaults with Byzantine FT enabled
 */
swarm_consensus_config_t swarm_consensus_default_config(uint16_t drone_id)
{
    swarm_consensus_config_t config;
    config.drone_id = drone_id;
    config.max_active_votes = SWARM_MAX_ACTIVE_VOTES;
    config.max_votes_per_proposal = SWARM_MAX_VOTES_PER_PROPOSAL;
    config.min_confidence = 0.3F;
    config.enable_byzantine_ft = true;
    config.enable_logging = true;
    config.user_data = NULL;
    config.enable_quantum_consensus = true;  /* Enabled by default */
    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create consensus context
 *
 * WHAT: Allocates and initializes consensus system
 * WHY:  Begin participating in swarm voting
 * HOW:  Allocates state, initializes locks, sets config
 */
swarm_consensus_t swarm_consensus_create(const swarm_consensus_config_t* config)
{
    consensus_init_bbb();

    /* Use default config if none provided */
    swarm_consensus_config_t default_cfg = swarm_consensus_default_config(0);
    if (!config) {
        config = &default_cfg;
    }

    /* Validate config with BBB */
    if (!bbb_check_pointer(config, "swarm_consensus_create")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_consensus", "create_error", "Invalid config pointer");
        LOG_ERROR("Invalid config pointer in swarm_consensus_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_consensus_create: bbb_check_pointer is NULL");
        return NULL;
    }

    /* Allocate context */
    swarm_consensus_t ctx = (swarm_consensus_t)nimcp_calloc(1, sizeof(*ctx));
    if (!ctx) {
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_consensus", "create_error", "Failed to allocate context");
        LOG_ERROR("Failed to allocate consensus context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate consensus context");
        return NULL;
    }

    /* Initialize fields */
    ctx->magic = NIMCP_SWARM_CONSENSUS_MAGIC;
    ctx->config = *config;
    ctx->next_proposal_id = 1;
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    memset(ctx->active_votes, 0, sizeof(ctx->active_votes));

    /* Initialize mutex */
    if (nimcp_mutex_init(&ctx->mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize consensus mutex");
        nimcp_free(ctx);
        return NULL;
    }
    ctx->mutex_initialized = true;

    /* Initialize bio-async fields */
    ctx->bio_ctx = NULL;
    ctx->bio_async_enabled = false;

    /* Initialize quantum bridge */
    ctx->quantum_bridge = NULL;
    ctx->quantum_decisions = 0;
    if (ctx->config.enable_quantum_consensus) {
        swarm_quantum_config_t qconfig = swarm_quantum_default_config();
        ctx->quantum_bridge = swarm_quantum_bridge_create(&qconfig);
        if (ctx->quantum_bridge) {
            LOG_INFO("Quantum-accelerated consensus enabled");
        }
    }

    bbb_audit_log(BBB_AUDIT_INFO, "swarm_consensus", "created",
                 "Consensus context created for drone %u", config->drone_id);
    LOG_INFO("Consensus context created for drone %u", config->drone_id);
    return ctx;
}

/**
 * @brief Destroy consensus context
 *
 * WHAT: Frees all consensus resources
 * WHY:  Clean shutdown
 * HOW:  Cancels active votes, invokes callbacks, frees memory
 */
void swarm_consensus_destroy(swarm_consensus_t ctx)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (ctx->bio_async_enabled) {
        swarm_consensus_disconnect_bio_async(ctx);
    }

    /* Cancel all active votes and collect callbacks to invoke outside lock */
    typedef struct { swarm_vote_result_t result; swarm_vote_callback_t cb; void* cb_ctx; } deferred_cb_t;
    deferred_cb_t deferred_callbacks[SWARM_MAX_ACTIVE_VOTES];
    uint32_t deferred_count = 0;

    nimcp_mutex_lock(&ctx->mutex);

    for (uint32_t i = 0; i < SWARM_MAX_ACTIVE_VOTES; i++) {
        active_vote_t* vote = &ctx->active_votes[i];
        if (vote->active && !vote->callback_invoked) {
            vote->result.status = VOTE_STATUS_CANCELLED;
            vote->callback_invoked = true;
            if (vote->callback && deferred_count < SWARM_MAX_ACTIVE_VOTES) {
                deferred_callbacks[deferred_count].result = vote->result;
                deferred_callbacks[deferred_count].cb = vote->callback;
                deferred_callbacks[deferred_count].cb_ctx = vote->callback_ctx;
                deferred_count++;
            }
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    /* Invoke callbacks outside the lock to prevent deadlock */
    for (uint32_t i = 0; i < deferred_count; i++) {
        deferred_callbacks[i].cb(&deferred_callbacks[i].result, deferred_callbacks[i].cb_ctx);
    }

    /* Destroy mutex */
    if (ctx->mutex_initialized) {
        nimcp_mutex_destroy(&ctx->mutex);
    }

    /* Destroy quantum bridge */
    if (ctx->quantum_bridge) {
        swarm_quantum_bridge_destroy(ctx->quantum_bridge);
    }

    /* Clear magic and free */
    ctx->magic = 0;
    nimcp_free(ctx);

    LOG_INFO("Consensus context destroyed");
}

//=============================================================================
// Voting Functions
//=============================================================================

/**
 * @brief Propose a vote
 *
 * WHAT: Submit new proposal for consensus
 * WHY:  Initiate collective decision-making
 * HOW:  Creates proposal, assigns ID, starts tracking
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
    uint32_t* proposal_id_out)
{
    /* Validate parameters with BBB */
    if (!bbb_check_pointer(ctx, "swarm_consensus_propose") ||
        ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_consensus", "propose_error", "Invalid context");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bbb_check_pointer(proposal_id_out, "swarm_consensus_propose")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_consensus", "propose_error", "Invalid proposal_id_out");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (proposal_values && !bbb_check_pointer(proposal_values, "swarm_consensus_propose")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_consensus", "propose_error", "Invalid proposal_values");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (threshold <= 0.0F) {
        threshold = SWARM_DEFAULT_THRESHOLD;
    }

    if (threshold > 1.0F) {
        threshold = 1.0F;
    }

    nimcp_mutex_lock(&ctx->mutex);

    /* Find free slot */
    active_vote_t* vote_slot = NULL;
    for (uint32_t i = 0; i < ctx->config.max_active_votes; i++) {
        if (!ctx->active_votes[i].active) {
            vote_slot = &ctx->active_votes[i];
            break;
        }
    }

    if (!vote_slot) {
        nimcp_mutex_unlock(&ctx->mutex);
        LOG_ERROR("No free vote slots available");
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Initialize proposal */
    memset(vote_slot, 0, sizeof(*vote_slot));
    vote_slot->proposal.proposal_id = ctx->next_proposal_id++;
    vote_slot->proposal.proposer_drone = ctx->config.drone_id;
    vote_slot->proposal.topic = topic;
    vote_slot->proposal.deadline_ms = deadline_ms;
    vote_slot->proposal.quorum_required = quorum_required;
    vote_slot->proposal.threshold = threshold;

    if (proposal_values) {
        memcpy(vote_slot->proposal.proposal_value, proposal_values,
               sizeof(vote_slot->proposal.proposal_value));
    }

    /* Initialize result */
    vote_slot->result.proposal_id = vote_slot->proposal.proposal_id;
    vote_slot->result.status = VOTE_STATUS_PENDING;
    vote_slot->result.passed = false;

    /* Set callback */
    vote_slot->callback = callback;
    vote_slot->callback_ctx = callback_ctx;
    vote_slot->active = true;
    vote_slot->callback_invoked = false;

    /* Update stats */
    ctx->stats.proposals_created++;
    ctx->stats.active_votes++;

    *proposal_id_out = vote_slot->proposal.proposal_id;

    nimcp_mutex_unlock(&ctx->mutex);

    bbb_audit_log(BBB_AUDIT_INFO, "swarm_consensus", "created",
                 "Proposal %u created: topic=%s, quorum=%u, threshold=%.2f from drone %u",
                 vote_slot->proposal.proposal_id,
                 swarm_vote_topic_name(topic),
                 quorum_required,
                 threshold,
                 ctx->config.drone_id);
    LOG_INFO("Proposal %u created: topic=%s, quorum=%u, threshold=%.2f",
             vote_slot->proposal.proposal_id,
             swarm_vote_topic_name(topic),
             quorum_required,
             threshold);

    return NIMCP_SUCCESS;
}

/**
 * @brief Cast vote on proposal
 *
 * WHAT: Submit this drone's vote
 * WHY:  Participate in consensus decision
 * HOW:  Creates vote response, processes locally
 */
nimcp_error_t swarm_consensus_vote(
    swarm_consensus_t ctx,
    uint32_t proposal_id,
    swarm_vote_choice_t choice,
    float confidence)
{
    /* Validate parameters */
    if (!ctx || ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (confidence < 0.0F) {
        confidence = 0.0F;
    }
    if (confidence > 1.0F) {
        confidence = 1.0F;
    }

    /* Create vote response */
    swarm_vote_response_t vote;
    vote.proposal_id = proposal_id;
    vote.voter_drone = ctx->config.drone_id;
    vote.choice = choice;
    vote.confidence = confidence;

    /* Process as received vote */
    nimcp_error_t result = swarm_consensus_receive_vote(ctx, &vote);

    if (result == NIMCP_SUCCESS) {
        nimcp_mutex_lock(&ctx->mutex);
        ctx->stats.votes_cast++;
        nimcp_mutex_unlock(&ctx->mutex);
    }

    return result;
}

/**
 * @brief Receive vote from another drone
 *
 * WHAT: Process incoming vote from network
 * WHY:  Aggregate votes from swarm members
 * HOW:  Validates vote, updates counts, checks completion
 */
nimcp_error_t swarm_consensus_receive_vote(
    swarm_consensus_t ctx,
    const swarm_vote_response_t* vote)
{
    /* Validate parameters with BBB */
    if (!bbb_check_pointer(ctx, "swarm_consensus_receive_vote") ||
        ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_consensus", "receive_vote_error", "Invalid context");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bbb_check_pointer(vote, "swarm_consensus_receive_vote")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_consensus", "receive_vote_error", "Invalid vote pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* SECURITY FIX: Early rejection of high drone IDs to prevent bypass */
    if (vote->voter_drone >= MAX_TRACKED_DRONE_ID) {
        bbb_audit_log(BBB_AUDIT_CRITICAL, "swarm_consensus", "byzantine_detect",
                     "Vote from drone ID %u rejected - exceeds MAX_TRACKED_DRONE_ID (%u)",
                     vote->voter_drone, MAX_TRACKED_DRONE_ID);
        LOG_ERROR("Vote from drone %u rejected - ID exceeds tracking limit", vote->voter_drone);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    bbb_audit_log(BBB_AUDIT_INFO, "swarm_consensus", "vote_received",
                 "Vote received: proposal=%u, drone=%u, choice=%s, confidence=%.2f",
                 vote->proposal_id, vote->voter_drone,
                 swarm_vote_choice_name(vote->choice), vote->confidence);

    nimcp_mutex_lock(&ctx->mutex);

    /* Find proposal */
    active_vote_t* vote_slot = NULL;
    for (uint32_t i = 0; i < ctx->config.max_active_votes; i++) {
        if (ctx->active_votes[i].active &&
            ctx->active_votes[i].proposal.proposal_id == vote->proposal_id) {
            vote_slot = &ctx->active_votes[i];
            break;
        }
    }

    if (!vote_slot) {
        nimcp_mutex_unlock(&ctx->mutex);
        LOG_WARN("Vote for unknown proposal %u", vote->proposal_id);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Check if already voted - DETECT DOUBLE VOTING (Byzantine attack) */
    for (uint32_t i = 0; i < vote_slot->vote_count; i++) {
        if (vote_slot->votes[i].voter_drone == vote->voter_drone) {
            nimcp_mutex_unlock(&ctx->mutex);
            bbb_audit_log(BBB_AUDIT_CRITICAL, "swarm_consensus", "byzantine_detect",
                         "Double voting detected from drone %u on proposal %u - Byzantine attack suspected",
                         vote->voter_drone, vote->proposal_id);
            LOG_WARN("Duplicate vote from drone %u on proposal %u",
                     vote->voter_drone, vote->proposal_id);

            // Track voting patterns with mutex protection and high-ID rejection
            nimcp_mutex_lock(&g_vote_tracking_mutex);
            if (vote->voter_drone < MAX_TRACKED_DRONE_ID) {
                g_vote_counts_by_drone[vote->voter_drone]++;
                if (g_vote_counts_by_drone[vote->voter_drone] > BYZANTINE_EXCESSIVE_VOTE_THRESHOLD) {
                    bbb_audit_log(BBB_AUDIT_CRITICAL, "swarm_consensus", "byzantine_detect",
                                 "Drone %u has excessive vote rejections (%u) - quarantining",
                                 vote->voter_drone, g_vote_counts_by_drone[vote->voter_drone]);
                    // In production, this would call bbb_quarantine_peer(vote->voter_drone)
                }
                nimcp_mutex_unlock(&g_vote_tracking_mutex);
            } else {
                nimcp_mutex_unlock(&g_vote_tracking_mutex);
                // SECURITY FIX: Reject votes from drones with IDs >= MAX_TRACKED_DRONE_ID
                // This prevents bypass of double-voting detection by using high drone IDs
                bbb_audit_log(BBB_AUDIT_CRITICAL, "swarm_consensus", "byzantine_detect",
                             "Drone ID %u exceeds tracking limit (%u) - vote rejected as potential attack",
                             vote->voter_drone, MAX_TRACKED_DRONE_ID);
                LOG_ERROR("Drone ID %u exceeds tracking limit, vote rejected", vote->voter_drone);
            }

            return NIMCP_ERROR_ALREADY_EXISTS;
        }
    }

    /* Check Byzantine fault */
    if (ctx->config.enable_byzantine_ft && is_byzantine_fault(vote_slot, vote)) {
        ctx->stats.byzantine_faults_detected++;
        nimcp_mutex_unlock(&ctx->mutex);
        LOG_WARN("Byzantine fault detected from drone %u", vote->voter_drone);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Filter by minimum confidence */
    if (vote->confidence < ctx->config.min_confidence) {
        nimcp_mutex_unlock(&ctx->mutex);
        LOG_DEBUG("Vote from drone %u rejected: confidence %.2f < %.2f",
                  vote->voter_drone, vote->confidence, ctx->config.min_confidence);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Add vote */
    if (vote_slot->vote_count >= ctx->config.max_votes_per_proposal) {
        nimcp_mutex_unlock(&ctx->mutex);
        LOG_ERROR("Too many votes for proposal %u", vote->proposal_id);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    vote_slot->votes[vote_slot->vote_count++] = *vote;
    ctx->stats.votes_received++;

    LOG_DEBUG("Vote received: proposal=%u, drone=%u, choice=%s, confidence=%.2f",
              vote->proposal_id, vote->voter_drone,
              swarm_vote_choice_name(vote->choice), vote->confidence);

    /* Evaluate result */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t current_time = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    nimcp_error_t eval_result = evaluate_vote_result(ctx, vote_slot, current_time);

    /* Capture callback info for deferred invocation outside lock */
    swarm_vote_callback_t eval_cb = NULL;
    void* eval_cb_ctx = NULL;
    swarm_vote_result_t eval_cb_result = {0};
    if (vote_slot->result.status != VOTE_STATUS_PENDING &&
        vote_slot->callback && !vote_slot->callback_invoked) {
        vote_slot->callback_invoked = true;
        eval_cb = vote_slot->callback;
        eval_cb_ctx = vote_slot->callback_ctx;
        eval_cb_result = vote_slot->result;
    }

    nimcp_mutex_unlock(&ctx->mutex);

    /* Invoke callback outside lock to prevent deadlock */
    if (eval_cb) {
        eval_cb(&eval_cb_result, eval_cb_ctx);
    }

    return eval_result;
}

/**
 * @brief Check if vote is complete
 *
 * WHAT: Determine if vote has reached conclusion
 * WHY:  Poll for completion without callback
 * HOW:  Checks quorum, deadline, threshold
 */
nimcp_error_t swarm_consensus_check_result(
    swarm_consensus_t ctx,
    uint32_t proposal_id,
    bool* is_complete_out)
{
    /* Validate parameters */
    if (!ctx || ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC || !is_complete_out) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(&ctx->mutex);

    /* Find proposal */
    active_vote_t* vote_slot = NULL;
    for (uint32_t i = 0; i < ctx->config.max_active_votes; i++) {
        if (ctx->active_votes[i].active &&
            ctx->active_votes[i].proposal.proposal_id == proposal_id) {
            vote_slot = &ctx->active_votes[i];
            break;
        }
    }

    if (!vote_slot) {
        nimcp_mutex_unlock(&ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    *is_complete_out = (vote_slot->result.status != VOTE_STATUS_PENDING);

    nimcp_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get vote result
 *
 * WHAT: Retrieve final or current result
 * WHY:  Access vote outcome details
 * HOW:  Copies result structure
 */
nimcp_error_t swarm_consensus_get_result(
    swarm_consensus_t ctx,
    uint32_t proposal_id,
    swarm_vote_result_t* result_out)
{
    /* Validate parameters */
    if (!ctx || ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC || !result_out) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(&ctx->mutex);

    /* Find proposal */
    active_vote_t* vote_slot = NULL;
    for (uint32_t i = 0; i < ctx->config.max_active_votes; i++) {
        if (ctx->active_votes[i].active &&
            ctx->active_votes[i].proposal.proposal_id == proposal_id) {
            vote_slot = &ctx->active_votes[i];
            break;
        }
    }

    if (!vote_slot) {
        nimcp_mutex_unlock(&ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    *result_out = vote_slot->result;

    nimcp_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Cancel active proposal
 *
 * WHAT: Cancel voting on proposal
 * WHY:  Abort obsolete or erroneous votes
 * HOW:  Marks cancelled, invokes callback with status
 */
nimcp_error_t swarm_consensus_cancel(
    swarm_consensus_t ctx,
    uint32_t proposal_id)
{
    /* Validate parameters */
    if (!ctx || ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(&ctx->mutex);

    /* Find proposal */
    active_vote_t* vote_slot = NULL;
    for (uint32_t i = 0; i < ctx->config.max_active_votes; i++) {
        if (ctx->active_votes[i].active &&
            ctx->active_votes[i].proposal.proposal_id == proposal_id) {
            vote_slot = &ctx->active_votes[i];
            break;
        }
    }

    if (!vote_slot) {
        nimcp_mutex_unlock(&ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (vote_slot->result.status != VOTE_STATUS_PENDING) {
        nimcp_mutex_unlock(&ctx->mutex);
        LOG_WARN("Cannot cancel non-pending proposal %u", proposal_id);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Mark cancelled */
    vote_slot->result.status = VOTE_STATUS_CANCELLED;
    ctx->stats.proposals_cancelled++;

    /* Capture callback to invoke outside lock */
    swarm_vote_callback_t cancel_cb = NULL;
    void* cancel_cb_ctx = NULL;
    swarm_vote_result_t cancel_result = {0};
    if (vote_slot->callback && !vote_slot->callback_invoked) {
        vote_slot->callback_invoked = true;
        cancel_cb = vote_slot->callback;
        cancel_cb_ctx = vote_slot->callback_ctx;
        cancel_result = vote_slot->result;
    }

    /* Free slot */
    vote_slot->active = false;
    ctx->stats.active_votes--;

    nimcp_mutex_unlock(&ctx->mutex);

    /* Invoke callback outside lock to prevent deadlock */
    if (cancel_cb) {
        cancel_cb(&cancel_result, cancel_cb_ctx);
    }

    LOG_INFO("Proposal %u cancelled", proposal_id);

    return NIMCP_SUCCESS;
}

/**
 * @brief Cleanup expired votes
 *
 * WHAT: Remove timed-out votes
 * WHY:  Prevent memory leaks from abandoned votes
 * HOW:  Checks deadlines, marks expired, frees slots
 */
nimcp_error_t swarm_consensus_cleanup_expired(
    swarm_consensus_t ctx,
    uint64_t current_time_ms)
{
    /* Validate parameters */
    if (!ctx || ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Collect expired callbacks to invoke outside lock */
    typedef struct { swarm_vote_result_t result; swarm_vote_callback_t cb; void* cb_ctx; uint32_t pid; } expired_cb_t;
    expired_cb_t expired_cbs[SWARM_MAX_ACTIVE_VOTES];
    uint32_t expired_count = 0;

    nimcp_mutex_lock(&ctx->mutex);

    uint32_t cleaned = 0;

    for (uint32_t i = 0; i < ctx->config.max_active_votes; i++) {
        active_vote_t* vote = &ctx->active_votes[i];

        if (!vote->active) {
            continue;
        }

        /* Check if expired */
        if (vote->proposal.deadline_ms > 0 &&
            current_time_ms > vote->proposal.deadline_ms &&
            vote->result.status == VOTE_STATUS_PENDING) {

            vote->result.status = VOTE_STATUS_EXPIRED;
            ctx->stats.proposals_expired++;

            /* Capture callback for deferred invocation */
            if (vote->callback && !vote->callback_invoked && expired_count < SWARM_MAX_ACTIVE_VOTES) {
                vote->callback_invoked = true;
                expired_cbs[expired_count].result = vote->result;
                expired_cbs[expired_count].cb = vote->callback;
                expired_cbs[expired_count].cb_ctx = vote->callback_ctx;
                expired_cbs[expired_count].pid = vote->proposal.proposal_id;
                expired_count++;
            }

            /* Free slot */
            vote->active = false;
            ctx->stats.active_votes--;
            cleaned++;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    /* Invoke callbacks outside lock to prevent deadlock */
    for (uint32_t i = 0; i < expired_count; i++) {
        expired_cbs[i].cb(&expired_cbs[i].result, expired_cbs[i].cb_ctx);
        LOG_INFO("Proposal %u expired", expired_cbs[i].pid);
    }

    if (cleaned > 0) {
        LOG_DEBUG("Cleaned %u expired proposals", cleaned);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get statistics
 *
 * WHAT: Retrieve consensus statistics
 * WHY:  Monitor system health
 * HOW:  Copies stats structure
 */
nimcp_error_t swarm_consensus_get_stats(
    swarm_consensus_t ctx,
    swarm_consensus_stats_t* stats_out)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC || !stats_out) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(&ctx->mutex);
    *stats_out = ctx->stats;
    nimcp_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get proposal info
 *
 * WHAT: Retrieve proposal details
 * WHY:  Inspect active or completed proposals
 * HOW:  Copies proposal structure
 */
nimcp_error_t swarm_consensus_get_proposal(
    swarm_consensus_t ctx,
    uint32_t proposal_id,
    swarm_vote_proposal_t* proposal_out)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC || !proposal_out) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(&ctx->mutex);

    /* Find proposal */
    active_vote_t* vote_slot = NULL;
    for (uint32_t i = 0; i < ctx->config.max_active_votes; i++) {
        if (ctx->active_votes[i].active &&
            ctx->active_votes[i].proposal.proposal_id == proposal_id) {
            vote_slot = &ctx->active_votes[i];
            break;
        }
    }

    if (!vote_slot) {
        nimcp_mutex_unlock(&ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    *proposal_out = vote_slot->proposal;

    nimcp_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Evaluate vote result
 *
 * WHAT: Check if vote has reached conclusion
 * WHY:  Determine pass/fail based on counts and threshold
 * HOW:  Counts votes, calculates weighted score, checks quorum/threshold
 */
static nimcp_error_t evaluate_vote_result(
    swarm_consensus_t ctx,
    active_vote_t* vote,
    uint64_t current_time_ms)
{
    /* Skip if already complete */
    if (vote->result.status != VOTE_STATUS_PENDING) {
        return NIMCP_SUCCESS;
    }

    /* Check deadline */
    if (vote->proposal.deadline_ms > 0 &&
        current_time_ms > vote->proposal.deadline_ms) {
        vote->result.status = VOTE_STATUS_EXPIRED;
        ctx->stats.proposals_expired++;
        /* NOTE: callback invoked by caller outside lock */
        vote->active = false;
        ctx->stats.active_votes--;
        return NIMCP_SUCCESS;
    }

    /* Count votes and calculate weighted agreement */
    uint32_t agree_count = 0;
    uint32_t disagree_count = 0;
    uint32_t abstain_count = 0;
    float weighted_agree = 0.0F;
    float weighted_total = 0.0F;

    for (uint32_t i = 0; i < vote->vote_count; i++) {
        const swarm_vote_response_t* v = &vote->votes[i];

        switch (v->choice) {
            case VOTE_CHOICE_AGREE:
                agree_count++;
                weighted_agree += v->confidence;
                weighted_total += v->confidence;
                break;
            case VOTE_CHOICE_DISAGREE:
                disagree_count++;
                weighted_total += v->confidence;
                break;
            case VOTE_CHOICE_ABSTAIN:
                abstain_count++;
                break;
        }
    }

    /* Update result counts */
    vote->result.agree_count = agree_count;
    vote->result.disagree_count = disagree_count;
    vote->result.abstain_count = abstain_count;

    /* Calculate weighted agreement */
    if (weighted_total > 0.0F) {
        vote->result.weighted_agreement = weighted_agree / weighted_total;
    } else {
        vote->result.weighted_agreement = 0.0F;
    }

    /* Check quorum */
    uint32_t participating = agree_count + disagree_count;
    bool quorum_met = (vote->proposal.quorum_required == 0) ||
                      (participating >= vote->proposal.quorum_required);

    if (!quorum_met) {
        return NIMCP_SUCCESS;  /* Still pending */
    }

    /* Check threshold */
    bool threshold_met = (vote->result.weighted_agreement >= vote->proposal.threshold);

    /* Determine final status */
    if (threshold_met) {
        vote->result.status = VOTE_STATUS_PASSED;
        vote->result.passed = true;
        ctx->stats.proposals_passed++;
    } else {
        vote->result.status = VOTE_STATUS_FAILED;
        vote->result.passed = false;
        ctx->stats.proposals_failed++;
    }

    /* NOTE: callback invoked by caller outside lock */

    /* Free slot */
    vote->active = false;
    ctx->stats.active_votes--;

    LOG_INFO("Proposal %u complete: status=%s, agree=%u, disagree=%u, "
             "weighted=%.2f, threshold=%.2f",
             vote->proposal.proposal_id,
             swarm_vote_status_name(vote->result.status),
             agree_count, disagree_count,
             vote->result.weighted_agreement,
             vote->proposal.threshold);

    return NIMCP_SUCCESS;
}

/**
 * @brief Invoke completion callback
 *
 * WHAT: Call user callback with result
 * WHY:  Notify application of vote completion
 * HOW:  Checks callback exists, marks invoked, calls function
 */
static void invoke_callback(active_vote_t* vote)
{
    if (!vote->callback || vote->callback_invoked) {
        return;
    }

    vote->callback_invoked = true;
    vote->callback(&vote->result, vote->callback_ctx);
}

/**
 * @brief Check for Byzantine fault
 *
 * WHAT: Detect potentially malicious or faulty votes
 * WHY:  Maintain Byzantine fault tolerance
 * HOW:  Statistical outlier detection, confidence analysis
 */
static bool is_byzantine_fault(
    const active_vote_t* vote,
    const swarm_vote_response_t* new_vote)
{
    /* Need at least 3 votes for statistical analysis */
    if (vote->vote_count < 3) {
        return false;
    }

    /* Check if confidence is suspiciously extreme */
    if (new_vote->confidence > BYZANTINE_EXTREME_CONFIDENCE_HIGH ||
        new_vote->confidence < BYZANTINE_EXTREME_CONFIDENCE_LOW) {
        /* Count other extreme confidences */
        uint32_t extreme_count = 0;
        for (uint32_t i = 0; i < vote->vote_count; i++) {
            float conf = vote->votes[i].confidence;
            if (conf > BYZANTINE_EXTREME_CONFIDENCE_HIGH ||
                conf < BYZANTINE_EXTREME_CONFIDENCE_LOW) {
                extreme_count++;
            }
        }

        /* If more than 1/3 have extreme confidence, might be attack */
        float extreme_ratio = (float)(extreme_count + 1) / (vote->vote_count + 1);
        if (extreme_ratio > SWARM_BFT_THRESHOLD) {
            return true;
        }
    }

    /* Check for pattern anomalies - all agree with high confidence is suspicious */
    if (new_vote->choice == VOTE_CHOICE_AGREE &&
        new_vote->confidence > BYZANTINE_HIGH_CONFIDENCE_THRESHOLD) {
        uint32_t high_conf_agree = 0;
        for (uint32_t i = 0; i < vote->vote_count; i++) {
            if (vote->votes[i].choice == VOTE_CHOICE_AGREE &&
                vote->votes[i].confidence > BYZANTINE_HIGH_CONFIDENCE_THRESHOLD) {
                high_conf_agree++;
            }
        }

        /* If ALL votes are high-confidence agrees, suspicious */
        if (high_conf_agree == vote->vote_count && vote->vote_count > 5) {
            return true;
        }
    }

    return false;
}

//=============================================================================
// Bio-async Integration API
//=============================================================================

/**
 * @brief Connect consensus context to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY:  Enable inter-module messaging for consensus coordination
 * HOW:  Register as BIO_MODULE_SWARM_CONSENSUS
 */
nimcp_error_t swarm_consensus_connect_bio_async(swarm_consensus_t ctx)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (ctx->bio_async_enabled) {
        return NIMCP_SUCCESS;  // Already connected
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SWARM_CONSENSUS,
        .module_name = "swarm_consensus",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = ctx
    };

    ctx->bio_ctx = bio_router_register_module(&info);
    if (ctx->bio_ctx) {
        ctx->bio_async_enabled = true;
        LOG_INFO("Connected to bio-async router");
    } else {
        LOG_INFO("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Disconnect consensus context from bio-async router
 *
 * WHAT: Unregister from bio-async messaging
 * WHY:  Clean shutdown
 * HOW:  Deregister and cleanup
 */
nimcp_error_t swarm_consensus_disconnect_bio_async(swarm_consensus_t ctx)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!ctx->bio_async_enabled) {
        return NIMCP_SUCCESS;  // Not connected
    }

    if (ctx->bio_ctx) {
        bio_router_unregister_module(ctx->bio_ctx);
        ctx->bio_ctx = NULL;
    }

    ctx->bio_async_enabled = false;
    LOG_INFO("Disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

/**
 * @brief Check if consensus is connected to bio-async
 *
 * WHAT: Query bio-async connection status
 * WHY:  Verify messaging availability
 * HOW:  Check flag
 */
bool swarm_consensus_is_bio_async_connected(const swarm_consensus_t ctx)
{
    if (!ctx || ctx->magic != NIMCP_SWARM_CONSENSUS_MAGIC) {
        return false;
    }

    return ctx->bio_async_enabled;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query knowledge graph for module self-knowledge
 *
 * WHAT: Introspect module identity from knowledge graph
 * WHY:  Enable self-awareness and runtime reflection
 * HOW:  Query KG for Swarm_Consensus entity and its relations
 *
 * @param kg Knowledge graph reader
 * @return 1 if self-knowledge found, 0 otherwise
 */
int swarm_consensus_query_self_knowledge(kg_reader_t* kg)
{
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Swarm_Consensus");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Consensus self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Swarm_Consensus");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Swarm_Consensus");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
