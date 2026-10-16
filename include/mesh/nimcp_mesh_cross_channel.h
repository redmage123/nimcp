/**
 * @file nimcp_mesh_cross_channel.h
 * @brief Cross-Channel Transaction Routing and System Coordinator
 *
 * WHAT: Cross-channel transaction handling and system-level coordination
 * WHY:  Enable communication between channels and resolve cross-channel conflicts
 * HOW:  Route through ordering service, arbitrate conflicts via FEP
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    CROSS-CHANNEL ARCHITECTURE                           │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  ┌──────────────────────────────────────────────────────────────────┐  │
 * │  │                    SYSTEM COORDINATOR                            │  │
 * │  │                                                                  │  │
 * │  │  • Arbitrates cross-hemisphere decisions                        │  │
 * │  │  • Resolves conflicts using FEP (minimize total free energy)    │  │
 * │  │  • Manages system-wide configuration                            │  │
 * │  │  • Monitors global health across all channels                   │  │
 * │  └──────────────────────────────────────────────────────────────────┘  │
 * │                              │                                         │
 * │                              ▼                                         │
 * │  ┌──────────────────────────────────────────────────────────────────┐  │
 * │  │                    CROSS-CHANNEL ROUTER                          │  │
 * │  │                                                                  │  │
 * │  │  ┌───────────┐    ┌───────────┐    ┌───────────┐               │  │
 * │  │  │ Channel 1 │◄──►│  ROUTER   │◄──►│ Channel 2 │               │  │
 * │  │  └───────────┘    └─────┬─────┘    └───────────┘               │  │
 * │  │                         │                                       │  │
 * │  │                         ▼                                       │  │
 * │  │                ┌─────────────────┐                              │  │
 * │  │                │ Ordering Service │                              │  │
 * │  │                │ (Sequencing)     │                              │  │
 * │  │                └─────────────────┘                              │  │
 * │  └──────────────────────────────────────────────────────────────────┘  │
 * │                                                                         │
 * │  CROSS-CHANNEL TRANSACTION FLOW:                                       │
 * │  1. Source channel proposes transaction                                │
 * │  2. MSP validates cross-channel access                                 │
 * │  3. Router collects endorsements from BOTH channels                    │
 * │  4. Ordering service sequences transaction                             │
 * │  5. Both channels validate and commit                                  │
 * │                                                                         │
 * │  CONFLICT RESOLUTION:                                                  │
 * │  • Competing transactions evaluated by free energy                     │
 * │  • Lower free energy transaction wins                                  │
 * │  • Loser notified for potential retry                                  │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_CROSS_CHANNEL_H
#define NIMCP_MESH_CROSS_CHANNEL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "mesh/nimcp_mesh_msp.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum channels in cross-channel router */
#define MESH_CROSS_MAX_CHANNELS             16

/** @brief Default cross-channel timeout (ms) */
#define MESH_CROSS_DEFAULT_TIMEOUT_MS       500.0f

/** @brief Default endorsement collection timeout (ms) */
#define MESH_CROSS_ENDORSEMENT_TIMEOUT_MS   200.0f

/** @brief Maximum pending cross-channel transactions */
#define MESH_CROSS_MAX_PENDING              256

/** @brief Conflict resolution free energy threshold */
#define MESH_CROSS_FE_THRESHOLD             0.01f

/* ============================================================================
 * Cross-Channel Transaction Status
 * ============================================================================ */

/**
 * @brief Cross-channel transaction status
 */
typedef enum mesh_cross_tx_status {
    MESH_CROSS_TX_PENDING = 0,          /**< Awaiting processing */
    MESH_CROSS_TX_VALIDATING,           /**< MSP validating access */
    MESH_CROSS_TX_ENDORSING_SOURCE,     /**< Collecting source endorsements */
    MESH_CROSS_TX_ENDORSING_TARGET,     /**< Collecting target endorsements */
    MESH_CROSS_TX_ORDERING,             /**< In ordering service */
    MESH_CROSS_TX_COMMITTING,           /**< Committing to both channels */
    MESH_CROSS_TX_COMMITTED,            /**< Successfully committed */
    MESH_CROSS_TX_FAILED,               /**< Transaction failed */
    MESH_CROSS_TX_CONFLICT,             /**< Lost conflict resolution */
    MESH_CROSS_TX_ACCESS_DENIED         /**< MSP denied access */
} mesh_cross_tx_status_t;

