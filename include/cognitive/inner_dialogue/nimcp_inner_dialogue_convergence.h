/**
 * @file nimcp_inner_dialogue_convergence.h
 * @brief Convergence, Deadlock, and Rumination Detection for Inner Dialogue
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Algorithms to detect when dialogue has converged, deadlocked, or is ruminating
 * WHY:  Without termination conditions, internal dialogue could loop indefinitely,
 *        wasting metabolic resources and blocking conscious processing
 * HOW:  Multi-signal analysis of agreement trend, repetition, emotional spiral,
 *        and Shannon entropy of act distribution
 *
 * BIOLOGICAL BASIS:
 * The anterior cingulate cortex (ACC) monitors conflict and detects when deliberation
 * has stalled or is circling.  High conflict → continued deliberation;
 * low conflict + agreement → convergence signal;
 * repetitive patterns → rumination detection (linked to depression-like states).
 *
 * QUANTUM ENHANCEMENT:
 * Shannon entropy computation for act diversity uses information-theoretic metrics
 * inspired by the nimcp_quantum_shannon module.  For small histograms (10 acts)
 * this is classical but the same framework supports future quantum walk-based
 * convergence detection over larger state spaces.
 *
 * ERROR CODE RANGE: 29200-29299 (Inner Dialogue Convergence module)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INNER_DIALOGUE_CONVERGENCE_H
#define NIMCP_INNER_DIALOGUE_CONVERGENCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_turn.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Codes (Range: 29200-29299)
 * ============================================================================ */

#define NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_BASE           29200
#define NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_NULL            (NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_BASE + 1)
#define NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_INSUFFICIENT    (NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_BASE + 2)
#define NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_NAN             (NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_BASE + 3)
#define NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_INVALID_CONFIG  (NIMCP_INNER_DIALOGUE_CONVERGENCE_ERROR_BASE + 4)

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Minimum turns required before convergence analysis is meaningful */
#define INNER_DIALOGUE_CONVERGENCE_MIN_TURNS     3

/** Default agreement threshold for convergence declaration */
#define INNER_DIALOGUE_DEFAULT_AGREEMENT_THRESHOLD    0.75f

/** Default deadlock score threshold */
#define INNER_DIALOGUE_DEFAULT_DEADLOCK_THRESHOLD     0.70f

/** Default rumination score threshold */
#define INNER_DIALOGUE_DEFAULT_RUMINATION_THRESHOLD   0.65f

/** Default emotional spiral threshold */
#define INNER_DIALOGUE_DEFAULT_EMOTIONAL_SPIRAL_THRESHOLD  0.80f

/** Window size for trend computation */
#define INNER_DIALOGUE_CONVERGENCE_TREND_WINDOW       8

/* ============================================================================
 * Termination Reasons
 * ============================================================================ */

/**
 * @brief Why a conversation should or did terminate
 *
 * WHAT: Discriminant for termination cause
 * WHY:  Plasticity learning needs to distinguish successful vs failed dialogues
 */
typedef enum {
    TERMINATION_NONE = 0,              /**< No termination recommended */
    TERMINATION_CONVERGED,             /**< Agreement reached */
    TERMINATION_MAX_TURNS,             /**< Turn budget exhausted */
    TERMINATION_DEADLOCKED,            /**< Perspectives irreconcilably opposed */
    TERMINATION_RUMINATING,            /**< Repetitive circling detected */
    TERMINATION_EMOTIONAL_SPIRAL,      /**< Emotional intensity escalating unchecked */
    TERMINATION_SUBSTRATE_SUPPRESSED,  /**< Metabolic state prohibits continuation */
    TERMINATION_CANCELLED,             /**< Externally cancelled */
    TERMINATION_ESCALATED,             /**< Escalated to executive controller */
    TERMINATION_COUNT                  /**< Sentinel */
} termination_reason_t;

/* ============================================================================
 * Convergence Analysis Result
 * ============================================================================ */

/**
 * @brief Complete convergence analysis output
 *
 * WHAT: Multi-dimensional assessment of dialogue state
 * WHY:  Engine uses this to decide continue/converge/terminate
 * HOW:  Computed from turn history by inner_dialogue_convergence_analyse()
 */
typedef struct {
    /* Agreement */
    float agreement_score;              /**< Overall agreement [0-1] */
    float agreement_trend;              /**< Trend: positive = converging */
    bool converged;                     /**< True if agreement >= threshold */

    /* Deadlock */
    float deadlock_score;               /**< Deadlock likelihood [0-1] */
    bool deadlocked;                    /**< True if deadlock >= threshold */

    /* Rumination */
    float rumination_score;             /**< Repetitiveness score [0-1] */
    bool ruminating;                    /**< True if rumination >= threshold */

    /* Emotional state */
    float emotional_temperature;        /**< Average emotional intensity [0-1] */
    bool emotional_spiral;              /**< True if escalating emotional pattern */

    /* Diversity metrics */
    float act_entropy;                  /**< Shannon entropy of act distribution (bits) */
    float perspective_entropy;          /**< Shannon entropy of perspective distribution */

    /* Recommended action */
    termination_reason_t recommended_action; /**< What to do next */

    /* Metadata */
    uint32_t turns_analysed;            /**< How many turns were analysed */
} convergence_analysis_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Convergence detection thresholds
 *
 * WHAT: Tunable parameters for all detection algorithms
 * WHY:  Different conversation types need different sensitivity
 */
