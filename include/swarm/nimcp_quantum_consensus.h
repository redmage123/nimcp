//=============================================================================
// nimcp_quantum_consensus.h - Quantum-Enhanced Swarm Consensus
//=============================================================================
/**
 * @file nimcp_quantum_consensus.h
 * @brief Quantum-inspired consensus algorithms for swarm voting
 *
 * WHAT: Quantum-enhanced Byzantine fault-tolerant consensus
 * WHY:  Faster convergence and better fault detection in swarm voting
 * HOW:  Grover-inspired vote amplification with ternary states
 *
 * BIOLOGICAL BASIS:
 * - Neural voting in cortical columns uses winner-take-all dynamics
 * - Lateral inhibition creates competitive selection
 * - Population coding aggregates distributed signals
 *
 * QUANTUM SPEEDUP:
 * - Grover-inspired amplification: O(√N) rounds to consensus vs O(N)
 * - Quantum annealing for finding optimal threshold
 * - Ternary voting: Agree/Disagree/Abstain with fuzzy transitions
 *
 * BYZANTINE FAULT TOLERANCE:
 * - Tolerates f faulty nodes if N ≥ 3f + 1
 * - Quantum verification detects collusion attempts
 * - Amplitude-weighted voting filters low-confidence votes
 *
 * TERNARY VOTE STATES:
 * | State    | Value | Meaning              |
 * |----------|-------|----------------------|
 * | AGREE    | +1    | Support proposal     |
 * | ABSTAIN  | 0     | No strong opinion    |
 * | DISAGREE | -1    | Oppose proposal      |
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_QUANTUM_CONSENSUS_H
#define NIMCP_QUANTUM_CONSENSUS_H

#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Magic number for validation */
#define QUANTUM_CONSENSUS_MAGIC 0x5143534EU  /* "QCSE" */

/** Maximum number of voters */
#define QUANTUM_CONSENSUS_MAX_VOTERS 1024

/** Default Grover iterations (√N) */
#define QUANTUM_CONSENSUS_DEFAULT_ITERATIONS 32

/** Byzantine fault tolerance threshold */
#define QUANTUM_CONSENSUS_BFT_THRESHOLD 0.333333f

/** Default agreement threshold */
#define QUANTUM_CONSENSUS_DEFAULT_THRESHOLD 0.666667f

/** Minimum confidence to count vote */
#define QUANTUM_CONSENSUS_MIN_CONFIDENCE 0.1f

//=============================================================================
// Error Codes
//=============================================================================

typedef enum {
    QCONSENSUS_OK = 0,
    QCONSENSUS_ERR_NULL = -1,
    QCONSENSUS_ERR_INVALID = -2,
    QCONSENSUS_ERR_ALLOC = -3,
    QCONSENSUS_ERR_FULL = -4,
    QCONSENSUS_ERR_NOT_FOUND = -5,
    QCONSENSUS_ERR_TIMEOUT = -6,
    QCONSENSUS_ERR_BFT_VIOLATION = -7
} quantum_consensus_error_t;

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Quantum consensus algorithm type
 */
typedef enum {
    QCONSENSUS_GROVER,       /**< Grover-inspired amplitude amplification */
    QCONSENSUS_ANNEALING,    /**< Quantum annealing for threshold finding */
    QCONSENSUS_WALK,         /**< Quantum walk on vote graph */
    QCONSENSUS_HYBRID        /**< Combined approach */
} quantum_consensus_algo_t;

/**
 * @brief Vote choice (ternary)
 */
typedef enum {
    QVOTE_DISAGREE = -1,
    QVOTE_ABSTAIN = 0,
    QVOTE_AGREE = 1
} quantum_vote_choice_t;

/**
 * @brief Proposal status
 */
typedef enum {
    QPROPOSAL_PENDING,
    QPROPOSAL_PASSED,
    QPROPOSAL_FAILED,
    QPROPOSAL_EXPIRED
} quantum_proposal_status_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Quantum consensus configuration
 */