/**
 * @brief Conflict resolution result
 */
typedef enum mesh_conflict_result {
    MESH_CONFLICT_NONE = 0,             /**< No conflict */
    MESH_CONFLICT_WINNER,               /**< Won conflict resolution */
    MESH_CONFLICT_LOSER,                /**< Lost conflict resolution */
    MESH_CONFLICT_MERGED,               /**< Transactions were merged */
    MESH_CONFLICT_DEFERRED              /**< Resolution deferred */
} mesh_conflict_result_t;

/* ============================================================================
 * FEP Payload Structure (for Free Energy computation)
 * ============================================================================ */

/**
 * @brief Structured payload for free energy computation
 *
 * This structure provides explicit fields for FEP-based conflict resolution
 * instead of relying on arbitrary interpretation of void* payload.
 */
typedef struct mesh_fep_payload {
    float prediction;           /**< Model prediction/expectation */
    float outcome;              /**< Actual outcome/observation */
    float confidence;           /**< Confidence in prediction (0-1) */
    float complexity;           /**< Model complexity penalty */
    float salience;             /**< Relevance/importance weight */
    float temporal_discount;    /**< Time-based discount factor */
    uint32_t flags;             /**< Additional flags */
} mesh_fep_payload_t;

#define MESH_FEP_FLAG_HAS_PREDICTION    0x01
#define MESH_FEP_FLAG_HAS_OUTCOME       0x02
#define MESH_FEP_FLAG_URGENT            0x04
#define MESH_FEP_FLAG_CROSS_MODAL       0x08

/**
 * @brief Initialize FEP payload with defaults
 */
static inline void mesh_fep_payload_init(mesh_fep_payload_t* p) {
    if (p) {
        p->prediction = 0.0f;
        p->outcome = 0.0f;
        p->confidence = 0.5f;
        p->complexity = 0.0f;
        p->salience = 1.0f;
        p->temporal_discount = 1.0f;
        p->flags = 0;
    }
}

/* ============================================================================
 * Cross-Channel Transaction
 * ============================================================================ */

/**
 * @brief Cross-channel transaction
 */
typedef struct mesh_cross_transaction {
    mesh_tx_id_t base_id;               /**< Base transaction ID */
    mesh_cross_tx_status_t status;      /**< Current status */

    /* Routing */
    mesh_channel_id_t source_channel;   /**< Source channel */
    mesh_channel_id_t target_channel;   /**< Target channel */
    mesh_participant_id_t proposer;     /**< Transaction proposer */

    /* Payload */
    mesh_tx_type_t tx_type;             /**< Transaction type */
    void* payload;                      /**< Transaction payload */
    size_t payload_size;                /**< Payload size */

    /* Structured FEP data (for conflict resolution) */
    mesh_fep_payload_t fep_data;        /**< FEP payload for free energy computation */
    bool has_fep_data;                  /**< Whether fep_data is populated */

    /* Endorsements */
    endorsement_set_t source_endorsements;  /**< Source channel endorsements */
    endorsement_set_t target_endorsements;  /**< Target channel endorsements */

    /* Timing */
    uint64_t submitted_ns;              /**< Submission time */
    uint64_t started_ns;                /**< Processing start */
    uint64_t completed_ns;              /**< Completion time */
    float timeout_ms;                   /**< Transaction timeout */

    /* Free energy (for conflict resolution) */
    float free_energy;                  /**< Computed free energy */
    bool fe_computed;                   /**< Whether FE has been computed */

    /* Result */
    nimcp_error_t error;                /**< Error code if failed */
    char error_msg[128];                /**< Error message */
    mesh_conflict_result_t conflict_result; /**< Conflict resolution result */

    /* Callback */
    mesh_tx_callback_t callback;        /**< Completion callback */
    void* callback_ctx;                 /**< Callback context */
} mesh_cross_transaction_t;

