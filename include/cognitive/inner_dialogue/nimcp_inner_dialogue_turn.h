/**
 * @file nimcp_inner_dialogue_turn.h
 * @brief Turn Representation, Dialogue Acts, and History for Inner Dialogue
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Defines dialogue turn structure, dialogue act taxonomy, and circular history buffer
 * WHY:  Structured turn-taking enables traceable multi-perspective deliberation
 * HOW:  Each turn captures perspective, act, content, confidence, and metadata;
 *        history uses a fixed-capacity circular buffer for O(1) append and O(1) lookup
 *
 * BIOLOGICAL BASIS:
 * Neural turn-taking mirrors cortical oscillatory gating where gamma bursts
 * within theta cycles provide ordered "slots" for different neural populations
 * to contribute information.  This header models those slots as typed dialogue
 * acts and records a bounded history analogous to working-memory decay.
 *
 * ERROR CODE RANGE: 29000-29099 (Inner Dialogue Turn module)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INNER_DIALOGUE_TURN_H
#define NIMCP_INNER_DIALOGUE_TURN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct nimcp_health_agent;

/* ============================================================================
 * Error Codes (Range: 29000-29099)
 * ============================================================================ */

#define NIMCP_INNER_DIALOGUE_TURN_ERROR_BASE         29000
#define NIMCP_INNER_DIALOGUE_TURN_ERROR_NULL          (NIMCP_INNER_DIALOGUE_TURN_ERROR_BASE + 1)
#define NIMCP_INNER_DIALOGUE_TURN_ERROR_OVERFLOW      (NIMCP_INNER_DIALOGUE_TURN_ERROR_BASE + 2)
#define NIMCP_INNER_DIALOGUE_TURN_ERROR_INVALID_ID    (NIMCP_INNER_DIALOGUE_TURN_ERROR_BASE + 3)
#define NIMCP_INNER_DIALOGUE_TURN_ERROR_NO_MEMORY     (NIMCP_INNER_DIALOGUE_TURN_ERROR_BASE + 4)
#define NIMCP_INNER_DIALOGUE_TURN_ERROR_EMPTY         (NIMCP_INNER_DIALOGUE_TURN_ERROR_BASE + 5)

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum content string length per turn */
#define INNER_DIALOGUE_TURN_MAX_CONTENT     512

/** Maximum history depth (power of 2 for efficient modular arithmetic) */
#define INNER_DIALOGUE_MAX_HISTORY          128

/** Maximum tags per turn for metadata annotation */
#define INNER_DIALOGUE_TURN_MAX_TAGS        4

/** Tag string length */
#define INNER_DIALOGUE_TURN_TAG_LEN         32

/* ============================================================================
 * Dialogue Act Taxonomy
 * ============================================================================ */

/**
 * @brief Dialogue act types for structured internal conversation
 *
 * WHAT: Categorises the communicative function of each turn
 * WHY:  Enables convergence analysis, pattern detection, and act-level learning
 * HOW:  Enum discriminant with clear semantics per category
 *
 * BIOLOGICAL: Analogous to distinct firing patterns in prefrontal cortex
 * during deliberation — assertions produce sustained activity, questions
 * trigger exploratory bursts, challenges elicit error-related negativity.
 */
typedef enum {
    DIALOGUE_ACT_ASSERT = 0,    /**< State a position or conclusion */
    DIALOGUE_ACT_QUESTION,      /**< Ask for information or clarification */
    DIALOGUE_ACT_CHALLENGE,     /**< Disagree with a prior assertion */
    DIALOGUE_ACT_ELABORATE,     /**< Expand on a previous thought */
    DIALOGUE_ACT_SYNTHESIZE,    /**< Combine multiple viewpoints */
    DIALOGUE_ACT_CONCLUDE,      /**< Propose final conclusion */
    DIALOGUE_ACT_DEFER,         /**< Yield to another perspective */
    DIALOGUE_ACT_INTROSPECT,    /**< Report on internal state */
    DIALOGUE_ACT_REFRAME,       /**< Reinterpret the topic from new angle */
    DIALOGUE_ACT_WARN,          /**< Flag a risk or concern */
    DIALOGUE_ACT_COUNT          /**< Sentinel — number of act types */
} dialogue_act_t;

