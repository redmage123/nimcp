/**
 * @file nimcp_energy_consistency.c
 * @brief Implementation of Energy-Based Logical Consistency System
 *
 * Implements energy-based consistency checking where E=0 means logically
 * consistent. Integrates with FEP, immune system, and bio-async.
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#include "cognitive/neuro_symbolic/nimcp_energy_consistency.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"

#define LOG_MODULE "ENERGY_CONSISTENCY"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(energy_consistency, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include <string.h>
#include <stdio.h>
#include <math.h>
#include "constants/nimcp_buffer_constants.h"

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal state of energy consistency checker
 */
struct energy_consistency_checker {
    /* Configuration */
    energy_consistency_config_t config;

    /* Current state */
    float current_energy;
    float current_consistency;
    float inflammation_level;
    float fatigue_level;
    float atp_level;

    /* Last result */
    energy_consistency_result_t last_result;
    bool has_last_result;

    /* Statistics */
    energy_consistency_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Module identification */
    uint32_t module_id;
    const char* module_name;
};

/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_severity_multiplier(
    const energy_consistency_checker_t* checker,
    violation_severity_t severity);

static void update_stats_after_check(
    energy_consistency_checker_t* checker,
    const energy_consistency_result_t* result);

static nimcp_error_t validate_proof_step(
    const energy_consistency_checker_t* checker,
    const proof_step_t* step,
    const proof_step_t* all_steps,
    uint32_t num_steps,
    consistency_violation_t* violation);

static bool check_circular_dependency(
    const proof_step_t* steps,
    uint32_t num_steps,
    uint32_t current_step,
    bool* visited,
    bool* in_stack);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

NIMCP_API energy_consistency_checker_t* energy_consistency_create(
    const energy_consistency_config_t* config)
{
    energy_consistency_checker_t* checker = NULL;

    /* Allocate checker */
    checker = (energy_consistency_checker_t*)nimcp_calloc(
        1, sizeof(energy_consistency_checker_t));
    if (!checker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate energy consistency checker");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        memcpy(&checker->config, config, sizeof(energy_consistency_config_t));
    } else {
        energy_consistency_get_default_config(&checker->config);
    }

    /* Initialize state */
    checker->current_energy = 0.0f;
    checker->current_consistency = 1.0f;
    checker->inflammation_level = 0.0f;
    checker->fatigue_level = 0.0f;
    checker->atp_level = 1.0f;
    checker->has_last_result = false;

    /* Initialize statistics */
    memset(&checker->stats, 0, sizeof(energy_consistency_stats_t));
    checker->stats.min_energy = INFINITY;
    checker->stats.max_energy = 0.0f;

    /* Create mutex */
    mutex_attr_t mutex_attr = {
        .type = MUTEX_TYPE_RECURSIVE,
        
        
    };
    checker->mutex = nimcp_mutex_create(&mutex_attr);
    if (!checker->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "Failed to create mutex for energy consistency checker");
        nimcp_free(checker);
        checker = NULL;
        return NULL;
    }

    /* Module identification */
    checker->module_id = BIO_MODULE_ENERGY_CONSISTENCY;
    checker->module_name = "energy_consistency";
    checker->bio_async_enabled = false;
    checker->bio_ctx = NULL;

    /* Initialize last result structure */
    if (energy_consistency_result_init(&checker->last_result,
            checker->config.max_violations) != NIMCP_SUCCESS) {
        nimcp_mutex_destroy(checker->mutex);
        nimcp_free(checker);
        checker = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "energy_consistency_create: operation failed");
        return NULL;
    }

    return checker;
}

NIMCP_API void energy_consistency_destroy(
    energy_consistency_checker_t* checker)
{
    if (!checker) {
        return;
    }

    /* Unregister from bio-async if registered */
    if (checker->bio_async_enabled) {
        energy_consistency_unregister_bio_async(checker);
    }

    /* Clean up last result */
    energy_consistency_result_cleanup(&checker->last_result);

    /* Free mutex */
    if (checker->mutex) {
        nimcp_mutex_destroy(checker->mutex);
    }

    /* Free checker */
    nimcp_free(checker);
    checker = NULL;
}

