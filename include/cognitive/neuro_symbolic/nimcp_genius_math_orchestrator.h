/**
 * @file nimcp_genius_math_orchestrator.h
 * @brief Genius Mathematics Orchestrator - Full System Integration
 *
 * Orchestrates all components of the Mathesis-inspired mathematics system:
 * - Energy-based consistency checking
 * - Evolutionary proof search
 * - Hypergraph knowledge representation
 * - Mathematical genius modules (Gauss/Newton/Erdős)
 * - Quantum MCTS planning
 * - Integration with existing parietal lobe modules
 *
 * The orchestrator provides a unified interface for mathematical reasoning
 * that automatically selects and coordinates the appropriate subsystems.
 *
 * Biological Basis:
 * - Prefrontal executive control for orchestration
 * - Parietal mathematical processing
 * - Temporal pattern recognition
 * - Hippocampal memory consolidation
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_GENIUS_MATH_ORCHESTRATOR_H
#define NIMCP_GENIUS_MATH_ORCHESTRATOR_H

#include <stdbool.h>
#include <stdint.h>
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"

/* Component headers */
#include "cognitive/neuro_symbolic/nimcp_energy_consistency.h"
#include "cognitive/neuro_symbolic/nimcp_hypergraph.h"
#include "cognitive/neuro_symbolic/nimcp_quantum_mcts.h"
#include "cognitive/parietal/nimcp_mathematical_genius.h"
#include "cognitive/parietal/nimcp_mathematical_intuition.h"
#include "cognitive/parietal/nimcp_equation_manipulation.h"
#include "cognitive/parietal/nimcp_number_sense.h"
#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include "cognitive/free_energy/nimcp_fep_planning.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Constants
 * ============================================================================ */

/** Bio-async module identifier */
#define BIO_MODULE_GENIUS_ORCHESTRATOR  0x0392

/** Maximum components in orchestration */
#define ORCHESTRATOR_MAX_COMPONENTS     32

/** Default timeout for operations (ms) */
#define ORCHESTRATOR_DEFAULT_TIMEOUT_MS 30000

/* ============================================================================
 * Orchestrator Operation Types
 * ============================================================================ */

/**
 * @brief Types of orchestrated operations
 */
typedef enum {
    ORCH_OP_SOLVE_PROBLEM = 0,       /**< General problem solving */
    ORCH_OP_PROVE_THEOREM,           /**< Theorem proving */
    ORCH_OP_GENERATE_CONJECTURES,    /**< Conjecture generation */
    ORCH_OP_CONSISTENCY_CHECK,       /**< Consistency verification */
    ORCH_OP_PATTERN_DISCOVERY,       /**< Pattern mining */
    ORCH_OP_ANALOGY_SEARCH,          /**< Cross-domain analogy */
    ORCH_OP_OPTIMIZATION,            /**< Mathematical optimization */
    ORCH_OP_GAME_THEORY_ANALYSIS,    /**< Game-theoretic analysis */
    ORCH_OP_QUANTUM_PLANNING,        /**< Quantum-enhanced planning */
    ORCH_OP_COUNT                    /**< Total operation types */
} orchestrator_operation_t;

/**
 * @brief Component activation flags
 */
typedef enum {
    ORCH_COMP_CONSISTENCY = (1 << 0),  /**< Energy consistency */
    ORCH_COMP_HYPERGRAPH = (1 << 1),   /**< Hypergraph KB */
    ORCH_COMP_GENIUS = (1 << 2),       /**< Mathematical genius */
    ORCH_COMP_QUANTUM_MCTS = (1 << 3), /**< Quantum MCTS */
    ORCH_COMP_FEP = (1 << 4),          /**< FEP planning */
    ORCH_COMP_GAME_THEORY = (1 << 5),  /**< Game theory */
    ORCH_COMP_PATTERN = (1 << 6),      /**< Pattern detection */
    ORCH_COMP_EQUATION = (1 << 7),     /**< Equation manipulation */
    ORCH_COMP_NUMBER_SENSE = (1 << 8), /**< Number sense */
    ORCH_COMP_SPATIAL = (1 << 9),      /**< Spatial reasoning */
    ORCH_COMP_ALL = 0xFFFFFFFF         /**< All components */
} orchestrator_components_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Proof result from orchestrated theorem proving
 */
