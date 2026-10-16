/**
 * @file test_wave13_curiosity_meta.cpp
 * @brief Unit test for KG-integration Wave W13.
 *
 * Wave W13 wires 9 meta-cognitive modules into `brain->internal_kg`:
 *   1. curiosity                 (cog_curiosity + gap event)
 *   2. information_forager       (cog_information_forager + learn event)
 *   3. meta_learning             (cog_meta_learning + LR / inner-loop events)
 *   4. consolidation (cognitive) (cog_consolidation_cognitive + cycle event)
 *   5. sleep_wake                (cog_sleep_wake + stage transition)
 *   6. self_curriculum           (cog_self_curriculum + uncertainty / gen)
 *   7. analogical_transfer       (cog_analogical_transfer + match event)
 *   8. multiscale_memory         (cog_multiscale_memory + push event)
 *   9. contrastive_self          (cog_contrastive_self + record event)
 *
 * Each test: (a) verifies the structural root exists after brain init,
 *            (b) calls the emit helper, (c) asserts an event node prefix
 *                appears, (d) calls the query helper and confirms a
 *                plausible bias float is returned.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "cognitive/kg/nimcp_wave13_metacog_kg.h"

//-----------------------------------------------------------------------------
// Fixture
//-----------------------------------------------------------------------------

class Wave13MetacogTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave13_kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     4, 2);
        ASSERT_NE(brain, nullptr) << "brain_create_minimal returned NULL";
        ASSERT_TRUE(brain->internal_kg_enabled)
            << "internal_kg_enabled must be true post-creation";
        ASSERT_NE(brain->internal_kg, nullptr)
            << "brain->internal_kg must be allocated";

        /* Structural init is safe to re-invoke (idempotent). */
        EXPECT_EQ(nimcp_wave13_metacog_kg_init(brain), 0);
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
// Structural roots: all 9 module nodes must exist after init.
//-----------------------------------------------------------------------------

TEST_F(Wave13MetacogTest, AllStructuralRootsPresent) {
    expect_node("cog_curiosity");
    expect_node("cog_information_forager");
    expect_node("cog_meta_learning");
    expect_node("cog_consolidation_cognitive");
    expect_node("cog_sleep_wake");
    expect_node("cog_self_curriculum");
    expect_node("cog_analogical_transfer");
    expect_node("cog_multiscale_memory");
    expect_node("cog_contrastive_self");
}

TEST_F(Wave13MetacogTest, Idempotent) {
    /* Re-init must not crash or create duplicate structural roots. */
    EXPECT_EQ(nimcp_wave13_metacog_kg_init(brain), 0);
    EXPECT_EQ(nimcp_wave13_metacog_kg_init(brain), 0);
    expect_node("cog_curiosity");
    expect_node("cog_contrastive_self");
}

//-----------------------------------------------------------------------------
// Per-module emit + query checks.
//-----------------------------------------------------------------------------

TEST_F(Wave13MetacogTest, CuriosityEmitAndQuery) {
    wave13_curiosity_emit_gap(brain, "prime_numbers", 0.85f, 0.72f);
    EXPECT_TRUE(any_node_with_prefix("cog_curiosity_event_gap_prime_numbers_"));
    float bias = wave13_curiosity_query_spike_bias(brain);
    EXPECT_NEAR(bias, 0.72f, 0.01f);
    float surprise = wave13_curiosity_query_recent_surprise(brain);
    EXPECT_NEAR(surprise, 0.85f, 0.01f);
}

TEST_F(Wave13MetacogTest, ForagerEmitAndQuery) {
    wave13_forager_emit_learn(brain, "calculus_limits", 0.68f, 0);
    EXPECT_TRUE(any_node_with_prefix("cog_forager_event_learn_calculus_limits_"));
    float bias = wave13_forager_query_quality_bias(brain);
    EXPECT_NEAR(bias, 0.68f, 0.01f);
}

TEST_F(Wave13MetacogTest, MetaLearningEmitAndQuery) {
    wave13_meta_emit_lr_adaptation(brain, 2, 0.001f, 0.00105f, 0.42f);
    EXPECT_TRUE(any_node_with_prefix("cog_meta_event_lr_adapt_2_"));
    wave13_meta_emit_inner_loop(brain, 5, 0.25f);
    EXPECT_TRUE(any_node_with_prefix("cog_meta_event_inner_loop_5_"));
    float bias = wave13_meta_query_adaptation_bias(brain);
    /* |0.00105-0.001|/0.001 = 0.05 */
    EXPECT_NEAR(bias, 0.05f, 0.01f);
}

TEST_F(Wave13MetacogTest, ConsolidationEmitAndQuery) {
    wave13_consolidation_emit_cycle(brain, 400000u, 100000u, 12.3f);
    EXPECT_TRUE(any_node_with_prefix("cog_consolidation_event_cycle_400000_"));
    float bias = wave13_consolidation_query_load_bias(brain);
    /* load = (400K+100K)/1M = 0.5 */
    EXPECT_NEAR(bias, 0.5f, 0.01f);
}

