/**
 * @file nimcp_energy_consistency.h
 * @brief Energy-Based Logical Consistency System
 *
 * Implements an energy-based approach to logical consistency checking where
 * E=0 means logically consistent. Inspired by the Mathesis paper's approach
 * to neuro-symbolic integration.
 *
 * Key Concepts:
 * - Total energy is sum of logical, mathematical, and thermodynamic costs
 * - Consistency score = 1/(1+E), so E=0 gives perfect consistency
 * - Violations are classified by type and severity
 * - Integrates with FEP as prediction error
 * - Integrates with brain immune system for recovery
 *
 * Biological Basis:
 * - Neural consistency checking in prefrontal cortex
 * - Error detection in anterior cingulate cortex (ACC)
 * - Conflict monitoring and resolution
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_ENERGY_CONSISTENCY_H
#define NIMCP_ENERGY_CONSISTENCY_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_async.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Constants
 * ============================================================================ */

/** Bio-async module identifier for energy consistency */
#define BIO_MODULE_ENERGY_CONSISTENCY   0x0393

/** Maximum number of violations to track */
#define ENERGY_CONSISTENCY_MAX_VIOLATIONS   64

/** Maximum proof trace steps for consistency checking */
#define ENERGY_CONSISTENCY_MAX_PROOF_STEPS  256

/** Default energy weights */
#define ENERGY_WEIGHT_LOGICAL_DEFAULT       1.0f
#define ENERGY_WEIGHT_MATHEMATICAL_DEFAULT  1.0f
#define ENERGY_WEIGHT_THERMODYNAMIC_DEFAULT 0.1f

/** Landauer limit at room temperature (kT ln 2) in joules */
#define LANDAUER_LIMIT_JOULES               2.87e-21

/** ATP cost per bit operation (approximate) */
#define ATP_COST_PER_BIT                    1e-20

/* ============================================================================
 * Violation Types
 * ============================================================================ */

/**
 * @brief Types of logical/mathematical consistency violations
 */
typedef enum {
    CONSISTENCY_CONTRADICTION = 0,       /**< P ∧ ¬P contradiction */
    CONSISTENCY_UNSATISFIED_RULE,        /**< Premises true, conclusion false */
    CONSISTENCY_CIRCULARITY,             /**< Circular proof dependency */
    CONSISTENCY_AXIOM_VIOLATION,         /**< Violates mathematical axiom */
    CONSISTENCY_TYPE_MISMATCH,           /**< Type system violation */
    CONSISTENCY_DOMAIN_ERROR,            /**< Operation outside valid domain */
    CONSISTENCY_UNDEFINED_REFERENCE,     /**< Reference to undefined entity */
    CONSISTENCY_ARITY_MISMATCH,          /**< Wrong number of arguments */
    CONSISTENCY_SCOPE_VIOLATION,         /**< Variable scope error */
    CONSISTENCY_SEMANTIC_ERROR,          /**< Semantic inconsistency */
    CONSISTENCY_TYPE_COUNT               /**< Total violation types */
} consistency_violation_type_t;

/**
 * @brief Severity levels for violations
 */
typedef enum {
    VIOLATION_SEVERITY_INFO = 0,         /**< Informational, no energy cost */
    VIOLATION_SEVERITY_WARNING,          /**< Minor inconsistency */
    VIOLATION_SEVERITY_ERROR,            /**< Significant inconsistency */
    VIOLATION_SEVERITY_CRITICAL,         /**< Severe logical failure */
    VIOLATION_SEVERITY_FATAL             /**< Unrecoverable inconsistency */
} violation_severity_t;

/* ============================================================================
 * Violation Structure
 * ============================================================================ */

/**
 * @brief Represents a single consistency violation
 */
typedef struct consistency_violation {
    consistency_violation_type_t type;   /**< Type of violation */
    violation_severity_t severity;       /**< Severity level */
    float energy_cost;                   /**< Energy contribution */
    uint32_t source_line;                /**< Source line if available */
    uint32_t node_id;                    /**< Related node ID if any */
    char description[256];               /**< Human-readable description */
    uint64_t timestamp_us;               /**< Detection timestamp */
} consistency_violation_t;

/* ============================================================================
 * Proof Trace for Consistency Checking
 * ============================================================================ */