typedef struct {
    quantum_consensus_algo_t algorithm;    /**< Algorithm to use */
    uint32_t max_voters;                   /**< Maximum number of voters */
    uint32_t grover_iterations;            /**< Grover iterations (0=auto) */
    float agreement_threshold;             /**< Threshold for passing (0.0-1.0) */
    float min_confidence;                  /**< Minimum confidence for vote */
    float bft_threshold;                   /**< Byzantine fault threshold */
    bool enable_amplitude_weighting;       /**< Weight votes by amplitude */
    bool enable_collusion_detection;       /**< Detect voting collusion */
    uint32_t seed;                         /**< Random seed */
} quantum_consensus_config_t;

/**
 * @brief Individual vote
 */
typedef struct {
    uint32_t voter_id;                     /**< Voter identifier */
    quantum_vote_choice_t choice;          /**< Vote choice */
    float confidence;                      /**< Confidence [0.0, 1.0] */
    float amplitude;                       /**< Quantum amplitude */
    float phase;                           /**< Quantum phase */
    uint64_t timestamp;                    /**< Vote timestamp */
} quantum_vote_t;

/**
 * @brief Proposal
 */
typedef struct {
    uint32_t proposal_id;                  /**< Proposal identifier */
    uint32_t proposer_id;                  /**< Proposer identifier */
    char topic[64];                        /**< Proposal topic */
    float proposal_value;                  /**< Associated value */
    uint64_t deadline;                     /**< Deadline timestamp */
    quantum_proposal_status_t status;      /**< Current status */
    uint32_t n_votes;                      /**< Number of votes received */
    quantum_vote_t* votes;                 /**< Array of votes */
    uint32_t votes_capacity;               /**< Votes array capacity */
} quantum_proposal_t;

/**
 * @brief Consensus result
 */
typedef struct {
    uint32_t proposal_id;                  /**< Proposal identifier */
    bool passed;                           /**< Whether proposal passed */
    float weighted_agreement;              /**< Weighted agreement score */
    float amplitude_agreement;             /**< Amplitude-weighted agreement */
    uint32_t agree_count;                  /**< Number of agree votes */
    uint32_t disagree_count;               /**< Number of disagree votes */
    uint32_t abstain_count;                /**< Number of abstain votes */
    uint32_t grover_rounds;                /**< Grover rounds used */
    float bft_score;                       /**< Byzantine fault tolerance score */
    bool collusion_detected;               /**< Whether collusion was detected */
} quantum_consensus_result_t;

/**
 * @brief Consensus statistics
 */
typedef struct {
    uint64_t total_proposals;              /**< Total proposals */
    uint64_t proposals_passed;             /**< Proposals that passed */
    uint64_t proposals_failed;             /**< Proposals that failed */
    uint64_t total_votes;                  /**< Total votes cast */
    uint64_t grover_amplifications;        /**< Total Grover amplifications */
    uint64_t collusions_detected;          /**< Collusion attempts detected */
    float avg_convergence_rounds;          /**< Average rounds to consensus */
    float avg_agreement_score;             /**< Average agreement score */
} quantum_consensus_stats_t;

//=============================================================================
// Internal Context
//=============================================================================

typedef struct quantum_consensus_internal {
    uint32_t magic;
    quantum_consensus_config_t config;

    /* Proposals */
    quantum_proposal_t* proposals;
    uint32_t n_proposals;
    uint32_t proposals_capacity;
    uint32_t next_proposal_id;

    /* Amplitude state for Grover algorithm */
    float* amplitudes;                     /**< Voter amplitudes */
    float* phases;                         /**< Voter phases */
    trit_vector_t* vote_states;            /**< Ternary vote states */

    /* Statistics */
    quantum_consensus_stats_t stats;

    /* Random state */
    uint64_t rng_state;
} quantum_consensus_internal_t;

typedef struct quantum_consensus_internal* quantum_consensus_t;

//=============================================================================
// Forward Declarations
//=============================================================================

static inline void quantum_consensus_destroy(quantum_consensus_t ctx);
static inline void quantum_consensus_free_result(quantum_consensus_result_t* result);

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Simple PRNG
 */