/* ============================================================================
 * System Coordinator Configuration
 * ============================================================================ */

/**
 * @brief System coordinator configuration
 */
typedef struct mesh_system_coord_config {
    float arbitration_timeout_ms;       /**< Conflict arbitration timeout */
    float health_check_interval_ms;     /**< System health check interval */
    bool enable_fep_arbitration;        /**< Use FEP for conflict resolution */
    float fe_threshold;                 /**< Free energy threshold for conflicts */
    bool enable_auto_rebalance;         /**< Auto-rebalance channel loads */
    uint32_t max_pending_conflicts;     /**< Max pending conflict resolutions */
} mesh_system_coord_config_t;

/**
 * @brief Cross-channel router configuration
 */
typedef struct mesh_cross_router_config {
    float endorsement_timeout_ms;       /**< Endorsement collection timeout */
    float transaction_timeout_ms;       /**< Overall transaction timeout */
    size_t max_pending;                 /**< Max pending cross-channel txs */
    bool require_both_endorsements;     /**< Require endorsements from both channels */
    bool enable_parallel_endorsement;   /**< Collect endorsements in parallel */
} mesh_cross_router_config_t;

/* ============================================================================
 * System Coordinator Statistics
 * ============================================================================ */

/**
 * @brief Per-channel statistics in system coordinator
 */
typedef struct mesh_channel_system_stats {
    mesh_channel_id_t channel_id;       /**< Channel ID */
    bool connected;                     /**< Channel is connected */
    bool healthy;                       /**< Channel is healthy */
    uint64_t cross_tx_sent;             /**< Cross-channel txs sent from */
    uint64_t cross_tx_received;         /**< Cross-channel txs received */
    uint64_t conflicts_won;             /**< Conflicts won */
    uint64_t conflicts_lost;            /**< Conflicts lost */
    float avg_latency_ms;               /**< Avg cross-channel latency */
} mesh_channel_system_stats_t;

/**
 * @brief System coordinator statistics
 */
typedef struct mesh_system_coord_stats {
    /* Global */
    uint64_t total_cross_transactions;  /**< Total cross-channel transactions */
    uint64_t successful_transactions;   /**< Successful completions */
    uint64_t failed_transactions;       /**< Failed transactions */
    uint64_t access_denied;             /**< MSP access denied */

    /* Conflict resolution */
    uint64_t conflicts_detected;        /**< Conflicts detected */
    uint64_t conflicts_resolved;        /**< Conflicts resolved via FEP */
    uint64_t conflicts_deferred;        /**< Conflicts requiring manual resolution */

    /* Timing */
    float avg_cross_latency_ms;         /**< Average cross-channel latency */
    float avg_conflict_resolution_ms;   /**< Average conflict resolution time */

    /* Per-channel */
    mesh_channel_system_stats_t* channel_stats;
    size_t channel_count;
} mesh_system_coord_stats_t;

/* ============================================================================
 * Opaque Types
 * ============================================================================ */

/**
 * @brief Opaque system coordinator context
 */
typedef struct mesh_system_coordinator_internal* mesh_system_coordinator_t;

/**
 * @brief Opaque cross-channel router context
 */
typedef struct mesh_cross_router_internal* mesh_cross_router_t;

/* ============================================================================
 * System Coordinator Lifecycle
 * ============================================================================ */

/**
 * @brief Create default system coordinator configuration
 *
 * @return Default configuration
 */
mesh_system_coord_config_t mesh_system_coord_default_config(void);

/**
 * @brief Create system coordinator
 *
 * WHAT: Initialize system-level coordinator
 * WHY:  Arbitrate cross-channel decisions and monitor system health
 * HOW:  Create FEP context, setup arbitration
 *
 * @param config Configuration (NULL for defaults)
 * @param ordering Ordering service (optional, for sequencing)
 * @param msp MSP for access control (optional)
 * @return System coordinator or NULL on failure
 */
mesh_system_coordinator_t mesh_system_coord_create(
    const mesh_system_coord_config_t* config,
    mesh_ordering_service_t* ordering,
    mesh_msp_t* msp
);

/**
 * @brief Destroy system coordinator
 *
 * @param coord Coordinator to destroy
 */