/**
 * @brief Types of proof steps
 */
typedef enum {
    PROOF_STEP_AXIOM = 0,                /**< Assert axiom */
    PROOF_STEP_HYPOTHESIS,               /**< Introduce hypothesis */
    PROOF_STEP_INFERENCE,                /**< Apply inference rule */
    PROOF_STEP_SUBSTITUTION,             /**< Variable substitution */
    PROOF_STEP_UNIFICATION,              /**< Unification step */
    PROOF_STEP_REWRITE,                  /**< Term rewriting */
    PROOF_STEP_LEMMA,                    /**< Apply lemma */
    PROOF_STEP_DISCHARGE,                /**< Discharge hypothesis */
    PROOF_STEP_QED                       /**< Proof complete */
} proof_step_type_t;

/**
 * @brief Single proof step for consistency verification
 */
typedef struct proof_step {
    proof_step_type_t type;              /**< Step type */
    uint32_t step_id;                    /**< Step identifier */
    uint32_t* premises;                  /**< Premise step IDs */
    uint32_t premise_count;              /**< Number of premises */
    char rule_name[64];                  /**< Applied rule name */
    char conclusion[256];                /**< Resulting proposition */
    float confidence;                    /**< Step confidence [0,1] */
    bool is_valid;                       /**< Validation result */
} proof_step_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Complete consistency check result
 */
typedef struct energy_consistency_result {
    /* Energy components */
    float total_energy;                  /**< Total energy (E=0 means consistent) */
    float logical_energy;                /**< From logical violations */
    float mathematical_energy;           /**< From math constraint violations */
    float thermodynamic_cost;            /**< ATP cost of checking */
    float landauer_cost;                 /**< Minimum bit erasure cost */

    /* Derived metrics */
    float final_consistency;             /**< 1/(1+total_energy) */
    float normalized_energy;             /**< Energy normalized to [0,1] */

    /* Violation details */
    uint32_t num_violations;             /**< Number of violations found */
    consistency_violation_t* violations; /**< Array of violations */
    uint32_t violation_capacity;         /**< Capacity of violations array */

    /* Proof verification */
    uint32_t proof_steps_checked;        /**< Number of proof steps verified */
    uint32_t invalid_steps;              /**< Number of invalid steps found */

    /* Timing */
    uint64_t check_duration_us;          /**< Time to perform check */
    uint64_t timestamp_us;               /**< Result timestamp */
} energy_consistency_result_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Configuration for energy consistency checker
 */
typedef struct energy_consistency_config {
    /* Energy weights */
    float logical_weight;                /**< Weight for logical violations */
    float mathematical_weight;           /**< Weight for math violations */
    float thermodynamic_weight;          /**< Weight for thermodynamic cost */

    /* Severity multipliers */
    float severity_multipliers[5];       /**< Per-severity energy multiplier */

    /* Checking parameters */
    uint32_t max_violations;             /**< Max violations to track */
    uint32_t max_proof_depth;            /**< Max proof depth to verify */
    bool strict_type_checking;           /**< Enable strict type checks */
    bool track_thermodynamic_cost;       /**< Track ATP/Landauer costs */

    /* Integration settings */
    bool enable_fep_integration;         /**< Feed energy to FEP */
    bool enable_immune_integration;      /**< Report to immune system */
    bool enable_bio_async;               /**< Enable async messaging */

    /* Modulation */
    float inflammation_sensitivity;      /**< Sensitivity to inflammation */
    float fatigue_sensitivity;           /**< Sensitivity to fatigue */
    float atp_sensitivity;               /**< Sensitivity to ATP levels */
} energy_consistency_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Statistics for energy consistency checker
 */
typedef struct energy_consistency_stats {
    uint64_t total_checks;               /**< Total checks performed */
    uint64_t consistent_count;           /**< Fully consistent results */
    uint64_t inconsistent_count;         /**< Results with violations */

    /* Violation counts by type */
    uint64_t violation_counts[CONSISTENCY_TYPE_COUNT];

    /* Energy statistics */
    float min_energy;                    /**< Minimum energy observed */
    float max_energy;                    /**< Maximum energy observed */
    float avg_energy;                    /**< Running average energy */
    double total_energy_sum;             /**< Sum for average computation */

    /* Proof statistics */
    uint64_t proof_steps_verified;       /**< Total proof steps verified */
    uint64_t invalid_proofs;             /**< Proofs with invalid steps */

    /* Timing */
    uint64_t total_check_time_us;        /**< Total time spent checking */
    float avg_check_time_us;             /**< Average check time */
} energy_consistency_stats_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Opaque handle to energy consistency checker
 */
