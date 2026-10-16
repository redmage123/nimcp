/**
 * @file nimcp_mathematical_genius.h
 * @brief Mathematical Genius Module - Parietal Lobe
 *
 * Implements a comprehensive mathematical reasoning system that emulates
 * the cognitive styles of legendary mathematicians (Gauss, Newton, Erdős,
 * Euler, Ramanujan) for advanced theorem proving, conjecture generation,
 * and mathematical insight discovery.
 *
 * Key Features:
 * - Multiple genius modes with distinct cognitive styles
 * - Pattern discovery and conjecture generation
 * - Theorem proving with elegance scoring
 * - Analogy finding across mathematical domains
 * - Integration with existing parietal lobe modules
 *
 * Biological Basis:
 * - Parietal cortex (IPS) for mathematical processing
 * - Prefrontal cortex for abstract reasoning
 * - Temporal lobe for pattern recognition
 * - Angular gyrus for symbolic manipulation
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_MATHEMATICAL_GENIUS_H
#define NIMCP_MATHEMATICAL_GENIUS_H

#include <stdbool.h>
#include <stdint.h>
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/parietal/nimcp_genius_modes.h"
#include "cognitive/parietal/nimcp_mathematical_intuition.h"
#include "cognitive/parietal/nimcp_equation_manipulation.h"
#include "cognitive/parietal/nimcp_number_sense.h"
#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_async.h"
#include "utils/exception/nimcp_exception_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Constants
 * ============================================================================ */

/** Bio-async module identifier */
#define BIO_MODULE_MATHEMATICAL_GENIUS  0x0390

/** Maximum conjecture depth */
#define GENIUS_MAX_CONJECTURE_DEPTH     20

/** Maximum proof steps */
#define GENIUS_MAX_PROOF_STEPS          1000

/** Maximum pattern length for detection */
#define GENIUS_MAX_PATTERN_LENGTH       256

/** Default creativity level */
#define GENIUS_DEFAULT_CREATIVITY       0.7f

/** Default rigor level */
#define GENIUS_DEFAULT_RIGOR            0.8f

/** Maximum insight candidates */
#define GENIUS_MAX_INSIGHTS             64

/** Maximum collaborating geniuses */
#define GENIUS_MAX_COLLABORATORS        8

/* ============================================================================
 * Problem and Conjecture Structures
 * ============================================================================ */

/**
 * @brief Mathematical problem statement
 */
typedef struct math_problem {
    char* statement;                 /**< Problem statement text */
    genius_domain_t domain;          /**< Primary domain */
    genius_domain_t* secondary_domains; /**< Related domains */
    uint32_t num_secondary;          /**< Number of secondary domains */
    void* constraints;               /**< Problem constraints */
    void* given;                     /**< Given information */
    void* target;                    /**< What to find/prove */
    float difficulty;                /**< Estimated difficulty [0,1] */
    uint64_t timeout_ms;             /**< Timeout for solving */
} math_problem_t;

/**
 * @brief Mathematical conjecture
 */
typedef struct conjecture {
    uint32_t id;                     /**< Unique identifier */
    char* statement;                 /**< Conjecture statement */
    genius_domain_t domain;          /**< Mathematical domain */
    float confidence;                /**< Confidence in conjecture [0,1] */
    float novelty;                   /**< Novelty score [0,1] */
    float importance;                /**< Estimated importance [0,1] */
    bool is_verified;                /**< Whether verified (not proven) */
    uint32_t counter_example_count;  /**< Number of counter-examples tried */
    void* supporting_evidence;       /**< Evidence supporting conjecture */
    genius_mode_t generating_mode;   /**< Mode that generated it */
    uint64_t generated_time_us;      /**< Generation timestamp */
} conjecture_t;

/**
 * @brief Proof step in a trace
 */
typedef struct genius_proof_step {
    uint32_t step_id;                /**< Step identifier */
    char* statement;                 /**< Statement at this step */
    char* justification;             /**< Justification/rule applied */
    uint32_t* premises;              /**< Premise step IDs */
    uint32_t num_premises;           /**< Number of premises */
    float confidence;                /**< Step confidence */
    bool is_axiom;                   /**< Is this an axiom? */
    bool is_hypothesis;              /**< Is this a hypothesis? */
} genius_proof_step_t;

