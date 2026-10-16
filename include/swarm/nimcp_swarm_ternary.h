//=============================================================================
// nimcp_swarm_ternary.h - Ternary Voting for Swarm Consensus
//=============================================================================
/**
 * @file nimcp_swarm_ternary.h
 * @brief Ternary voting operations for swarm consensus
 *
 * WHAT: Three-state voting {DISAGREE, ABSTAIN, AGREE}
 * WHY:  Swarm consensus naturally uses ternary votes
 * HOW:  Map existing swarm_vote_choice_t to ternary
 *
 * BIOLOGICAL BASIS:
 * - Neurons can vote for, against, or remain silent on proposals
 * - Quorum sensing in biological swarms uses threshold activation
 * - Byzantine fault tolerance requires explicit abstention
 *
 * CONSENSUS ALGORITHMS:
 * - Majority: sign(sum of votes)
 * - Supermajority: require 2/3 agreement
 * - Unanimous: all must agree (ABSTAIN treated as blocking)
 * - Quorum: minimum participation required
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_SWARM_TERNARY_H
#define NIMCP_SWARM_TERNARY_H

#include "utils/ternary/nimcp_ternary.h"
#include "swarm/nimcp_swarm_consensus.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Swarm Ternary Types
//=============================================================================

/**
 * @brief Ternary vote value
 */
typedef trit_t trit_vote_t;

#define TRIT_VOTE_DISAGREE  TRIT_NEGATIVE  /**< Against proposal */
#define TRIT_VOTE_ABSTAIN   TRIT_UNKNOWN   /**< No vote / neutral */
#define TRIT_VOTE_AGREE     TRIT_POSITIVE  /**< For proposal */

//=============================================================================
// Conversion Functions
//=============================================================================

/**
 * @brief Convert swarm vote choice to ternary
 *
 * @param choice Swarm vote choice enum
 * @return Ternary vote
 */
static inline trit_vote_t trit_from_vote_choice(swarm_vote_choice_t choice) {
    switch (choice) {
        case VOTE_CHOICE_AGREE:    return TRIT_VOTE_AGREE;
        case VOTE_CHOICE_DISAGREE: return TRIT_VOTE_DISAGREE;
        case VOTE_CHOICE_ABSTAIN:
        default:                   return TRIT_VOTE_ABSTAIN;
    }
}

/**
 * @brief Convert ternary vote to swarm vote choice
 *
 * @param vote Ternary vote
 * @return Swarm vote choice enum
 */
static inline swarm_vote_choice_t trit_to_vote_choice(trit_vote_t vote) {
    switch (vote) {
        case TRIT_VOTE_AGREE:    return VOTE_CHOICE_AGREE;
        case TRIT_VOTE_DISAGREE: return VOTE_CHOICE_DISAGREE;
        default:                 return VOTE_CHOICE_ABSTAIN;
    }
}

//=============================================================================
// Consensus Functions
//=============================================================================

/**
 * @brief Compute simple majority consensus
 *
 * @param votes Array of votes
 * @param n_votes Number of votes
 * @return Majority vote (AGREE/DISAGREE/ABSTAIN for ties)
 */
static inline trit_vote_t trit_consensus_majority(
    const trit_vote_t* votes,
    size_t n_votes
) {
    if (!votes || n_votes == 0) return TRIT_VOTE_ABSTAIN;
    return trit_majority(votes, n_votes);
}

/**
 * @brief Check if supermajority (2/3) agrees
 *
 * @param votes Array of votes
 * @param n_votes Number of votes
 * @param threshold Fraction required (default 0.67)
 * @return AGREE if supermajority, DISAGREE if super-minority, ABSTAIN otherwise
 */
static inline trit_vote_t trit_consensus_supermajority(
    const trit_vote_t* votes,
    size_t n_votes,
    float threshold
) {
    if (!votes || n_votes == 0) return TRIT_VOTE_ABSTAIN;

    size_t n_agree = 0, n_disagree = 0;
    for (size_t i = 0; i < n_votes; i++) {
        if (votes[i] == TRIT_VOTE_AGREE) n_agree++;
        else if (votes[i] == TRIT_VOTE_DISAGREE) n_disagree++;
    }

    float agree_ratio = (float)n_agree / (float)n_votes;
    float disagree_ratio = (float)n_disagree / (float)n_votes;

    if (agree_ratio >= threshold) return TRIT_VOTE_AGREE;
    if (disagree_ratio >= threshold) return TRIT_VOTE_DISAGREE;
    return TRIT_VOTE_ABSTAIN;
}

/**
 * @brief Check for unanimous agreement
 *
 * @param votes Array of votes
 * @param n_votes Number of votes
 * @param ignore_abstain If true, abstentions don't block
 * @return AGREE if unanimous, DISAGREE if any disagree, ABSTAIN otherwise
 */