TEST_F(Wave13MetacogTest, SleepStageTransitionEmitAndQuery) {
    /* 0=AWAKE, 3=DEEP_NREM for example. */
    wave13_sleep_emit_stage_transition(brain, 0, 3, 0.81f);
    EXPECT_TRUE(any_node_with_prefix("cog_sleep_event_stage_0_3_"));
    float bias = wave13_sleep_query_pressure_bias(brain);
    EXPECT_NEAR(bias, 0.81f, 0.01f);
}

TEST_F(Wave13MetacogTest, CurriculumEmitAndQuery) {
    wave13_curriculum_emit_uncertainty_update(brain, "geometry", 0.67f);
    EXPECT_TRUE(any_node_with_prefix("cog_curriculum_event_uncertain_geometry_"));
    wave13_curriculum_emit_item_generated(brain, "geometry", 8u);
    EXPECT_TRUE(any_node_with_prefix("cog_curriculum_event_gen_geometry_8_"));
    float bias = wave13_curriculum_query_uncertainty_bias(brain);
    EXPECT_NEAR(bias, 0.67f, 0.01f);
}

TEST_F(Wave13MetacogTest, AnalogyEmitAndQuery) {
    wave13_analogy_emit_match(brain, "heat_flow_like_diffusion", 0.88f, 0.93f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_analogy_event_match_heat_flow_like_diffusion_"));
    float bias = wave13_analogy_query_similarity_bias(brain);
    EXPECT_NEAR(bias, 0.88f, 0.01f);
}

TEST_F(Wave13MetacogTest, MultiscaleEmitAndQuery) {
    wave13_multiscale_emit_push(brain, "training_step", 0.55f);
    EXPECT_TRUE(any_node_with_prefix("cog_multiscale_event_push_training_step_"));
    float bias = wave13_multiscale_query_importance_bias(brain);
    EXPECT_NEAR(bias, 0.55f, 0.01f);
}

TEST_F(Wave13MetacogTest, ContrastiveEmitAndQuery) {
    wave13_contrastive_emit_record(brain, "cats_vs_dogs", 250u);
    EXPECT_TRUE(any_node_with_prefix("cog_contrastive_event_rec_cats_vs_dogs_250_"));
    float bias = wave13_contrastive_query_buffer_bias(brain);
    /* fill = 250/1000 = 0.25 */
    EXPECT_NEAR(bias, 0.25f, 0.01f);
}

//-----------------------------------------------------------------------------
// Null-safety
//-----------------------------------------------------------------------------

TEST(Wave13NullSafety, AllEmitAndQueryHelpersAreNullSafe) {
    EXPECT_EQ(nimcp_wave13_metacog_kg_init(nullptr), 0);

    wave13_curiosity_emit_gap(nullptr, nullptr, 0.0f, 0.0f);
    wave13_forager_emit_learn(nullptr, nullptr, 0.0f, 0);
    wave13_meta_emit_lr_adaptation(nullptr, 0, 0.0f, 0.0f, 0.0f);
    wave13_meta_emit_inner_loop(nullptr, 0u, 0.0f);
    wave13_consolidation_emit_cycle(nullptr, 0u, 0u, 0.0f);
    wave13_sleep_emit_stage_transition(nullptr, 0, 0, 0.0f);
    wave13_curriculum_emit_uncertainty_update(nullptr, nullptr, 0.0f);
    wave13_curriculum_emit_item_generated(nullptr, nullptr, 0u);
    wave13_analogy_emit_match(nullptr, nullptr, 0.0f, 0.0f);
    wave13_multiscale_emit_push(nullptr, nullptr, 0.0f);
    wave13_contrastive_emit_record(nullptr, nullptr, 0u);

    EXPECT_EQ(wave13_curiosity_query_spike_bias(nullptr), 0.5f);
    EXPECT_EQ(wave13_curiosity_query_recent_surprise(nullptr), 0.0f);
    EXPECT_EQ(wave13_forager_query_quality_bias(nullptr), 0.5f);
    EXPECT_EQ(wave13_meta_query_adaptation_bias(nullptr), 0.5f);
    EXPECT_EQ(wave13_consolidation_query_load_bias(nullptr), 0.5f);
    EXPECT_EQ(wave13_sleep_query_pressure_bias(nullptr), 0.5f);
    EXPECT_EQ(wave13_curriculum_query_uncertainty_bias(nullptr), 0.5f);
    EXPECT_EQ(wave13_analogy_query_similarity_bias(nullptr), 0.5f);
    EXPECT_EQ(wave13_multiscale_query_importance_bias(nullptr), 0.5f);
    EXPECT_EQ(wave13_contrastive_query_buffer_bias(nullptr), 0.5f);
}
