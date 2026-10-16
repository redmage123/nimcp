/**
 * @file nimcp_genius_modes.h
 * @brief Genius Mode Definitions for Mathematical Genius Module
 *
 * Defines the different "genius modes" that emulate the cognitive styles
 * of legendary mathematicians. Each mode has distinct strengths and
 * approaches to mathematical problem-solving.
 *
 * Modes:
 * - GAUSS: Number theory, pattern recognition, statistics
 * - NEWTON: Calculus, physics, differential equations
 * - ERDŐS: Combinatorics, graph theory, probabilistic method
 * - EULER: Universal connections, analysis, graph theory
 * - RAMANUJAN: Infinite series, partitions, intuition
 * - ADAPTIVE: Auto-select based on problem characteristics
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_GENIUS_MODES_H
#define NIMCP_GENIUS_MODES_H

#include <stdbool.h>
#include <stdint.h>
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Genius Mode Enumeration
 * ============================================================================ */

/**
 * @brief Mathematical genius modes
 *
 * Each mode represents a distinct cognitive style and mathematical approach.
 */
typedef enum {
    GENIUS_MODE_GAUSS = 0,           /**< Number theory, patterns, statistics */
    GENIUS_MODE_NEWTON,              /**< Calculus, physics, analysis */
    GENIUS_MODE_ERDOS,               /**< Combinatorics, graph theory, probabilistic */
    GENIUS_MODE_EULER,               /**< Universal connections, analysis */
    GENIUS_MODE_RAMANUJAN,           /**< Infinite series, partitions, intuition */
    GENIUS_MODE_TURING,              /**< Computation, logic, decidability */
    GENIUS_MODE_NOETHER,             /**< Abstract algebra, symmetry, invariants */
    GENIUS_MODE_ADAPTIVE,            /**< Auto-select based on problem */
    GENIUS_MODE_COUNT                /**< Total number of modes */
} genius_mode_t;

/* ============================================================================
 * Mode Characteristics
 * ============================================================================ */

/**
 * @brief Problem domain types for mode selection
 */
typedef enum {
    GENIUS_DOMAIN_NUMBER_THEORY = 0, /**< Primes, modular arithmetic, etc. */
    GENIUS_DOMAIN_CALCULUS,          /**< Derivatives, integrals, limits */
    GENIUS_DOMAIN_COMBINATORICS,     /**< Counting, permutations, etc. */
    GENIUS_DOMAIN_GRAPH_THEORY,      /**< Graphs, networks, paths */
    GENIUS_DOMAIN_ALGEBRA,           /**< Equations, structures */
    GENIUS_DOMAIN_GEOMETRY,          /**< Shapes, space, coordinates */
    GENIUS_DOMAIN_PROBABILITY,       /**< Random processes, distributions */
    GENIUS_DOMAIN_LOGIC,             /**< Propositions, proofs, decidability */
    GENIUS_DOMAIN_ANALYSIS,          /**< Series, convergence, functions */
    GENIUS_DOMAIN_PHYSICS,           /**< Physical laws, dynamics */
    GENIUS_DOMAIN_OPTIMIZATION,      /**< Finding extrema */
    GENIUS_DOMAIN_STATISTICS,        /**< Statistical analysis, distributions */
    GENIUS_DOMAIN_DIFFERENTIAL_EQ,   /**< Differential equations */
    GENIUS_DOMAIN_TOPOLOGY,          /**< Topological structures */
    GENIUS_DOMAIN_INFINITE_SERIES,   /**< Infinite series and convergence */
    GENIUS_DOMAIN_UNKNOWN,           /**< Unknown/mixed domain */
    GENIUS_DOMAIN_COUNT              /**< Total domains */
} genius_domain_t;

/**
 * @brief Cognitive approach types
 */
typedef enum {
    GENIUS_APPROACH_ANALYTIC = 0,    /**< Break down into parts */
    GENIUS_APPROACH_SYNTHETIC,       /**< Build up from elements */
    GENIUS_APPROACH_INTUITIVE,       /**< Pattern recognition, guessing */
    GENIUS_APPROACH_RIGOROUS,        /**< Formal proof construction */
    GENIUS_APPROACH_COMPUTATIONAL,   /**< Algorithmic, numeric */
    GENIUS_APPROACH_GEOMETRIC,       /**< Visual, spatial reasoning */
    GENIUS_APPROACH_PROBABILISTIC,   /**< Randomized, expected values */
    GENIUS_APPROACH_COUNT            /**< Total approaches */
} genius_approach_t;