static inline trit_vote_t trit_consensus_unanimous(
    const trit_vote_t* votes,
    size_t n_votes,
    bool ignore_abstain
) {
    if (!votes || n_votes == 0) return TRIT_VOTE_ABSTAIN;

    bool all_agree = true;
    bool any_disagree = false;

    for (size_t i = 0; i < n_votes; i++) {
        if (votes[i] == TRIT_VOTE_DISAGREE) {
            any_disagree = true;
            break;
        }
        if (votes[i] != TRIT_VOTE_AGREE) {
            if (!ignore_abstain) {
                all_agree = false;
            }
        }
    }

    if (any_disagree) return TRIT_VOTE_DISAGREE;
    if (all_agree) return TRIT_VOTE_AGREE;
    return TRIT_VOTE_ABSTAIN;
}

/**
 * @brief Check quorum and compute consensus
 *
 * @param votes Array of votes
 * @param n_votes Number of votes cast
 * @param n_total Total eligible voters
 * @param quorum_threshold Minimum participation (0-1)
 * @return ABSTAIN if no quorum, otherwise majority result
 */
static inline trit_vote_t trit_consensus_quorum(
    const trit_vote_t* votes,
    size_t n_votes,
    size_t n_total,
    float quorum_threshold
) {
    if (!votes || n_votes == 0 || n_total == 0) return TRIT_VOTE_ABSTAIN;

    /* Count non-abstaining votes */
    size_t n_participating = 0;
    for (size_t i = 0; i < n_votes; i++) {
        if (votes[i] != TRIT_VOTE_ABSTAIN) n_participating++;
    }

    /* Check quorum */
    float participation = (float)n_participating / (float)n_total;
    if (participation < quorum_threshold) return TRIT_VOTE_ABSTAIN;

    /* Compute majority among participating voters */
    return trit_consensus_majority(votes, n_votes);
}

//=============================================================================
// Vote Statistics
//=============================================================================

/**
 * @brief Vote count statistics
 */
typedef struct {
    size_t n_agree;      /**< Count of AGREE votes */
    size_t n_abstain;    /**< Count of ABSTAIN votes */
    size_t n_disagree;   /**< Count of DISAGREE votes */
    size_t n_total;      /**< Total votes */
    float agree_ratio;   /**< Fraction agreeing */
    float disagree_ratio;/**< Fraction disagreeing */
    float participation; /**< Fraction participating (non-abstain) */
} trit_vote_stats_t;

/**
 * @brief Compute vote statistics
 *
 * @param votes Array of votes
 * @param n_votes Number of votes
 * @param stats Output statistics
 */
static inline void trit_vote_count(
    const trit_vote_t* votes,
    size_t n_votes,
    trit_vote_stats_t* stats
) {
    if (!stats) return;

    stats->n_agree = 0;
    stats->n_abstain = 0;
    stats->n_disagree = 0;
    stats->n_total = n_votes;

    if (!votes || n_votes == 0) {
        stats->agree_ratio = 0.0f;
        stats->disagree_ratio = 0.0f;
        stats->participation = 0.0f;
        return;
    }

    for (size_t i = 0; i < n_votes; i++) {
        if (votes[i] == TRIT_VOTE_AGREE) stats->n_agree++;
        else if (votes[i] == TRIT_VOTE_DISAGREE) stats->n_disagree++;
        else stats->n_abstain++;
    }

    stats->agree_ratio = (float)stats->n_agree / (float)n_votes;
    stats->disagree_ratio = (float)stats->n_disagree / (float)n_votes;
    stats->participation = 1.0f - (float)stats->n_abstain / (float)n_votes;
}

//=============================================================================
// Weighted Voting
//=============================================================================

/**
 * @brief Compute weighted majority consensus
 *
 * @param votes Array of votes
 * @param weights Array of vote weights
 * @param n_votes Number of votes
 * @return Weighted majority result
 */
static inline trit_vote_t trit_consensus_weighted(
    const trit_vote_t* votes,
    const float* weights,
    size_t n_votes
) {
    if (!votes || !weights || n_votes == 0) return TRIT_VOTE_ABSTAIN;

    float weighted_sum = 0.0f;
    float total_weight = 0.0f;

    for (size_t i = 0; i < n_votes; i++) {
        if (votes[i] != TRIT_VOTE_ABSTAIN) {
            weighted_sum += (float)votes[i] * weights[i];
            total_weight += weights[i];
        }
    }

    if (total_weight < 1e-6f) return TRIT_VOTE_ABSTAIN;

    float avg = weighted_sum / total_weight;
    if (avg > 0.0f) return TRIT_VOTE_AGREE;
    if (avg < 0.0f) return TRIT_VOTE_DISAGREE;
    return TRIT_VOTE_ABSTAIN;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_TERNARY_H */
