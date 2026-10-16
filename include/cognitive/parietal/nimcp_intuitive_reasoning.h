/**
 * @file nimcp_intuitive_reasoning.h
 * @brief Intuitive reasoning engine for parietal lobe
 *
 * WHAT: Core engine for intuitive, heuristic-based reasoning
 * WHY:  Enable formation of NEW knowledge through educated guesses and hunches
 * HOW:  Plausibility estimation, confidence gradients, intuitive leaps
 *
 * BIOLOGICAL BASIS:
 * The parietal cortex integrates sensory information to form rapid, intuitive
 * judgments before formal analytical processing. This mirrors "gut feelings"
 * and expert intuition that emerges from pattern recognition.
 *
 * CAPABILITIES:
 * - Hunches: Form preliminary hypotheses from incomplete data
 * - Plausibility: Rate how "reasonable" an idea feels before proof
 * - Confidence gradients: Track certainty levels across reasoning chains
 * - Intuitive leaps: Skip intermediate steps when patterns are recognized
 * - Gestalt perception: See the whole before analyzing parts
 *
 * USAGE:
 * ```c
 * intuitive_engine_t* engine = intuitive_engine_create();
 *
 * // Form a hunch from observations
 * hunch_t* hunch = intuitive_form_hunch(engine, observations, num_obs);
 *
 * // Estimate plausibility
 * float plausibility = intuitive_estimate_plausibility(engine, hypothesis);
 *
 * // Attempt an intuitive leap
 * insight_t* insight = intuitive_leap(engine, problem);
 *
 * intuitive_engine_destroy(engine);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_INTUITIVE_REASONING_H
#define NIMCP_INTUITIVE_REASONING_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum observation dimensions */
#define INTUITIVE_MAX_OBSERVATION_DIM       256

/** Maximum number of observations for hunch formation */
#define INTUITIVE_MAX_OBSERVATIONS          128

/** Maximum hypothesis description length */
#define INTUITIVE_MAX_DESCRIPTION           512

/** Maximum reasoning chain length */
#define INTUITIVE_MAX_CHAIN_LENGTH          64

/** Maximum features for pattern matching */
#define INTUITIVE_MAX_FEATURES              128

/** Bio-async module ID for intuitive reasoning */
#define BIO_MODULE_INTUITIVE_REASONING      0x03A0

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

/** Opaque handle for intuitive reasoning engine */
typedef struct intuitive_engine intuitive_engine_t;

/* ============================================================================
 * CORE TYPES
 * ============================================================================ */

/**
 * @brief Intuition quality scores
 *
 * Multi-dimensional assessment of an intuitive judgment.
 */
typedef struct {
    float plausibility;     /**< How reasonable does this feel? [0,1] */
    float novelty;          /**< How new/unexpected is this? [0,1] */
    float coherence;        /**< How well does it fit existing knowledge? [0,1] */
    float fertility;        /**< How many new ideas does it generate? [0,1] */
    float confidence;       /**< Overall confidence in intuition [0,1] */
    float urgency;          /**< How time-sensitive is acting on this? [0,1] */
} intuition_score_t;

/**
 * @brief Observation data for intuitive processing
 */
typedef struct {
    float* data;                    /**< Raw observation data */
    uint32_t dim;                   /**< Dimensionality */
    float salience;                 /**< How attention-grabbing [0,1] */
    float reliability;              /**< Source reliability [0,1] */
    uint64_t timestamp;             /**< When observed */
    char source[64];                /**< Observation source identifier */
} observation_t;

/**
 * @brief A hunch - preliminary hypothesis from incomplete data
 */
typedef struct {
    uint32_t id;                    /**< Unique hunch identifier */
    char description[INTUITIVE_MAX_DESCRIPTION];  /**< What the hunch is about */

    intuition_score_t score;        /**< Quality assessment */

    float* predicted_pattern;       /**< Predicted underlying pattern */
    uint32_t pattern_dim;           /**< Pattern dimensionality */

    uint32_t* supporting_obs;       /**< Indices of supporting observations */
    uint32_t num_supporting;        /**< Count of supporting observations */

    uint32_t* conflicting_obs;      /**< Indices of conflicting observations */
    uint32_t num_conflicting;       /**< Count of conflicting observations */

    float prior_probability;        /**< Prior belief before observations */
    float posterior_probability;    /**< Updated belief after observations */

    bool is_actionable;             /**< Can we act on this hunch? */
    bool needs_verification;        /**< Should this be formally verified? */

    uint64_t formation_time;        /**< When hunch was formed */
} hunch_t;