typedef struct energy_consistency_checker energy_consistency_checker_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create energy consistency checker with configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Checker handle or NULL on failure
 */
NIMCP_API energy_consistency_checker_t* energy_consistency_create(
    const energy_consistency_config_t* config);

/**
 * @brief Destroy energy consistency checker
 *
 * @param checker Checker to destroy
 */
NIMCP_API void energy_consistency_destroy(
    energy_consistency_checker_t* checker);

/**
 * @brief Reset checker state and statistics
 *
 * @param checker Checker to reset
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_reset(
    energy_consistency_checker_t* checker);

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_get_default_config(
    energy_consistency_config_t* config);

/* ============================================================================
 * Core Checking Functions
 * ============================================================================ */

/**
 * @brief Check consistency of a logical knowledge base
 *
 * Examines the knowledge base for logical contradictions, unsatisfied
 * rules, type mismatches, and other inconsistencies. Computes total
 * energy where E=0 means fully consistent.
 *
 * @param checker The consistency checker
 * @param logic Pointer to logic/knowledge base (nimcp_logic_t* or similar)
 * @param result Output result structure
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_check(
    energy_consistency_checker_t* checker,
    const void* logic,
    energy_consistency_result_t* result);

/**
 * @brief Check consistency of a proof trace
 *
 * Verifies each step in a proof trace for logical validity.
 * Checks that premises exist, rules are applied correctly,
 * and no circular dependencies exist.
 *
 * @param checker The consistency checker
 * @param proof_trace Array of proof steps
 * @param num_steps Number of steps in trace
 * @param result Output result structure
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_check_proof(
    energy_consistency_checker_t* checker,
    const proof_step_t* proof_trace,
    uint32_t num_steps,
    energy_consistency_result_t* result);

/**
 * @brief Check consistency of a single proposition
 *
 * @param checker The consistency checker
 * @param proposition Proposition string
 * @param context Optional context/knowledge base
 * @param result Output result structure
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_check_proposition(
    energy_consistency_checker_t* checker,
    const char* proposition,
    const void* context,
    energy_consistency_result_t* result);

/**
 * @brief Check consistency between two expressions
 *
 * Determines if two expressions are consistent with each other
 * (can both be true simultaneously).
 *
 * @param checker The consistency checker
 * @param expr_a First expression
 * @param expr_b Second expression
 * @param result Output result structure
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_check_pair(
    energy_consistency_checker_t* checker,
    const char* expr_a,
    const char* expr_b,
    energy_consistency_result_t* result);

/* ============================================================================
 * Energy Computation
 * ============================================================================ */

/**
 * @brief Get current consistency score (1/(1+E))
 *
 * @param checker The consistency checker
 * @return Consistency score in [0,1], or -1 on error
 */
NIMCP_API float energy_consistency_get_score(
    const energy_consistency_checker_t* checker);

/**
 * @brief Compute energy for a specific violation
 *
 * @param checker The consistency checker
 * @param violation The violation to evaluate
 * @return Energy contribution
 */
NIMCP_API float energy_consistency_compute_violation_energy(
    const energy_consistency_checker_t* checker,
    const consistency_violation_t* violation);

/**
 * @brief Compute thermodynamic cost of reasoning
 *
 * Estimates ATP cost and Landauer limit based on
 * bits processed during consistency checking.
 *
 * @param checker The consistency checker
 * @param bits_processed Number of bits processed
 * @param atp_cost Output ATP cost
 * @param landauer_cost Output Landauer limit
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_compute_thermo_cost(
    const energy_consistency_checker_t* checker,
    uint64_t bits_processed,
    float* atp_cost,
    float* landauer_cost);

/* ============================================================================
 * FEP Integration
 * ============================================================================ */