/* ============================================================================
 * Turn Structure
 * ============================================================================ */

/**
 * @brief Single dialogue turn produced by a perspective
 *
 * WHAT: Immutable record of one perspective's contribution
 * WHY:  Full provenance for convergence analysis and STDP learning
 * HOW:  Value struct with perspective ID, act, content, and scores
 */
typedef struct {
    /* Identity */
    uint32_t turn_id;                           /**< Monotonic sequence number */
    uint32_t conversation_id;                   /**< Owning conversation */
    uint32_t perspective_idx;                    /**< Which perspective produced this */

    /* Act and content */
    dialogue_act_t act;                         /**< Communicative function */
    char content[INNER_DIALOGUE_TURN_MAX_CONTENT]; /**< Textual content (null-terminated) */
    uint32_t content_len;                       /**< Actual content length (bytes) */

    /* Scores */
    float confidence;                           /**< Perspective's self-assessed confidence [0-1] */
    float relevance;                            /**< Relevance to current topic [0-1] */
    float novelty;                              /**< Information novelty vs prior turns [0-1] */
    float agreement_with_prior;                 /**< Agreement with immediately preceding turn [0-1] */
    float emotional_valence;                    /**< Emotional tone [-1..+1] */

    /* References */
    uint32_t references_turn_id;                /**< Turn this responds to (0 = none) */

    /* Timing */
    uint64_t timestamp_us;                      /**< Microsecond timestamp */
    float formulation_time_ms;                  /**< How long perspective took to formulate */

    /* Metadata tags */
    char tags[INNER_DIALOGUE_TURN_MAX_TAGS][INNER_DIALOGUE_TURN_TAG_LEN];
    uint32_t num_tags;
} inner_dialogue_turn_t;

/* ============================================================================
 * Turn History (Circular Buffer)
 * ============================================================================ */

/**
 * @brief Turn history statistics
 *
 * WHAT: Aggregate metrics across recorded turns
 * WHY:  Fast convergence/rumination queries without re-scanning history
 */
typedef struct {
    uint32_t total_turns_recorded;              /**< Cumulative (may exceed capacity) */
    uint32_t current_count;                     /**< Turns currently in buffer */
    float avg_confidence;                       /**< Running average confidence */
    float avg_relevance;                        /**< Running average relevance */
    float avg_novelty;                          /**< Running average novelty */
    uint32_t act_counts[DIALOGUE_ACT_COUNT];    /**< Count per dialogue act type */
    uint32_t perspective_counts[16];            /**< Count per perspective (up to 16) */
} inner_dialogue_turn_history_stats_t;

/**
 * @brief Circular-buffer turn history
 *
 * WHAT: Bounded FIFO of dialogue turns with aggregate statistics
 * WHY:  Convergence and rumination detection need recent-history access
 * HOW:  Power-of-2 capacity enables bitwise-AND modular indexing
 *
 * THREAD SAFETY: Caller must hold engine mutex before mutating history.
 */
typedef struct {
    inner_dialogue_turn_t* turns;               /**< Heap-allocated ring buffer */
    uint32_t capacity;                          /**< Always INNER_DIALOGUE_MAX_HISTORY */
    uint32_t head;                              /**< Next write index */
    uint32_t count;                             /**< Valid entries (<= capacity) */
    uint32_t next_turn_id;                      /**< Monotonic ID generator */
    inner_dialogue_turn_history_stats_t stats;  /**< Running statistics */
} inner_dialogue_turn_history_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create a new turn history buffer
 *
 * WHAT: Allocate and initialise circular history buffer
 * WHY:  Engine needs per-conversation turn storage
 * HOW:  nimcp_malloc the ring array, zero stats
 *
 * @return New history or NULL on allocation failure
 */
inner_dialogue_turn_history_t* inner_dialogue_turn_history_create(void);

