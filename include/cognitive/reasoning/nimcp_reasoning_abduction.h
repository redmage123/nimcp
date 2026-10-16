/**
 * @file nimcp_reasoning_abduction.h
 * @brief Abductive Reasoning — Inference to Best Explanation
 *
 * WHAT: Generate and evaluate explanatory hypotheses for observed phenomena
 * WHY:  Forward chaining derives consequences, backward chaining proves goals,
 *       but abduction generates EXPLANATIONS — the most common form of human
 *       reasoning. "Why did X happen? What best explains these observations?"
 * HOW:  Collect observations, generate candidate hypotheses via keyword extraction
 *       and pattern matching, score by plausibility (simplicity + explanatory
 *       power + coherence), rank by weighted score.
 *
 * BIOLOGICAL BASIS:
 * Models the anterior prefrontal cortex (aPFC) and temporoparietal junction (TPJ)
 * which generate causal explanations for observed events. Abductive reasoning is
 * the brain's primary mode of everyday inference — perceiving causes behind effects.
 *
 * FEP INTEGRATION:
 * Abduction is naturally connected to the Free Energy Principle:
 * - Hypotheses that minimize surprise (free energy) are preferred
 * - free_energy = -log(plausibility) provides a direct mapping
 * - Good explanations reduce prediction error in the generative model
 *
 * DESIGN PRINCIPLES:
 * - Thread-safe: mutex-protected internal state
 * - Graceful degradation: works without KB (uses observation-based heuristics)
 * - Configurable: weights for simplicity/explanatory_power/coherence
 * - FEP-compatible: computes free_energy for each hypothesis
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#ifndef NIMCP_REASONING_ABDUCTION_H
#define NIMCP_REASONING_ABDUCTION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum hypotheses that can be generated per abduction query */
#define ABDUCTION_MAX_HYPOTHESES 16

/** Maximum observations that can be accumulated before generation */
#define ABDUCTION_MAX_OBSERVATIONS 32

/** Maximum length of an explanation or observation description string */
#define ABDUCTION_MAX_EXPLANATION_LEN 256

/** Default minimum plausibility to retain a hypothesis */
#define ABDUCTION_DEFAULT_MIN_PLAUSIBILITY 0.1f

/** Default maximum hypotheses to return */
#define ABDUCTION_DEFAULT_MAX_HYPOTHESES 8

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief A single observed phenomenon to be explained
 *
 * WHAT: One discrete observation with metadata
 * WHY:  Abductive reasoning starts from observations that need explanation
 */
typedef struct {
    char description[ABDUCTION_MAX_EXPLANATION_LEN]; /**< Human-readable description */
    float confidence;                                 /**< Observation confidence [0-1] */
    uint32_t domain;                                  /**< Knowledge domain identifier */
    uint64_t timestamp_us;                            /**< When observation was recorded */
} abductive_observation_t;

/**
 * @brief A candidate explanatory hypothesis
 *
 * WHAT: One possible explanation for observed phenomena
 * WHY:  Abduction generates multiple hypotheses ranked by plausibility
 */
typedef struct {
    char explanation[ABDUCTION_MAX_EXPLANATION_LEN]; /**< Human-readable explanation */
    float plausibility;                               /**< Overall plausibility [0-1] */
    float simplicity;                                 /**< Occam's razor score [0-1] */
    float explanatory_power;                          /**< Fraction of observations explained [0-1] */
    float coherence;                                  /**< Internal consistency [0-1] */
    uint32_t observations_explained;                  /**< Count of observations this explains */
    uint32_t total_observations;                      /**< Total observations considered */
    float free_energy;                                /**< -log(plausibility + 1e-6) for FEP */
} abductive_hypothesis_t;

/**
 * @brief Result of an abduction query
 *
 * WHAT: Collection of generated hypotheses with best-selection metadata
 * WHY:  Caller receives ranked hypotheses and can inspect the best
 */
typedef struct {
    abductive_hypothesis_t hypotheses[ABDUCTION_MAX_HYPOTHESES]; /**< Generated hypotheses */
    uint32_t num_hypotheses;                                      /**< Number of hypotheses */
    uint32_t best_hypothesis_index;                               /**< Index of best hypothesis */
    float best_plausibility;                                      /**< Plausibility of best */
    uint64_t generation_time_us;                                  /**< Time to generate */
} abduction_result_t;

/**
 * @brief Configuration for the abductive reasoning module
 *
 * WHAT: Tuneable parameters for hypothesis generation and scoring
 * WHY:  Allow domain-specific tuning of abductive reasoning behavior
 */
