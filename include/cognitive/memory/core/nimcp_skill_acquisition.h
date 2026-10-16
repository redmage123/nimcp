//=============================================================================
// nimcp_skill_acquisition.h - Skill Acquisition System for Prime Resonant Memory
//=============================================================================
/**
 * @file nimcp_skill_acquisition.h
 * @brief Procedural skill learning with power law, transfer, and plateaus
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Skill acquisition system implementing learning curves, transfer, and
 *       deliberate practice for procedural memory in Prime Resonant architecture
 * WHY:  Enable realistic skill learning dynamics including power law of practice,
 *       positive/negative transfer between skills, and plateau detection
 * HOW:  Models skill acquisition using psychologically-grounded learning curves
 *       with integration into PR memory's prime signatures and resonance
 *
 * PSYCHOLOGICAL FOUNDATIONS:
 *
 *   Power Law of Practice (Newell & Rosenbloom, 1981):
 *   +-----------------------------------------------------------------------+
 *   |  Performance improves as a power function of practice:                |
 *   |                                                                       |
 *   |  RT = a * n^(-b) + c                                                  |
 *   |                                                                       |
 *   |  Where:                                                               |
 *   |    RT = Response time / error rate (performance measure)              |
 *   |    a  = Initial performance (starting point)                          |
 *   |    n  = Practice count (trial number)                                 |
 *   |    b  = Learning rate exponent (typically 0.2-0.5)                    |
 *   |    c  = Asymptote (minimum achievable performance)                    |
 *   |                                                                       |
 *   |  Example with a=1000ms, b=0.3, c=200ms:                               |
 *   |    Trial 1:   1000 * 1^(-0.3) + 200 = 1200ms                         |
 *   |    Trial 10:  1000 * 10^(-0.3) + 200 = 701ms                         |
 *   |    Trial 100: 1000 * 100^(-0.3) + 200 = 451ms                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Skill Transfer Theory (Thorndike & Woodworth, 1901):
 *   +-----------------------------------------------------------------------+
 *   |  Prior skills affect new skill acquisition:                           |
 *   |                                                                       |
 *   |  POSITIVE TRANSFER: Shared elements accelerate learning               |
 *   |    - Piano -> Keyboard typing (motor patterns)                        |
 *   |    - Chess -> Go (strategic thinking)                                 |
 *   |    - Spanish -> Portuguese (linguistic similarity)                    |
 *   |                                                                       |
 *   |  NEGATIVE TRANSFER: Conflicting elements impede learning              |
 *   |    - Left-hand drive -> Right-hand drive (motor conflict)             |
 *   |    - Java -> Python (syntax interference)                             |
 *   |    - Tennis -> Badminton (grip/swing conflict)                        |
 *   |                                                                       |
 *   |  NEUTRAL: No significant effect                                       |
 *   |    - Completely unrelated skill domains                               |
 *   |                                                                       |
 *   |  Transfer Magnitude: -1.0 (max negative) to +1.0 (max positive)       |
 *   +-----------------------------------------------------------------------+
 *
 *   Learning Plateaus (Anderson, 1982):
 *   +-----------------------------------------------------------------------+
 *   |  Periods of no improvement despite continued practice:                |
 *   |                                                                       |
 *   |  Performance                                                          |
 *   |      ^                                                                |
 *   |      |      ___plateau___                                             |
 *   |      |     /             \                                            |
 *   |      |    /               \                                           |
 *   |      |   /                 \___                                       |
 *   |      |  /                      \_____                                 |
 *   |      | /                             \____                            |
 *   |      +-----------------------------------------> Practice             |
 *   |                                                                       |
 *   |  Causes:                                                              |
 *   |    - Strategy ceiling (current approach optimized)                    |
 *   |    - Knowledge restructuring in progress                              |
 *   |    - Motivational factors                                             |
 *   |                                                                       |
 *   |  Breakthrough strategies:                                             |
 *   |    - Strategy change (new approach)                                   |
 *   |    - Component practice (focus on weak elements)                      |
 *   |    - Contextual variation                                             |
 *   +-----------------------------------------------------------------------+
 *
 *   Deliberate Practice (Ericsson, 1993):
 *   +-----------------------------------------------------------------------+
 *   |  Structured practice targeting specific weaknesses:                   |
 *   |                                                                       |
 *   |  Key elements:                                                        |
 *   |    1. Explicit focus on improvement                                   |
 *   |    2. Immediate feedback                                              |
 *   |    3. High cognitive effort                                           |
 *   |    4. Targeted weak point practice                                    |
 *   |                                                                       |
 *   |  vs Naive Practice:                                                   |
 *   |    - Deliberate: Slow initial improvement, higher ceiling             |
 *   |    - Naive: Fast initial improvement, earlier plateau                 |
 *   +-----------------------------------------------------------------------+
 *
 * ARCHITECTURE:
 *
 *   Skill Acquisition Integration with PR Memory:
 *   +-----------------------------------------------------------------------+
 *   |                                                                       |
 *   |  +------------------+     +----------------------+                    |
 *   |  | Procedural Skill |     | Prime Signature      |                    |
 *   |  | - Steps/elements |<--->| - Content fingerprint|                    |
 *   |  | - Motor patterns |     | - Similarity matching|                    |
 *   |  +------------------+     +----------------------+                    |
 *   |          |                          |                                 |
 *   |          v                          v                                 |
 *   |  +------------------+     +----------------------+                    |
 *   |  | Acquisition State|     | Resonance Scoring    |                    |
 *   |  | - Power law curve|     | - Transfer detection |                    |
 *   |  | - Plateau detect |     | - Element overlap    |                    |
 *   |  | - Weak points    |     +----------------------+                    |
 *   |  +------------------+                                                 |
 *   |          |                                                            |
 *   |          v                                                            |
 *   |  +------------------+                                                 |
 *   |  | Deliberate       |                                                 |
 *   |  | Practice Planner |                                                 |
 *   |  | - Focus on weak  |                                                 |
 *   |  | - Break plateaus |                                                 |
 *   |  +------------------+                                                 |
 *   |                                                                       |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Record trial: ~1us
 * - Predict performance: ~50ns
 * - Compute transfer: ~500ns (per skill pair)
 * - Detect plateau: ~100ns
 * - Fit power law: ~100us (iterative fitting)
 * - Analyze weak points: ~10us
 *
 * MEMORY:
 * - skill_acquisition_t: ~2KB base + N*sizeof(state) for N skills
 * - skill_acquisition_state_t: ~256 bytes + history buffer
 * - Performance history: history_len * 4 bytes per skill
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe
 * - Internal mutex protects skill registry
 * - Per-skill operations can be parallelized
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SKILL_ACQUISITION_H
#define NIMCP_SKILL_ACQUISITION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Prime Resonant core dependencies */
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_resonance.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Export macro (for shared library builds) */
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default initial performance coefficient (a) */
#define SKILL_DEFAULT_INITIAL_PERFORMANCE   1000.0f

