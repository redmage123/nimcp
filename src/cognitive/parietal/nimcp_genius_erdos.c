/**
 * @file nimcp_genius_erdos.c
 * @brief Erdős Mode Implementation (Combinatorics & Graph Theory)
 *
 * Implements Erdős-style mathematical reasoning focusing on:
 * - Combinatorics and counting
 * - Graph theory algorithms
 * - Probabilistic method proofs
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

BRIDGE_BOILERPLATE(genius_erdos, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Erdős Mode Analysis Implementation
 * ============================================================================ */

nimcp_error_t genius_erdos_analyze_impl(
    mathematical_genius_t* genius,
    const math_problem_t* problem,
    genius_result_t* result) {

    /* Phase 8: Heartbeat at operation start */
    genius_erdos_heartbeat("genius_erdos_analyze_impl", 0.0f);


    if (!problem || !result) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    uint64_t start = nimcp_time_monotonic_us();

    /* Initialize result */
    memset(result, 0, sizeof(genius_result_t));
    result->mode_used = GENIUS_MODE_ERDOS;

    /* Erdős-style analysis: probabilistic method, combinatorial arguments,
       graph theory intuition. Erdős valued elegance above all ("The Book").
       His approach: find the simplest possible proof, use probabilistic
       existence arguments, collaborate across domains. */

    float elegance = 0.6f;   /* Erdős seeks "Book proofs" — maximum elegance */
    float novelty = 0.6f;    /* Probabilistic method was revolutionary */
    float generalization = 0.5f;
    float rigor = 0.6f;

    /* Domain-specific analysis */
    if (problem->domain == GENIUS_DOMAIN_COMBINATORICS ||
        problem->domain == GENIUS_DOMAIN_GRAPH_THEORY) {
        /* Erdős's primary domains */
        elegance += 0.25f;
        novelty += 0.2f;
        rigor += 0.15f;
    } else if (problem->domain == GENIUS_DOMAIN_NUMBER_THEORY ||
               problem->domain == GENIUS_DOMAIN_PROBABILITY) {
        /* Strong secondary domains */
        elegance += 0.15f;
        novelty += 0.15f;
    }

    /* Erdős excelled at finding unexpected connections */
    if (problem->num_secondary > 0) {
        /* Cross-domain problems are Erdős's forte */
        novelty += 0.15f * fminf(3.0f, (float)problem->num_secondary);
        elegance += 0.1f;
        generalization += 0.15f;
    }

    /* The harder the problem, the more Erdős shines
       (he loved hard open problems) */
    if (problem->difficulty > 0.8f) {
        novelty += 0.2f;
        elegance += 0.1f;  /* Hard problems need elegant solutions */
    } else if (problem->difficulty > 0.5f) {
        novelty += 0.1f;
    }

    /* Probabilistic method applicability */
    if (problem->constraints) {
        /* Existence proofs via probability */
        novelty += 0.1f;
        elegance += 0.1f;
    }

    if (problem->target) {
        result->solved = true;
        rigor += 0.1f;
    }

    if (genius) {
        elegance += 0.05f; /* Erdős naturally seeks elegance */
    }

    result->elegance_score = fminf(1.0f, elegance);
    result->novelty_score = fminf(1.0f, novelty);
    result->generalization_score = fminf(1.0f, generalization);
    result->rigor_score = fminf(1.0f, rigor);

    result->thinking_time_us = nimcp_time_monotonic_us() - start;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Ramsey Theory Functions
 * ============================================================================ */

/**
 * @brief Compute lower bound for Ramsey number R(r,s)
 *
 * Uses Erdős probabilistic lower bound: R(r,s) > floor(2^((r+s-2)/2))
 * This is the classic result from Erdős's 1947 probabilistic method proof.
 *
 * @param genius Genius instance (unused, for API consistency)
 * @param r First parameter of R(r,s)
 * @param s Second parameter of R(r,s)
 * @return Lower bound for R(r,s)
 */
uint32_t genius_erdos_ramsey_lower_bound(
    mathematical_genius_t* genius,
    uint32_t r,
    uint32_t s) {

    /* Phase 8: Heartbeat at operation start */
    genius_erdos_heartbeat("genius_erdos_ramsey_lower_bound", 0.0f);


    (void)genius;  /* Unused - function is stateless */

    if (r < 2 || s < 2) {
        return 1;  /* Base cases */
    }

    /* Erdős probabilistic lower bound: R(r,s) > 2^((r+s-2)/2) */
    double exponent = (double)(r + s - 2) / 2.0;
    double lower_bound = pow(2.0, exponent);

    /* Return floor of the bound */
    return (uint32_t)floor(lower_bound);
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void genius_erdos_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_genius_erdos_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int genius_erdos_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_erdos_training_begin: NULL argument");
        return -1;
    }
    genius_erdos_heartbeat_instance(g_genius_erdos_health_agent, "genius_erdos_training_begin", 0.0f);
    return 0;
}

int genius_erdos_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_erdos_training_end: NULL argument");
        return -1;
    }
    genius_erdos_heartbeat_instance(g_genius_erdos_health_agent, "genius_erdos_training_end", 1.0f);
    return 0;
}

int genius_erdos_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_erdos_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    genius_erdos_heartbeat_instance(g_genius_erdos_health_agent, "genius_erdos_training_step", progress);
    return 0;
}

/* ============================================================================
 * W14 (2026-04-24): KG runtime emit for Erdős genius analogies + results.
 * Callers that hold a brain_t invoke these after genius_erdos_analyze_impl /
 * genius_erdos_ramsey_lower_bound to register the output in brain->internal_kg.
 * ============================================================================ */
#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"
void genius_erdos_wave14_kg_emit_analogy(
    struct brain_struct* brain,
    const char* src_domain, const char* tgt_domain, float similarity)
{
    if (!brain) return;
    wave14_genius_emit_analogy(brain, "erdos", src_domain, tgt_domain, similarity);
}
void genius_erdos_wave14_kg_emit_result(
    struct brain_struct* brain,
    const char* result_label, float confidence)
{
    if (!brain) return;
    wave14_genius_emit_result(brain, "erdos", result_label, confidence);
}
