/**
 * @file nimcp_backward_chaining.c
 * @brief MODULE 4: Backward Chaining Engine implementation
 *
 * SINGLE RESPONSIBILITY: Prove goals from facts and rules (goal-driven reasoning)
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#include "cognitive/reasoning/nimcp_backward_chaining.h"
#include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "core/events/nimcp_event_bus.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_executive.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

//=============================================================================
// Event Types
//=============================================================================

#define EVENT_BACKWARD_CHAIN_STEP 0x0947
#define EVENT_PROOF_FOUND 0x0948
#define EVENT_PROOF_FAILED 0x0949

//=============================================================================
// Configuration
//=============================================================================

#define DEFAULT_PROOF_SALIENCE 0.9f

//=============================================================================
// Error Handling
//=============================================================================

static __thread char last_error[256] = {0};

static void set_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

const char* backward_chain_get_last_error(void)
{
    return last_error;
}

//=============================================================================
// Backward Chaining Implementation
//=============================================================================

bool brain_backward_chain(
    brain_t brain,
    const char* goal_str,
    backward_chain_result_t* result)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        NIMCP_LOGGING_ERROR("brain_backward_chain: brain is NULL");
        return false;
    }

    if (!brain_has_symbolic_logic(brain)) {
        set_error("Symbolic logic engine not attached");
        NIMCP_LOGGING_ERROR("brain_backward_chain: no logic engine attached");
        return false;
    }

    if (!nimcp_validate_pointer(goal_str, "goal_str")) {
        set_error("Goal string is NULL");
        NIMCP_LOGGING_ERROR("brain_backward_chain: goal_str is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(result, "result")) {
        set_error("Result pointer is NULL");
        NIMCP_LOGGING_ERROR("brain_backward_chain: result is NULL");
        return false;
    }

    // Initialize result
    memset(result, 0, sizeof(backward_chain_result_t));

    // Get logic engine
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    // Parse goal
    logical_formula_t* formula = symbolic_logic_parse(goal_str);
    if (!formula) {
        set_error("Failed to parse goal: %s", goal_str);
        NIMCP_LOGGING_ERROR("brain_backward_chain: parse failed for '%s'", goal_str);
        return false;
    }

    // Convert to CNF clause
    logic_clause_t** clauses = NULL;
    int num_clauses = 0;
    bool success = symbolic_logic_to_cnf(formula, &clauses, &num_clauses);
    logic_formula_destroy(formula);

    if (!success || num_clauses == 0) {
        set_error("Failed to convert goal to CNF: %s", goal_str);
        NIMCP_LOGGING_ERROR("brain_backward_chain: CNF conversion failed");
        return false;
    }

    // Create executive task for planning
    uint32_t task_id = 0;
    if (brain->executive && brain->config.enable_executive) {
        task_descriptor_t task = {
            .type = TASK_TYPE_PLANNING,
            .priority = PRIORITY_HIGH,
            .status = TASK_STATUS_PENDING
        };
        strncpy(task.name, "Backward Chaining Proof", sizeof(task.name) - 1);
        task_id = executive_add_task(brain->executive, &task);
    }

    // Perform backward chaining
    uint64_t start_time = nimcp_time_monotonic_ms();
    inference_rule_t** proof_trace = NULL;
    int num_steps = 0;

    success = symbolic_logic_backward_chain(
        engine,
        clauses[0],
        &proof_trace,
        &num_steps
    );

    uint64_t end_time = nimcp_time_monotonic_ms();

    // Clean up goal clauses
    for (int i = 0; i < num_clauses; i++) {
        nimcp_free(clauses[i]);
    }
    nimcp_free(clauses);

    // Populate result
    result->proven = success;
    result->proof_steps = proof_trace;
    result->num_steps = num_steps;
    result->confidence = success ? 0.95f : 0.0f;
    result->inference_time_ms = end_time - start_time;
    result->depth_reached = num_steps;

    if (success) {
        // Store proof in working memory if enabled
        if (brain->working_memory && brain->config.enable_working_memory && num_steps > 0) {
            float proof_encoding[4] = {DEFAULT_PROOF_SALIENCE, 0.0f, (float)num_steps, 0.0f};
            working_memory_add_item(brain->working_memory, proof_encoding, 4,
                                   DEFAULT_PROOF_SALIENCE);
            NIMCP_LOGGING_DEBUG("Stored proof trace in working memory (steps=%d)", num_steps);
        }

        // Publish proof found event
        if (brain->event_bus) {
            event_data_t event = {
                .event_type = EVENT_PROOF_FOUND,
                .timestamp = 0,
                .priority = EVENT_PRIORITY_HIGH,
                .data_size = strlen(goal_str) + 1,
                .data = (void*)goal_str
            };
            event_bus_publish(brain->event_bus, &event);
        }

        // Complete executive task
        if (task_id > 0 && brain->executive) {
            executive_mark_task_completed(brain->executive, task_id);
        }

        NIMCP_LOGGING_INFO("Backward chaining successful: goal '%s' proven in %d steps (%llu ms)",
                           goal_str, num_steps, (unsigned long long)(end_time - start_time));
    } else {
        // Publish proof failed event
        if (brain->event_bus) {
            event_data_t event = {
                .event_type = EVENT_PROOF_FAILED,
                .timestamp = 0,
                .priority = EVENT_PRIORITY_NORMAL,
                .data_size = strlen(goal_str) + 1,
                .data = (void*)goal_str
            };
            event_bus_publish(brain->event_bus, &event);
        }

        // Fail executive task
        if (task_id > 0 && brain->executive) {
            executive_mark_task_failed(brain->executive, task_id);
        }

        NIMCP_LOGGING_INFO("Goal not proven: %s", goal_str);
    }

    return success;
}

bool brain_backward_chain_step(
    brain_t brain,
    const char* subgoal_str,
    logic_clause_t*** premises,
    uint32_t* num_premises)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        NIMCP_LOGGING_ERROR("brain_backward_chain_step: brain is NULL");
        return false;
    }

    if (!brain_has_symbolic_logic(brain)) {
        set_error("Symbolic logic engine not attached");
        NIMCP_LOGGING_ERROR("brain_backward_chain_step: no logic engine attached");
        return false;
    }

    if (!nimcp_validate_pointer(subgoal_str, "subgoal_str")) {
        set_error("Subgoal string is NULL");
        NIMCP_LOGGING_ERROR("brain_backward_chain_step: subgoal_str is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(premises, "premises")) {
        set_error("Premises pointer is NULL");
        NIMCP_LOGGING_ERROR("brain_backward_chain_step: premises is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(num_premises, "num_premises")) {
        set_error("Num premises pointer is NULL");
        NIMCP_LOGGING_ERROR("brain_backward_chain_step: num_premises is NULL");
        return false;
    }

    // Implementation would find matching rules and return premises
    // For now, return false (not implemented in base symbolic_logic API)
    set_error("Backward chain step not implemented");
    NIMCP_LOGGING_WARNING("brain_backward_chain_step: not implemented");

    *premises = NULL;
    *num_premises = 0;

    // Publish step event
    if (brain->event_bus) {
        event_data_t event = {
            .event_type = EVENT_BACKWARD_CHAIN_STEP,
            .timestamp = 0,
            .priority = EVENT_PRIORITY_LOW,
            .data_size = strlen(subgoal_str) + 1,
            .data = (void*)subgoal_str
        };
        event_bus_publish(brain->event_bus, &event);
    }

    return false;
}

void backward_chain_free_result(backward_chain_result_t* result)
{
    if (!result) return;

    if (result->proof_steps) {
        nimcp_free(result->proof_steps);
        result->proof_steps = NULL;
    }

    memset(result, 0, sizeof(backward_chain_result_t));
}

bool brain_get_backward_chain_stats(
    brain_t brain,
    uint32_t* proofs_attempted,
    uint32_t* proofs_succeeded,
    float* avg_depth)
{
    if (!brain || !brain_has_symbolic_logic(brain)) {
        set_error("Brain is NULL or no logic engine attached");
        return false;
    }

    // Get statistics from logic engine
    logic_stats_t stats;
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    if (!symbolic_logic_get_stats(engine, &stats)) {
        set_error("Failed to get logic engine statistics");
        return false;
    }

    // Populate outputs (if provided)
    if (proofs_attempted) {
        *proofs_attempted = stats.inferences_performed;
    }

    if (proofs_succeeded) {
        *proofs_succeeded = 0; // Not tracked separately
    }

    if (avg_depth) {
        *avg_depth = 0.0f; // Not tracked separately
    }

    return true;
}