/** Default learning rate exponent (b) - typical range 0.2-0.5 */
#define SKILL_DEFAULT_LEARNING_RATE         0.3f

/** Default asymptote (c) - minimum achievable */
#define SKILL_DEFAULT_ASYMPTOTE             100.0f

/** Default history buffer length (trials to retain) */
#define SKILL_DEFAULT_HISTORY_LEN           1000

/** Maximum skill steps per procedural skill */
#define SKILL_MAX_STEPS                     256

/** Maximum transfers per skill */
#define SKILL_MAX_TRANSFERS                 64

/** Maximum plateaus tracked per skill */
#define SKILL_MAX_PLATEAUS                  32

/** Default plateau detection window (trials) */
#define SKILL_DEFAULT_PLATEAU_WINDOW        50

/** Default plateau threshold (variance ratio) */
#define SKILL_DEFAULT_PLATEAU_THRESHOLD     0.01f

/** Minimum trials before plateau detection */
#define SKILL_MIN_TRIALS_FOR_PLATEAU        20

/** Default weak point threshold (error ratio) */
#define SKILL_DEFAULT_WEAK_POINT_THRESHOLD  0.2f

/** Maximum skill name length */
#define SKILL_MAX_NAME_LEN                  128

/** Invalid skill ID sentinel */
#define SKILL_INVALID_ID                    UINT64_MAX

/** Minimum practice count for valid prediction */
#define SKILL_MIN_PRACTICE_COUNT            1

/** Power law fitting tolerance */
#define SKILL_FIT_TOLERANCE                 1e-4f

/** Maximum fitting iterations */
#define SKILL_FIT_MAX_ITERATIONS            100

/** Numerical epsilon */
#define SKILL_EPSILON                       1e-6f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Transfer type between skills
 */
typedef enum {
    TRANSFER_POSITIVE = 0,    /**< Prior skill helps new skill */
    TRANSFER_NEGATIVE,        /**< Prior skill interferes */
    TRANSFER_NEUTRAL          /**< No transfer effect */
} transfer_type_t;

/**
 * @brief Plateau breakthrough strategy
 */
