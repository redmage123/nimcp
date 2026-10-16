/**
 * @file nimcp_column_voting.h
 * @brief Column-Level Voting for Thousand Brains Consensus
 *
 * WHAT: Each cortical column independently hypothesizes what object it's sensing.
 *       Columns vote through lateral connections. Consensus broadcasts to global
 *       workspace. Disagreement = uncertainty.
 * WHY:  Multiple independent models (columns) each recognize from different
 *       viewpoints. Voting merges these into a unified percept. This is the
 *       core of Hawkins' Thousand Brains theory.
 * HOW:  Each column submits hypotheses. Voting rounds broadcast to lateral
 *       neighbors, blend sensory evidence + received votes, check >70% consensus.
 *       On consensus, broadcast to global workspace.
 *
 * Based on Hawkins' Thousand Brains theory (Numenta, 2019).
 */

#ifndef NIMCP_COLUMN_VOTING_H
#define NIMCP_COLUMN_VOTING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Constants
 * ========================================================================= */

#define COLUMN_VOTING_MAX_HYPOTHESES    8    /**< Max hypotheses per column */
#define COLUMN_VOTING_MAX_EVIDENCE      16   /**< Max feature evidence per hypothesis */
#define COLUMN_VOTING_MAX_COLUMNS       128  /**< Max voting columns */
#define COLUMN_VOTING_MAX_ROUNDS        5    /**< Max voting rounds before timeout */
#define COLUMN_VOTING_CONSENSUS_RATIO   0.7f /**< >70% agreement = consensus */
#define COLUMN_VOTING_SENSORY_WEIGHT    0.3f /**< Sensory evidence weight */
#define COLUMN_VOTING_VOTE_WEIGHT       0.7f /**< Lateral vote weight */

/* =========================================================================
 * Types
 * ========================================================================= */

/**
 * @brief A single hypothesis about what object a column is sensing.
 */
typedef struct {
    uint32_t object_id;                                        /**< Hypothesized object */
    float confidence;                                           /**< Confidence [0,1] */
    float feature_evidence[COLUMN_VOTING_MAX_EVIDENCE];        /**< Supporting feature evidence */
    uint32_t num_evidence;                                      /**< Evidence count */
} column_hypothesis_t;

/**
 * @brief Per-column voting state.
 */
typedef struct {
    uint32_t column_id;
    column_hypothesis_t hypotheses[COLUMN_VOTING_MAX_HYPOTHESES]; /**< Column's hypotheses */
    uint32_t num_hypotheses;

    /* Received votes from lateral neighbors */
    float votes[COLUMN_VOTING_MAX_HYPOTHESES];    /**< Accumulated vote weights per hyp */
    uint32_t vote_object_ids[COLUMN_VOTING_MAX_HYPOTHESES]; /**< Object IDs of voted hyps */
    uint32_t num_votes;

    /* Consensus state */
    uint32_t best_object_id;                       /**< Current best hypothesis */
    float best_confidence;                          /**< Current best confidence */
    bool has_consensus;                             /**< This column reached consensus */
} column_voting_state_t;

/**
 * @brief Configuration for the voting manager.
 */
typedef struct {
    uint32_t max_columns;               /**< Max participating columns (default 128) */
    uint32_t max_voting_rounds;         /**< Max rounds before timeout (default 5) */
    float consensus_threshold;          /**< Fraction for consensus (default 0.7) */
    float sensory_weight;               /**< Weight for sensory evidence (default 0.3) */
    float vote_weight;                  /**< Weight for lateral votes (default 0.7) */
    float min_confidence;               /**< Minimum confidence to submit hypothesis (default 0.1) */
    bool enable_workspace_broadcast;    /**< Broadcast consensus to global workspace (default true) */
} column_voting_config_t;

/**
 * @brief Statistics for the voting system.
 */
typedef struct {
    uint64_t total_rounds;              /**< Total voting rounds executed */
    uint64_t total_consensus_reached;   /**< Times consensus was reached */
    uint64_t total_timeouts;            /**< Times voting timed out (no consensus) */
    float mean_rounds_to_consensus;     /**< Average rounds to reach consensus */
    float mean_agreement_ratio;         /**< Average agreement ratio at consensus */
    uint32_t last_consensus_object_id;  /**< Object ID of last consensus */
    float last_consensus_confidence;    /**< Confidence of last consensus */
} column_voting_stats_t;