typedef struct orchestrator_proof_result {
    bool proved;                     /**< Whether theorem was proved */
    proof_trace_t* proof_trace;      /**< The proof trace */
    float elegance_score;            /**< Proof elegance [0,1] */
    float confidence;                /**< Confidence in proof [0,1] */
    genius_mode_t proving_mode;      /**< Mode that found proof */
    uint32_t steps_used;             /**< Number of proof steps */
    uint64_t proving_time_us;        /**< Time to find proof */
    float consistency_energy;        /**< Consistency energy of proof */
} orchestrator_proof_result_t;

/**
 * @brief Conjecture result from orchestrated generation
 */
typedef struct orchestrator_conjecture_result {
    conjecture_t* conjectures;       /**< Generated conjectures */
    uint32_t num_conjectures;        /**< Number of conjectures */
    float avg_confidence;            /**< Average confidence */
    float avg_novelty;               /**< Average novelty */
    genius_mode_t* generating_modes; /**< Modes that generated each */
    uint64_t generation_time_us;     /**< Time to generate */
} orchestrator_conjecture_result_t;

/**
 * @brief Game-theoretic analysis result
 */
typedef struct orchestrator_game_result {
    bool equilibrium_found;          /**< Whether equilibrium found */
    float* nash_strategies;          /**< Nash equilibrium strategies */
    uint32_t num_strategies;         /**< Number of strategies */
    float* shapley_values;           /**< Shapley value attribution */
    float social_welfare;            /**< Social welfare at equilibrium */
    float fairness_score;            /**< Fairness metric */
    uint64_t analysis_time_us;       /**< Time to analyze */
} orchestrator_game_result_t;

/**
 * @brief Optimization result
 */
typedef struct orchestrator_optimization_result {
    bool optimal_found;              /**< Whether optimal found */
    float* optimal_point;            /**< Optimal point */
    uint32_t dim;                    /**< Dimensionality */
    float optimal_value;             /**< Value at optimal */
    float* gradient_at_optimal;      /**< Gradient (should be ~0) */
    bool is_global;                  /**< Whether globally optimal */
    uint32_t iterations;             /**< Iterations to converge */
    uint64_t optimization_time_us;   /**< Time to optimize */
} orchestrator_optimization_result_t;

/**
 * @brief Complete orchestrator result
 */
typedef struct orchestrator_result {
    orchestrator_operation_t operation; /**< Operation performed */
    bool success;                    /**< Whether operation succeeded */

    /* Component results */
    genius_result_t* genius_result;  /**< Genius module result */
    orchestrator_proof_result_t* proof_result;   /**< Proof result */
    orchestrator_conjecture_result_t* conjecture_result; /**< Conjecture result */
    orchestrator_game_result_t* game_result;     /**< Game theory result */
    orchestrator_optimization_result_t* opt_result; /**< Optimization result */

    /* Consistency check */
    energy_consistency_result_t* consistency_result; /**< Consistency result */

    /* Performance metrics */
    uint64_t total_time_us;          /**< Total operation time */
    float atp_consumed;              /**< ATP consumed */
    uint32_t components_used;        /**< Bitmask of components used */

    /* Quality metrics */
    float overall_confidence;        /**< Combined confidence */
    float solution_quality;          /**< Solution quality score */
} orchestrator_result_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Configuration for genius math orchestrator
 */
typedef struct orchestrator_config {
    /* Component enablement */
    uint32_t enabled_components;     /**< Bitmask of enabled components */

    /* Timeout settings */
    uint64_t operation_timeout_ms;   /**< Timeout per operation */
    uint64_t component_timeout_ms;   /**< Timeout per component */

    /* Mode selection */
    genius_mode_t default_mode;      /**< Default genius mode */
    bool auto_select_mode;           /**< Auto-select based on problem */

    /* Quality settings */
    float min_confidence_threshold;  /**< Minimum confidence to accept */
    float target_elegance;           /**< Target elegance for proofs */
    bool require_consistency_check;  /**< Always verify consistency */

    /* Resource limits */
    float max_atp_budget;            /**< Maximum ATP to consume */
    uint32_t max_proof_depth;        /**< Maximum proof depth */
    uint32_t max_iterations;         /**< Maximum iterations */

    /* Integration */
    bool enable_quantum_enhancement; /**< Use quantum methods */
    bool enable_game_theory;         /**< Use game-theoretic methods */
    bool enable_bio_async;           /**< Enable async messaging */

    /* Modulation */
    float inflammation_sensitivity;  /**< Sensitivity to inflammation */
    float fatigue_sensitivity;       /**< Sensitivity to fatigue */
    float atp_sensitivity;           /**< Sensitivity to ATP levels */
} orchestrator_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Statistics for orchestrator
 */