/**
 * @brief Hypothesis for formal evaluation
 */
#ifndef NIMCP_HYPOTHESIS_T_DEFINED
#define NIMCP_HYPOTHESIS_T_DEFINED
typedef struct {
    uint32_t id;                    /**< Unique hypothesis identifier */
    char statement[INTUITIVE_MAX_DESCRIPTION];  /**< Formal statement */

    float* parameters;              /**< Hypothesis parameters */
    uint32_t num_params;            /**< Number of parameters */

    float explanatory_power;        /**< How much does it explain? [0,1] */
    float parsimony;                /**< How simple is it? [0,1] */
    float falsifiability;           /**< Can it be tested? [0,1] */
    float prior;                    /**< Prior probability */
    float likelihood;               /**< P(data|hypothesis) */
    float posterior;                /**< P(hypothesis|data) */

    bool is_null;                   /**< Is this the null hypothesis? */
} hypothesis_t;
#endif

/**
 * @brief Problem definition for intuitive solving
 */
typedef struct {
    uint32_t id;                    /**< Unique problem identifier */
    char description[INTUITIVE_MAX_DESCRIPTION];  /**< Problem description */

    float* initial_state;           /**< Starting state */
    float* goal_state;              /**< Target state (if known) */
    uint32_t state_dim;             /**< State dimensionality */

    float* constraints;             /**< Known constraints */
    uint32_t num_constraints;       /**< Number of constraints */

    bool goal_known;                /**< Is the goal explicitly known? */
    float estimated_difficulty;     /**< Estimated difficulty [0,1] */
    uint32_t time_pressure;         /**< Time available (0 = unlimited) */

    void* domain_context;           /**< Domain-specific context */
} problem_t;

/**
 * @brief An insight - breakthrough understanding
 */
typedef struct {
    uint32_t id;                    /**< Unique insight identifier */
    char description[INTUITIVE_MAX_DESCRIPTION];  /**< What was realized */

    problem_t* original_problem;    /**< The problem that was solved */

    float* solution;                /**< The insight solution */
    uint32_t solution_dim;          /**< Solution dimensionality */

    float surprise_factor;          /**< How unexpected was this? [0,1] */
    float elegance;                 /**< How elegant is the solution? [0,1] */
    float generalizability;         /**< Can it apply elsewhere? [0,1] */

    char* key_realization;          /**< The crucial "aha" moment */

    uint32_t steps_skipped;         /**< Intermediate steps bypassed */
    float time_saved_estimate;      /**< Estimated time saved vs analytical */

    bool verified;                  /**< Has insight been verified? */
    float verification_confidence;  /**< Confidence in verification */
} insight_t;

/**
 * @brief Confidence gradient along reasoning chain
 */
typedef struct {
    float* confidence_values;       /**< Confidence at each step */
    uint32_t num_steps;             /**< Number of reasoning steps */
    float decay_rate;               /**< How fast confidence decays */
    float min_confidence;           /**< Minimum confidence in chain */
    float max_confidence;           /**< Maximum confidence in chain */
    uint32_t weakest_link;          /**< Index of lowest confidence step */
} confidence_gradient_t;

/**
 * @brief Gestalt perception result
 */
typedef struct {
    char pattern_type[64];          /**< Type of gestalt pattern seen */
    float* whole_representation;    /**< Representation of the whole */
    uint32_t repr_dim;              /**< Representation dimensionality */
    float closure_strength;         /**< How complete is the pattern? [0,1] */
    float figure_ground_separation; /**< Figure-ground clarity [0,1] */
    float grouping_strength;        /**< How strongly elements group [0,1] */
    uint32_t num_parts;             /**< Number of parts identified */
} gestalt_result_t;

