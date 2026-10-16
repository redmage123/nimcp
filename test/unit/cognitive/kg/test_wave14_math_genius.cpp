/**
 * @file test_wave14_math_genius.cpp
 * @brief Unit test for KG-integration Wave W14.
 *
 * Wave W14 wires the math / game-theory / parietal-genius / financial
 * cognitive family into `brain->internal_kg`:
 *
 *   math disciplines (roots):
 *     cog_math, cog_math_algebra, cog_math_calculus, cog_math_topology,
 *     cog_math_probability, cog_math_number_theory, cog_math_logic,
 *     cog_math_category_theory, cog_math_numerical_methods,
 *     cog_math_optimization, cog_math_combinatorics, cog_math_graph_theory,
 *     cog_math_information_theory, cog_math_complexity_theory
 *
 *   game theory:    cog_game_theory, cog_game_theory_equilibrium,
 *                   cog_game_theory_coalition
 *   genius:         cog_parietal_genius, cog_genius_erdos, cog_genius_gauss,
 *                   cog_genius_newton
 *   parietal ops:   cog_parietal_analogical_reasoning,
 *                   cog_parietal_hypothesis_generation,
 *                   cog_parietal_insight_discovery,
 *                   cog_parietal_financial_orchestrator
 *
 * Tests verify structural roots, idempotency, per-family emit+read paths,
 * and null safety.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"

//-----------------------------------------------------------------------------
// Fixture
//-----------------------------------------------------------------------------

class Wave14MathGeniusTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave14_kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     4, 2);
        ASSERT_NE(brain, nullptr) << "brain_create_minimal returned NULL";
        ASSERT_TRUE(brain->internal_kg_enabled)
            << "internal_kg_enabled must be true post-creation";
        ASSERT_NE(brain->internal_kg, nullptr)
            << "brain->internal_kg must be allocated";

        /* Structural init is safe to re-invoke (idempotent). */
        EXPECT_EQ(nimcp_wave14_math_genius_kg_init(brain), 0);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    void expect_node(const char* name) {
        brain_kg_node_id_t id = brain_kg_find_node(brain->internal_kg, name);
        EXPECT_NE(id, BRAIN_KG_INVALID_NODE)
            << "expected KG node '" << name << "' to be present";
    }

    bool any_node_with_prefix(const char* prefix) {
        const size_t plen = strlen(prefix);
        for (uint32_t t = 0; t < BRAIN_KG_NODE_TYPE_COUNT; ++t) {
            brain_kg_node_list_t* list =
                brain_kg_get_nodes_by_type(brain->internal_kg,
                                           (brain_kg_node_type_t)t);
            if (!list) continue;
            bool found = false;
            for (uint32_t i = 0; i < list->count && !found; ++i) {
                const brain_kg_node_t* n = list->nodes[i];
                if (n && strncmp(n->name, prefix, plen) == 0) {
                    found = true;
                }
            }
            brain_kg_node_list_destroy(list);
            if (found) return true;
        }
        return false;
    }
};

//-----------------------------------------------------------------------------
// Structural roots: all umbrellas, disciplines, genius, parietal roots.
//-----------------------------------------------------------------------------

TEST_F(Wave14MathGeniusTest, MathUmbrellaAndDisciplineRootsPresent) {
    expect_node("cog_math");
    expect_node("cog_math_algebra");
    expect_node("cog_math_calculus");
    expect_node("cog_math_topology");
    expect_node("cog_math_probability");
    expect_node("cog_math_number_theory");
    expect_node("cog_math_logic");
    expect_node("cog_math_category_theory");
    expect_node("cog_math_numerical_methods");
    expect_node("cog_math_optimization");
    expect_node("cog_math_combinatorics");
    expect_node("cog_math_graph_theory");
    expect_node("cog_math_information_theory");
    expect_node("cog_math_complexity_theory");
}

TEST_F(Wave14MathGeniusTest, GameTheoryGeniusAndParietalRootsPresent) {
    expect_node("cog_game_theory");
    expect_node("cog_game_theory_equilibrium");
    expect_node("cog_game_theory_coalition");
    expect_node("cog_parietal_genius");
    expect_node("cog_genius_erdos");
    expect_node("cog_genius_gauss");
    expect_node("cog_genius_newton");
    expect_node("cog_parietal_analogical_reasoning");
    expect_node("cog_parietal_hypothesis_generation");
    expect_node("cog_parietal_insight_discovery");
    expect_node("cog_parietal_financial_orchestrator");
}

TEST_F(Wave14MathGeniusTest, Idempotent) {
    /* Re-init must not crash or create dup roots. */
    EXPECT_EQ(nimcp_wave14_math_genius_kg_init(brain), 0);
    EXPECT_EQ(nimcp_wave14_math_genius_kg_init(brain), 0);
    expect_node("cog_math_algebra");
    expect_node("cog_genius_gauss");
    expect_node("cog_parietal_financial_orchestrator");
}

//-----------------------------------------------------------------------------
// Per-family emit + query checks.
//-----------------------------------------------------------------------------

TEST_F(Wave14MathGeniusTest, MathProofEmitAndQuery) {
    wave14_math_emit_proof(brain, "algebra", "fundamental_theorem", 0.87f);
    EXPECT_TRUE(any_node_with_prefix("cog_math_algebra_event_proof_fundamental"));
    float bias = wave14_math_query_confidence_bias(brain, "algebra");
    EXPECT_NEAR(bias, 0.87f, 0.01f);
}