/**
 * @brief Column voting manager — manages all voting states and lateral connections.
 */
typedef struct column_voting_manager {
    column_voting_state_t* states;      /**< Array of column states [max_columns] */
    uint32_t num_columns;               /**< Active column count */
    uint32_t max_columns;

    /* Lateral connectivity (adjacency list) */
    uint32_t** lateral_neighbors;       /**< [column_idx][neighbor_idx] */
    uint32_t* num_neighbors;            /**< [column_idx] = count */
    uint32_t max_neighbors;             /**< Max neighbors per column */

    /* Consensus state */
    uint32_t consensus_object_id;       /**< Global consensus object */
    float consensus_confidence;         /**< Global consensus confidence */
    float agreement_ratio;              /**< Current agreement ratio */
    bool global_consensus;              /**< Global consensus reached */

    /* Configuration */
    uint32_t max_voting_rounds;
    float consensus_threshold;
    float sensory_weight;
    float vote_weight;
    float min_confidence;
    bool enable_workspace_broadcast;

    /* Workspace pointer (for consensus broadcast) */
    void* workspace;                    /**< global_workspace_t* */

    /* Statistics */
    column_voting_stats_t stats;

    /* Thread safety */
    void* mutex;                        /**< nimcp_mutex_t* */
} column_voting_manager_t;

/* =========================================================================
 * API
 * ========================================================================= */

/** Set default configuration values */
void column_voting_config_default(column_voting_config_t* config);

/** Create voting manager */
column_voting_manager_t* column_voting_create(const column_voting_config_t* config);

/** Destroy manager and free all resources */
void column_voting_destroy(column_voting_manager_t* mgr);

/**
 * @brief Connect lateral neighbors between two columns.
 * @return 0 on success, -1 on error
 */
int column_voting_connect_lateral(column_voting_manager_t* mgr,
                                   uint32_t col_a, uint32_t col_b);

/**
 * @brief Connect to global workspace for consensus broadcast.
 * @return 0 on success, -1 on error
 */
int column_voting_connect_workspace(column_voting_manager_t* mgr,
                                     void* workspace);

/**
 * @brief Submit a hypothesis from a column.
 * @return 0 on success, -1 on error
 */
int column_voting_submit_hypothesis(column_voting_manager_t* mgr,
                                     uint32_t column_idx,
                                     uint32_t object_id, float confidence,
                                     const float* evidence, uint32_t num_evidence);

/** Clear all hypotheses and votes for all columns */
int column_voting_clear_hypotheses(column_voting_manager_t* mgr);

/**
 * @brief Run one voting round: broadcast → accumulate → blend → check.
 * @return 0 on success, 1 if consensus reached, -1 on error
 */
int column_voting_run_round(column_voting_manager_t* mgr);

/**
 * @brief Run voting rounds until consensus or timeout.
 * @param rounds_taken Output: how many rounds were executed
 * @return 0 if consensus reached, 1 if timed out, -1 on error
 */
int column_voting_run_to_consensus(column_voting_manager_t* mgr,
                                    uint32_t* rounds_taken);

/** Check if global consensus has been reached */
bool column_voting_has_consensus(const column_voting_manager_t* mgr);

/**
 * @brief Get consensus result.
 * @return 0 if consensus exists, 1 if no consensus, -1 on error
 */
int column_voting_get_consensus(const column_voting_manager_t* mgr,
                                 uint32_t* object_id, float* confidence);

/** Get current agreement ratio (fraction of columns agreeing on best hypothesis) */
float column_voting_get_agreement_ratio(const column_voting_manager_t* mgr);

/**
 * @brief Get a specific column's current best belief.
 * @return 0 on success, -1 on error
 */
int column_voting_get_column_belief(const column_voting_manager_t* mgr,
                                     uint32_t column_idx,
                                     uint32_t* object_id, float* confidence);

/** Get full statistics */
int column_voting_get_stats(const column_voting_manager_t* mgr,
                             column_voting_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COLUMN_VOTING_H */