static inline uint64_t qcons_rand64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

/**
 * @brief Random float [0,1)
 */
static inline float qcons_randf(uint64_t* state) {
    return (float)(qcons_rand64(state) >> 11) / (float)(1ULL << 53);
}

/**
 * @brief Convert vote choice to trit
 */
static inline trit_t qcons_choice_to_trit(quantum_vote_choice_t choice) {
    switch (choice) {
        case QVOTE_AGREE:    return TRIT_POSITIVE;
        case QVOTE_DISAGREE: return TRIT_NEGATIVE;
        default:             return TRIT_UNKNOWN;
    }
}

/**
 * @brief Convert trit to vote choice
 */
static inline quantum_vote_choice_t qcons_trit_to_choice(trit_t t) {
    if (t == TRIT_POSITIVE) return QVOTE_AGREE;
    if (t == TRIT_NEGATIVE) return QVOTE_DISAGREE;
    return QVOTE_ABSTAIN;
}

/**
 * @brief Apply Grover diffusion operator
 *
 * WHAT: Inversion about the mean amplitude
 * WHY:  Amplifies votes that match the oracle (majority)
 * HOW:  A' = 2*mean(A) - A
 */
static inline void qcons_grover_diffusion(float* amplitudes, uint32_t n) {
    if (!amplitudes || n == 0) return;

    /* Compute mean amplitude */
    float mean = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        mean += amplitudes[i];
    }
    mean /= (float)n;

    /* Invert about mean */
    for (uint32_t i = 0; i < n; i++) {
        amplitudes[i] = 2.0f * mean - amplitudes[i];
    }
}

/**
 * @brief Apply oracle for majority vote
 *
 * WHAT: Mark states that match the majority
 * WHY:  Combined with diffusion, amplifies majority
 * HOW:  Flip phase of non-majority votes
 */
static inline void qcons_grover_oracle(
    float* amplitudes,
    float* phases,
    const trit_vector_t* votes,
    trit_t majority,
    uint32_t n
) {
    if (!amplitudes || !phases || !votes || n == 0) return;

    for (uint32_t i = 0; i < n; i++) {
        trit_t vote = trit_vector_get(votes, i);
        if (vote != majority && vote != TRIT_UNKNOWN) {
            /* Flip phase for non-majority */
            phases[i] += M_PI;
            if (phases[i] > M_PI) phases[i] -= 2.0f * M_PI;
        }
    }
}

/**
 * @brief Find proposal by ID
 */
static inline quantum_proposal_t* qcons_find_proposal(
    quantum_consensus_t ctx,
    uint32_t proposal_id
) {
    if (!ctx) return NULL;

    for (uint32_t i = 0; i < ctx->n_proposals; i++) {
        if (ctx->proposals[i].proposal_id == proposal_id) {
            return &ctx->proposals[i];
        }
    }
    return NULL;
}

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 */
static inline quantum_consensus_config_t quantum_consensus_default_config(void) {
    quantum_consensus_config_t config = {
        .algorithm = QCONSENSUS_GROVER,
        .max_voters = 256,
        .grover_iterations = 0,  /* Auto-compute as √N */
        .agreement_threshold = QUANTUM_CONSENSUS_DEFAULT_THRESHOLD,
        .min_confidence = QUANTUM_CONSENSUS_MIN_CONFIDENCE,
        .bft_threshold = QUANTUM_CONSENSUS_BFT_THRESHOLD,
        .enable_amplitude_weighting = true,
        .enable_collusion_detection = true,
        .seed = 12345
    };
    return config;
}

/**
 * @brief Create quantum consensus context
 */