TEST_F(Wave14MathGeniusTest, MathConjectureEmit) {
    wave14_math_emit_conjecture(brain, "number_theory", "twin_primes", 0.6f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_math_number_theory_event_conjecture_twin_primes"));
}

TEST_F(Wave14MathGeniusTest, MathInsightEmitUnknownDisciplineFallsBack) {
    /* Unknown discipline routes to the umbrella "cog_math" root. */
    wave14_math_emit_insight(brain, "foo_weird_discipline",
                             "symmetry_noticed", 0.42f);
    EXPECT_TRUE(any_node_with_prefix("cog_math_event_insight_symmetry_noticed"));
}

TEST_F(Wave14MathGeniusTest, GameEquilibriumEmitAndQuery) {
    wave14_game_emit_equilibrium(brain, "nash", 3u, 0.73f);
    EXPECT_TRUE(any_node_with_prefix("cog_game_theory_event_eq_nash_3_"));
    float bias = wave14_game_query_payoff_bias(brain);
    EXPECT_NEAR(bias, 0.73f, 0.01f);
}

TEST_F(Wave14MathGeniusTest, GameCoalitionEmit) {
    wave14_game_emit_coalition(brain, 5u, 0.61f);
    EXPECT_TRUE(any_node_with_prefix("cog_game_theory_event_coalition_5_"));
}

TEST_F(Wave14MathGeniusTest, GeniusAnalogyEmitAndQuery) {
    wave14_genius_emit_analogy(brain, "erdos", "graph", "probability", 0.72f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_genius_erdos_event_analogy_graph_probability_"));
    float bias = wave14_genius_query_similarity_bias(brain, "erdos");
    EXPECT_NEAR(bias, 0.72f, 0.01f);
}

TEST_F(Wave14MathGeniusTest, GeniusResultEmitAllThree) {
    wave14_genius_emit_result(brain, "gauss", "quadratic_reciprocity", 0.95f);
    wave14_genius_emit_result(brain, "newton", "principia", 0.93f);
    wave14_genius_emit_result(brain, "erdos", "ramsey_bound", 0.85f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_genius_gauss_event_result_quadratic_reciprocity_"));
    EXPECT_TRUE(any_node_with_prefix("cog_genius_newton_event_result_principia_"));
    EXPECT_TRUE(any_node_with_prefix("cog_genius_erdos_event_result_ramsey_bound_"));
}

TEST_F(Wave14MathGeniusTest, AnalogicalMappingEmitAndQuery) {
    wave14_analogical_emit_mapping(brain, 7u, 13u, 0.64f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_parietal_analogical_event_map_7_13_"));
    float bias = wave14_analogical_query_score_bias(brain);
    EXPECT_NEAR(bias, 0.64f, 0.01f);
}

TEST_F(Wave14MathGeniusTest, HypothesisEmitAndQuery) {
    wave14_hypothesis_emit_generation(brain, "dark_matter_modulates_h0", 0.58f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_parietal_hypothesis_event_gen_dark_matter_modulates_h0_"));
    float bias = wave14_hypothesis_query_plausibility_bias(brain);
    EXPECT_NEAR(bias, 0.58f, 0.01f);
}

TEST_F(Wave14MathGeniusTest, InsightEurekaEmitAndQuery) {
    wave14_insight_emit_eureka(brain, 101u, 0.88f, 0.77f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_parietal_insight_event_eureka_101_"));
    float bias = wave14_insight_query_surprise_bias(brain);
    EXPECT_NEAR(bias, 0.88f, 0.01f);
}

TEST_F(Wave14MathGeniusTest, FinancialDecisionEmitAndQuery) {
    wave14_financial_emit_decision(brain, "long_SPY", 0.08f, 0.12f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_parietal_financial_event_decide_long_SPY_"));
    float bias = wave14_financial_query_return_bias(brain);
    EXPECT_NEAR(bias, 0.08f, 0.01f);
}

//-----------------------------------------------------------------------------
// Null-safety: every emit + query is tolerant of NULL brain.
//-----------------------------------------------------------------------------

TEST(Wave14NullSafety, AllHelpersAreNullSafe) {
    EXPECT_EQ(nimcp_wave14_math_genius_kg_init(nullptr), 0);

    wave14_math_emit_proof(nullptr, "algebra", "t", 0.0f);
    wave14_math_emit_conjecture(nullptr, "topology", "c", 0.0f);
    wave14_math_emit_insight(nullptr, nullptr, nullptr, 0.0f);
    wave14_game_emit_equilibrium(nullptr, nullptr, 0u, 0.0f);
    wave14_game_emit_coalition(nullptr, 0u, 0.0f);
    wave14_genius_emit_analogy(nullptr, nullptr, nullptr, nullptr, 0.0f);
    wave14_genius_emit_result(nullptr, nullptr, nullptr, 0.0f);
    wave14_analogical_emit_mapping(nullptr, 0u, 0u, 0.0f);
    wave14_hypothesis_emit_generation(nullptr, nullptr, 0.0f);
    wave14_insight_emit_eureka(nullptr, 0u, 0.0f, 0.0f);
    wave14_financial_emit_decision(nullptr, nullptr, 0.0f, 0.0f);

    EXPECT_EQ(wave14_math_query_confidence_bias(nullptr, "algebra"), 0.5f);
    EXPECT_EQ(wave14_game_query_payoff_bias(nullptr), 0.5f);
    EXPECT_EQ(wave14_genius_query_similarity_bias(nullptr, "erdos"), 0.5f);
    EXPECT_EQ(wave14_analogical_query_score_bias(nullptr), 0.5f);
    EXPECT_EQ(wave14_hypothesis_query_plausibility_bias(nullptr), 0.5f);
    EXPECT_EQ(wave14_insight_query_surprise_bias(nullptr), 0.5f);
    EXPECT_EQ(wave14_financial_query_return_bias(nullptr), 0.5f);
}