/**
 * @brief Proof trace
 */
typedef struct proof_trace {
    genius_proof_step_t* steps;      /**< Array of proof steps */
    uint32_t num_steps;              /**< Number of steps */
    uint32_t capacity;               /**< Allocated capacity */
    bool is_complete;                /**< Is proof complete? */
    bool is_valid;                   /**< Has proof been validated? */
    float elegance_score;            /**< Proof elegance [0,1] */
    float difficulty_score;          /**< Proof difficulty [0,1] */
    genius_mode_t constructing_mode; /**< Mode that constructed proof */
    uint64_t construction_time_us;   /**< Time to construct */
} proof_trace_t;

/**
 * @brief Mathematical insight
 */
typedef struct insight {
    uint32_t id;                     /**< Insight identifier */
    char* description;               /**< Description of insight */
    genius_domain_t source_domain;   /**< Source domain */
    genius_domain_t target_domain;   /**< Target domain (for cross-domain) */
    float significance;              /**< Significance score [0,1] */
    float applicability;             /**< How broadly applicable [0,1] */
    void* related_patterns;          /**< Related patterns */
    genius_mode_t discovering_mode;  /**< Mode that discovered it */
} insight_t;

/**
 * @brief Analogy between mathematical structures
 */
typedef struct analogy_result {
    char* source;                    /**< Source structure/concept */
    char* target;                    /**< Target structure/concept */
    char* mapping;                   /**< Description of mapping */
    float similarity;                /**< Structural similarity [0,1] */
    float usefulness;                /**< How useful the analogy is [0,1] */
    genius_domain_t source_domain;   /**< Source domain */
    genius_domain_t target_domain;   /**< Target domain */
} genius_analogy_result_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Complete result from genius problem solving
 */
typedef struct genius_result {
    /* Problem info */
    genius_mode_t mode_used;         /**< Mode(s) used */
    bool solved;                     /**< Whether problem was solved */

    /* Patterns detected */
    detected_pattern_t* detected_patterns; /**< Detected patterns */
    uint32_t num_patterns;           /**< Number of patterns */

    /* Conjectures generated */
    conjecture_t* conjectures;       /**< Generated conjectures */
    uint32_t num_conjectures;        /**< Number of conjectures */

    /* Proofs found */
    proof_trace_t* proofs;           /**< Proofs constructed */
    uint32_t num_proofs;             /**< Number of proofs */

    /* Insights discovered */
    insight_t* insights;             /**< Discovered insights */
    uint32_t num_insights;           /**< Number of insights */

    /* Analogies found */
    genius_analogy_result_t* analogies; /**< Found analogies */
    uint32_t num_analogies;          /**< Number of analogies */

    /* Performance metrics */
    float elegance_score;            /**< Overall elegance */
    float novelty_score;             /**< Novelty of approach */
    float generalization_score;      /**< Generalizability */
    float rigor_score;               /**< Rigor of solution */

    /* Resource usage */
    uint64_t thinking_time_us;       /**< Time spent thinking */
    float atp_consumed;              /**< Estimated ATP consumed */

    /* Mode-specific results */
    gauss_result_t* gauss_result;    /**< Gauss-specific results */
    newton_result_t* newton_result;  /**< Newton-specific results */
    erdos_result_t* erdos_result;    /**< Erdős-specific results */
} genius_result_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Configuration for mathematical genius
 */
typedef struct genius_config {
    /* Mode settings */
    genius_mode_t default_mode;      /**< Default genius mode */
    genius_mode_config_t mode_config; /**< Mode-specific config */

    /* Cognitive parameters */
    float creativity_level;          /**< 0-1: conservative to creative */
    float rigor_level;               /**< 0-1: intuitive to rigorous */
    float collaboration_weight;      /**< Weight for collaboration */

    /* Search parameters */
    bool enable_quantum_search;      /**< Use quantum-enhanced search */
    bool enable_pattern_mining;      /**< Enable pattern discovery */
    bool enable_analogy_engine;      /**< Enable analogy finding */
    uint32_t max_proof_depth;        /**< Maximum proof search depth */
    uint32_t max_conjecture_candidates; /**< Max conjectures to generate */

    /* Integration */
    bool enable_fep_integration;     /**< Integrate with FEP */
    bool enable_bio_async;           /**< Enable async messaging */

    /* Modulation */
    float inflammation_sensitivity;  /**< Sensitivity to inflammation */
    float fatigue_sensitivity;       /**< Sensitivity to fatigue */
    float atp_sensitivity;           /**< Sensitivity to ATP levels */

    /* Resource limits */
    uint64_t max_thinking_time_ms;   /**< Maximum thinking time */
    float max_atp_budget;            /**< Maximum ATP to consume */
} genius_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Statistics for mathematical genius
 */