/**
 * @brief Pattern similarity for intuitive matching
 */
typedef struct {
    float structural_similarity;    /**< Structural match [0,1] */
    float functional_similarity;    /**< Functional match [0,1] */
    float surface_similarity;       /**< Surface feature match [0,1] */
    float overall_similarity;       /**< Weighted overall [0,1] */
    char* matched_pattern_name;     /**< Name of matched pattern */
    uint32_t matched_pattern_id;    /**< ID of matched pattern */
} intuitive_pattern_match_t;

/**
 * @brief Intuitive reasoning strategy
 */
typedef enum {
    INTUITIVE_STRATEGY_RECOGNITION,     /**< Pattern recognition */
    INTUITIVE_STRATEGY_SIMULATION,      /**< Mental simulation */
    INTUITIVE_STRATEGY_ANALOGY,         /**< Analogical transfer */
    INTUITIVE_STRATEGY_HEURISTIC,       /**< Heuristic rules */
    INTUITIVE_STRATEGY_GESTALT,         /**< Holistic perception */
    INTUITIVE_STRATEGY_EMOTIONAL,       /**< Emotional/somatic markers */
    INTUITIVE_STRATEGY_ASSOCIATIVE,     /**< Free association */
    INTUITIVE_STRATEGY_INCUBATION       /**< Background processing */
} intuitive_strategy_t;

/**
 * @brief Engine configuration
 */
typedef struct {
    float plausibility_threshold;   /**< Min plausibility for hunches [0,1] */
    float confidence_threshold;     /**< Min confidence for insights [0,1] */
    float novelty_weight;           /**< Weight for novelty in scoring */
    float coherence_weight;         /**< Weight for coherence in scoring */
    float fertility_weight;         /**< Weight for fertility in scoring */

    bool enable_gestalt;            /**< Enable gestalt perception */
    bool enable_incubation;         /**< Enable background processing */
    bool enable_emotional_markers;  /**< Enable somatic marker integration */

    float prior_strength;           /**< Strength of prior beliefs */
    float learning_rate;            /**< How fast to update beliefs */

    float inflammation_sensitivity; /**< Response to inflammation */
    float fatigue_sensitivity;      /**< Response to fatigue */

    uint32_t max_incubation_problems; /**< Max problems in incubation */
    uint32_t pattern_memory_size;   /**< Size of pattern memory */
} intuitive_config_t;

/**
 * @brief Engine statistics
 */
typedef struct {
    uint64_t hunches_formed;        /**< Total hunches generated */
    uint64_t hunches_verified;      /**< Hunches that were verified */
    uint64_t hunches_rejected;      /**< Hunches that were rejected */
    uint64_t insights_generated;    /**< Total insights produced */
    uint64_t intuitive_leaps;       /**< Successful intuitive leaps */
    uint64_t patterns_matched;      /**< Pattern matches made */
    uint64_t gestalt_perceptions;   /**< Gestalt perceptions performed */

    float avg_plausibility;         /**< Average hunch plausibility */
    float avg_confidence;           /**< Average insight confidence */
    float hunch_accuracy;           /**< Fraction of verified hunches correct */
    float avg_processing_time_us;   /**< Average processing time */
} intuitive_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create intuitive reasoning engine with default config
 * @return Engine handle or NULL on failure
 */
intuitive_engine_t* intuitive_engine_create(void);

/**
 * @brief Create engine with custom configuration
 * @param config Configuration parameters
 * @return Engine handle or NULL on failure
 */
intuitive_engine_t* intuitive_engine_create_custom(const intuitive_config_t* config);

/**
 * @brief Destroy engine and free resources
 * @param engine Engine to destroy
 */
void intuitive_engine_destroy(intuitive_engine_t* engine);

/**
 * @brief Get default configuration
 * @return Default config structure
 */
intuitive_config_t intuitive_engine_default_config(void);

/* ============================================================================
 * HUNCH FORMATION API
 * ============================================================================ */

/**
 * @brief Form a hunch from observations
 *
 * Analyzes incomplete data to form a preliminary hypothesis.
 *
 * @param engine Engine handle
 * @param observations Array of observations
 * @param num_observations Number of observations
 * @return Formed hunch (caller must free) or NULL
 */
