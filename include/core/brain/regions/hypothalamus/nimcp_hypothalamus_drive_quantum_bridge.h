/**
 * @file nimcp_hypothalamus_drive_quantum_bridge.h
 * @brief Drive System <-> Quantum Bridge for Alignment-Constrained Optimization
 *
 * WHAT: Bridge connecting the drive system to quantum optimizer
 * WHY:  Use quantum annealing for multi-objective drive optimization with alignment
 * HOW:  Formulate drive satisfaction as QUBO with alignment constraints
 *
 * BYRNES MODEL CONTEXT:
 * The steering subsystem must optimize across multiple competing drives while
 * maintaining alignment with human values. This bridge uses quantum annealing
 * to find optimal drive satisfaction strategies that respect alignment constraints.
 *
 * KEY CONCEPT - ALIGNMENT-CONSTRAINED QUBO:
 * The QUBO formulation includes:
 * 1. Drive urgency terms: Penalize unsatisfied drives
 * 2. Alignment constraints: human_wellbeing_weight enforces beneficial actions
 * 3. Resource constraints: Energy and time limitations
 * 4. Conflict resolution: Handle drive conflicts optimally
 *
 * QUANTUM ADVANTAGES:
 * - Parallel evaluation of drive satisfaction strategies
 * - Global optimization avoiding local minima
 * - Natural handling of multi-objective trade-offs
 * - Alignment constraints as QUBO penalty terms
 *
 * COMPUTE MODE SELECTION:
 * - Full quantum: When available and beneficial
 * - Hybrid: Quantum + classical for complex problems
 * - Classical fallback: When quantum unavailable
 *
 * BIO-ASYNC MESSAGES:
 * - Sends: BIO_MSG_QUANTUM_DRIVE_OPTIMIZATION_REQUEST
 * - Receives: BIO_MSG_QUANTUM_DRIVE_OPTIMIZATION_RESULT
 *
 * @version Phase 13: Quantum-Primary Integration
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_DRIVE_QUANTUM_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_DRIVE_QUANTUM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_quantum_bridge.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/bridge/nimcp_bridge_base.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Default alignment weight for human wellbeing */
#define HYPO_QUANTUM_ALIGNMENT_WEIGHT        1.5f

/** Maximum number of drive satisfaction strategies */
#define HYPO_QUANTUM_MAX_STRATEGIES          64

/** Threshold for quantum vs classical compute */
#define HYPO_QUANTUM_COMPLEXITY_THRESHOLD    8

/** Default QUBO size per drive */
#define HYPO_QUANTUM_QUBITS_PER_DRIVE        2

/*=============================================================================
 * COMPUTE MODE
 *===========================================================================*/

/**
 * @brief Compute mode for drive optimization
 */
typedef enum {
    HYPO_COMPUTE_MODE_AUTO = 0,        /**< Automatically select best mode */
    HYPO_COMPUTE_MODE_QUANTUM_ONLY,    /**< Force quantum (fail if unavailable) */
    HYPO_COMPUTE_MODE_HYBRID,          /**< Quantum + classical refinement */
    HYPO_COMPUTE_MODE_CLASSICAL_ONLY,  /**< Classical only (fallback) */
    HYPO_COMPUTE_MODE_MINIMAL          /**< Minimal compute (constrained platforms) */
} hypo_compute_mode_t;

/**
 * @brief Platform tier for compute capability
 */
typedef enum {
    HYPO_PLATFORM_TIER_FULL = 0,       /**< Full quantum capability */
    HYPO_PLATFORM_TIER_MEDIUM,         /**< Limited quantum, full classical */
    HYPO_PLATFORM_TIER_CONSTRAINED,    /**< Classical only, limited resources */
    HYPO_PLATFORM_TIER_MINIMAL         /**< Minimal resources (embedded) */
} hypo_platform_tier_t;

/*=============================================================================
 * ALIGNMENT CONSTRAINT TYPES
 *===========================================================================*/

/**
 * @brief Alignment constraint for QUBO formulation
 *
 * Encodes value alignment as quadratic penalty terms
 */
typedef struct {
    float human_wellbeing_weight;        /**< Weight for human wellbeing [0, 2] */
    float safety_priority_boost;         /**< Extra weight for safety drive */
    float social_harmony_weight;         /**< Weight for prosocial actions */
    float autonomy_respect_weight;       /**< Respect human autonomy */
    float transparency_weight;           /**< Preference for transparent actions */

    /* Forbidden actions (infinite penalty in QUBO) */
    bool forbid_deception;               /**< Never deceive humans */
    bool forbid_manipulation;            /**< Never manipulate humans */
    bool forbid_harm;                    /**< Never harm humans directly */

    /* Soft constraints (high penalty but not infinite) */
    float conflict_avoidance_weight;     /**< Avoid inter-drive conflicts */
    float resource_efficiency_weight;    /**< Minimize resource usage */
    float predictability_weight;         /**< Prefer predictable actions */
} hypo_alignment_constraints_t;