/* ============================================================================
 * Mode Capability Structures
 * ============================================================================ */

/**
 * @brief Capabilities and strengths of a genius mode
 */
typedef struct genius_mode_capabilities {
    genius_mode_t mode;              /**< The mode */
    const char* name;                /**< Human-readable name */
    const char* description;         /**< Mode description */

    /* Domain strengths (0-1) */
    float domain_strengths[GENIUS_DOMAIN_COUNT];

    /* Approach preferences (0-1) */
    float approach_preferences[GENIUS_APPROACH_COUNT];

    /* Cognitive characteristics */
    float creativity;                /**< Creative/novel solutions [0,1] */
    float rigor;                     /**< Formal rigor [0,1] */
    float intuition;                 /**< Intuitive leaps [0,1] */
    float computation;               /**< Computational power [0,1] */
    float abstraction;               /**< Abstract thinking [0,1] */
    float visualization;             /**< Spatial visualization [0,1] */
    float collaboration;             /**< Multi-mode collaboration [0,1] */

    /* Biological basis */
    const char* brain_regions;       /**< Associated brain regions */
    float atp_cost_multiplier;       /**< Metabolic cost factor */
} genius_mode_capabilities_t;

/* ============================================================================
 * Mode-Specific Result Structures
 * ============================================================================ */

/**
 * @brief Gauss mode specific results
 */
typedef struct gauss_result {
    /* Pattern discovery */
    uint64_t pattern_sum;            /**< Closed form for sums */
    bool has_closed_form;            /**< Whether closed form found */

    /* Number theory */
    uint64_t* prime_factors;         /**< Prime factorization */
    uint32_t num_factors;            /**< Number of prime factors */
    bool is_prime;                   /**< Primality result */
    int64_t quadratic_residue;       /**< Quadratic residue if applicable */

    /* Statistics */
    float mean;                      /**< Sample mean */
    float variance;                  /**< Sample variance */
    float* distribution_params;      /**< Distribution parameters */
    uint32_t num_params;             /**< Number of parameters */
} gauss_result_t;

/**
 * @brief Newton mode specific results
 */
typedef struct newton_result {
    /* Calculus results */
    void* derivative;                /**< Symbolic derivative (expr_node_t*) */
    void* integral;                  /**< Symbolic integral */
    void* taylor_expansion;          /**< Taylor series */
    uint32_t taylor_terms;           /**< Number of Taylor terms */

    /* Root finding */
    float* roots;                    /**< Found roots */
    uint32_t num_roots;              /**< Number of roots */
    float convergence_rate;          /**< Newton-Raphson convergence */

    /* Differential equations */
    void* ode_solution;              /**< ODE solution */
    bool is_exact_solution;          /**< Whether solution is exact */
    float approximation_error;       /**< Error bound if approximate */
} newton_result_t;

/**
 * @brief Erdős mode specific results
 */
typedef struct erdos_result {
    /* Graph theory */
    uint32_t chromatic_number;       /**< Graph chromatic number */
    uint32_t independence_number;    /**< Max independent set size */
    uint32_t clique_number;          /**< Max clique size */
    bool has_hamiltonian_path;       /**< Hamiltonian path exists */

    /* Combinatorics */
    uint64_t counting_result;        /**< Combinatorial count */
    void* generating_function;       /**< Generating function */

    /* Probabilistic method */
    bool existence_proved;           /**< Existence proof found */
    float probability_bound;         /**< Probability lower/upper bound */
    float expected_value;            /**< Expected value computation */

    /* Ramsey theory */
    uint32_t ramsey_bound_lower;     /**< Ramsey number lower bound */
    uint32_t ramsey_bound_upper;     /**< Ramsey number upper bound */
} erdos_result_t;

/* ============================================================================
 * Mode Selection and Configuration
 * ============================================================================ */

/**
 * @brief Configuration for mode selection
 */