typedef enum {
    PLATEAU_STRATEGY_NONE = 0,        /**< No strategy needed */
    PLATEAU_STRATEGY_VARY_CONTEXT,    /**< Vary practice context */
    PLATEAU_STRATEGY_FOCUS_WEAK,      /**< Focus on weak steps */
    PLATEAU_STRATEGY_CHANGE_APPROACH, /**< Try different strategy */
    PLATEAU_STRATEGY_REST,            /**< Allow consolidation time */
    PLATEAU_STRATEGY_INCREASE_FEEDBACK, /**< More immediate feedback */
    PLATEAU_STRATEGY_DECOMPOSE,       /**< Break into sub-skills */
    PLATEAU_STRATEGY_COUNT            /**< Number of strategies */
} plateau_strategy_t;

/**
 * @brief Practice quality level
 */
typedef enum {
    PRACTICE_NAIVE = 0,       /**< Unfocused repetition */
    PRACTICE_INTENTIONAL,     /**< Aware but not targeted */
    PRACTICE_DELIBERATE,      /**< Targeted, focused practice */
    PRACTICE_EXPERT           /**< Expert-level optimization */
} practice_quality_t;

/**
 * @brief Skill acquisition error codes
 */
typedef enum {
    SKILL_SUCCESS = 0,                    /**< Operation succeeded */
    SKILL_ERROR_NULL_POINTER = -1,        /**< NULL pointer argument */
    SKILL_ERROR_INVALID_ID = -2,          /**< Invalid skill ID */
    SKILL_ERROR_NOT_FOUND = -3,           /**< Skill not found */
    SKILL_ERROR_NO_MEMORY = -4,           /**< Memory allocation failed */
    SKILL_ERROR_INVALID_PARAM = -5,       /**< Invalid parameter value */
    SKILL_ERROR_ALREADY_EXISTS = -6,      /**< Skill already registered */
    SKILL_ERROR_FULL = -7,                /**< No capacity for more skills */
    SKILL_ERROR_INSUFFICIENT_DATA = -8,   /**< Not enough data for operation */
    SKILL_ERROR_FIT_FAILED = -9,          /**< Power law fitting failed */
    SKILL_ERROR_NOT_ON_PLATEAU = -10      /**< Not currently on plateau */
} skill_error_t;

/**
 * @brief Power law learning curve state
 *
 * Models performance as: P(n) = a * n^(-b) + c
 */
typedef struct {
    float initial_performance;   /**< a coefficient (initial performance) */
    float learning_rate;         /**< b exponent (learning rate, typically 0.2-0.5) */
    float asymptote;             /**< c constant (minimum achievable performance) */
    size_t practice_count;       /**< n (total practice trials) */
    float fit_r_squared;         /**< R^2 goodness of fit (0-1) */
    bool fit_valid;              /**< Whether fit is reliable */
} power_law_state_t;

/**
 * @brief Skill transfer relationship
 */
typedef struct {
    uint64_t source_skill_id;    /**< Prior skill ID */
    uint64_t target_skill_id;    /**< New skill ID */
    transfer_type_t type;        /**< Transfer type */
    float transfer_magnitude;    /**< How much transfer (-1 to +1) */
    float element_overlap;       /**< Shared elements ratio (0-1) */
    float surface_similarity;    /**< Surface feature similarity (0-1) */
    float structural_similarity; /**< Deep structural similarity (0-1) */
    float prime_sig_similarity;  /**< Prime signature Jaccard (0-1) */
} skill_transfer_t;

/**
 * @brief Learning plateau record
 */
typedef struct {
    size_t start_trial;          /**< Trial where plateau began */
    size_t end_trial;            /**< Trial where plateau ended (0 if ongoing) */
    float plateau_performance;   /**< Performance level during plateau */
    float performance_variance;  /**< Variance during plateau period */
    size_t duration;             /**< Plateau duration in trials */
    bool overcome;               /**< Whether plateau was overcome */
    plateau_strategy_t strategy_used; /**< What broke the plateau */
} learning_plateau_t;

/**
 * @brief Procedural skill definition
 *
 * Represents a skill composed of sequential steps/elements
 */
typedef struct {
    uint64_t skill_id;           /**< Unique skill identifier */
    char name[SKILL_MAX_NAME_LEN]; /**< Human-readable name */

    /* Skill structure */
    size_t num_steps;            /**< Number of steps in procedure */
    char** step_names;           /**< Names for each step (optional) */

    /* Prime signature for content-based matching */
    prime_signature_t* signature; /**< Skill content signature */

    /* Semantic state */
    nimcp_quaternion_t state;    /**< Consolidation/emotion/salience/access */

    /* Creation timestamp */
    uint64_t created_time_ms;    /**< When skill was registered */
} procedural_skill_t;