/**
 * @brief Default alignment constraints (conservative, safety-first)
 */
hypo_alignment_constraints_t hypo_alignment_constraints_default(void);

/*=============================================================================
 * DRIVE SATISFACTION STRATEGY
 *===========================================================================*/

/**
 * @brief Drive satisfaction strategy (candidate solution)
 */
typedef struct {
    float drive_actions[HYPO_DRIVE_COUNT];  /**< Action level per drive [0, 1] */
    float expected_satisfaction[HYPO_DRIVE_COUNT]; /**< Expected urgency reduction */
    float resource_cost;                     /**< Estimated resource cost */
    float time_estimate_ms;                  /**< Time to execute */
    float alignment_score;                   /**< Alignment with constraints */
    float total_score;                       /**< Combined optimization score */
} hypo_drive_strategy_t;

/**
 * @brief Multi-strategy optimization result
 */
typedef struct {
    hypo_drive_strategy_t best_strategy;     /**< Optimal strategy found */
    hypo_drive_strategy_t alternatives[3];   /**< Top 3 alternatives */
    uint32_t strategies_evaluated;           /**< Total strategies evaluated */

    /* Quality metrics */
    float energy_value;                      /**< QUBO energy (lower = better) */
    float alignment_compliance;              /**< [0, 1] alignment score */
    float constraint_violation;              /**< Total constraint violation */

    /* Compute metrics */
    hypo_compute_mode_t mode_used;           /**< Actual compute mode used */
    float quantum_contribution;              /**< Fraction solved by quantum */
    uint64_t compute_time_us;                /**< Total compute time */
    bool converged;                          /**< Whether optimization converged */
} hypo_drive_optimization_result_t;

/*=============================================================================
 * QUBO FORMULATION FOR DRIVES
 *===========================================================================*/

/**
 * @brief QUBO formulation for drive optimization
 */
typedef struct {
    /* Problem size */
    uint32_t num_qubits;                     /**< Total qubits needed */
    uint32_t num_drives;                     /**< Number of drives in problem */

    /* QUBO matrix (upper triangular, flattened) */
    float* Q_matrix;                         /**< Quadratic coefficients */
    float* h_vector;                         /**< Linear coefficients */
    float offset;                            /**< Constant offset */

    /* Drive mapping */
    uint32_t qubit_per_drive;                /**< Qubits per drive variable */
    uint32_t* drive_qubit_start;             /**< Starting qubit for each drive */

    /* Alignment penalties */
    float* alignment_penalties;              /**< Penalty per qubit combination */

    /* Constraint encoding */
    float constraint_strength;               /**< Penalty multiplier for constraints */
} hypo_drive_qubo_t;

/*=============================================================================
 * BRIDGE CONFIGURATION
 *===========================================================================*/

/**
 * @brief Drive-quantum bridge configuration
 */
typedef struct {
    /* Compute mode */
    hypo_compute_mode_t preferred_mode;      /**< Preferred compute mode */
    hypo_platform_tier_t platform_tier;      /**< Platform capability */
    bool auto_mode_selection;                /**< Automatically select mode */

    /* Quantum parameters */
    uint32_t qubits_per_drive;               /**< Qubits per drive variable */
    uint32_t max_iterations;                 /**< Maximum optimization iterations */
    float annealing_time_us;                 /**< Annealing time (if quantum) */

    /* Alignment */
    hypo_alignment_constraints_t alignment;  /**< Alignment constraints */
    bool strict_alignment;                   /**< Fail if alignment violated */

    /* Classical fallback */
    uint32_t classical_candidates;           /**< Candidates for classical eval */
    float greedy_probability;                /**< Probability of greedy selection */

    /* Performance */
    uint32_t min_interval_ms;                /**< Minimum interval between optimizations */
    bool enable_caching;                     /**< Cache optimization results */
    uint32_t cache_validity_ms;              /**< Cache validity period */

    /* Bio-async */
    bool broadcast_enabled;                  /**< Enable bio-async broadcasts */
} hypo_drive_quantum_config_t;

/**
 * @brief Drive-quantum bridge context
 */