void mesh_system_coord_destroy(mesh_system_coordinator_t coord);

/**
 * @brief Register channel with system coordinator
 *
 * @param coord System coordinator
 * @param channel_id Channel ID
 * @param channel_name Human-readable name
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_system_coord_register_channel(
    mesh_system_coordinator_t coord,
    mesh_channel_id_t channel_id,
    const char* channel_name
);

/**
 * @brief Unregister channel from system coordinator
 *
 * @param coord System coordinator
 * @param channel_id Channel ID
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_system_coord_unregister_channel(
    mesh_system_coordinator_t coord,
    mesh_channel_id_t channel_id
);

/* ============================================================================
 * Cross-Channel Router Lifecycle
 * ============================================================================ */

/**
 * @brief Create default router configuration
 *
 * @return Default configuration
 */
mesh_cross_router_config_t mesh_cross_router_default_config(void);

/**
 * @brief Create cross-channel router
 *
 * WHAT: Initialize cross-channel transaction router
 * WHY:  Route transactions between channels
 * HOW:  Setup routing tables, connect to ordering service
 *
 * @param config Configuration (NULL for defaults)
 * @param system_coord System coordinator (for conflict resolution)
 * @return Router or NULL on failure
 */
mesh_cross_router_t mesh_cross_router_create(
    const mesh_cross_router_config_t* config,
    mesh_system_coordinator_t system_coord
);

/**
 * @brief Destroy cross-channel router
 *
 * @param router Router to destroy
 */
void mesh_cross_router_destroy(mesh_cross_router_t router);

/**
 * @brief Start router processing
 *
 * @param router Cross-channel router
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_cross_router_start(mesh_cross_router_t router);

/**
 * @brief Stop router processing
 *
 * @param router Cross-channel router
 * @param drain Wait for pending transactions
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_cross_router_stop(mesh_cross_router_t router, bool drain);

/* ============================================================================
 * Cross-Channel Transaction API
 * ============================================================================ */

/**
 * @brief Create cross-channel transaction
 *
 * @param source_channel Source channel
 * @param target_channel Target channel
 * @param proposer Proposing participant
 * @param tx_type Transaction type
 * @param payload Payload data (copied)
 * @param payload_size Payload size
 * @return New transaction or NULL
 */
mesh_cross_transaction_t* mesh_cross_transaction_create(
    mesh_channel_id_t source_channel,
    mesh_channel_id_t target_channel,
    mesh_participant_id_t proposer,
    mesh_tx_type_t tx_type,
    const void* payload,
    size_t payload_size
);

/**
 * @brief Destroy cross-channel transaction
 *
 * @param tx Transaction to destroy
 */
void mesh_cross_transaction_destroy(mesh_cross_transaction_t* tx);

/**
 * @brief Submit cross-channel transaction
 *
 * WHAT: Submit transaction for cross-channel processing
 * WHY:  Route transaction between channels
 * HOW:  Validate access, collect endorsements, sequence, commit
 *
 * @param router Cross-channel router
 * @param tx Transaction to submit (router takes ownership)
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_cross_router_submit(
    mesh_cross_router_t router,
    mesh_cross_transaction_t* tx
);

/**
 * @brief Submit with callback
 *
 * @param router Cross-channel router
 * @param tx Transaction to submit
 * @param callback Completion callback
 * @param ctx Callback context
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_cross_router_submit_async(
    mesh_cross_router_t router,
    mesh_cross_transaction_t* tx,
    mesh_tx_callback_t callback,
    void* ctx
);

/**
 * @brief Wait for transaction completion
 *
 * @param router Cross-channel router
 * @param tx_id Transaction ID
 * @param timeout_ms Timeout (0 = infinite)
 * @return NIMCP_OK on completion
 */
nimcp_error_t mesh_cross_router_wait(
    mesh_cross_router_t router,
    const mesh_tx_id_t* tx_id,
    uint32_t timeout_ms
);

/* ============================================================================
 * Conflict Resolution
 * ============================================================================ */