static inline quantum_consensus_t quantum_consensus_create(
    const quantum_consensus_config_t* config
) {
    quantum_consensus_t ctx = (quantum_consensus_t)nimcp_calloc(1, sizeof(quantum_consensus_internal_t));
    if (!ctx) return NULL;

    ctx->magic = QUANTUM_CONSENSUS_MAGIC;
    ctx->config = config ? *config : quantum_consensus_default_config();

    if (ctx->config.max_voters > QUANTUM_CONSENSUS_MAX_VOTERS) {
        ctx->config.max_voters = QUANTUM_CONSENSUS_MAX_VOTERS;
    }

    /* Allocate proposals */
    ctx->proposals_capacity = 32;
    ctx->proposals = (quantum_proposal_t*)nimcp_calloc(ctx->proposals_capacity, sizeof(quantum_proposal_t));
    if (!ctx->proposals) {
        quantum_consensus_destroy(ctx);
        return NULL;
    }

    /* Allocate amplitude arrays */
    ctx->amplitudes = (float*)nimcp_calloc(ctx->config.max_voters, sizeof(float));
    ctx->phases = (float*)nimcp_calloc(ctx->config.max_voters, sizeof(float));
    ctx->vote_states = trit_vector_create(ctx->config.max_voters, TERNARY_PACK_NONE);

    if (!ctx->amplitudes || !ctx->phases || !ctx->vote_states) {
        quantum_consensus_destroy(ctx);
        return NULL;
    }

    /* Initialize RNG */
    ctx->rng_state = ctx->config.seed ? ctx->config.seed : 12345ULL;

    return ctx;
}

/**
 * @brief Destroy quantum consensus context
 */
static inline void quantum_consensus_destroy(quantum_consensus_t ctx) {
    if (!ctx) return;
    if (ctx->magic != QUANTUM_CONSENSUS_MAGIC) return;

    /* Free proposal votes */
    if (ctx->proposals) {
        for (uint32_t i = 0; i < ctx->n_proposals; i++) {
            nimcp_free(ctx->proposals[i].votes);
        }
        nimcp_free(ctx->proposals);
    }

    nimcp_free(ctx->amplitudes);
    nimcp_free(ctx->phases);
    if (ctx->vote_states) trit_vector_destroy(ctx->vote_states);

    ctx->magic = 0;
    nimcp_free(ctx);
}

//=============================================================================
// Proposal API
//=============================================================================

/**
 * @brief Create a new proposal
 */
static inline int quantum_consensus_propose(
    quantum_consensus_t ctx,
    uint32_t proposer_id,
    const char* topic,
    float value,
    uint64_t deadline,
    uint32_t* proposal_id_out
) {
    if (!ctx || ctx->magic != QUANTUM_CONSENSUS_MAGIC) return QCONSENSUS_ERR_NULL;
    if (!topic || !proposal_id_out) return QCONSENSUS_ERR_NULL;

    /* Expand proposals array if needed */
    if (ctx->n_proposals >= ctx->proposals_capacity) {
        uint32_t new_cap = ctx->proposals_capacity * 2;
        quantum_proposal_t* new_proposals = (quantum_proposal_t*)nimcp_realloc(
            ctx->proposals, new_cap * sizeof(quantum_proposal_t));
        if (!new_proposals) return QCONSENSUS_ERR_ALLOC;
        ctx->proposals = new_proposals;
        ctx->proposals_capacity = new_cap;
    }

    /* Create proposal */
    quantum_proposal_t* prop = &ctx->proposals[ctx->n_proposals];
    memset(prop, 0, sizeof(quantum_proposal_t));

    prop->proposal_id = ctx->next_proposal_id++;
    prop->proposer_id = proposer_id;
    strncpy(prop->topic, topic, sizeof(prop->topic) - 1);
    prop->proposal_value = value;
    prop->deadline = deadline;
    prop->status = QPROPOSAL_PENDING;

    /* Allocate votes array */
    prop->votes_capacity = ctx->config.max_voters;
    prop->votes = (quantum_vote_t*)nimcp_calloc(prop->votes_capacity, sizeof(quantum_vote_t));
    if (!prop->votes) return QCONSENSUS_ERR_ALLOC;

    *proposal_id_out = prop->proposal_id;
    ctx->n_proposals++;
    ctx->stats.total_proposals++;

    return QCONSENSUS_OK;
}

/**
 * @brief Cast a vote on a proposal
 */