typedef struct {
    bool enabled;               /**< Whether abductive reasoning is active */
    uint32_t max_hypotheses;    /**< Maximum hypotheses to generate */
    float min_plausibility;     /**< Minimum plausibility to retain */
    bool prefer_simplicity;     /**< Prefer simpler explanations (Occam) */
    float simplicity_weight;    /**< Weight for simplicity in score (default 0.3) */
    float explanatory_weight;   /**< Weight for explanatory power (default 0.5) */
    float coherence_weight;     /**< Weight for coherence (default 0.2) */
} abduction_config_t;

/**
 * @brief Aggregate statistics for the abduction module
 *
 * WHAT: Metrics across all abduction queries
 * WHY:  Monitor abductive reasoning performance and quality
 */
typedef struct {
    uint32_t total_abductions;              /**< Total abduction queries */
    float avg_hypotheses_generated;         /**< Running average hypotheses per query */
    float avg_best_plausibility;            /**< Running average of best plausibility */
    uint32_t total_observations_processed;  /**< Total observations across all queries */
} abduction_stats_t;

/**
 * @brief Opaque abductive reasoning engine instance
 */
typedef struct reasoning_abduction reasoning_abduction_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Get default abduction configuration
 *
 * WHAT: Return configuration with sensible defaults
 * WHY:  Simplify creation with proven parameters
 *
 * @return Default configuration struct
 */
abduction_config_t reasoning_abduction_default_config(void);

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create an abductive reasoning engine
 *
 * WHAT: Allocate and initialize an abduction instance
 * WHY:  Required before any abductive reasoning operations
 *
 * @param config Configuration (NULL for defaults)
 * @return Instance or NULL on allocation failure
 *
 * COMPLEXITY: O(1)
 */
reasoning_abduction_t* reasoning_abduction_create(const abduction_config_t* config);

/**
 * @brief Destroy an abductive reasoning engine
 *
 * WHAT: Free all resources
 * WHY:  Prevent memory leaks
 *
 * @param abduction Instance to destroy (NULL safe)
 */
void reasoning_abduction_destroy(reasoning_abduction_t* abduction);

/*=============================================================================
 * OBSERVATION MANAGEMENT
 *===========================================================================*/

/**
 * @brief Add an observation to be explained
 *
 * WHAT: Accumulate an observation for the next hypothesis generation
 * WHY:  Abduction requires observations before generating explanations
 *
 * @param abduction Abduction instance
 * @param observation Observation to add (deep copied)
 * @return 0 on success, -1 on error (NULL, full)
 */
int reasoning_abduction_add_observation(reasoning_abduction_t* abduction,
                                         const abductive_observation_t* observation);

/**
 * @brief Clear all accumulated observations
 *
 * WHAT: Reset observation buffer for a new abduction query
 * WHY:  Allow reuse of instance across multiple queries
 *
 * @param abduction Abduction instance
 * @return 0 on success, -1 on error
 */
int reasoning_abduction_clear_observations(reasoning_abduction_t* abduction);

/*=============================================================================
 * HYPOTHESIS GENERATION AND EVALUATION
 *===========================================================================*/

/**
 * @brief Generate hypotheses from accumulated observations
 *
 * WHAT: Run the abductive reasoning algorithm to produce explanations
 * WHY:  Core function — transforms observations into ranked hypotheses
 * HOW:  1. Extract keywords from observations
 *       2. Generate candidate hypotheses (direct cause, generalization, analogy)
 *       3. Score each hypothesis (simplicity, explanatory_power, coherence)
 *       4. Sort by plausibility descending
 *       5. Return top max_hypotheses
 *
 * @param abduction Abduction instance
 * @param result Output result struct (caller provides)
 * @return 0 on success, -1 on error
 */
int reasoning_abduction_generate(reasoning_abduction_t* abduction,
                                  abduction_result_t* result);

/**
 * @brief Evaluate (re-score) a hypothesis
 *
 * WHAT: Compute plausibility score for a hypothesis
 * WHY:  Allow re-evaluation after hypothesis modification
 *
 * @param abduction Abduction instance
 * @param hypothesis Hypothesis to evaluate (modified in place)
 * @return 0 on success, -1 on error
 */
int reasoning_abduction_evaluate(reasoning_abduction_t* abduction,
                                  abductive_hypothesis_t* hypothesis);

/**
 * @brief Select the best hypothesis from a result
 *
 * WHAT: Return pointer to the highest-plausibility hypothesis
 * WHY:  Convenience function for the most common use case
 *
 * @param result Abduction result
 * @return Pointer to best hypothesis, or NULL if no hypotheses
 */
const abductive_hypothesis_t* reasoning_abduction_select_best(
    const abduction_result_t* result);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get abduction statistics
 *
 * @param abduction Abduction instance
 * @param stats Output statistics struct
 * @return 0 on success, -1 on error
 */
int reasoning_abduction_get_stats(const reasoning_abduction_t* abduction,
                                   abduction_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_ABDUCTION_H */