/**
 * @brief Arbitrate between conflicting transactions
 *
 * WHAT: Resolve conflict between competing cross-channel transactions
 * WHY:  Only one transaction can win in case of conflict
 * HOW:  Compare free energy - lower wins
 *
 * @param coord System coordinator
 * @param tx1 First transaction
 * @param tx2 Second transaction
 * @param winner Output: winning transaction
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_system_coord_arbitrate(
    mesh_system_coordinator_t coord,
    mesh_cross_transaction_t* tx1,
    mesh_cross_transaction_t* tx2,
    mesh_cross_transaction_t** winner
);

/**
 * @brief Compute free energy for transaction
 *
 * WHAT: Calculate free energy cost of transaction
 * WHY:  Lower free energy = better transaction
 * HOW:  F = KL[q||p] + E[-ln p(o|s)]
 *
 * @param coord System coordinator
 * @param tx Transaction to evaluate
 * @param free_energy Output: computed free energy
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_system_coord_compute_free_energy(
    mesh_system_coordinator_t coord,
    mesh_cross_transaction_t* tx,
    float* free_energy
);

/**
 * @brief Check if transactions conflict
 *
 * @param tx1 First transaction
 * @param tx2 Second transaction
 * @return true if transactions conflict
 */
bool mesh_cross_transactions_conflict(
    const mesh_cross_transaction_t* tx1,
    const mesh_cross_transaction_t* tx2
);

/* ============================================================================
 * System Health
 * ============================================================================ */

/**
 * @brief Check system health
 *
 * WHAT: Verify all channels and coordinators are healthy
 * WHY:  Detect and respond to system issues
 *
 * @param coord System coordinator
 * @return true if system is healthy
 */
bool mesh_system_coord_is_healthy(mesh_system_coordinator_t coord);

/**
 * @brief Get channel health
 *
 * @param coord System coordinator
 * @param channel_id Channel to check
 * @return true if channel is healthy
 */
bool mesh_system_coord_channel_healthy(
    mesh_system_coordinator_t coord,
    mesh_channel_id_t channel_id
);

/**
 * @brief Mark channel unhealthy
 *
 * @param coord System coordinator
 * @param channel_id Channel to mark
 * @param reason Reason for marking unhealthy
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_system_coord_mark_unhealthy(
    mesh_system_coordinator_t coord,
    mesh_channel_id_t channel_id,
    const char* reason
);

/**
 * @brief Mark channel healthy
 *
 * @param coord System coordinator
 * @param channel_id Channel to mark
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_system_coord_mark_healthy(
    mesh_system_coordinator_t coord,
    mesh_channel_id_t channel_id
);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Get system coordinator statistics
 *
 * @param coord System coordinator
 * @param stats Output statistics
 * @return NIMCP_OK on success
 *
 * @note Caller must free stats->channel_stats if not NULL
 */
nimcp_error_t mesh_system_coord_get_stats(
    mesh_system_coordinator_t coord,
    mesh_system_coord_stats_t* stats
);

/**
 * @brief Reset system coordinator statistics
 *
 * @param coord System coordinator
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_system_coord_reset_stats(mesh_system_coordinator_t coord);

/**
 * @brief Free statistics resources
 *
 * @param stats Statistics to free
 */
void mesh_system_coord_stats_free(mesh_system_coord_stats_t* stats);

/**
 * @brief Get router pending count
 *
 * @param router Cross-channel router
 * @return Number of pending transactions
 */
size_t mesh_cross_router_pending_count(mesh_cross_router_t router);

/* ============================================================================
 * Debug
 * ============================================================================ */

/**
 * @brief Print system coordinator debug info
 *
 * @param coord System coordinator
 */
void mesh_system_coord_print_debug(mesh_system_coordinator_t coord);

/**
 * @brief Print router debug info
 *
 * @param router Cross-channel router
 */
void mesh_cross_router_print_debug(mesh_cross_router_t router);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get cross-channel transaction status name
 *
 * @param status Status
 * @return Status name
 */
const char* mesh_cross_tx_status_to_string(mesh_cross_tx_status_t status);

/**
 * @brief Get conflict result name
 *
 * @param result Conflict result
 * @return Result name
 */
const char* mesh_conflict_result_to_string(mesh_conflict_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_CROSS_CHANNEL_H */
