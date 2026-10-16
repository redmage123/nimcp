/**
 * @file nimcp_safety_verification.h
 * @brief Formal Safety Verification Module
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Formal verification of safety rule consistency and completeness
 * WHY:  Prove safety properties hold, no bypass paths exist
 * HOW:  SAT solver for consistency, coverage analysis for completeness
 *
 * VERIFICATION CHECKS:
 * - CONSISTENCY: No contradictory rules
 * - COMPLETENESS: All action types covered
 * - TERMINATION: Evaluation always terminates
 * - MONOTONICITY: Adding rules doesn't weaken safety
 * - PRIORITY_RESPECT: Higher priority rules always win
 * - NO_BYPASS: No action can bypass all safety checks
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SAFETY_VERIFICATION_H
#define NIMCP_SAFETY_VERIFICATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/error/nimcp_error_codes.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct sat_solver;
typedef struct sat_solver sat_solver_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Safety verification magic number */
#define SAFETY_VERIFICATION_MAGIC           0x56455249  /* "VERI" */

/** @brief Maximum verification results */
#define SAFETY_MAX_VERIFICATION_RESULTS     32

/** @brief Maximum counterexample length */
#define SAFETY_COUNTEREXAMPLE_MAX_LENGTH    512

/** @brief Maximum rules to verify */
#define SAFETY_MAX_RULES                    1024

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Verification check types
 */
typedef enum verification_check {
    VERIFY_CONSISTENCY = 0,     /**< No contradictory rules */
    VERIFY_COMPLETENESS,        /**< All action types covered */
    VERIFY_TERMINATION,         /**< Evaluation always terminates */
    VERIFY_MONOTONICITY,        /**< Adding rules doesn't weaken safety */
    VERIFY_PRIORITY_RESPECT,    /**< Higher priority rules always win */
    VERIFY_NO_BYPASS,           /**< No action can bypass all safety checks */
    VERIFY_COUNT
} verification_check_t;

/**
 * @brief Single verification result
 */
typedef struct verification_result {
    bool passed;
    verification_check_t check_type;
    char counterexample[SAFETY_COUNTEREXAMPLE_MAX_LENGTH];
    float coverage_percentage;
    uint64_t verification_time_ms;
    uint32_t rules_checked;
    uint32_t violations_found;
} verification_result_t;

/**
 * @brief Complete verification report
 */
typedef struct verification_report {
    verification_result_t results[VERIFY_COUNT];
    uint32_t result_count;
    bool all_passed;
    float overall_coverage;
    uint64_t total_time_ms;
    char summary[1024];
} verification_report_t;

/**
 * @brief Safety rule (abstract representation)
 */
typedef struct safety_rule {
    uint32_t rule_id;
    char name[64];
    uint32_t priority;              /**< Higher = more important */
    bool is_blocking;               /**< Blocks action if triggered */
    bool is_mandatory;              /**< Must be satisfied */
    char condition[256];            /**< Rule condition expression */
} safety_rule_t;

/**
 * @brief Safety verification configuration
 */
typedef struct safety_verification_config {
    float timeout_per_check_ms;
    bool enable_all_checks;
    bool continue_on_failure;
    bool generate_counterexamples;
    uint32_t max_counterexamples;
    bool verify_incrementally;
} safety_verification_config_t;

/**
 * @brief Safety verification statistics
 */
typedef struct safety_verification_stats {
    uint64_t total_verifications;
    uint64_t consistency_checks;
    uint64_t completeness_checks;
    uint64_t no_bypass_checks;
    uint64_t checks_passed;
    uint64_t checks_failed;
    float avg_verification_time_ms;
} safety_verification_stats_t;

/**
 * @brief Safety verification system (opaque)
 */
typedef struct safety_verification safety_verification_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default verification configuration
 */
NIMCP_EXPORT safety_verification_config_t safety_verification_default_config(void);

/**
 * @brief Create safety verification system
 */
NIMCP_EXPORT safety_verification_t* safety_verification_create(
    const safety_verification_config_t* config
);

/**
 * @brief Destroy safety verification system
 */
NIMCP_EXPORT void safety_verification_destroy(safety_verification_t* system);

/* ============================================================================
 * Verification API
 * ============================================================================ */

/**
 * @brief Run full verification suite
 *
 * @param system Verification system handle
 * @param sat SAT solver instance
 * @param rules Safety rules to verify
 * @param rule_count Number of rules
 * @param report Output: verification report
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t safety_verification_run_suite(
    safety_verification_t* system,
    sat_solver_t* sat,
    const safety_rule_t* rules,
    size_t rule_count,
    verification_report_t* report
);

/**
 * @brief Verify rule consistency (no contradictions)
 */
NIMCP_EXPORT nimcp_error_t safety_verify_consistency(
    safety_verification_t* system,
    sat_solver_t* sat,
    const safety_rule_t* rules,
    size_t rule_count,
    verification_result_t* result
);

/**
 * @brief Verify rule completeness (all actions covered)
 */
NIMCP_EXPORT nimcp_error_t safety_verify_completeness(
    safety_verification_t* system,
    const safety_rule_t* rules,
    size_t rule_count,
    verification_result_t* result
);

/**
 * @brief Verify no bypass path exists
 */
NIMCP_EXPORT nimcp_error_t safety_verify_no_bypass(
    safety_verification_t* system,
    sat_solver_t* sat,
    const safety_rule_t* rules,
    size_t rule_count,
    verification_result_t* result
);

/**
 * @brief Verify priority ordering is respected
 */
NIMCP_EXPORT nimcp_error_t safety_verify_priority_respect(
    safety_verification_t* system,
    const safety_rule_t* rules,
    size_t rule_count,
    verification_result_t* result
);

/* ============================================================================
 * Status API
 * ============================================================================ */

/**
 * @brief Get verification statistics
 */
NIMCP_EXPORT nimcp_error_t safety_verification_get_stats(
    const safety_verification_t* system,
    safety_verification_stats_t* stats
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async messaging
 */
NIMCP_EXPORT nimcp_error_t safety_verification_connect_bio_async(
    safety_verification_t* system
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get check type name
 */
NIMCP_EXPORT const char* safety_verification_check_name(verification_check_t check);

/**
 * @brief Format verification report as text
 */
NIMCP_EXPORT size_t safety_verification_format_report(
    const verification_report_t* report,
    char* buffer,
    size_t buffer_size
);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for heartbeat reporting
 *
 * @param agent Health agent handle from brain init
 */
struct nimcp_health_agent;
NIMCP_EXPORT void safety_verification_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SAFETY_VERIFICATION_H */