/**
 * @brief Skill acquisition state for a single skill
 *
 * Tracks learning progress, plateaus, and weak points
 */
typedef struct {
    procedural_skill_t* skill;   /**< Associated skill */

    /* Learning curve state */
    power_law_state_t power_law; /**< Power law parameters */
    float* performance_history;  /**< Performance per trial */
    size_t history_len;          /**< History buffer size */
    size_t history_count;        /**< Actual history entries */

    /* Transfer relationships */
    skill_transfer_t* transfers; /**< Transfer relationships */
    size_t num_transfers;        /**< Number of transfers */
    size_t max_transfers;        /**< Transfer array capacity */

    /* Plateau tracking */
    learning_plateau_t* plateaus; /**< Plateau history */
    size_t num_plateaus;         /**< Number of plateaus recorded */
    size_t max_plateaus;         /**< Plateau array capacity */
    learning_plateau_t* current_plateau; /**< Active plateau (NULL if none) */

    /* Step-level analysis */
    size_t* step_errors;         /**< Error count per step */
    size_t* step_practice_count; /**< Practice count per step */
    float* step_difficulty;      /**< Estimated difficulty per step (0-1) */
    size_t most_difficult_step;  /**< Index of hardest step */

    /* Deliberate practice metrics */
    practice_quality_t practice_quality; /**< Current practice quality */
    float feedback_frequency;    /**< How often feedback given (0-1) */
    float focus_intensity;       /**< Mental focus level (0-1) */

    /* Statistics */
    uint64_t total_practice_time_ms; /**< Total time spent practicing */
    float best_performance;      /**< Best performance achieved */
    float worst_performance;     /**< Worst performance recorded */
    float recent_improvement;    /**< Improvement in last N trials */

} skill_acquisition_state_t;

/**
 * @brief Skill acquisition system configuration
 */
typedef struct {
    /* Power law defaults */
    float default_initial_performance; /**< Default a coefficient */
    float default_learning_rate;       /**< Default b exponent */
    float default_asymptote;           /**< Default c constant */

    /* History settings */
    size_t default_history_len;        /**< Default history buffer size */

    /* Plateau detection */
    float plateau_threshold;           /**< Performance variance threshold */
    size_t plateau_window;             /**< Trials to check for plateau */
    size_t min_trials_for_plateau;     /**< Minimum trials before detection */

    /* Weak point analysis */
    float weak_point_threshold;        /**< Error ratio for weak point */

    /* Transfer computation */
    float element_weight;              /**< Weight for element overlap */
    float surface_weight;              /**< Weight for surface similarity */
    float structural_weight;           /**< Weight for structural similarity */
    float signature_weight;            /**< Weight for prime signature */

    /* Performance */
    bool enable_statistics;            /**< Track detailed statistics */
    bool auto_fit_power_law;           /**< Auto-refit after trials */
    size_t fit_interval;               /**< Trials between refitting */
} skill_acquisition_config_t;

/**
 * @brief Skill acquisition system handle
 */
typedef struct {
    /* Current skill being acquired */
    skill_acquisition_state_t* current; /**< Currently active skill */

    /* All acquisition states */
    skill_acquisition_state_t** states; /**< Array of skill states */
    size_t num_states;                  /**< Number of skills */
    size_t max_states;                  /**< Capacity */

    /* Transfer network */
    float** transfer_matrix;            /**< Skills x Skills transfer magnitudes */
    size_t matrix_size;                 /**< Matrix dimension */

    /* Plateau detection settings */
    float plateau_threshold;            /**< Performance variance threshold */
    size_t plateau_window;              /**< Trials to check */

    /* Configuration */
    skill_acquisition_config_t config;  /**< System configuration */

    /* Statistics */
    uint64_t total_trials_recorded;     /**< Total trials across all skills */
    uint64_t plateaus_detected;         /**< Total plateaus detected */
    uint64_t plateaus_overcome;         /**< Total plateaus broken */
    float avg_transfer_magnitude;       /**< Average transfer effect */

} skill_acquisition_t;

/**
 * @brief Trial result data
 */
typedef struct {
    float performance;           /**< Performance measure (e.g., time, errors) */
    float* step_performances;    /**< Per-step performance (optional) */
    size_t num_steps;            /**< Number of steps recorded */
    uint64_t timestamp_ms;       /**< Trial timestamp */
    bool success;                /**< Whether trial was successful */
    float feedback_given;        /**< Feedback provided (0-1) */
} trial_result_t;

/**
 * @brief Performance prediction result
 */