hunch_t* intuitive_form_hunch(
    intuitive_engine_t* engine,
    const observation_t* observations,
    uint32_t num_observations
);

/**
 * @brief Form hunch from raw numerical data
 *
 * Convenience function for simple numerical patterns.
 *
 * @param engine Engine handle
 * @param data Raw data array
 * @param length Data length
 * @return Formed hunch or NULL
 */
hunch_t* intuitive_form_hunch_from_data(
    intuitive_engine_t* engine,
    const float* data,
    uint32_t length
);

/**
 * @brief Update existing hunch with new observation
 *
 * @param engine Engine handle
 * @param hunch Hunch to update
 * @param observation New observation
 * @return 0 on success, -1 on error
 */
int intuitive_update_hunch(
    intuitive_engine_t* engine,
    hunch_t* hunch,
    const observation_t* observation
);

/**
 * @brief Refine hunch with additional context
 *
 * @param engine Engine handle
 * @param hunch Hunch to refine
 * @param context Additional context data
 * @param context_dim Context dimensionality
 * @return 0 on success, -1 on error
 */
int intuitive_refine_hunch(
    intuitive_engine_t* engine,
    hunch_t* hunch,
    const float* context,
    uint32_t context_dim
);

/**
 * @brief Free hunch resources
 * @param hunch Hunch to free
 */
void intuitive_free_hunch(hunch_t* hunch);

/* ============================================================================
 * PLAUSIBILITY ESTIMATION API
 * ============================================================================ */

/**
 * @brief Estimate plausibility of a hypothesis
 *
 * Rates how "reasonable" an idea feels before formal proof.
 *
 * @param engine Engine handle
 * @param hypothesis Hypothesis to evaluate
 * @return Plausibility score [0,1]
 */
float intuitive_estimate_plausibility(
    intuitive_engine_t* engine,
    const hypothesis_t* hypothesis
);

/**
 * @brief Get full intuition score for hypothesis
 *
 * @param engine Engine handle
 * @param hypothesis Hypothesis to evaluate
 * @param score Output score structure
 * @return 0 on success, -1 on error
 */
int intuitive_score_hypothesis(
    intuitive_engine_t* engine,
    const hypothesis_t* hypothesis,
    intuition_score_t* score
);

/**
 * @brief Compare plausibility of multiple hypotheses
 *
 * @param engine Engine handle
 * @param hypotheses Array of hypotheses
 * @param num_hypotheses Number of hypotheses
 * @param rankings Output array of indices (most plausible first)
 * @return 0 on success, -1 on error
 */
int intuitive_rank_hypotheses(
    intuitive_engine_t* engine,
    const hypothesis_t* hypotheses,
    uint32_t num_hypotheses,
    uint32_t* rankings
);

/**
 * @brief Estimate plausibility from raw statement
 *
 * @param engine Engine handle
 * @param statement Hypothesis statement
 * @param domain_features Domain-specific features
 * @param num_features Number of features
 * @return Plausibility score [0,1]
 */
float intuitive_estimate_statement_plausibility(
    intuitive_engine_t* engine,
    const char* statement,
    const float* domain_features,
    uint32_t num_features
);

/* ============================================================================
 * CONFIDENCE GRADIENT API
 * ============================================================================ */

/**
 * @brief Track confidence through reasoning chain
 *
 * @param engine Engine handle
 * @param step_confidences Confidence at each step
 * @param num_steps Number of steps
 * @param gradient Output gradient structure
 * @return 0 on success, -1 on error
 */
int intuitive_track_confidence(
    intuitive_engine_t* engine,
    const float* step_confidences,
    uint32_t num_steps,
    confidence_gradient_t* gradient
);

/**
 * @brief Propagate confidence through reasoning
 *
 * Updates confidence as reasoning proceeds.
 *
 * @param engine Engine handle
 * @param current_confidence Current confidence level
 * @param step_reliability Reliability of current step
 * @return Updated confidence
 */
float intuitive_propagate_confidence(
    intuitive_engine_t* engine,
    float current_confidence,
    float step_reliability
);