typedef struct orchestrator_stats {
    /* Operation counts */
    uint64_t operations_total;       /**< Total operations */
    uint64_t operations_succeeded;   /**< Successful operations */
    uint64_t operation_counts[ORCH_OP_COUNT]; /**< Per-operation counts */

    /* Component usage */
    uint64_t component_activations[10]; /**< Per-component activations */

    /* Performance */
    uint64_t total_time_us;          /**< Total processing time */
    float avg_time_per_operation_us; /**< Average time per operation */
    float total_atp_consumed;        /**< Total ATP consumed */

    /* Quality */
    float avg_confidence;            /**< Average confidence */
    float avg_elegance;              /**< Average elegance */
    uint64_t consistency_failures;   /**< Consistency check failures */

    /* Mode statistics */
    uint64_t mode_usage[GENIUS_MODE_COUNT]; /**< Per-mode usage */
    float mode_success_rates[GENIUS_MODE_COUNT]; /**< Per-mode success */
} orchestrator_stats_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Opaque handle to genius math orchestrator
 */
typedef struct genius_math_orchestrator genius_math_orchestrator_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create genius math orchestrator
 *
 * @param config Configuration (NULL for defaults)
 * @return Orchestrator handle or NULL on failure
 */
NIMCP_API genius_math_orchestrator_t* genius_orchestrator_create(
    const orchestrator_config_t* config);

/**
 * @brief Destroy orchestrator
 *
 * @param orch Orchestrator to destroy
 */
NIMCP_API void genius_orchestrator_destroy(genius_math_orchestrator_t* orch);

/**
 * @brief Reset orchestrator state
 *
 * @param orch The orchestrator
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_reset(
    genius_math_orchestrator_t* orch);

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_get_default_config(
    orchestrator_config_t* config);

/* ============================================================================
 * Component Management
 * ============================================================================ */

/**
 * @brief Initialize all components
 *
 * @param orch The orchestrator
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_init_components(
    genius_math_orchestrator_t* orch);

/**
 * @brief Set external consistency checker
 *
 * @param orch The orchestrator
 * @param checker Energy consistency checker
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_set_consistency(
    genius_math_orchestrator_t* orch,
    energy_consistency_checker_t* checker);

/**
 * @brief Set external hypergraph
 *
 * @param orch The orchestrator
 * @param hypergraph Hypergraph for knowledge
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_set_hypergraph(
    genius_math_orchestrator_t* orch,
    nimcp_hypergraph_t* hypergraph);

/**
 * @brief Set external genius module
 *
 * @param orch The orchestrator
 * @param genius Mathematical genius
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_set_genius(
    genius_math_orchestrator_t* orch,
    mathematical_genius_t* genius);

/**
 * @brief Set external quantum MCTS
 *
 * @param orch The orchestrator
 * @param qmcts Quantum MCTS system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_set_quantum_mcts(
    genius_math_orchestrator_t* orch,
    quantum_mcts_t* qmcts);

/**
 * @brief Set external FEP planner
 *
 * @param orch The orchestrator
 * @param fep_planner FEP planning system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_set_fep_planner(
    genius_math_orchestrator_t* orch,
    fep_planning_system_t* fep_planner);

/**
 * @brief Set external game theory system
 *
 * @param orch The orchestrator
 * @param game_theory Game theory system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_set_game_theory(
    genius_math_orchestrator_t* orch,
    nimcp_gt_system_t* game_theory);

/* ============================================================================
 * High-Level Operations
 * ============================================================================ */