typedef struct {
    float predicted_performance; /**< Predicted performance at trial N */
    float confidence_low;        /**< Lower confidence bound */
    float confidence_high;       /**< Upper confidence bound */
    float improvement_rate;      /**< Current improvement rate */
    size_t target_trial;         /**< Trial number predicted for */
} performance_prediction_t;

/**
 * @brief Mastery estimation result
 */
typedef struct {
    size_t trials_to_mastery;    /**< Estimated trials to reach target */
    float time_to_mastery_ms;    /**< Estimated time to mastery */
    float current_performance;   /**< Current performance level */
    float target_performance;    /**< Target performance level */
    float confidence;            /**< Confidence in estimate (0-1) */
    bool already_mastered;       /**< Whether target already achieved */
} mastery_estimate_t;

/**
 * @brief Weak point analysis result
 */
typedef struct {
    size_t* weak_step_indices;   /**< Indices of weak steps */
    float* weak_step_errors;     /**< Error rates for weak steps */
    size_t num_weak_steps;       /**< Number of weak steps */
    size_t most_critical;        /**< Most critical weak point index */
    float overall_weakness;      /**< Overall weakness score (0-1) */
} weak_point_analysis_t;

/**
 * @brief Deliberate practice plan
 */
typedef struct {
    size_t* focus_steps;         /**< Steps to focus on */
    float* focus_durations;      /**< Relative time per step */
    size_t num_focus_steps;      /**< Number of focus areas */
    float recommended_feedback_freq; /**< Recommended feedback frequency */
    practice_quality_t target_quality; /**< Target practice quality */
    char* strategy_description;  /**< Human-readable strategy */
} deliberate_practice_plan_t;

/**
 * @brief Plateau breakthrough suggestion
 */
typedef struct {
    plateau_strategy_t primary_strategy;   /**< Recommended primary strategy */
    plateau_strategy_t secondary_strategy; /**< Backup strategy */
    float estimated_breakthrough_trials;   /**< Trials to break plateau */
    float confidence;                      /**< Confidence in suggestion */
    char* rationale;                       /**< Explanation of suggestion */
} breakthrough_suggestion_t;

/**
 * @brief Learning curve data for visualization
 */
typedef struct {
    float* trial_numbers;        /**< X-axis: trial numbers */
    float* performances;         /**< Y-axis: actual performances */
    float* predicted;            /**< Y-axis: power law predictions */
    size_t num_points;           /**< Number of data points */
    float fit_r_squared;         /**< Goodness of fit */
} learning_curve_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default skill acquisition configuration
 *
 * @return Default configuration with sensible values
 *
 * Defaults:
 * - default_initial_performance: 1000.0
 * - default_learning_rate: 0.3
 * - default_asymptote: 100.0
 * - plateau_threshold: 0.01
 * - plateau_window: 50
 */