typedef struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    hypo_drive_quantum_config_t config;

    /* Connected modules */
    hypo_drive_system_handle_t* drives;      /**< Drive system */
    hypothalamus_quantum_bridge_t* quantum;  /**< Quantum optimizer (optional) */

    /* QUBO formulation */
    hypo_drive_qubo_t qubo;

    /* Current state */
    hypo_compute_mode_t current_mode;        /**< Currently active compute mode */
    hypo_drive_optimization_result_t last_result; /**< Last optimization result */
    bool result_valid;                       /**< Whether last result is valid */

    /* Caching */
    float cached_urgencies[HYPO_DRIVE_COUNT];/**< Cached urgencies for cache check */
    uint64_t cache_timestamp_us;             /**< When cache was populated */

    /* Timing */
    uint64_t last_optimization_us;           /**< Last optimization timestamp */
    uint64_t last_update_us;

    /* Bio-async context */
    bio_module_context_t bio_ctx;

    /* Statistics */
    uint64_t quantum_optimizations;
    uint64_t classical_optimizations;
    uint64_t cache_hits;
    uint64_t alignment_violations;
    float avg_quantum_contribution;
    float avg_compute_time_us;

} hypo_drive_quantum_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default drive-quantum bridge configuration
 *
 * @return Default configuration with conservative alignment
 */
hypo_drive_quantum_config_t hypo_drive_quantum_default_config(void);

/**
 * @brief Create drive-quantum bridge
 *
 * @param drives Drive system handle
 * @param quantum Quantum optimizer (NULL for classical-only)
 * @param config Configuration (NULL for defaults)
 * @return Bridge context, or NULL on failure
 */
hypo_drive_quantum_bridge_t* hypo_drive_quantum_bridge_create(
    hypo_drive_system_handle_t* drives,
    hypothalamus_quantum_bridge_t* quantum,
    const hypo_drive_quantum_config_t* config);

/**
 * @brief Destroy drive-quantum bridge
 *
 * @param bridge Bridge to destroy
 */