typedef struct {
    float agreement_threshold;          /**< Min agreement for convergence [0-1] */
    float deadlock_threshold;           /**< Min score for deadlock [0-1] */
    float rumination_threshold;         /**< Min score for rumination [0-1] */
    float emotional_spiral_threshold;   /**< Min for emotional spiral [0-1] */
    float min_act_entropy;              /**< Below this → monotonous (rumination clue) */
    uint32_t trend_window;              /**< Turns for trend line computation */
    bool enable_quantum_entropy;        /**< Use extended entropy metrics (future) */
} convergence_config_t;

/**
 * @brief Get default convergence configuration
 *
 * @return Configuration with sensible defaults
 */
convergence_config_t inner_dialogue_convergence_default_config(void);

/* ============================================================================
 * Analysis API
 * ============================================================================ */

/**
 * @brief Run full convergence analysis on turn history
 *
 * WHAT: Compute all convergence/deadlock/rumination signals
 * WHY:  Engine calls this after every turn to check termination conditions
 * HOW:  Sequential analysis: agreement → deadlock → rumination → emotion → entropy
 *
 * @param history   Turn history to analyse
 * @param config    Detection thresholds
 * @param analysis  Output analysis result
 * @return 0 on success, error code on failure
 */
int inner_dialogue_convergence_analyse(
    const inner_dialogue_turn_history_t* history,
    const convergence_config_t* config,
    convergence_analysis_t* analysis);

/* ============================================================================
 * Component Analysis Functions
 * ============================================================================ */

/**
 * @brief Compute agreement score from recent turns
 *
 * WHAT: Average of agreement_with_prior across recent turns
 * WHY:  Core convergence signal
 * HOW:  Weighted average with recency bias (exponential decay)
 *
 * @param history  Turn history
 * @param window   Number of recent turns (0 = all)
 * @return Agreement [0-1], or -1.0f on error
 */
float inner_dialogue_convergence_agreement(
    const inner_dialogue_turn_history_t* history,
    uint32_t window);

/**
 * @brief Compute agreement trend via linear regression
 *
 * WHAT: Slope of agreement_with_prior over recent turns
 * WHY:  Positive trend → converging; negative → diverging
 * HOW:  Ordinary least-squares fit on (turn_index, agreement) pairs
 *
 * @param history  Turn history
 * @param window   Number of recent turns for fit
 * @return Slope (positive = converging), or NAN on error
 */
float inner_dialogue_convergence_trend(
    const inner_dialogue_turn_history_t* history,
    uint32_t window);

/**
 * @brief Compute deadlock score
 *
 * WHAT: Detect oscillating disagreement pattern
 * WHY:  Deadlock wastes metabolic resources
 * HOW:  Count challenge-assert-challenge oscillations; ratio to window size
 *
 * @param history  Turn history
 * @param window   Number of recent turns
 * @return Deadlock score [0-1], or -1.0f on error
 */
float inner_dialogue_convergence_deadlock(
    const inner_dialogue_turn_history_t* history,
    uint32_t window);

/**
 * @brief Compute rumination score
 *
 * WHAT: Detect repetitive content and low-diversity act patterns
 * WHY:  Rumination is cognitively destructive and metabolically wasteful
 * HOW:  Content similarity of consecutive turns + low act entropy
 *
 * @param history  Turn history
 * @param window   Number of recent turns
 * @return Rumination score [0-1], or -1.0f on error
 */
float inner_dialogue_convergence_rumination(
    const inner_dialogue_turn_history_t* history,
    uint32_t window);

/**
 * @brief Detect emotional spiral
 *
 * WHAT: Monotonically increasing emotional intensity
 * WHY:  Unchecked emotional escalation can destabilise the dialogue
 * HOW:  Check if abs(emotional_valence) is trending upward across window
 *
 * @param history  Turn history
 * @param window   Number of recent turns
 * @return Emotional temperature [0-1], or -1.0f on error
 */
float inner_dialogue_convergence_emotional_temperature(
    const inner_dialogue_turn_history_t* history,
    uint32_t window);

/**
 * @brief Compute Shannon entropy of perspective distribution
 *
 * WHAT: How evenly are perspectives contributing?
 * WHY:  Monopoly by one perspective suggests imbalanced dialogue
 * HOW:  H = -sum(p_i * log2(p_i)) over perspective frequency counts
 *
 * @param history  Turn history
 * @param window   Number of recent turns (0 = all)
 * @return Entropy in bits, or -1.0f on error
 */
float inner_dialogue_convergence_perspective_entropy(
    const inner_dialogue_turn_history_t* history,
    uint32_t window);

/* ============================================================================
 * Utility
 * ============================================================================ */

/**
 * @brief Convert termination reason to string
 *
 * @param reason Termination reason
 * @return Static string, or "UNKNOWN"
 */
const char* termination_reason_to_string(termination_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INNER_DIALOGUE_CONVERGENCE_H */