NIMCP_EXPORT skill_acquisition_config_t skill_acquisition_config_default(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool skill_acquisition_config_validate(
    const skill_acquisition_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create skill acquisition system
 *
 * WHAT: Initialize skill acquisition system
 * WHY:  Entry point for skill learning functionality
 * HOW:  Allocates state, initializes transfer matrix, sets configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return System handle, or NULL on failure
 *
 * Performance: ~1ms
 * Memory: ~2KB base + space for skills
 */
NIMCP_EXPORT skill_acquisition_t* skill_acquisition_create(
    const skill_acquisition_config_t* config);

/**
 * @brief Destroy skill acquisition system
 *
 * WHAT: Free all resources
 * WHY:  Clean shutdown
 *
 * @param sa System handle (NULL safe)
 *
 * Performance: ~100us
 */
NIMCP_EXPORT void skill_acquisition_destroy(skill_acquisition_t* sa);

/**
 * @brief Reset skill acquisition system
 *
 * @param sa System handle
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_reset(skill_acquisition_t* sa);

//=============================================================================
// Skill Registration Functions
//=============================================================================

/**
 * @brief Register a new procedural skill
 *
 * WHAT: Add a skill to the acquisition system
 * WHY:  Skills must be registered before tracking
 *
 * @param sa System handle
 * @param name Human-readable skill name
 * @param num_steps Number of steps/elements in skill
 * @param step_names Optional names for each step (can be NULL)
 * @param signature Optional prime signature (can be NULL for auto-compute)
 * @return Skill ID, or SKILL_INVALID_ID on error
 *
 * Performance: ~10us
 */
NIMCP_EXPORT uint64_t skill_acquisition_register_skill(
    skill_acquisition_t* sa,
    const char* name,
    size_t num_steps,
    const char** step_names,
    const prime_signature_t* signature);

/**
 * @brief Unregister a skill
 *
 * @param sa System handle
 * @param skill_id Skill to unregister
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_unregister_skill(
    skill_acquisition_t* sa,
    uint64_t skill_id);

/**
 * @brief Get skill by ID
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @return Skill pointer, or NULL if not found
 */
NIMCP_EXPORT procedural_skill_t* skill_acquisition_get_skill(
    skill_acquisition_t* sa,
    uint64_t skill_id);

/**
 * @brief Get skill acquisition state
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @return Acquisition state, or NULL if not found
 */
NIMCP_EXPORT skill_acquisition_state_t* skill_acquisition_get_state(
    skill_acquisition_t* sa,
    uint64_t skill_id);

/**
 * @brief Set current active skill
 *
 * @param sa System handle
 * @param skill_id Skill to set as current
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_set_current(
    skill_acquisition_t* sa,
    uint64_t skill_id);

//=============================================================================
// Trial Recording Functions
//=============================================================================

/**
 * @brief Record a practice trial
 *
 * WHAT: Record performance from a single practice trial
 * WHY:  Accumulate data for learning curve analysis
 * HOW:  Adds to history, updates power law fit, checks for plateau
 *
 * @param sa System handle
 * @param skill_id Skill practiced
 * @param result Trial result data
 * @return SKILL_SUCCESS or error code
 *
 * Side effects:
 * - Updates performance history
 * - Updates practice count
 * - May refit power law
 * - May detect/update plateau
 *
 * Performance: ~1us (without refitting)
 */
NIMCP_EXPORT skill_error_t skill_acquisition_record_trial(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    const trial_result_t* result);

/**
 * @brief Record multiple trials (batch)
 *
 * @param sa System handle
 * @param skill_id Skill practiced
 * @param results Array of trial results
 * @param count Number of trials
 * @return Number of trials successfully recorded
 */
NIMCP_EXPORT size_t skill_acquisition_record_trials(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    const trial_result_t* results,
    size_t count);

/**
 * @brief Record step-level errors
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param step_index Step where error occurred
 * @param error_count Number of errors to add
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_record_step_error(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    size_t step_index,
    size_t error_count);

//=============================================================================
// Performance Prediction Functions
//=============================================================================

/**
 * @brief Predict performance at future trial
 *
 * WHAT: Estimate performance at trial N using power law
 * WHY:  Project learning trajectory
 * HOW:  P(n) = a * n^(-b) + c with confidence bounds
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param trial_number Target trial number
 * @param prediction Output prediction result
 * @return SKILL_SUCCESS or error code
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT skill_error_t skill_acquisition_predict_performance(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    size_t trial_number,
    performance_prediction_t* prediction);

/**
 * @brief Estimate time to reach target performance
 *
 * WHAT: Calculate trials/time to reach mastery level
 * WHY:  Set realistic learning goals
 * HOW:  Solve P(n) = target for n
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param target_performance Target performance level
 * @param estimate Output mastery estimate
 * @return SKILL_SUCCESS or error code
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT skill_error_t skill_acquisition_estimate_time_to_mastery(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    float target_performance,
    mastery_estimate_t* estimate);

/**
 * @brief Get current performance level
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @return Current performance, or NaN on error
 */
NIMCP_EXPORT float skill_acquisition_get_current_performance(
    skill_acquisition_t* sa,
    uint64_t skill_id);

//=============================================================================
// Transfer Computation Functions
//=============================================================================

/**
 * @brief Compute transfer between two skills
 *
 * WHAT: Calculate transfer magnitude and type between skills
 * WHY:  Predict how prior skill affects new skill learning
 * HOW:  Analyze element overlap, signature similarity, structure
 *
 * @param sa System handle
 * @param source_skill_id Prior skill
 * @param target_skill_id New skill
 * @param transfer Output transfer relationship
 * @return SKILL_SUCCESS or error code
 *
 * Transfer is computed as weighted combination of:
 * - Element overlap: Shared steps/operations
 * - Surface similarity: Similar appearance/context
 * - Structural similarity: Similar underlying organization
 * - Prime signature: Content-based similarity
 *
 * Performance: ~500ns
 */
NIMCP_EXPORT skill_error_t skill_acquisition_compute_transfer(
    skill_acquisition_t* sa,
    uint64_t source_skill_id,
    uint64_t target_skill_id,
    skill_transfer_t* transfer);

/**
 * @brief Apply transfer effect to skill learning
 *
 * WHAT: Adjust learning parameters based on transfer
 * WHY:  Model how prior skills affect new learning
 * HOW:  Modifies power law parameters based on transfer
 *
 * @param sa System handle
 * @param skill_id Skill being learned
 * @param transfer Transfer relationship to apply
 * @return SKILL_SUCCESS or error code
 *
 * Effects:
 * - Positive transfer: Lower initial_performance (better start)
 * - Negative transfer: Higher initial_performance + slower rate
 * - Also affects asymptote
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT skill_error_t skill_acquisition_apply_transfer(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    const skill_transfer_t* transfer);

/**
 * @brief Find most similar skill for transfer
 *
 * @param sa System handle
 * @param skill_id Target skill
 * @param source_skill_id Output: most similar source skill
 * @param similarity Output: similarity score
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_find_best_transfer(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    uint64_t* source_skill_id,
    float* similarity);

/**
 * @brief Get transfer matrix value
 *
 * @param sa System handle
 * @param source_skill_id Source skill
 * @param target_skill_id Target skill
 * @return Transfer magnitude (-1 to +1), or NaN if not computed
 */
NIMCP_EXPORT float skill_acquisition_get_transfer(
    skill_acquisition_t* sa,
    uint64_t source_skill_id,
    uint64_t target_skill_id);

//=============================================================================
// Plateau Detection Functions
//=============================================================================

/**
 * @brief Detect if currently on learning plateau
 *
 * WHAT: Check if performance has stalled
 * WHY:  Identify when to change practice strategy
 * HOW:  Analyze performance variance over window
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @return true if on plateau, false otherwise
 *
 * Detection criteria:
 * - Moving variance below threshold
 * - No improvement trend over window
 * - Minimum trials met
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT bool skill_acquisition_detect_plateau(
    skill_acquisition_t* sa,
    uint64_t skill_id);

/**
 * @brief Get current plateau if any
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @return Current plateau, or NULL if not on plateau
 */
NIMCP_EXPORT learning_plateau_t* skill_acquisition_get_current_plateau(
    skill_acquisition_t* sa,
    uint64_t skill_id);

/**
 * @brief Suggest breakthrough strategy
 *
 * WHAT: Recommend strategy to overcome plateau
 * WHY:  Help learner break through stalled progress
 * HOW:  Analyze plateau characteristics and weak points
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param suggestion Output suggestion
 * @return SKILL_SUCCESS or error code
 *
 * Considers:
 * - Plateau duration
 * - Weak point distribution
 * - Previous successful strategies
 * - Practice quality level
 *
 * Performance: ~10us
 */
NIMCP_EXPORT skill_error_t skill_acquisition_suggest_breakthrough(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    breakthrough_suggestion_t* suggestion);

/**
 * @brief Mark plateau as overcome
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param strategy Strategy that worked
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_overcome_plateau(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    plateau_strategy_t strategy);

/**
 * @brief Get plateau history
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param plateaus Output array (caller-allocated)
 * @param max_plateaus Array capacity
 * @param count Output: actual count
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_get_plateau_history(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    learning_plateau_t* plateaus,
    size_t max_plateaus,
    size_t* count);

//=============================================================================
// Weak Point Analysis Functions
//=============================================================================

/**
 * @brief Analyze weak points in skill
 *
 * WHAT: Identify steps with highest error rates
 * WHY:  Target practice on problem areas
 * HOW:  Compare error rates across steps
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param analysis Output analysis result
 * @return SKILL_SUCCESS or error code
 *
 * Performance: ~10us
 */
NIMCP_EXPORT skill_error_t skill_acquisition_analyze_weak_points(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    weak_point_analysis_t* analysis);

/**
 * @brief Get step difficulty
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param step_index Step to query
 * @return Difficulty (0-1), or NaN on error
 */
NIMCP_EXPORT float skill_acquisition_get_step_difficulty(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    size_t step_index);

/**
 * @brief Get most difficult step
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param difficulty Output: difficulty value
 * @return Step index, or SIZE_MAX on error
 */
NIMCP_EXPORT size_t skill_acquisition_get_most_difficult_step(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    float* difficulty);

/**
 * @brief Free weak point analysis resources
 *
 * @param analysis Analysis to free (NULL safe)
 */
NIMCP_EXPORT void skill_acquisition_free_weak_point_analysis(
    weak_point_analysis_t* analysis);

//=============================================================================
// Deliberate Practice Functions
//=============================================================================

/**
 * @brief Plan deliberate practice session
 *
 * WHAT: Generate focused practice plan
 * WHY:  Maximize learning efficiency
 * HOW:  Target weak points with appropriate intensity
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param plan Output practice plan
 * @return SKILL_SUCCESS or error code
 *
 * Plan includes:
 * - Steps to focus on (weak points)
 * - Time allocation per step
 * - Recommended feedback frequency
 * - Target practice quality
 *
 * Performance: ~50us
 */
NIMCP_EXPORT skill_error_t skill_acquisition_plan_deliberate_practice(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    deliberate_practice_plan_t* plan);

/**
 * @brief Set practice quality level
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param quality New quality level
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_set_practice_quality(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    practice_quality_t quality);

/**
 * @brief Set feedback frequency
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param frequency Frequency (0-1, where 1 = feedback every trial)
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_set_feedback_frequency(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    float frequency);

/**
 * @brief Free deliberate practice plan resources
 *
 * @param plan Plan to free (NULL safe)
 */
NIMCP_EXPORT void skill_acquisition_free_practice_plan(
    deliberate_practice_plan_t* plan);

//=============================================================================
// Power Law Fitting Functions
//=============================================================================

/**
 * @brief Fit power law to performance data
 *
 * WHAT: Estimate power law parameters from data
 * WHY:  Enable accurate performance prediction
 * HOW:  Iterative least-squares fitting
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @return SKILL_SUCCESS or error code
 *
 * Updates:
 * - initial_performance (a)
 * - learning_rate (b)
 * - asymptote (c)
 * - fit_r_squared
 *
 * Performance: ~100us (iterative fitting)
 */
NIMCP_EXPORT skill_error_t skill_acquisition_fit_power_law(
    skill_acquisition_t* sa,
    uint64_t skill_id);

/**
 * @brief Get power law parameters
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param state Output power law state
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_get_power_law(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    power_law_state_t* state);

/**
 * @brief Set power law parameters manually
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param initial_performance a coefficient
 * @param learning_rate b exponent
 * @param asymptote c constant
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_set_power_law(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    float initial_performance,
    float learning_rate,
    float asymptote);

//=============================================================================
// Learning Curve Functions
//=============================================================================

/**
 * @brief Get learning curve data
 *
 * WHAT: Retrieve performance history with predictions
 * WHY:  Visualization and analysis
 * HOW:  Combine actual data with power law predictions
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param curve Output learning curve data
 * @return SKILL_SUCCESS or error code
 *
 * Performance: ~100us
 */
NIMCP_EXPORT skill_error_t skill_acquisition_get_learning_curve(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    learning_curve_t* curve);

/**
 * @brief Free learning curve resources
 *
 * @param curve Curve to free (NULL safe)
 */
NIMCP_EXPORT void skill_acquisition_free_learning_curve(
    learning_curve_t* curve);

/**
 * @brief Get performance history
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param performances Output array (caller-allocated)
 * @param max_count Array capacity
 * @param count Output: actual count
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_get_performance_history(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    float* performances,
    size_t max_count,
    size_t* count);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get skill acquisition statistics
 *
 * @param sa System handle
 * @param total_skills Output: total skills registered
 * @param total_trials Output: total trials recorded
 * @param plateaus_detected Output: total plateaus detected
 * @param avg_learning_rate Output: average learning rate across skills
 * @return SKILL_SUCCESS or error code
 */
NIMCP_EXPORT skill_error_t skill_acquisition_get_statistics(
    skill_acquisition_t* sa,
    size_t* total_skills,
    uint64_t* total_trials,
    uint64_t* plateaus_detected,
    float* avg_learning_rate);

/**
 * @brief Get improvement rate
 *
 * @param sa System handle
 * @param skill_id Skill ID
 * @param window Number of recent trials to consider
 * @return Improvement rate (positive = improving), or NaN on error
 */
NIMCP_EXPORT float skill_acquisition_get_improvement_rate(
    skill_acquisition_t* sa,
    uint64_t skill_id,
    size_t window);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get transfer type name as string
 *
 * @param type Transfer type
 * @return Human-readable string
 */
NIMCP_EXPORT const char* skill_transfer_type_name(transfer_type_t type);

/**
 * @brief Get plateau strategy name as string
 *
 * @param strategy Plateau strategy
 * @return Human-readable string
 */
NIMCP_EXPORT const char* skill_plateau_strategy_name(plateau_strategy_t strategy);

/**
 * @brief Get practice quality name as string
 *
 * @param quality Practice quality
 * @return Human-readable string
 */
NIMCP_EXPORT const char* skill_practice_quality_name(practice_quality_t quality);

/**
 * @brief Get error string
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* skill_error_string(skill_error_t error);

/**
 * @brief Print skill acquisition state to stdout
 *
 * @param state Acquisition state to print
 */
NIMCP_EXPORT void skill_acquisition_print_state(
    const skill_acquisition_state_t* state);

/**
 * @brief Print power law parameters to stdout
 *
 * @param state Power law state to print
 */
NIMCP_EXPORT void skill_acquisition_print_power_law(
    const power_law_state_t* state);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t skill_current_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SKILL_ACQUISITION_H */