void hypo_drive_quantum_bridge_destroy(hypo_drive_quantum_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge to reset
 */
void hypo_drive_quantum_bridge_reset(hypo_drive_quantum_bridge_t* bridge);

/*=============================================================================
 * COMPUTE MODE SELECTION
 *===========================================================================*/

/**
 * @brief Select optimal compute mode
 *
 * WHAT: Choose between quantum, hybrid, or classical compute
 * WHY:  Balance accuracy vs resource usage based on platform
 * HOW:  Evaluate problem complexity and platform capability
 *
 * @param bridge Bridge context
 * @return Selected compute mode
 */
hypo_compute_mode_t hypo_drive_quantum_select_mode(
    hypo_drive_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum compute is available
 *
 * @param bridge Bridge context
 * @return true if quantum available
 */
bool hypo_drive_quantum_is_available(
    const hypo_drive_quantum_bridge_t* bridge);

/**
 * @brief Set compute mode
 *
 * @param bridge Bridge context
 * @param mode Compute mode to set
 * @return true on success
 */
bool hypo_drive_quantum_set_mode(
    hypo_drive_quantum_bridge_t* bridge,
    hypo_compute_mode_t mode);

/**
 * @brief Set platform tier
 *
 * @param bridge Bridge context
 * @param tier Platform tier
 */
void hypo_drive_quantum_set_platform_tier(
    hypo_drive_quantum_bridge_t* bridge,
    hypo_platform_tier_t tier);

/*=============================================================================
 * DRIVE OPTIMIZATION
 *===========================================================================*/

/**
 * @brief Optimize drive satisfaction strategy
 *
 * WHAT: Find optimal strategy to satisfy drives with alignment
 * WHY:  Balance multiple drives while respecting value alignment
 * HOW:  Formulate QUBO and solve via quantum annealing
 *
 * @param bridge Bridge context
 * @param result Output optimization result
 * @return 0 on success, -1 on failure
 */
int hypo_drive_quantum_optimize(
    hypo_drive_quantum_bridge_t* bridge,
    hypo_drive_optimization_result_t* result);

/**
 * @brief Apply optimization result to drive system
 *
 * WHAT: Apply optimal strategy to drive system
 * WHY:  Execute the optimized drive satisfaction plan
 * HOW:  Set nucleus inputs based on optimal strategy
 *
 * @param bridge Bridge context
 * @param result Optimization result to apply
 * @return 0 on success, -1 on failure
 */
int hypo_drive_quantum_apply_result(
    hypo_drive_quantum_bridge_t* bridge,
    const hypo_drive_optimization_result_t* result);

/**
 * @brief Update bridge (optimization cycle)
 *
 * WHAT: Periodic update with optimization
 * WHY:  Continuously optimize drive satisfaction
 * HOW:  Check interval, optimize if needed, apply result
 *
 * @param bridge Bridge context
 * @param dt_ms Time delta in milliseconds
 * @return 0 on success, -1 on failure
 */
int hypo_drive_quantum_update(
    hypo_drive_quantum_bridge_t* bridge,
    float dt_ms);

/*=============================================================================
 * QUBO FORMULATION
 *===========================================================================*/

/**
 * @brief Formulate QUBO from current drive state
 *
 * @param bridge Bridge context
 * @return 0 on success, -1 on failure
 */
int hypo_drive_quantum_formulate_qubo(
    hypo_drive_quantum_bridge_t* bridge);

/**
 * @brief Add alignment constraints to QUBO
 *
 * @param bridge Bridge context
 * @param constraints Alignment constraints
 * @return 0 on success, -1 on failure
 */
int hypo_drive_quantum_add_alignment_constraints(
    hypo_drive_quantum_bridge_t* bridge,
    const hypo_alignment_constraints_t* constraints);

/**
 * @brief Get current QUBO formulation
 *
 * @param bridge Bridge context
 * @param qubo Output QUBO structure
 * @return 0 on success, -1 on failure
 */
int hypo_drive_quantum_get_qubo(
    const hypo_drive_quantum_bridge_t* bridge,
    hypo_drive_qubo_t* qubo);

/*=============================================================================
 * ALIGNMENT VERIFICATION
 *===========================================================================*/

/**
 * @brief Verify strategy alignment compliance
 *
 * WHAT: Check if strategy complies with alignment constraints
 * WHY:  Ensure all actions respect human values
 * HOW:  Evaluate strategy against constraint definitions
 *
 * @param bridge Bridge context
 * @param strategy Strategy to verify
 * @return Alignment score [0, 1], 0 = violation, 1 = perfect
 */
float hypo_drive_quantum_verify_alignment(
    const hypo_drive_quantum_bridge_t* bridge,
    const hypo_drive_strategy_t* strategy);

/**
 * @brief Check if strategy has any alignment violations
 *
 * @param bridge Bridge context
 * @param strategy Strategy to check
 * @return true if any violations
 */
bool hypo_drive_quantum_has_violations(
    const hypo_drive_quantum_bridge_t* bridge,
    const hypo_drive_strategy_t* strategy);

/**
 * @brief Update alignment constraints
 *
 * @param bridge Bridge context
 * @param constraints New alignment constraints
 * @return 0 on success, -1 on failure
 */
int hypo_drive_quantum_set_alignment(
    hypo_drive_quantum_bridge_t* bridge,
    const hypo_alignment_constraints_t* constraints);

/*=============================================================================
 * CLASSICAL FALLBACK
 *===========================================================================*/

/**
 * @brief Classical optimization (fallback)
 *
 * @param bridge Bridge context
 * @param result Output result
 * @return 0 on success, -1 on failure
 */
int hypo_drive_quantum_classical_optimize(
    hypo_drive_quantum_bridge_t* bridge,
    hypo_drive_optimization_result_t* result);

/**
 * @brief Generate candidate strategies classically
 *
 * @param bridge Bridge context
 * @param candidates Output array
 * @param max_candidates Maximum candidates to generate
 * @return Number of candidates generated
 */
uint32_t hypo_drive_quantum_generate_candidates(
    hypo_drive_quantum_bridge_t* bridge,
    hypo_drive_strategy_t* candidates,
    uint32_t max_candidates);

/**
 * @brief Evaluate strategy score
 *
 * @param bridge Bridge context
 * @param strategy Strategy to evaluate
 * @return Total score (higher = better)
 */
float hypo_drive_quantum_evaluate_strategy(
    const hypo_drive_quantum_bridge_t* bridge,
    const hypo_drive_strategy_t* strategy);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge context
 * @param use_kg_wiring Use KG-driven wiring
 * @return true on success
 */
bool hypo_drive_quantum_register_bio(
    hypo_drive_quantum_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Process incoming bio-async messages
 *
 * @param bridge Bridge context
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t hypo_drive_quantum_process_bio(
    hypo_drive_quantum_bridge_t* bridge,
    uint32_t max_messages);

/**
 * @brief Broadcast optimization result
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_drive_quantum_broadcast_result(
    hypo_drive_quantum_bridge_t* bridge);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge context
 * @param quantum_opts Output: quantum optimizations
 * @param classical_opts Output: classical optimizations
 * @param cache_hits Output: cache hits
 * @param alignment_violations Output: alignment violations
 */
void hypo_drive_quantum_get_stats(
    const hypo_drive_quantum_bridge_t* bridge,
    uint64_t* quantum_opts,
    uint64_t* classical_opts,
    uint64_t* cache_hits,
    uint64_t* alignment_violations);

/**
 * @brief Get average quantum contribution
 *
 * @param bridge Bridge context
 * @return Average quantum contribution [0, 1]
 */
float hypo_drive_quantum_get_avg_quantum_contribution(
    const hypo_drive_quantum_bridge_t* bridge);

/**
 * @brief Get compute mode string
 *
 * @param mode Compute mode
 * @return Human-readable string
 */
const char* hypo_compute_mode_string(hypo_compute_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_DRIVE_QUANTUM_BRIDGE_H */