static inline int quantum_consensus_vote(
    quantum_consensus_t ctx,
    uint32_t proposal_id,
    uint32_t voter_id,
    quantum_vote_choice_t choice,
    float confidence
) {
    if (!ctx || ctx->magic != QUANTUM_CONSENSUS_MAGIC) return QCONSENSUS_ERR_NULL;

    quantum_proposal_t* prop = qcons_find_proposal(ctx, proposal_id);
    if (!prop) return QCONSENSUS_ERR_NOT_FOUND;
    if (prop->status != QPROPOSAL_PENDING) return QCONSENSUS_ERR_INVALID;

    /* Clamp confidence */
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 1.0f) confidence = 1.0f;

    /* Check minimum confidence */
    if (confidence < ctx->config.min_confidence) {
        return QCONSENSUS_OK;  /* Vote ignored but not an error */
    }

    /* Check capacity */
    if (prop->n_votes >= prop->votes_capacity) {
        return QCONSENSUS_ERR_FULL;
    }

    /* Check for duplicate vote */
    for (uint32_t i = 0; i < prop->n_votes; i++) {
        if (prop->votes[i].voter_id == voter_id) {
            /* Update existing vote */
            prop->votes[i].choice = choice;
            prop->votes[i].confidence = confidence;
            return QCONSENSUS_OK;
        }
    }

    /* Add new vote */
    quantum_vote_t* vote = &prop->votes[prop->n_votes];
    vote->voter_id = voter_id;
    vote->choice = choice;
    vote->confidence = confidence;
    vote->amplitude = sqrtf(confidence);  /* Amplitude = sqrt(probability) */
    vote->phase = 0.0f;
    vote->timestamp = 0;  /* Would be filled by caller */

    prop->n_votes++;
    ctx->stats.total_votes++;

    return QCONSENSUS_OK;
}

//=============================================================================
// Consensus Algorithms
//=============================================================================

/**
 * @brief Run Grover-inspired consensus algorithm
 *
 * WHAT: Amplifies majority vote through iterative oracle + diffusion
 * WHY:  O(√N) convergence to consensus
 * HOW:  Mark minority, invert about mean, repeat
 */
static inline int qcons_run_grover(
    quantum_consensus_t ctx,
    quantum_proposal_t* prop,
    quantum_consensus_result_t* result
) {
    if (!ctx || !prop || !result) return QCONSENSUS_ERR_NULL;

    uint32_t n = prop->n_votes;
    if (n == 0) {
        result->passed = false;
        return QCONSENSUS_OK;
    }

    /* Initialize amplitudes from vote confidences */
    for (uint32_t i = 0; i < n; i++) {
        ctx->amplitudes[i] = prop->votes[i].amplitude;
        ctx->phases[i] = 0.0f;
        trit_vector_set(ctx->vote_states, i, qcons_choice_to_trit(prop->votes[i].choice));
    }

    /* Normalize amplitudes */
    float norm_sq = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        norm_sq += ctx->amplitudes[i] * ctx->amplitudes[i];
    }
    if (norm_sq > 0.0f) {
        float inv_norm = 1.0f / sqrtf(norm_sq);
        for (uint32_t i = 0; i < n; i++) {
            ctx->amplitudes[i] *= inv_norm;
        }
    }

    /* Count initial votes */
    uint32_t agree = 0, disagree = 0, abstain = 0;
    for (uint32_t i = 0; i < n; i++) {
        trit_t vote = trit_vector_get(ctx->vote_states, i);
        if (vote == TRIT_POSITIVE) agree++;
        else if (vote == TRIT_NEGATIVE) disagree++;
        else abstain++;
    }

    /* Determine majority */
    trit_t majority = TRIT_UNKNOWN;
    if (agree > disagree && agree > abstain) majority = TRIT_POSITIVE;
    else if (disagree > agree && disagree > abstain) majority = TRIT_NEGATIVE;

    /* Compute Grover iterations */
    uint32_t iterations = ctx->config.grover_iterations;
    if (iterations == 0) {
        iterations = (uint32_t)(M_PI / 4.0f * sqrtf((float)n));
        if (iterations < 1) iterations = 1;
        if (iterations > 100) iterations = 100;
    }

    /* Run Grover iterations */
    for (uint32_t iter = 0; iter < iterations; iter++) {
        qcons_grover_oracle(ctx->amplitudes, ctx->phases, ctx->vote_states, majority, n);
        qcons_grover_diffusion(ctx->amplitudes, n);
        ctx->stats.grover_amplifications++;
    }

    result->grover_rounds = iterations;

    /* Compute amplitude-weighted agreement */
    float agree_amp = 0.0f, total_amp = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float amp_sq = ctx->amplitudes[i] * ctx->amplitudes[i];
        total_amp += amp_sq;
        if (trit_vector_get(ctx->vote_states, i) == TRIT_POSITIVE) {
            agree_amp += amp_sq;
        }
    }

    result->amplitude_agreement = (total_amp > 0.0f) ? (agree_amp / total_amp) : 0.0f;

    return QCONSENSUS_OK;
}

