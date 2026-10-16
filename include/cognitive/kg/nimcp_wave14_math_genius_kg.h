//=============================================================================
// nimcp_wave14_math_genius_kg.h - Wave W14: Math / Game Theory / Parietal Genius
//=============================================================================
/**
 * @file nimcp_wave14_math_genius_kg.h
 * @brief Wave W14 structural roots + emit + query helpers for the math /
 *        game-theory / parietal-genius / financial cognitive family.
 *
 * WHY: the audit (docs/claude/kg-integration-audit-2026-04-24.md §2.10) labels
 *      this family the "largest silent cognitive family" — 18 math files, 17
 *      game-theory files, 65 parietal files with *zero* KG refs between them.
 *      W14 adds a single shared helper file that:
 *
 *        (a) registers one structural root node per *discipline* inside math
 *            (algebra, calculus/real_analysis, topology, probability,
 *            number_theory, logic/category_theory, numerical_methods,
 *            optimization, combinatorics, graph_theory, information_theory,
 *            complexity_theory), plus one root for game_theory with two
 *            children (equilibrium, coalition), plus three genius roots
 *            (erdos, gauss, newton), plus four parietal cognitive-op roots
 *            (analogical_reasoning, hypothesis_generation, insight_discovery,
 *            financial_orchestrator);
 *
 *        (b) emits runtime event nodes for proofs, conjectures, insights,
 *            equilibria, analogies, hypotheses, investment decisions;
 *
 *        (c) exposes metadata-backed query helpers returning [0,1] biases
 *            so downstream modules can read the most recent event without
 *            coupling their types to KG structs.
 *
 * All writes elevate ADMIN via brain->internal_kg_admin_token and restore
 * READ (kg-node-naming-registry.md §7). All emits are null/disabled/kg-missing
 * tolerant (silent no-op).
 *
 * Node naming (registry §2):
 *   - math disciplines:  cog_math_<discipline>
 *   - game theory:       cog_game_theory, cog_game_theory_equilibrium,
 *                        cog_game_theory_coalition
 *   - genius profiles:   cog_genius_erdos, cog_genius_gauss, cog_genius_newton
 *   - parietal ops:      cog_parietal_analogical_reasoning,
 *                        cog_parietal_hypothesis_generation,
 *                        cog_parietal_insight_discovery,
 *                        cog_parietal_financial_orchestrator
 *
 * Event names (registry §4): <owner>_event_<kind>_<timestamp_us>.
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_WAVE14_MATH_GENIUS_KG_H
#define NIMCP_WAVE14_MATH_GENIUS_KG_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

/* ---------------------------------------------------------------------------
 * Structural init — called once per brain at init time. Idempotent.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT int nimcp_wave14_math_genius_kg_init(struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * Math emit helpers.
 *
 * `discipline` selects the root: "algebra", "calculus", "topology",
 * "probability", "number_theory", "logic", "category_theory",
 * "numerical_methods", "optimization", "combinatorics", "graph_theory",
 * "information_theory", "complexity_theory". Unknown disciplines fall back
 * to the umbrella node `cog_math`.
 * --------------------------------------------------------------------------- */

/** Emit a completed proof / theorem-verified event. confidence in [0,1]. */
NIMCP_EXPORT void wave14_math_emit_proof(
    struct brain_struct* brain,
    const char* discipline,
    const char* theorem_label,
    float confidence);

/** Emit a conjecture-formed event. novelty in [0,1]. */
NIMCP_EXPORT void wave14_math_emit_conjecture(
    struct brain_struct* brain,
    const char* discipline,
    const char* conjecture_label,
    float novelty);

/** Emit a math insight / structural realization event. strength in [0,1]. */
NIMCP_EXPORT void wave14_math_emit_insight(
    struct brain_struct* brain,
    const char* discipline,
    const char* insight_label,
    float strength);

/** Returns stored confidence bias from the discipline's root, else 0.5f. */
NIMCP_EXPORT float wave14_math_query_confidence_bias(
    struct brain_struct* brain,
    const char* discipline);

/* ---------------------------------------------------------------------------
 * Game-theory emit helpers.
 * --------------------------------------------------------------------------- */

/** Emit an equilibrium-solved event (Nash / Bayes-Nash / correlated / etc.). */
NIMCP_EXPORT void wave14_game_emit_equilibrium(
    struct brain_struct* brain,
    const char* equilibrium_type,
    uint32_t num_players,
    float payoff);

/** Emit a coalition-formed event. */
NIMCP_EXPORT void wave14_game_emit_coalition(
    struct brain_struct* brain,
    uint32_t coalition_size,
    float stability);

/** Returns stored last equilibrium payoff bias, else 0.5f. */
NIMCP_EXPORT float wave14_game_query_payoff_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * Genius profile emit helpers (Erdős / Gauss / Newton).
 *
 * `genius_name` in {"erdos", "gauss", "newton"}; unknown -> umbrella node.
 * --------------------------------------------------------------------------- */

/** Emit a cross-domain analogy produced by the genius profile. */
NIMCP_EXPORT void wave14_genius_emit_analogy(
    struct brain_struct* brain,
    const char* genius_name,
    const char* src_domain,
    const char* tgt_domain,
    float similarity);

/** Emit a genius-style theorem/ramsey-bound/calculation event. */
NIMCP_EXPORT void wave14_genius_emit_result(
    struct brain_struct* brain,
    const char* genius_name,
    const char* result_label,
    float confidence);

/** Returns stored last-similarity from a genius node, else 0.5f. */
NIMCP_EXPORT float wave14_genius_query_similarity_bias(
    struct brain_struct* brain,
    const char* genius_name);

/* ---------------------------------------------------------------------------
 * Parietal cognitive-op emit helpers.
 * --------------------------------------------------------------------------- */

/** Parietal analogical reasoning emit. */
NIMCP_EXPORT void wave14_analogical_emit_mapping(
    struct brain_struct* brain,
    uint32_t src_domain_id,
    uint32_t tgt_domain_id,
    float mapping_score);

/** Parietal hypothesis-generation emit. */
NIMCP_EXPORT void wave14_hypothesis_emit_generation(
    struct brain_struct* brain,
    const char* hypothesis_label,
    float plausibility);

/** Parietal insight-discovery emit (eureka / impasse-resolved). */
NIMCP_EXPORT void wave14_insight_emit_eureka(
    struct brain_struct* brain,
    uint32_t problem_id,
    float surprise,
    float elegance);

/** Parietal financial-orchestrator emit. */
NIMCP_EXPORT void wave14_financial_emit_decision(
    struct brain_struct* brain,
    const char* decision_label,
    float expected_return,
    float risk);

/** Query helpers (all return 0.5f neutral on miss). */
NIMCP_EXPORT float wave14_analogical_query_score_bias(
    struct brain_struct* brain);
NIMCP_EXPORT float wave14_hypothesis_query_plausibility_bias(
    struct brain_struct* brain);
NIMCP_EXPORT float wave14_insight_query_surprise_bias(
    struct brain_struct* brain);
NIMCP_EXPORT float wave14_financial_query_return_bias(
    struct brain_struct* brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WAVE14_MATH_GENIUS_KG_H */