/**
 * @brief Destroy turn history and free resources
 *
 * @param history History to destroy (NULL-safe)
 */
void inner_dialogue_turn_history_destroy(inner_dialogue_turn_history_t* history);

/**
 * @brief Reset history to empty state without reallocation
 *
 * @param history History to reset
 * @return 0 on success, error code on failure
 */
int inner_dialogue_turn_history_reset(inner_dialogue_turn_history_t* history);

/* ============================================================================
 * Turn Recording API
 * ============================================================================ */

/**
 * @brief Record a turn into history
 *
 * WHAT: Append turn to circular buffer, update running stats
 * WHY:  Every perspective formulation must be recorded for analysis
 * HOW:  Write at head, advance, recalculate incremental stats
 *
 * @param history History buffer
 * @param turn    Turn to record (copied into buffer)
 * @return Assigned turn_id on success, or -1 on error
 */
int inner_dialogue_turn_history_record(inner_dialogue_turn_history_t* history,
                                        const inner_dialogue_turn_t* turn);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get the most recent turn
 *
 * @param history History buffer
 * @return Pointer to most recent turn (valid until next record), or NULL if empty
 */
const inner_dialogue_turn_t* inner_dialogue_turn_history_get_latest(
    const inner_dialogue_turn_history_t* history);

/**
 * @brief Get turn by index (0 = most recent, 1 = second most recent, ...)
 *
 * @param history History buffer
 * @param index   Reverse index from head
 * @return Pointer to turn, or NULL if index out of range
 */
const inner_dialogue_turn_t* inner_dialogue_turn_history_get_at(
    const inner_dialogue_turn_history_t* history, uint32_t index);

/**
 * @brief Get turn by absolute turn_id
 *
 * @param history History buffer
 * @param turn_id Turn ID to find
 * @return Pointer to turn, or NULL if not in buffer
 */
const inner_dialogue_turn_t* inner_dialogue_turn_history_get_by_id(
    const inner_dialogue_turn_history_t* history, uint32_t turn_id);

/**
 * @brief Get number of turns currently in buffer
 *
 * @param history History buffer
 * @return Current count, or 0 if history is NULL
 */
uint32_t inner_dialogue_turn_history_count(
    const inner_dialogue_turn_history_t* history);

/**
 * @brief Get aggregate statistics
 *
 * @param history History buffer
 * @param stats   Output statistics struct
 * @return 0 on success, error code on failure
 */
int inner_dialogue_turn_history_get_stats(
    const inner_dialogue_turn_history_t* history,
    inner_dialogue_turn_history_stats_t* stats);

/* ============================================================================
 * Analysis Helpers
 * ============================================================================ */

/**
 * @brief Compute Shannon entropy of dialogue act distribution
 *
 * WHAT: Information-theoretic diversity measure of act usage
 * WHY:  Low entropy indicates monotonous dialogue (possible rumination)
 * HOW:  H = -sum(p_i * log2(p_i)) over act frequencies in recent turns
 *
 * @param history History buffer
 * @param window  Number of recent turns to analyse (0 = all in buffer)
 * @return Entropy in bits, or -1.0f on error
 */
float inner_dialogue_turn_history_act_entropy(
    const inner_dialogue_turn_history_t* history, uint32_t window);

/**
 * @brief Compute content similarity between two turns using Jaccard index
 *
 * WHAT: Word-level overlap metric
 * WHY:  Detect repetitive content (rumination indicator)
 * HOW:  Tokenise on whitespace, compute |intersection| / |union|
 *
 * @param a First turn
 * @param b Second turn
 * @return Similarity [0-1], or -1.0f on error
 */
float inner_dialogue_turn_content_similarity(const inner_dialogue_turn_t* a,
                                              const inner_dialogue_turn_t* b);

/**
 * @brief Convert dialogue act enum to string
 *
 * @param act Dialogue act
 * @return Static string name, or "UNKNOWN"
 */
const char* dialogue_act_to_string(dialogue_act_t act);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INNER_DIALOGUE_TURN_H */