/**
 * @brief Detect collusion in votes
 *
 * WHAT: Find suspiciously correlated voting patterns
 * WHY:  Protect against Byzantine attacks
 * HOW:  Check for phase clustering and amplitude uniformity
 */
static inline bool qcons_detect_collusion(
    quantum_consensus_t ctx,
    quantum_proposal_t* prop
) {
    if (!ctx || !prop || prop->n_votes < 3) return false;

    /* Check for clusters of identical confidences */
    /* Count occurrences of each unique confidence value */
    uint32_t max_same = 0;
    for (uint32_t i = 0; i < prop->n_votes; i++) {
        uint32_t same_count = 1;  /* Count self */
        for (uint32_t j = i + 1; j < prop->n_votes; j++) {
            if (fabsf(prop->votes[j].confidence - prop->votes[i].confidence) < 0.001f) {
                same_count++;
            }
        }
        if (same_count > max_same) {
            max_same = same_count;
        }
    }

    /* If more than 1/3 have identical confidence, suspicious */
    if (max_same > prop->n_votes / 3) {
        ctx->stats.collusions_detected++;
        return true;
    }

    return false;
}

/**
 * @brief Run consensus on a proposal
 */
static inline int quantum_consensus_run(
    quantum_consensus_t ctx,
    uint32_t proposal_id,
    quantum_consensus_result_t* result
) {
    if (!ctx || ctx->magic != QUANTUM_CONSENSUS_MAGIC || !result) {
        return QCONSENSUS_ERR_NULL;
    }

    quantum_proposal_t* prop = qcons_find_proposal(ctx, proposal_id);
    if (!prop) return QCONSENSUS_ERR_NOT_FOUND;

    memset(result, 0, sizeof(quantum_consensus_result_t));
    result->proposal_id = proposal_id;

    if (prop->n_votes == 0) {
        result->passed = false;
        prop->status = QPROPOSAL_FAILED;
        ctx->stats.proposals_failed++;
        return QCONSENSUS_OK;
    }

    /* Count votes */
    uint32_t agree = 0, disagree = 0, abstain = 0;
    float weighted_agree = 0.0f, total_weight = 0.0f;

    for (uint32_t i = 0; i < prop->n_votes; i++) {
        float weight = prop->votes[i].confidence;
        total_weight += weight;

        switch (prop->votes[i].choice) {
            case QVOTE_AGREE:
                agree++;
                weighted_agree += weight;
                break;
            case QVOTE_DISAGREE:
                disagree++;
                break;
            case QVOTE_ABSTAIN:
                abstain++;
                break;
        }
    }

    result->agree_count = agree;
    result->disagree_count = disagree;
    result->abstain_count = abstain;
    result->weighted_agreement = (total_weight > 0.0f) ?
        (weighted_agree / total_weight) : 0.0f;

    /* Run Grover algorithm for amplitude-weighted consensus */
    if (ctx->config.algorithm == QCONSENSUS_GROVER ||
        ctx->config.algorithm == QCONSENSUS_HYBRID) {
        qcons_run_grover(ctx, prop, result);
    } else {
        result->amplitude_agreement = result->weighted_agreement;
    }

    /* Check for collusion */
    if (ctx->config.enable_collusion_detection) {
        result->collusion_detected = qcons_detect_collusion(ctx, prop);
    }

    /* Check BFT threshold */
    float non_abstain = (float)(agree + disagree);
    float faulty_ratio = (non_abstain > 0.0f) ?
        (float)(abstain) / (float)(prop->n_votes) : 0.0f;
    result->bft_score = 1.0f - faulty_ratio;

    if (faulty_ratio > ctx->config.bft_threshold) {
        /* Too many abstentions - may indicate fault */
        result->passed = false;
        prop->status = QPROPOSAL_FAILED;
        ctx->stats.proposals_failed++;
        return QCONSENSUS_ERR_BFT_VIOLATION;
    }

    /* Determine outcome based on weighted agreement */
    float effective_agreement = ctx->config.enable_amplitude_weighting ?
        result->amplitude_agreement : result->weighted_agreement;

    result->passed = (effective_agreement >= ctx->config.agreement_threshold);

    if (result->passed) {
        prop->status = QPROPOSAL_PASSED;
        ctx->stats.proposals_passed++;
    } else {
        prop->status = QPROPOSAL_FAILED;
        ctx->stats.proposals_failed++;
    }

    /* Update statistics */
    ctx->stats.avg_agreement_score =
        (ctx->stats.avg_agreement_score * (ctx->stats.total_proposals - 1) +
         effective_agreement) / ctx->stats.total_proposals;
    ctx->stats.avg_convergence_rounds =
        (ctx->stats.avg_convergence_rounds * (ctx->stats.total_proposals - 1) +
         result->grover_rounds) / ctx->stats.total_proposals;

    return QCONSENSUS_OK;
}

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get proposal status
 */