/**
 * @brief Identify weak points in reasoning chain
 *
 * @param engine Engine handle
 * @param gradient Confidence gradient
 * @param weak_indices Output array of weak step indices
 * @param max_weak Maximum weak points to find
 * @param num_found Output number found
 * @return 0 on success, -1 on error
 */
int intuitive_find_weak_links(
    intuitive_engine_t* engine,
    const confidence_gradient_t* gradient,
    uint32_t* weak_indices,
    uint32_t max_weak,
    uint32_t* num_found
);

/**
 * @brief Free confidence gradient resources
 * @param gradient Gradient to free
 */
void intuitive_free_gradient(confidence_gradient_t* gradient);

/* ============================================================================
 * INTUITIVE LEAP API
 * ============================================================================ */

/**
 * @brief Attempt an intuitive leap to solve problem
 *
 * Tries to skip intermediate reasoning steps when pattern is recognized.
 *
 * @param engine Engine handle
 * @param problem Problem to solve
 * @return Insight if successful, NULL otherwise
 */
insight_t* intuitive_leap(
    intuitive_engine_t* engine,
    const problem_t* problem
);

/**
 * @brief Attempt leap with specific strategy
 *
 * @param engine Engine handle
 * @param problem Problem to solve
 * @param strategy Strategy to use
 * @return Insight if successful, NULL otherwise
 */
insight_t* intuitive_leap_with_strategy(
    intuitive_engine_t* engine,
    const problem_t* problem,
    intuitive_strategy_t strategy
);

/**
 * @brief Check if intuitive leap is possible
 *
 * Estimates whether a leap might succeed without fully attempting it.
 *
 * @param engine Engine handle
 * @param problem Problem to evaluate
 * @return Probability of successful leap [0,1]
 */
float intuitive_can_leap(
    intuitive_engine_t* engine,
    const problem_t* problem
);

/**
 * @brief Free insight resources
 * @param insight Insight to free
 */
void intuitive_free_insight(insight_t* insight);

/* ============================================================================
 * GESTALT PERCEPTION API
 * ============================================================================ */

/**
 * @brief Perform gestalt perception on data
 *
 * See the whole pattern before analyzing parts.
 *
 * @param engine Engine handle
 * @param data Input data
 * @param dim Data dimensionality
 * @param result Output gestalt result
 * @return 0 on success, -1 on error
 */
int intuitive_gestalt_perceive(
    intuitive_engine_t* engine,
    const float* data,
    uint32_t dim,
    gestalt_result_t* result
);

/**
 * @brief Apply gestalt grouping principles
 *
 * @param engine Engine handle
 * @param elements Array of elements to group
 * @param num_elements Number of elements
 * @param element_dim Element dimensionality
 * @param group_assignments Output group for each element
 * @param num_groups Output number of groups formed
 * @return 0 on success, -1 on error
 */
int intuitive_gestalt_group(
    intuitive_engine_t* engine,
    const float* elements,
    uint32_t num_elements,
    uint32_t element_dim,
    uint32_t* group_assignments,
    uint32_t* num_groups
);

/**
 * @brief Free gestalt result resources
 * @param result Result to free
 */
void intuitive_free_gestalt(gestalt_result_t* result);

/* ============================================================================
 * PATTERN MATCHING API
 * ============================================================================ */

/**
 * @brief Find matching patterns intuitively
 *
 * @param engine Engine handle
 * @param input Input pattern to match
 * @param input_dim Input dimensionality
 * @param matches Output array of matches
 * @param max_matches Maximum matches to return
 * @param num_found Output number found
 * @return 0 on success, -1 on error
 */
int intuitive_match_patterns(
    intuitive_engine_t* engine,
    const float* input,
    uint32_t input_dim,
    intuitive_pattern_match_t* matches,
    uint32_t max_matches,
    uint32_t* num_found
);

/**
 * @brief Register pattern in memory
 *
 * @param engine Engine handle
 * @param pattern Pattern to register
 * @param pattern_dim Pattern dimensionality
 * @param name Pattern name
 * @return Pattern ID or 0 on failure
 */