typedef struct genius_stats {
    /* Problem solving */
    uint64_t problems_attempted;     /**< Total problems attempted */
    uint64_t problems_solved;        /**< Problems successfully solved */
    uint64_t proofs_constructed;     /**< Total proofs constructed */
    uint64_t conjectures_generated;  /**< Total conjectures generated */
    uint64_t insights_discovered;    /**< Total insights discovered */

    /* Mode usage */
    uint64_t mode_usage[GENIUS_MODE_COUNT]; /**< Usage count per mode */
    float mode_success_rate[GENIUS_MODE_COUNT]; /**< Success rate per mode */

    /* Quality metrics */
    float avg_elegance;              /**< Average elegance score */
    float avg_novelty;               /**< Average novelty score */
    float avg_rigor;                 /**< Average rigor score */

    /* Performance */
    uint64_t total_thinking_time_us; /**< Total thinking time */
    float avg_thinking_time_us;      /**< Average per problem */
    float total_atp_consumed;        /**< Total ATP consumed */

    /* Collaboration */
    uint64_t collaborations;         /**< Number of collaborations */
    float collaboration_improvement; /**< Avg improvement from collab */
} genius_stats_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Opaque handle to mathematical genius
 */
typedef struct mathematical_genius mathematical_genius_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create mathematical genius with configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Genius handle or NULL on failure
 */
NIMCP_API mathematical_genius_t* genius_create(const genius_config_t* config);

/**
 * @brief Destroy mathematical genius
 *
 * @param genius Genius to destroy
 */
NIMCP_API void genius_destroy(mathematical_genius_t* genius);

/**
 * @brief Reset genius state
 *
 * @param genius The genius
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_reset(mathematical_genius_t* genius);

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_get_default_config(genius_config_t* config);

/* ============================================================================
 * Problem Solving Interface
 * ============================================================================ */

/**
 * @brief Solve a mathematical problem
 *
 * Main problem-solving interface that automatically selects modes
 * and strategies based on problem characteristics.
 *
 * @param genius The mathematical genius
 * @param problem Problem to solve
 * @param result Output result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_solve_problem(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result);

/**
 * @brief Generate conjectures in a domain
 *
 * @param genius The mathematical genius
 * @param domain Mathematical domain
 * @param context Optional context/constraints
 * @param conjectures Output conjectures
 * @param max_conjectures Maximum to generate
 * @return Number of conjectures generated
 */
NIMCP_API uint32_t genius_generate_conjectures(
    mathematical_genius_t* genius,
    genius_domain_t domain,
    const void* context,
    conjecture_t* conjectures,
    uint32_t max_conjectures);

/**
 * @brief Prove a theorem
 *
 * Attempts to construct a proof for the given theorem statement.
 *
 * @param genius The mathematical genius
 * @param theorem Theorem to prove
 * @param max_depth Maximum proof depth
 * @param trace Output proof trace
 * @return NIMCP_OK if proof found
 */
NIMCP_API nimcp_error_t genius_prove_theorem(
    mathematical_genius_t* genius,
    const char* theorem,
    uint32_t max_depth,
    proof_trace_t* trace);

/**
 * @brief Find analogies between domains
 *
 * @param genius The mathematical genius
 * @param source_domain Source domain
 * @param target_domain Target domain
 * @param analogies Output analogies
 * @param max_analogies Maximum to find
 * @return Number of analogies found
 */
NIMCP_API uint32_t genius_find_analogies(
    mathematical_genius_t* genius,
    genius_domain_t source_domain,
    genius_domain_t target_domain,
    genius_analogy_result_t* analogies,
    uint32_t max_analogies);

/* ============================================================================
 * Mode-Specific Entry Points
 * ============================================================================ */