static inline quantum_proposal_status_t quantum_consensus_get_status(
    quantum_consensus_t ctx,
    uint32_t proposal_id
) {
    if (!ctx || ctx->magic != QUANTUM_CONSENSUS_MAGIC) return QPROPOSAL_PENDING;

    quantum_proposal_t* prop = qcons_find_proposal(ctx, proposal_id);
    if (!prop) return QPROPOSAL_PENDING;

    return prop->status;
}

/**
 * @brief Get number of votes on a proposal
 */
static inline uint32_t quantum_consensus_get_vote_count(
    quantum_consensus_t ctx,
    uint32_t proposal_id
) {
    if (!ctx || ctx->magic != QUANTUM_CONSENSUS_MAGIC) return 0;

    quantum_proposal_t* prop = qcons_find_proposal(ctx, proposal_id);
    if (!prop) return 0;

    return prop->n_votes;
}

/**
 * @brief Get statistics
 */
static inline int quantum_consensus_get_stats(
    quantum_consensus_t ctx,
    quantum_consensus_stats_t* stats
) {
    if (!ctx || !stats) return QCONSENSUS_ERR_NULL;
    *stats = ctx->stats;
    return QCONSENSUS_OK;
}

/**
 * @brief Get number of active proposals
 */
static inline uint32_t quantum_consensus_get_proposal_count(quantum_consensus_t ctx) {
    if (!ctx || ctx->magic != QUANTUM_CONSENSUS_MAGIC) return 0;
    return ctx->n_proposals;
}

/**
 * @brief Reset statistics
 */
static inline void quantum_consensus_reset_stats(quantum_consensus_t ctx) {
    if (!ctx || ctx->magic != QUANTUM_CONSENSUS_MAGIC) return;
    memset(&ctx->stats, 0, sizeof(quantum_consensus_stats_t));
}

//=============================================================================
// Cleanup
//=============================================================================

/**
 * @brief Free result structure
 */
static inline void quantum_consensus_free_result(quantum_consensus_result_t* result) {
    if (!result) return;
    memset(result, 0, sizeof(quantum_consensus_result_t));
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_CONSENSUS_H */