uint32_t intuitive_register_pattern(
    intuitive_engine_t* engine,
    const float* pattern,
    uint32_t pattern_dim,
    const char* name
);

/**
 * @brief Forget pattern from memory
 *
 * @param engine Engine handle
 * @param pattern_id Pattern to forget
 * @return 0 on success, -1 on error
 */
int intuitive_forget_pattern(
    intuitive_engine_t* engine,
    uint32_t pattern_id
);

/* ============================================================================
 * INCUBATION API
 * ============================================================================ */

/**
 * @brief Submit problem for background incubation
 *
 * @param engine Engine handle
 * @param problem Problem to incubate
 * @return Problem ID for later retrieval, or 0 on failure
 */
uint32_t intuitive_incubate(
    intuitive_engine_t* engine,
    const problem_t* problem
);

/**
 * @brief Check incubation progress
 *
 * @param engine Engine handle
 * @param problem_id Problem to check
 * @param insight Output insight if ready
 * @return 1 if insight ready, 0 if still incubating, -1 on error
 */
int intuitive_check_incubation(
    intuitive_engine_t* engine,
    uint32_t problem_id,
    insight_t** insight
);

/**
 * @brief Process one step of all incubating problems
 *
 * Should be called periodically to advance incubation.
 *
 * @param engine Engine handle
 * @return Number of insights generated
 */
int intuitive_process_incubation(intuitive_engine_t* engine);

/**
 * @brief Cancel incubating problem
 *
 * @param engine Engine handle
 * @param problem_id Problem to cancel
 * @return 0 on success, -1 on error
 */
int intuitive_cancel_incubation(
    intuitive_engine_t* engine,
    uint32_t problem_id
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level
 *
 * Higher inflammation increases caution, reduces risky hunches.
 *
 * @param engine Engine handle
 * @param level Inflammation level [0,1]
 * @return 0 on success, -1 on error
 */
int intuitive_set_inflammation(intuitive_engine_t* engine, float level);

/**
 * @brief Set fatigue level
 *
 * Higher fatigue reduces insight quality and incubation speed.
 *
 * @param engine Engine handle
 * @param level Fatigue level [0,1]
 * @return 0 on success, -1 on error
 */
int intuitive_set_fatigue(intuitive_engine_t* engine, float level);

/**
 * @brief Set emotional valence
 *
 * Positive emotion broadens intuitive search; negative narrows it.
 *
 * @param engine Engine handle
 * @param valence Emotional valence [-1,1]
 * @return 0 on success, -1 on error
 */
int intuitive_set_emotional_valence(intuitive_engine_t* engine, float valence);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get engine statistics
 *
 * @param engine Engine handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int intuitive_get_stats(const intuitive_engine_t* engine, intuitive_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param engine Engine handle
 */
void intuitive_reset_stats(intuitive_engine_t* engine);

/**
 * @brief Get last error message
 *
 * @return Error message string
 */
const char* intuitive_get_last_error(void);

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Create observation from raw data
 *
 * @param data Raw data
 * @param dim Dimensionality
 * @param salience Salience level
 * @return Observation structure
 */
observation_t intuitive_create_observation(
    const float* data,
    uint32_t dim,
    float salience
);

/**
 * @brief Create problem from initial and goal states
 *
 * @param initial Initial state
 * @param goal Goal state (can be NULL)
 * @param dim State dimensionality
 * @param description Problem description
 * @return Problem structure
 */
problem_t intuitive_create_problem(
    const float* initial,
    const float* goal,
    uint32_t dim,
    const char* description
);

/**
 * @brief Create hypothesis from statement
 *
 * @param statement Hypothesis statement
 * @param parameters Parameters (can be NULL)
 * @param num_params Number of parameters
 * @return Hypothesis structure
 */
hypothesis_t intuitive_create_hypothesis(
    const char* statement,
    const float* parameters,
    uint32_t num_params
);

/**
 * @brief Get strategy name
 *
 * @param strategy Strategy enum
 * @return Strategy name string
 */
const char* intuitive_strategy_name(intuitive_strategy_t strategy);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTUITIVE_REASONING_H */