/**
 * @brief Analyze using Gauss mode (number theory, patterns)
 *
 * @param genius The mathematical genius
 * @param problem Problem to analyze
 * @param result Output result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_gauss_analyze(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result);

/**
 * @brief Analyze using Newton mode (calculus, physics)
 *
 * @param genius The mathematical genius
 * @param problem Problem to analyze
 * @param result Output result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_newton_analyze(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result);

/**
 * @brief Analyze using Erdős mode (combinatorics, graph theory)
 *
 * @param genius The mathematical genius
 * @param problem Problem to analyze
 * @param result Output result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_erdos_analyze(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result);

/* ============================================================================
 * Gauss Mode Functions
 * ============================================================================ */

/**
 * @brief Discover pattern in sequence (like young Gauss: 1+2+...+100)
 *
 * @param genius The mathematical genius
 * @param sequence Number sequence
 * @param length Sequence length
 * @param conjecture Output conjecture
 * @return NIMCP_OK if pattern found
 */
NIMCP_API nimcp_error_t genius_gauss_discover_pattern(
    mathematical_genius_t* genius,
    const int64_t* sequence,
    uint32_t length,
    conjecture_t* conjecture);

/**
 * @brief Test primality with Gauss intuition
 *
 * @param genius The mathematical genius
 * @param n Number to test
 * @param certainty Required certainty level
 * @return true if (probably) prime
 */
NIMCP_API bool genius_gauss_is_prime(
    mathematical_genius_t* genius,
    uint64_t n,
    float certainty);

/**
 * @brief Factor number using Gauss methods
 *
 * @param genius The mathematical genius
 * @param n Number to factor
 * @param factors Output factors
 * @param max_factors Maximum factors
 * @return Number of factors found
 */
NIMCP_API uint32_t genius_gauss_factor(
    mathematical_genius_t* genius,
    uint64_t n,
    uint64_t* factors,
    uint32_t max_factors);

/**
 * @brief Compute modular exponentiation
 *
 * @param base Base
 * @param exp Exponent
 * @param mod Modulus
 * @return (base^exp) mod mod
 */
NIMCP_API uint64_t genius_gauss_modular_pow(
    uint64_t base,
    uint64_t exp,
    uint64_t mod);

/* ============================================================================
 * Newton Mode Functions
 * ============================================================================ */

/**
 * @brief Compute symbolic derivative
 *
 * @param genius The mathematical genius
 * @param expr Expression to differentiate
 * @param variable Variable to differentiate by
 * @param order Derivative order
 * @return Derivative expression (caller owns)
 */
NIMCP_API expr_node_t* genius_newton_differentiate(
    mathematical_genius_t* genius,
    const expr_node_t* expr,
    const char* variable,
    uint32_t order);

/**
 * @brief Compute symbolic integral
 *
 * @param genius The mathematical genius
 * @param expr Expression to integrate
 * @param variable Variable to integrate by
 * @return Integral expression (caller owns)
 */
NIMCP_API expr_node_t* genius_newton_integrate(
    mathematical_genius_t* genius,
    const expr_node_t* expr,
    const char* variable);

/**
 * @brief Compute Taylor series expansion
 *
 * @param genius The mathematical genius
 * @param expr Expression to expand
 * @param variable Expansion variable
 * @param center Expansion center
 * @param num_terms Number of terms
 * @return Taylor series expression (caller owns)
 */
NIMCP_API expr_node_t* genius_newton_taylor_series(
    mathematical_genius_t* genius,
    const expr_node_t* expr,
    const char* variable,
    float center,
    uint32_t num_terms);

/**
 * @brief Find root using Newton-Raphson
 *
 * @param genius The mathematical genius
 * @param f Function to find root of
 * @param df Derivative of function
 * @param x0 Initial guess
 * @param tolerance Convergence tolerance
 * @return Root value
 */
NIMCP_API float genius_newton_find_root(
    mathematical_genius_t* genius,
    float (*f)(float),
    float (*df)(float),
    float x0,
    float tolerance);

/* ============================================================================
 * Erdős Mode Functions
 * ============================================================================ */

/**
 * @brief Prove existence using probabilistic method
 *
 * @param genius The mathematical genius
 * @param problem Problem statement
 * @param trace Output proof trace
 * @return NIMCP_OK if existence proved
 */