/**
 * @brief Solve a mathematical problem
 *
 * Main entry point for problem solving. Automatically selects and
 * coordinates appropriate components.
 *
 * @param orch The orchestrator
 * @param problem Problem to solve
 * @param result Output result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_solve(
    genius_math_orchestrator_t* orch,
    const math_problem_t* problem,
    orchestrator_result_t* result);

/**
 * @brief Prove a theorem
 *
 * Attempts to construct a formal proof with elegance optimization.
 *
 * @param orch The orchestrator
 * @param theorem Theorem statement
 * @param result Output proof result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_prove(
    genius_math_orchestrator_t* orch,
    const char* theorem,
    orchestrator_proof_result_t* result);

/**
 * @brief Generate conjectures in a domain
 *
 * @param orch The orchestrator
 * @param domain Mathematical domain
 * @param constraints Optional constraints
 * @param result Output conjecture result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_conjecture(
    genius_math_orchestrator_t* orch,
    genius_domain_t domain,
    const void* constraints,
    orchestrator_conjecture_result_t* result);

/**
 * @brief Perform game-theoretic analysis
 *
 * Uses game theory for multi-agent mathematical reasoning.
 *
 * @param orch The orchestrator
 * @param game Game specification
 * @param result Output game result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_game_theory_analysis(
    genius_math_orchestrator_t* orch,
    const void* game,
    orchestrator_game_result_t* result);

/**
 * @brief Perform quantum-enhanced optimization
 *
 * Uses quantum MCTS and annealing for optimization.
 *
 * @param orch The orchestrator
 * @param objective Objective function
 * @param constraints Constraints
 * @param result Output optimization result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_optimize(
    genius_math_orchestrator_t* orch,
    const void* objective,
    const void* constraints,
    orchestrator_optimization_result_t* result);

/* ============================================================================
 * Consistency Operations
 * ============================================================================ */

/**
 * @brief Check consistency of a result
 *
 * @param orch The orchestrator
 * @param result Result to check
 * @param consistency_result Output consistency result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_check_consistency(
    genius_math_orchestrator_t* orch,
    const orchestrator_result_t* result,
    energy_consistency_result_t* consistency_result);

/**
 * @brief Verify a proof trace
 *
 * @param orch The orchestrator
 * @param proof Proof result to verify
 * @param is_valid Output: true if proof is valid
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_verify_proof(
    genius_math_orchestrator_t* orch,
    const orchestrator_proof_result_t* proof,
    bool* is_valid);

/* ============================================================================
 * Modulation
 * ============================================================================ */

/**
 * @brief Apply inflammation modulation to all components
 *
 * @param orch The orchestrator
 * @param inflammation Inflammation level [0,1]
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_modulate_inflammation(
    genius_math_orchestrator_t* orch,
    float inflammation);

/**
 * @brief Apply fatigue modulation to all components
 *
 * @param orch The orchestrator
 * @param fatigue Fatigue level [0,1]
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_modulate_fatigue(
    genius_math_orchestrator_t* orch,
    float fatigue);

/**
 * @brief Apply ATP level modulation to all components
 *
 * @param orch The orchestrator
 * @param atp_level ATP level [0,1]
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_modulate_atp(
    genius_math_orchestrator_t* orch,
    float atp_level);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Register with bio-async router
 *
 * @param orch The orchestrator
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_register_bio_async(
    genius_math_orchestrator_t* orch);

/**
 * @brief Unregister from bio-async router
 *
 * @param orch The orchestrator
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_unregister_bio_async(
    genius_math_orchestrator_t* orch);

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param orch The orchestrator
 * @param stats Output statistics
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_get_stats(
    const genius_math_orchestrator_t* orch,
    orchestrator_stats_t* stats);

/**
 * @brief Print diagnostic information
 *
 * @param orch The orchestrator
 */
NIMCP_API void genius_orchestrator_print_diagnostics(
    const genius_math_orchestrator_t* orch);

/* ============================================================================
 * Result Management
 * ============================================================================ */

/**
 * @brief Initialize orchestrator result
 *
 * @param result Result to initialize
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_result_init(
    orchestrator_result_t* result);

/**
 * @brief Clean up orchestrator result
 *
 * @param result Result to clean up
 */
NIMCP_API void genius_orchestrator_result_cleanup(
    orchestrator_result_t* result);

/**
 * @brief Initialize proof result
 *
 * @param result Result to initialize
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t genius_orchestrator_proof_result_init(
    orchestrator_proof_result_t* result);

/**
 * @brief Clean up proof result
 *
 * @param result Result to clean up
 */
NIMCP_API void genius_orchestrator_proof_result_cleanup(
    orchestrator_proof_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GENIUS_MATH_ORCHESTRATOR_H */