/**
 * @brief Feed consistency energy to FEP system as prediction error
 *
 * Maps consistency violations to FEP prediction error for
 * active inference and belief updating.
 *
 * @param checker The consistency checker
 * @param fep FEP system handle
 * @param energy Energy value to feed
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_feed_to_fep(
    energy_consistency_checker_t* checker,
    void* fep,
    float energy);

/**
 * @brief Get precision-weighted energy from FEP
 *
 * Uses FEP precision weighting to modulate violation energy.
 *
 * @param checker The consistency checker
 * @param fep FEP system handle
 * @param violation The violation to weight
 * @return Precision-weighted energy
 */
NIMCP_API float energy_consistency_get_precision_weighted(
    const energy_consistency_checker_t* checker,
    const void* fep,
    const consistency_violation_t* violation);

/* ============================================================================
 * Immune System Integration
 * ============================================================================ */

/**
 * @brief Report violation to brain immune system
 *
 * Presents consistency violation as an antigen for immune
 * system processing and potential recovery action.
 *
 * @param checker The consistency checker
 * @param violation Violation to report
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_report_to_immune(
    energy_consistency_checker_t* checker,
    const consistency_violation_t* violation);

/**
 * @brief Request recovery for critical violation
 *
 * Triggers immune system recovery workflow for severe violations.
 *
 * @param checker The consistency checker
 * @param result Result containing violations
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_request_recovery(
    energy_consistency_checker_t* checker,
    const energy_consistency_result_t* result);

/* ============================================================================
 * Result Management
 * ============================================================================ */

/**
 * @brief Initialize result structure
 *
 * @param result Result to initialize
 * @param max_violations Maximum violations to allocate
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_result_init(
    energy_consistency_result_t* result,
    uint32_t max_violations);

/**
 * @brief Clean up result structure
 *
 * @param result Result to clean up
 */
NIMCP_API void energy_consistency_result_cleanup(
    energy_consistency_result_t* result);

/**
 * @brief Add violation to result
 *
 * @param result Result to add to
 * @param violation Violation to add
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_result_add_violation(
    energy_consistency_result_t* result,
    const consistency_violation_t* violation);

/* ============================================================================
 * Modulation
 * ============================================================================ */

/**
 * @brief Apply inflammation modulation
 *
 * High inflammation increases sensitivity to violations.
 *
 * @param checker The consistency checker
 * @param inflammation Inflammation level [0,1]
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_modulate_inflammation(
    energy_consistency_checker_t* checker,
    float inflammation);

/**
 * @brief Apply fatigue modulation
 *
 * High fatigue reduces checking precision.
 *
 * @param checker The consistency checker
 * @param fatigue Fatigue level [0,1]
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_modulate_fatigue(
    energy_consistency_checker_t* checker,
    float fatigue);

/**
 * @brief Apply ATP level modulation
 *
 * Low ATP limits thermodynamic budget for checking.
 *
 * @param checker The consistency checker
 * @param atp_level ATP level [0,1]
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_modulate_atp(
    energy_consistency_checker_t* checker,
    float atp_level);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Register with bio-async router
 *
 * Registers this module with the global bio-async router if available.
 *
 * @param checker The consistency checker
 * @return NIMCP_SUCCESS on success
 */
NIMCP_API nimcp_error_t energy_consistency_register_bio_async(
    energy_consistency_checker_t* checker);

/**
 * @brief Unregister from bio-async router
 *
 * @param checker The consistency checker
 * @return NIMCP_SUCCESS on success
 */
NIMCP_API nimcp_error_t energy_consistency_unregister_bio_async(
    energy_consistency_checker_t* checker);

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param checker The consistency checker
 * @param stats Output statistics
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_consistency_get_stats(
    const energy_consistency_checker_t* checker,
    energy_consistency_stats_t* stats);

/**
 * @brief Get last result
 *
 * @param checker The consistency checker
 * @return Pointer to last result, or NULL
 */
NIMCP_API const energy_consistency_result_t* energy_consistency_get_last_result(
    const energy_consistency_checker_t* checker);

/**
 * @brief Print diagnostic information
 *
 * @param checker The consistency checker
 */
NIMCP_API void energy_consistency_print_diagnostics(
    const energy_consistency_checker_t* checker);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENERGY_CONSISTENCY_H */