NIMCP_API nimcp_error_t genius_erdos_probabilistic_existence(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    proof_trace_t* trace);

/**
 * @brief Compute Ramsey lower bound R(r,s)
 *
 * @param genius The mathematical genius
 * @param r First parameter
 * @param s Second parameter
 * @return Lower bound for R(r,s)
 */
NIMCP_API uint32_t genius_erdos_ramsey_lower_bound(
    mathematical_genius_t* genius,
    uint32_t r,
    uint32_t s);

/**
 * @brief Estimate chromatic number of graph
 *
 * @param genius The mathematical genius
 * @param adjacency Adjacency matrix
 * @param num_vertices Number of vertices
 * @return Estimated chromatic number
 */
NIMCP_API uint32_t genius_erdos_chromatic_number(
    mathematical_genius_t* genius,
    const uint8_t* adjacency,
    uint32_t num_vertices);

/* ============================================================================
 * Collaboration Functions
 * ============================================================================ */

/**
 * @brief Collaborative problem solving with multiple geniuses
 *
 * Erdős-style collaboration where multiple genius modes work together.
 *
 * @param geniuses Array of genius handles
 * @param num_geniuses Number of geniuses
 * @param problem Problem to solve
 * @param result Output result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_collaborate(
    mathematical_genius_t** geniuses,
    uint32_t num_geniuses,
    const math_problem_t* problem,
    genius_result_t* result);

/* ============================================================================
 * Integration Functions
 * ============================================================================ */

/**
 * @brief Link to game theory system
 *
 * @param genius The mathematical genius
 * @param game_theory Game theory system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_link_game_theory(
    mathematical_genius_t* genius,
    void* game_theory);

/**
 * @brief Link to quantum engine
 *
 * @param genius The mathematical genius
 * @param quantum_engine Quantum engine
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_link_quantum_engine(
    mathematical_genius_t* genius,
    void* quantum_engine);

/**
 * @brief Link to hypergraph for knowledge representation
 *
 * @param genius The mathematical genius
 * @param hypergraph Hypergraph structure
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_link_hypergraph(
    mathematical_genius_t* genius,
    void* hypergraph);

/* ============================================================================
 * Modulation
 * ============================================================================ */

/**
 * @brief Apply inflammation modulation
 *
 * @param genius The mathematical genius
 * @param inflammation Inflammation level [0,1]
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_modulate_inflammation(
    mathematical_genius_t* genius,
    float inflammation);

/**
 * @brief Apply fatigue modulation
 *
 * @param genius The mathematical genius
 * @param fatigue Fatigue level [0,1]
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_modulate_fatigue(
    mathematical_genius_t* genius,
    float fatigue);

/**
 * @brief Apply ATP level modulation
 *
 * @param genius The mathematical genius
 * @param atp_level ATP level [0,1]
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_modulate_atp(
    mathematical_genius_t* genius,
    float atp_level);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Register with bio-async router
 *
 * @param genius The mathematical genius
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_register_bio_async(
    mathematical_genius_t* genius);

/**
 * @brief Unregister from bio-async router
 *
 * @param genius The mathematical genius
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_unregister_bio_async(
    mathematical_genius_t* genius);

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param genius The mathematical genius
 * @param stats Output statistics
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_get_stats(
    const mathematical_genius_t* genius,
    genius_stats_t* stats);

/**
 * @brief Print diagnostic information
 *
 * @param genius The mathematical genius
 */
NIMCP_API void genius_print_diagnostics(const mathematical_genius_t* genius);

/* ============================================================================
 * Result Management
 * ============================================================================ */

/**
 * @brief Initialize result structure
 *
 * @param result Result to initialize
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_result_init(genius_result_t* result);

/**
 * @brief Clean up result structure
 *
 * @param result Result to clean up
 */
NIMCP_API void genius_result_cleanup(genius_result_t* result);

/**
 * @brief Initialize proof trace
 *
 * @param trace Trace to initialize
 * @param capacity Initial capacity
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_proof_trace_init(
    proof_trace_t* trace,
    uint32_t capacity);

/**
 * @brief Clean up proof trace
 *
 * @param trace Trace to clean up
 */
NIMCP_API void genius_proof_trace_cleanup(proof_trace_t* trace);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MATHEMATICAL_GENIUS_H */
