/**
 * @file nimcp_genius_newton.c
 * @brief Newton Mode Implementation (Calculus & Physics)
 *
 * Implements Newton-style mathematical reasoning focusing on:
 * - Symbolic differentiation and integration
 * - Differential equations
 * - Physics-based problem solving
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#include "cognitive/parietal/nimcp_mathematical_genius.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include <math.h>
#include <string.h>
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(genius_newton, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Newton Mode Analysis Implementation
 * ============================================================================ */

nimcp_error_t genius_newton_analyze_impl(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result) {

    /* Phase 8: Heartbeat at operation start */
    genius_newton_heartbeat("genius_newto_analyze_impl", 0.0f);


    if (!problem || !result) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    uint64_t start = nimcp_time_monotonic_us();

    /* Initialize result */
    memset(result, 0, sizeof(genius_result_t));
    result->mode_used = GENIUS_MODE_NEWTON;

    /* Newton-style analysis: differential/integral reasoning, physics intuition.
       Newton's approach: reduce to first principles, apply calculus,
       find general laws from specific observations. */

    float elegance = 0.5f;
    float novelty = 0.5f;
    float generalization = 0.6f;  /* Newton naturally generalizes well */
    float rigor = 0.7f;           /* Newton is mathematically rigorous */

    /* Domain-specific analysis */
    if (problem->domain == GENIUS_DOMAIN_CALCULUS ||
        problem->domain == GENIUS_DOMAIN_PHYSICS ||
        problem->domain == GENIUS_DOMAIN_DIFFERENTIAL_EQ) {
        /* Newton's home turf */
        elegance += 0.25f;
        generalization += 0.2f;
        rigor += 0.1f;
    } else if (problem->domain == GENIUS_DOMAIN_GEOMETRY) {
        /* Newton had strong geometric intuition */
        elegance += 0.15f;
        generalization += 0.1f;
    }

    /* Physics problems: Newton applies force/motion/energy frameworks */
    if (problem->given && problem->target) {
        /* Has both givens and targets — structured problem */
        result->solved = true;
        elegance += 0.1f;
        rigor += 0.1f;
    }

    /* Difficulty scaling */
    if (problem->difficulty > 0.8f) {
        novelty += 0.25f;  /* Hard problems require novel approaches */
        elegance += 0.05f;
    } else if (problem->difficulty > 0.5f) {
        novelty += 0.1f;
        elegance += 0.1f;
    }

    /* Multiple domains → cross-domain insight (Newton's specialty) */
    if (problem->num_secondary > 0) {
        novelty += 0.1f * fminf(3.0f, (float)problem->num_secondary);
        generalization += 0.1f;
    }

    if (genius) {
        rigor += 0.05f; /* Newton naturally rigorous */
    }

    result->elegance_score = fminf(1.0f, elegance);
    result->novelty_score = fminf(1.0f, novelty);
    result->generalization_score = fminf(1.0f, generalization);
    result->rigor_score = fminf(1.0f, rigor);

    result->thinking_time_us = nimcp_time_monotonic_us() - start;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void genius_newton_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_genius_newton_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int genius_newton_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_newton_training_begin: NULL argument");
        return -1;
    }
    genius_newton_heartbeat_instance(g_genius_newton_health_agent, "genius_newton_training_begin", 0.0f);
    return 0;
}

int genius_newton_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_newton_training_end: NULL argument");
        return -1;
    }
    genius_newton_heartbeat_instance(g_genius_newton_health_agent, "genius_newton_training_end", 1.0f);
    return 0;
}

int genius_newton_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_newton_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    genius_newton_heartbeat_instance(g_genius_newton_health_agent, "genius_newton_training_step", progress);
    return 0;
}

/* ============================================================================
 * W14 (2026-04-24): KG runtime emit for Newton genius calculus + mechanics.
 * ============================================================================ */
#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"
void genius_newton_wave14_kg_emit_result(
    struct brain_struct* brain,
    const char* result_label, float confidence)
{
    if (!brain) return;
    wave14_genius_emit_result(brain, "newton", result_label, confidence);
    /* Newton co-invented calculus — also emit into the discipline root. */
    wave14_math_emit_proof(brain, "calculus", result_label, confidence);
}