typedef struct genius_mode_config {
    genius_mode_t preferred_mode;    /**< Preferred mode (ADAPTIVE for auto) */
    float creativity_level;          /**< 0-1: conservative to creative */
    float rigor_level;               /**< 0-1: intuitive to rigorous */
    float collaboration_weight;      /**< Weight for multi-mode collaboration */
    float mode_switch_threshold;     /**< Threshold for adaptive mode switch */
    bool enable_mode_ensemble;       /**< Use multiple modes in parallel */
    uint32_t max_ensemble_modes;     /**< Maximum modes in ensemble */
} genius_mode_config_t;

/**
 * @brief Mode selection recommendation
 */
typedef struct mode_recommendation {
    genius_mode_t recommended_mode;  /**< Best mode for problem */
    float confidence;                /**< Confidence in recommendation */
    genius_mode_t* alternative_modes; /**< Alternative modes */
    float* alternative_scores;       /**< Scores for alternatives */
    uint32_t num_alternatives;       /**< Number of alternatives */
    const char* reasoning;           /**< Why this mode was chosen */
} mode_recommendation_t;

/* ============================================================================
 * Mode Information Functions
 * ============================================================================ */

/**
 * @brief Get capabilities for a genius mode
 *
 * @param mode The genius mode
 * @param caps Output capabilities
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_mode_get_capabilities(
    genius_mode_t mode,
    genius_mode_capabilities_t* caps);

/**
 * @brief Get mode name
 *
 * @param mode The genius mode
 * @return Mode name string
 */
NIMCP_API const char* genius_mode_get_name(genius_mode_t mode);

/**
 * @brief Get mode description
 *
 * @param mode The genius mode
 * @return Mode description string
 */
NIMCP_API const char* genius_mode_get_description(genius_mode_t mode);

/**
 * @brief Recommend mode for a problem domain
 *
 * @param domain Problem domain
 * @param config Mode configuration
 * @param recommendation Output recommendation
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_mode_recommend(
    genius_domain_t domain,
    const genius_mode_config_t* config,
    mode_recommendation_t* recommendation);

/**
 * @brief Get default mode configuration
 *
 * @param config Configuration to fill
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_mode_get_default_config(
    genius_mode_config_t* config);

/**
 * @brief Free mode recommendation resources
 *
 * @param recommendation Recommendation to free
 */
NIMCP_API void genius_mode_recommendation_free(
    mode_recommendation_t* recommendation);

/* ============================================================================
 * Mode Strength Constants
 * ============================================================================ */

/* Gauss mode strengths */
#define GAUSS_STRENGTH_NUMBER_THEORY    0.95f
#define GAUSS_STRENGTH_STATISTICS       0.90f
#define GAUSS_STRENGTH_ALGEBRA          0.85f
#define GAUSS_STRENGTH_GEOMETRY         0.80f
#define GAUSS_STRENGTH_ANALYSIS         0.75f

/* Newton mode strengths */
#define NEWTON_STRENGTH_CALCULUS        0.98f
#define NEWTON_STRENGTH_PHYSICS         0.95f
#define NEWTON_STRENGTH_ANALYSIS        0.90f
#define NEWTON_STRENGTH_OPTIMIZATION    0.85f
#define NEWTON_STRENGTH_GEOMETRY        0.80f

/* Erdős mode strengths */
#define ERDOS_STRENGTH_COMBINATORICS    0.98f
#define ERDOS_STRENGTH_GRAPH_THEORY     0.95f
#define ERDOS_STRENGTH_PROBABILITY      0.90f
#define ERDOS_STRENGTH_NUMBER_THEORY    0.85f

/* Euler mode strengths */
#define EULER_STRENGTH_ANALYSIS         0.95f
#define EULER_STRENGTH_NUMBER_THEORY    0.90f
#define EULER_STRENGTH_GRAPH_THEORY     0.90f
#define EULER_STRENGTH_COMBINATORICS    0.85f

/* Ramanujan mode strengths */
#define RAMANUJAN_STRENGTH_ANALYSIS     0.98f
#define RAMANUJAN_STRENGTH_NUMBER_THEORY 0.95f
#define RAMANUJAN_STRENGTH_INTUITION    0.99f

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GENIUS_MODES_H */