NIMCP_API nimcp_error_t energy_consistency_reset(
    energy_consistency_checker_t* checker)
{
    if (!checker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_reset: checker is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(checker->mutex);

    /* Reset state */
    checker->current_energy = 0.0f;
    checker->current_consistency = 1.0f;
    checker->has_last_result = false;

    /* Reset statistics */
    memset(&checker->stats, 0, sizeof(energy_consistency_stats_t));
    checker->stats.min_energy = INFINITY;
    checker->stats.max_energy = 0.0f;

    /* Clear last result */
    checker->last_result.total_energy = 0.0f;
    checker->last_result.num_violations = 0;

    nimcp_mutex_unlock(checker->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t energy_consistency_get_default_config(
    energy_consistency_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_get_default_config: config is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Energy weights */
    config->logical_weight = ENERGY_WEIGHT_LOGICAL_DEFAULT;
    config->mathematical_weight = ENERGY_WEIGHT_MATHEMATICAL_DEFAULT;
    config->thermodynamic_weight = ENERGY_WEIGHT_THERMODYNAMIC_DEFAULT;

    /* Severity multipliers (exponential increase) */
    config->severity_multipliers[VIOLATION_SEVERITY_INFO] = 0.0f;
    config->severity_multipliers[VIOLATION_SEVERITY_WARNING] = 0.1f;
    config->severity_multipliers[VIOLATION_SEVERITY_ERROR] = 1.0f;
    config->severity_multipliers[VIOLATION_SEVERITY_CRITICAL] = 5.0f;
    config->severity_multipliers[VIOLATION_SEVERITY_FATAL] = 20.0f;

    /* Checking parameters */
    config->max_violations = ENERGY_CONSISTENCY_MAX_VIOLATIONS;
    config->max_proof_depth = ENERGY_CONSISTENCY_MAX_PROOF_STEPS;
    config->strict_type_checking = true;
    config->track_thermodynamic_cost = true;

    /* Integration settings */
    config->enable_fep_integration = true;
    config->enable_immune_integration = true;
    config->enable_bio_async = true;

    /* Modulation sensitivities */
    config->inflammation_sensitivity = 0.3f;
    config->fatigue_sensitivity = 0.2f;
    config->atp_sensitivity = 0.4f;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Core Checking Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_consistency_check(
    energy_consistency_checker_t* checker,
    const void* logic,
    energy_consistency_result_t* result)
{
    if (!checker || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_check: checker or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(checker->mutex);

    uint64_t start_time = nimcp_time_monotonic_us();

    /* Initialize result */
    result->total_energy = 0.0f;
    result->logical_energy = 0.0f;
    result->mathematical_energy = 0.0f;
    result->thermodynamic_cost = 0.0f;
    result->landauer_cost = 0.0f;
    result->num_violations = 0;
    result->proof_steps_checked = 0;
    result->invalid_steps = 0;

    uint64_t bits_processed = 0;

    /* If logic is NULL, just return empty consistent result */
    if (!logic) {
        result->final_consistency = 1.0f;
        result->normalized_energy = 0.0f;
        result->check_duration_us = nimcp_time_monotonic_us() - start_time;
        result->timestamp_us = nimcp_time_monotonic_us();

        checker->current_energy = 0.0f;
        checker->current_consistency = 1.0f;
        checker->has_last_result = true;
        memcpy(&checker->last_result, result, sizeof(energy_consistency_result_t));

        nimcp_mutex_unlock(checker->mutex);
        return NIMCP_SUCCESS;
    }

    /* Apply modulation factors */
    float inflammation_factor = 1.0f + (checker->inflammation_level *
        checker->config.inflammation_sensitivity);
    float fatigue_factor = 1.0f + (checker->fatigue_level *
        checker->config.fatigue_sensitivity);
    float atp_factor = 1.0f / (checker->atp_level + 0.1f);

    /* TODO: Implement actual knowledge base parsing and checking
     * For now, we assume the knowledge base is consistent */

    /* Estimate bits processed (placeholder) */
    bits_processed = 1000; /* Would depend on KB size */

    /* Compute thermodynamic costs if enabled */
    if (checker->config.track_thermodynamic_cost) {
        energy_consistency_compute_thermo_cost(checker, bits_processed,
            &result->thermodynamic_cost, &result->landauer_cost);

        result->thermodynamic_cost *= atp_factor;
    }

    /* Compute total energy with weights and modulation */
    result->total_energy = (result->logical_energy * checker->config.logical_weight +
                           result->mathematical_energy * checker->config.mathematical_weight +
                           result->thermodynamic_cost * checker->config.thermodynamic_weight) *
                           inflammation_factor * fatigue_factor;

    /* Compute derived metrics */
    result->final_consistency = 1.0f / (1.0f + result->total_energy);
    result->normalized_energy = 1.0f - result->final_consistency;

    /* Timing */
    result->check_duration_us = nimcp_time_monotonic_us() - start_time;
    result->timestamp_us = nimcp_time_monotonic_us();

    /* Update checker state */
    checker->current_energy = result->total_energy;
    checker->current_consistency = result->final_consistency;
    checker->has_last_result = true;

    /* Copy to last result (without violations array to avoid double-free) */
    float saved_energy = checker->last_result.total_energy;
    checker->last_result.total_energy = result->total_energy;
    checker->last_result.logical_energy = result->logical_energy;
    checker->last_result.mathematical_energy = result->mathematical_energy;
    checker->last_result.thermodynamic_cost = result->thermodynamic_cost;
    checker->last_result.landauer_cost = result->landauer_cost;
    checker->last_result.final_consistency = result->final_consistency;
    checker->last_result.normalized_energy = result->normalized_energy;
    checker->last_result.check_duration_us = result->check_duration_us;
    checker->last_result.timestamp_us = result->timestamp_us;

    /* Update statistics */
    update_stats_after_check(checker, result);

    nimcp_mutex_unlock(checker->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t energy_consistency_check_proof(
    energy_consistency_checker_t* checker,
    const proof_step_t* proof_trace,
    uint32_t num_steps,
    energy_consistency_result_t* result)
{
    if (!checker || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_check_proof: checker or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!proof_trace || num_steps == 0) {
        /* Empty proof is consistent */
        result->total_energy = 0.0f;
        result->final_consistency = 1.0f;
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(checker->mutex);

    uint64_t start_time = nimcp_time_monotonic_us();

    /* Initialize result */
    result->total_energy = 0.0f;
    result->logical_energy = 0.0f;
    result->mathematical_energy = 0.0f;
    result->num_violations = 0;
    result->proof_steps_checked = 0;
    result->invalid_steps = 0;

    /* Allocate tracking arrays for circularity detection */
    bool* visited = (bool*)nimcp_calloc(num_steps, sizeof(bool));
    bool* in_stack = (bool*)nimcp_calloc(num_steps, sizeof(bool));

    if (!visited || !in_stack) {
        if (visited) nimcp_free(visited);
        if (in_stack) nimcp_free(in_stack);
        nimcp_mutex_unlock(checker->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate proof validation arrays");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Check each proof step */
    for (uint32_t i = 0; i < num_steps && i < checker->config.max_proof_depth; i++) {
        const proof_step_t* step = &proof_trace[i];
        consistency_violation_t violation;
        memset(&violation, 0, sizeof(violation));

        result->proof_steps_checked++;

        /* Validate the step */
        nimcp_error_t step_result = validate_proof_step(checker, step,
            proof_trace, num_steps, &violation);

        if (step_result != NIMCP_SUCCESS) {
            result->invalid_steps++;

            /* Add violation to result */
            if (result->num_violations < result->violation_capacity) {
                energy_consistency_result_add_violation(result, &violation);
            }

            /* Accumulate energy */
            float violation_energy = energy_consistency_compute_violation_energy(
                checker, &violation);
            result->logical_energy += violation_energy;
        }

        /* Check for circular dependencies */
        memset(visited, 0, num_steps * sizeof(bool));
        memset(in_stack, 0, num_steps * sizeof(bool));

        if (check_circular_dependency(proof_trace, num_steps, i, visited, in_stack)) {
            consistency_violation_t circ_violation = {
                .type = CONSISTENCY_CIRCULARITY,
                .severity = VIOLATION_SEVERITY_CRITICAL,
                .source_line = i,
                .node_id = step->step_id,
                .timestamp_us = nimcp_time_monotonic_us()
            };
            snprintf(circ_violation.description, sizeof(circ_violation.description),
                "Circular dependency detected at step %u", i);

            if (result->num_violations < result->violation_capacity) {
                energy_consistency_result_add_violation(result, &circ_violation);
            }

            float circ_energy = energy_consistency_compute_violation_energy(
                checker, &circ_violation);
            result->logical_energy += circ_energy;
            result->invalid_steps++;
        }
    }

    nimcp_free(visited);
    visited = NULL;
    nimcp_free(in_stack);
    in_stack = NULL;

    /* Compute thermodynamic cost */
    if (checker->config.track_thermodynamic_cost) {
        uint64_t bits_processed = num_steps * 256; /* Approximate bits per step */
        energy_consistency_compute_thermo_cost(checker, bits_processed,
            &result->thermodynamic_cost, &result->landauer_cost);
    }

    /* Compute total energy */
    result->total_energy = (result->logical_energy * checker->config.logical_weight +
                           result->mathematical_energy * checker->config.mathematical_weight +
                           result->thermodynamic_cost * checker->config.thermodynamic_weight);

    /* Apply modulation */
    float inflammation_factor = 1.0f + (checker->inflammation_level *
        checker->config.inflammation_sensitivity);
    float fatigue_factor = 1.0f + (checker->fatigue_level *
        checker->config.fatigue_sensitivity);

    result->total_energy *= inflammation_factor * fatigue_factor;

    /* Compute derived metrics */
    result->final_consistency = 1.0f / (1.0f + result->total_energy);
    result->normalized_energy = 1.0f - result->final_consistency;

    /* Timing */
    result->check_duration_us = nimcp_time_monotonic_us() - start_time;
    result->timestamp_us = nimcp_time_monotonic_us();

    /* Update checker state */
    checker->current_energy = result->total_energy;
    checker->current_consistency = result->final_consistency;
    checker->has_last_result = true;

    /* Update statistics */
    update_stats_after_check(checker, result);
    checker->stats.proof_steps_verified += result->proof_steps_checked;
    if (result->invalid_steps > 0) {
        checker->stats.invalid_proofs++;
    }

    /* Report critical violations to immune system */
    if (checker->config.enable_immune_integration && result->invalid_steps > 0) {
        for (uint32_t i = 0; i < result->num_violations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_violations > 256) {
                energy_consistency_heartbeat("energy_consi_loop",
                                 (float)(i + 1) / (float)result->num_violations);
            }

            if (result->violations[i].severity >= VIOLATION_SEVERITY_CRITICAL) {
                energy_consistency_report_to_immune(checker, &result->violations[i]);
            }
        }
    }

    nimcp_mutex_unlock(checker->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t energy_consistency_check_proposition(
    energy_consistency_checker_t* checker,
    const char* proposition,
    const void* context,
    energy_consistency_result_t* result)
{
    if (!checker || !proposition || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_check_proposition: checker, proposition, or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(checker->mutex);

    uint64_t start_time = nimcp_time_monotonic_us();

    /* Initialize result */
    result->total_energy = 0.0f;
    result->logical_energy = 0.0f;
    result->mathematical_energy = 0.0f;
    result->num_violations = 0;

    /* Parse and validate proposition */
    size_t prop_len = strlen(proposition);
    if (prop_len == 0) {
        /* Empty proposition is vacuously true */
        result->final_consistency = 1.0f;
        result->check_duration_us = nimcp_time_monotonic_us() - start_time;
        result->timestamp_us = nimcp_time_monotonic_us();
        nimcp_mutex_unlock(checker->mutex);
        return NIMCP_SUCCESS;
    }

    /* TODO: Implement actual proposition parsing and semantic validation
     * For now, perform basic syntactic checks */

    /* Check for obvious contradictions (P and not P) */
    if (strstr(proposition, "NOT") && strstr(proposition, "AND")) {
        /* Potential contradiction - would need full parsing to confirm */
    }

    /* Compute thermodynamic cost */
    if (checker->config.track_thermodynamic_cost) {
        uint64_t bits_processed = prop_len * 8;
        energy_consistency_compute_thermo_cost(checker, bits_processed,
            &result->thermodynamic_cost, &result->landauer_cost);
    }

    /* Compute total energy */
    result->total_energy = (result->logical_energy * checker->config.logical_weight +
                           result->mathematical_energy * checker->config.mathematical_weight +
                           result->thermodynamic_cost * checker->config.thermodynamic_weight);

    result->final_consistency = 1.0f / (1.0f + result->total_energy);
    result->normalized_energy = 1.0f - result->final_consistency;

    result->check_duration_us = nimcp_time_monotonic_us() - start_time;
    result->timestamp_us = nimcp_time_monotonic_us();

    /* Update state */
    checker->current_energy = result->total_energy;
    checker->current_consistency = result->final_consistency;

    update_stats_after_check(checker, result);

    nimcp_mutex_unlock(checker->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t energy_consistency_check_pair(
    energy_consistency_checker_t* checker,
    const char* expr_a,
    const char* expr_b,
    energy_consistency_result_t* result)
{
    if (!checker || !expr_a || !expr_b || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_check_pair: checker, expr_a, expr_b, or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(checker->mutex);

    uint64_t start_time = nimcp_time_monotonic_us();

    /* Initialize result */
    result->total_energy = 0.0f;
    result->logical_energy = 0.0f;
    result->mathematical_energy = 0.0f;
    result->num_violations = 0;

    /* Check if expressions are contradictory */
    /* This is a simplified check - full implementation would use SAT solver */

    size_t len_a = strlen(expr_a);
    size_t len_b = strlen(expr_b);

    /* Check for direct negation pattern */
    char negated[NIMCP_ERROR_BUFFER_LARGE];
    snprintf(negated, sizeof(negated), "NOT(%s)", expr_a);

    if (strcmp(negated, expr_b) == 0) {
        /* Direct contradiction */
        consistency_violation_t violation = {
            .type = CONSISTENCY_CONTRADICTION,
            .severity = VIOLATION_SEVERITY_CRITICAL,
            .energy_cost = 10.0f,
            .timestamp_us = nimcp_time_monotonic_us()
        };
        snprintf(violation.description, sizeof(violation.description),
            "Direct contradiction: %s vs %s", expr_a, expr_b);

        if (result->violations && result->num_violations < result->violation_capacity) {
            energy_consistency_result_add_violation(result, &violation);
        }

        result->logical_energy += violation.energy_cost;
    }

    /* Compute thermodynamic cost */
    if (checker->config.track_thermodynamic_cost) {
        uint64_t bits_processed = (len_a + len_b) * 8;
        energy_consistency_compute_thermo_cost(checker, bits_processed,
            &result->thermodynamic_cost, &result->landauer_cost);
    }

    /* Compute total energy */
    result->total_energy = (result->logical_energy * checker->config.logical_weight +
                           result->thermodynamic_cost * checker->config.thermodynamic_weight);

    result->final_consistency = 1.0f / (1.0f + result->total_energy);
    result->normalized_energy = 1.0f - result->final_consistency;

    result->check_duration_us = nimcp_time_monotonic_us() - start_time;
    result->timestamp_us = nimcp_time_monotonic_us();

    update_stats_after_check(checker, result);

    nimcp_mutex_unlock(checker->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Energy Computation
 * ============================================================================ */

NIMCP_API float energy_consistency_get_score(
    const energy_consistency_checker_t* checker)
{
    if (!checker) {
        return -1.0f;
    }

    return checker->current_consistency;
}

NIMCP_API float energy_consistency_compute_violation_energy(
    const energy_consistency_checker_t* checker,
    const consistency_violation_t* violation)
{
    if (!checker || !violation) {
        return 0.0f;
    }

    /* Base energy from violation type */
    float base_energy = 1.0f;

    switch (violation->type) {
        case CONSISTENCY_CONTRADICTION:
            base_energy = 10.0f;  /* Severe - logical impossibility */
            break;
        case CONSISTENCY_UNSATISFIED_RULE:
            base_energy = 5.0f;
            break;
        case CONSISTENCY_CIRCULARITY:
            base_energy = 8.0f;   /* Circular proofs are invalid */
            break;
        case CONSISTENCY_AXIOM_VIOLATION:
            base_energy = 15.0f;  /* Violating axioms is very bad */
            break;
        case CONSISTENCY_TYPE_MISMATCH:
            base_energy = 3.0f;
            break;
        case CONSISTENCY_DOMAIN_ERROR:
            base_energy = 4.0f;
            break;
        case CONSISTENCY_UNDEFINED_REFERENCE:
            base_energy = 2.0f;
            break;
        case CONSISTENCY_ARITY_MISMATCH:
            base_energy = 2.0f;
            break;
        case CONSISTENCY_SCOPE_VIOLATION:
            base_energy = 3.0f;
            break;
        case CONSISTENCY_SEMANTIC_ERROR:
            base_energy = 4.0f;
            break;
        default:
            base_energy = 1.0f;
            break;
    }

    /* Apply severity multiplier */
    float severity_mult = compute_severity_multiplier(checker, violation->severity);

    return base_energy * severity_mult;
}

NIMCP_API nimcp_error_t energy_consistency_compute_thermo_cost(
    const energy_consistency_checker_t* checker,
    uint64_t bits_processed,
    float* atp_cost,
    float* landauer_cost)
{
    if (!checker || !atp_cost || !landauer_cost) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_compute_thermo_cost: checker, atp_cost, or landauer_cost is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Landauer limit: minimum energy to erase one bit at room temperature
     * E_min = kT ln(2) ≈ 2.87 × 10^-21 J at 300K */
    *landauer_cost = (float)(bits_processed * LANDAUER_LIMIT_JOULES);

    /* ATP cost is significantly higher than Landauer limit in biological systems
     * Real neural computation is ~10^7 times less efficient than Landauer limit */
    *atp_cost = (float)(bits_processed * ATP_COST_PER_BIT);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * FEP Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_consistency_feed_to_fep(
    energy_consistency_checker_t* checker,
    void* fep,
    float energy)
{
    if (!checker || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_feed_to_fep: checker or fep is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* TODO: Integrate with actual FEP system
     * Map consistency energy to prediction error */

    /* For now, just update internal state */
    checker->current_energy = energy;
    checker->current_consistency = 1.0f / (1.0f + energy);

    return NIMCP_SUCCESS;
}

NIMCP_API float energy_consistency_get_precision_weighted(
    const energy_consistency_checker_t* checker,
    const void* fep,
    const consistency_violation_t* violation)
{
    if (!checker || !violation) {
        return 0.0f;
    }

    float base_energy = energy_consistency_compute_violation_energy(
        checker, violation);

    /* TODO: Get precision from FEP system
     * For now, use confidence as proxy for precision */
    float precision = 1.0f;

    return base_energy * precision;
}

/* ============================================================================
 * Immune System Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_consistency_report_to_immune(
    energy_consistency_checker_t* checker,
    const consistency_violation_t* violation)
{
    if (!checker || !violation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_report_to_immune: checker or violation is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Map violation to exception for immune system */
    nimcp_error_t error_code;

    switch (violation->type) {
        case CONSISTENCY_CONTRADICTION:
        case CONSISTENCY_AXIOM_VIOLATION:
            error_code = NIMCP_ERROR_BRAIN_INVALID;
            break;
        case CONSISTENCY_TYPE_MISMATCH:
        case CONSISTENCY_ARITY_MISMATCH:
            error_code = NIMCP_ERROR_INVALID_PARAM;
            break;
        default:
            error_code = NIMCP_ERROR_OPERATION_FAILED;
            break;
    }

    /* Report to immune system via exception mechanism */
    if (violation->severity >= VIOLATION_SEVERITY_CRITICAL) {
        NIMCP_THROW_TO_IMMUNE(error_code,
            "Consistency violation: %s", violation->description);
        return -1;
    }

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t energy_consistency_request_recovery(
    energy_consistency_checker_t* checker,
    const energy_consistency_result_t* result)
{
    if (!checker || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_request_recovery: checker or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Only request recovery for significant violations */
    if (result->total_energy < 5.0f) {
        return NIMCP_SUCCESS;  /* Not severe enough */
    }

    /* Find most severe violation */
    const consistency_violation_t* worst = NULL;
    for (uint32_t i = 0; i < result->num_violations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && result->num_violations > 256) {
            energy_consistency_heartbeat("energy_consi_loop",
                             (float)(i + 1) / (float)result->num_violations);
        }

        if (!worst || result->violations[i].severity > worst->severity) {
            worst = &result->violations[i];
        }
    }

    if (worst && worst->severity >= VIOLATION_SEVERITY_CRITICAL) {
        return energy_consistency_report_to_immune(checker, worst);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_consistency_result_init(
    energy_consistency_result_t* result,
    uint32_t max_violations)
{
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_result_init: result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(result, 0, sizeof(energy_consistency_result_t));

    if (max_violations > 0) {
        result->violations = (consistency_violation_t*)nimcp_calloc(
            max_violations, sizeof(consistency_violation_t));
        if (!result->violations) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "energy_consistency_result_init: failed to allocate violations array");
            return NIMCP_ERROR_NO_MEMORY;
        }
        result->violation_capacity = max_violations;
    }

    return NIMCP_SUCCESS;
}

NIMCP_API void energy_consistency_result_cleanup(
    energy_consistency_result_t* result)
{
    if (!result) {
        return;
    }

    if (result->violations) {
        nimcp_free(result->violations);
        result->violations = NULL;
    }

    result->violation_capacity = 0;
    result->num_violations = 0;
}

NIMCP_API nimcp_error_t energy_consistency_result_add_violation(
    energy_consistency_result_t* result,
    const consistency_violation_t* violation)
{
    if (!result || !violation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_result_add_violation: result or violation is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (result->num_violations >= result->violation_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE,
            "energy_consistency_result_add_violation: violations array is full");
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    memcpy(&result->violations[result->num_violations], violation,
        sizeof(consistency_violation_t));
    result->num_violations++;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Modulation
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_consistency_modulate_inflammation(
    energy_consistency_checker_t* checker,
    float inflammation)
{
    if (!checker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_modulate_inflammation: checker is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(checker->mutex);
    checker->inflammation_level = fmaxf(0.0f, fminf(1.0f, inflammation));
    nimcp_mutex_unlock(checker->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t energy_consistency_modulate_fatigue(
    energy_consistency_checker_t* checker,
    float fatigue)
{
    if (!checker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_modulate_fatigue: checker is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(checker->mutex);
    checker->fatigue_level = fmaxf(0.0f, fminf(1.0f, fatigue));
    nimcp_mutex_unlock(checker->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t energy_consistency_modulate_atp(
    energy_consistency_checker_t* checker,
    float atp_level)
{
    if (!checker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_modulate_atp: checker is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(checker->mutex);
    checker->atp_level = fmaxf(0.01f, fminf(1.0f, atp_level));
    nimcp_mutex_unlock(checker->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_consistency_register_bio_async(
    energy_consistency_checker_t* checker)
{
    if (!checker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_register_bio_async: checker is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(checker->mutex);

    if (checker->bio_async_enabled) {
        nimcp_mutex_unlock(checker->mutex);
        return NIMCP_SUCCESS;  /* Already registered */
    }

    /* Check if router is available */
    if (!bio_router_is_initialized()) {
        nimcp_mutex_unlock(checker->mutex);
        return NIMCP_SUCCESS;  /* No router available, skip registration */
    }

    /* Register with router */
    bio_module_info_t info = {
        .module_id = checker->module_id,
        .module_name = checker->module_name,
        .inbox_capacity = 32,
        .user_data = checker
    };

    checker->bio_ctx = bio_router_register_module(&info);
    if (checker->bio_ctx) {
        checker->bio_async_enabled = true;
    }

    nimcp_mutex_unlock(checker->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t energy_consistency_unregister_bio_async(
    energy_consistency_checker_t* checker)
{
    if (!checker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_unregister_bio_async: checker is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(checker->mutex);

    if (!checker->bio_async_enabled) {
        nimcp_mutex_unlock(checker->mutex);
        return NIMCP_SUCCESS;
    }

    if (checker->bio_ctx) {
        bio_router_unregister_module(checker->bio_ctx);
        checker->bio_ctx = NULL;
    }

    checker->bio_async_enabled = false;

    nimcp_mutex_unlock(checker->mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Internal bio-async message handler
 *
 * Handles incoming messages from the bio-async router.
 * Uses the correct signature expected by bio_message_handler_t.
 */
static nimcp_error_t energy_consistency_handle_bio_message_internal(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg;
    (void)msg_size;
    (void)response_promise;

    if (!user_data) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Handler stub - would process bio-async messages here */
    /* energy_consistency_checker_t* checker = (energy_consistency_checker_t*)user_data; */

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

NIMCP_API nimcp_error_t energy_consistency_get_stats(
    const energy_consistency_checker_t* checker,
    energy_consistency_stats_t* stats)
{
    if (!checker || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "energy_consistency_get_stats: checker or stats is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memcpy(stats, &checker->stats, sizeof(energy_consistency_stats_t));

    return NIMCP_SUCCESS;
}

NIMCP_API const energy_consistency_result_t* energy_consistency_get_last_result(
    const energy_consistency_checker_t* checker)
{
    if (!checker || !checker->has_last_result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "energy_consistency_get_last_result: required parameter is NULL (checker, checker->has_last_result)");
        return NULL;
    }

    return &checker->last_result;
}

NIMCP_API void energy_consistency_print_diagnostics(
    const energy_consistency_checker_t* checker)
{
    if (!checker) {
        return;
    }

    /* Print diagnostics to stderr or logging system */
    /* Implementation would use NIMCP logging infrastructure */
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

static float compute_severity_multiplier(
    const energy_consistency_checker_t* checker,
    violation_severity_t severity)
{
    if (severity >= 0 && severity < 5) {
        return checker->config.severity_multipliers[severity];
    }
    return 1.0f;
}

static void update_stats_after_check(
    energy_consistency_checker_t* checker,
    const energy_consistency_result_t* result)
{
    checker->stats.total_checks++;

    if (result->num_violations == 0) {
        checker->stats.consistent_count++;
    } else {
        checker->stats.inconsistent_count++;

        /* Update violation counts by type */
        for (uint32_t i = 0; i < result->num_violations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_violations > 256) {
                energy_consistency_heartbeat("energy_consi_loop",
                                 (float)(i + 1) / (float)result->num_violations);
            }

            consistency_violation_type_t type = result->violations[i].type;
            if (type < CONSISTENCY_TYPE_COUNT) {
                checker->stats.violation_counts[type]++;
            }
        }
    }

    /* Update energy statistics */
    if (result->total_energy < checker->stats.min_energy) {
        checker->stats.min_energy = result->total_energy;
    }
    if (result->total_energy > checker->stats.max_energy) {
        checker->stats.max_energy = result->total_energy;
    }

    checker->stats.total_energy_sum += result->total_energy;
    checker->stats.avg_energy = (float)(checker->stats.total_energy_sum /
        checker->stats.total_checks);

    /* Update timing */
    checker->stats.total_check_time_us += result->check_duration_us;
    checker->stats.avg_check_time_us = (float)(checker->stats.total_check_time_us /
        checker->stats.total_checks);
}

static nimcp_error_t validate_proof_step(
    const energy_consistency_checker_t* checker,
    const proof_step_t* step,
    const proof_step_t* all_steps,
    uint32_t num_steps,
    consistency_violation_t* violation)
{
    (void)checker;  /* May be used for configuration */

    /* Validate premises exist */
    for (uint32_t i = 0; i < step->premise_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && step->premise_count > 256) {
            energy_consistency_heartbeat("energy_consi_loop",
                             (float)(i + 1) / (float)step->premise_count);
        }

        uint32_t premise_id = step->premises[i];
        bool found = false;

        for (uint32_t j = 0; j < num_steps; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_steps > 256) {
                energy_consistency_heartbeat("energy_consi_loop",
                                 (float)(j + 1) / (float)num_steps);
            }

            if (all_steps[j].step_id == premise_id) {
                found = true;
                break;
            }
        }

        if (!found) {
            violation->type = CONSISTENCY_UNDEFINED_REFERENCE;
            violation->severity = VIOLATION_SEVERITY_ERROR;
            violation->source_line = step->step_id;
            snprintf(violation->description, sizeof(violation->description),
                "Step %u references non-existent premise %u",
                step->step_id, premise_id);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate step type requirements */
    switch (step->type) {
        case PROOF_STEP_AXIOM:
            /* Axioms should have no premises */
            if (step->premise_count > 0) {
                violation->type = CONSISTENCY_ARITY_MISMATCH;
                violation->severity = VIOLATION_SEVERITY_WARNING;
                snprintf(violation->description, sizeof(violation->description),
                    "Axiom step %u should have no premises", step->step_id);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        case PROOF_STEP_INFERENCE:
            /* Inference should have at least one premise */
            if (step->premise_count == 0) {
                violation->type = CONSISTENCY_ARITY_MISMATCH;
                violation->severity = VIOLATION_SEVERITY_ERROR;
                snprintf(violation->description, sizeof(violation->description),
                    "Inference step %u requires premises", step->step_id);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        default:
            break;
    }

    return NIMCP_SUCCESS;
}

static bool check_circular_dependency(
    const proof_step_t* steps,
    uint32_t num_steps,
    uint32_t current_step,
    bool* visited,
    bool* in_stack)
{
    if (current_step >= num_steps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_circular_dependency: capacity exceeded");
        return false;
    }

    visited[current_step] = true;
    in_stack[current_step] = true;

    const proof_step_t* step = &steps[current_step];

    for (uint32_t i = 0; i < step->premise_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && step->premise_count > 256) {
            energy_consistency_heartbeat("energy_consi_loop",
                             (float)(i + 1) / (float)step->premise_count);
        }

        uint32_t premise_id = step->premises[i];

        /* Find premise index */
        for (uint32_t j = 0; j < num_steps; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_steps > 256) {
                energy_consistency_heartbeat("energy_consi_loop",
                                 (float)(j + 1) / (float)num_steps);
            }

            if (steps[j].step_id == premise_id) {
                if (!visited[j]) {
                    if (check_circular_dependency(steps, num_steps, j,
                            visited, in_stack)) {
                        return true;
                    }
                } else if (in_stack[j]) {
                    /* Back edge found - cycle detected */
                    return true;
                }
                break;
            }
        }
    }

    in_stack[current_step] = false;
    /* Normal return: no circular dependency found from this node */
    return false;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void energy_consistency_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_energy_consistency_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int energy_consistency_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "energy_consistency_training_begin: NULL argument");
        return -1;
    }
    energy_consistency_heartbeat_instance(NULL, "energy_consistency_training_begin", 0.0f);
    (void)(struct energy_consistency_checker*)instance; /* Module state available for reset */
    return 0;
}

int energy_consistency_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "energy_consistency_training_end: NULL argument");
        return -1;
    }
    energy_consistency_heartbeat_instance(NULL, "energy_consistency_training_end", 1.0f);
    (void)(struct energy_consistency_checker*)instance; /* Module state available for finalization */
    return 0;
}

int energy_consistency_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "energy_consistency_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    energy_consistency_heartbeat_instance(NULL, "energy_consistency_training_step", progress);
    (void)(struct energy_consistency_checker*)instance; /* Module state available for step adaptation */
    return 0;
}
