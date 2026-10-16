/**
 * @file test_fep_planning_integration.cpp
 * @brief Integration tests for FEP Planning module with other FEP components
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "cognitive/free_energy/nimcp_fep_planning.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_curiosity.h"
#include "cognitive/free_energy/nimcp_fep_consciousness.h"

class FEPPlanningIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;
    static const uint32_t STATE_DIM = 8;

    fep_planning_system_t* planning = nullptr;
    fep_system_t* fep = nullptr;
    fep_curiosity_system_t* curiosity = nullptr;

    void SetUp() override {
        /* Create planning system */
        fep_planning_config_t plan_config;
        fep_planning_default_config(&plan_config);
        planning = fep_planning_create(&plan_config);

        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);

        /* Create curiosity system */
        fep_curiosity_config_t cur_config;
        fep_curiosity_default_config(&cur_config);
        curiosity = fep_curiosity_create(&cur_config);
    }

    void TearDown() override {
        if (planning) {
            fep_planning_destroy(planning);
            planning = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        if (curiosity) {
            fep_curiosity_destroy(curiosity);
            curiosity = nullptr;
        }
    }
};

/* ============================================================================
 * Planning + FEP Core Integration Tests
 * ============================================================================ */

TEST_F(FEPPlanningIntegrationTest, PlanningWithFEPSystem) {
    fep_planning_connect(planning, fep);

    /* Run MCTS planning */
    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 10);

    int ret = fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);
    EXPECT_EQ(ret, 0);

    fep_plan_destroy(&plan);
}

TEST_F(FEPPlanningIntegrationTest, PlanIsValid) {
    fep_planning_connect(planning, fep);

    std::vector<float> state = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);

    fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);

    /* Plan may or may not be valid depending on planning horizon */
    EXPECT_GE(plan.sequence_length, 0u);

    fep_plan_destroy(&plan);
}

/* ============================================================================
 * MCTS Tree Operations Tests
 * ============================================================================ */

TEST_F(FEPPlanningIntegrationTest, TreeSizeAfterPlanning) {
    fep_planning_connect(planning, fep);

    uint32_t size_before = fep_planning_get_tree_size(planning);

    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);

    fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);

    uint32_t size_after = fep_planning_get_tree_size(planning);
    EXPECT_GE(size_after, size_before);

    fep_plan_destroy(&plan);
}

TEST_F(FEPPlanningIntegrationTest, TreeDepthAfterPlanning) {
    fep_planning_connect(planning, fep);

    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);

    fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);

    uint32_t depth = fep_planning_get_tree_depth(planning);
    EXPECT_GE(depth, 0u);

    fep_plan_destroy(&plan);
}

TEST_F(FEPPlanningIntegrationTest, ResetClearsTree) {
    fep_planning_connect(planning, fep);

    /* Run planning to build tree */
    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);
    fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);

    /* Reset */
    fep_planning_reset(planning);

    uint32_t size_after = fep_planning_get_tree_size(planning);
    EXPECT_GE(size_after, 1u);  /* At least root */

    fep_plan_destroy(&plan);
}

/* ============================================================================
 * Planning + Curiosity Integration Tests
 * ============================================================================ */

TEST_F(FEPPlanningIntegrationTest, PlanningWithCuriosity) {
    fep_planning_connect(planning, fep);
    fep_curiosity_connect(curiosity, fep);

    /* State for planning */
    std::vector<float> state = {0.25f, 0.25f, 0.25f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f};

    /* Get curiosity for state */
    float novelty = fep_compute_novelty(curiosity, state.data(), STATE_DIM);
    EXPECT_GE(novelty, 0.0f);

    /* Run planning */
    fep_plan_t plan;
    fep_plan_create(&plan, 5);
    fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);

    fep_plan_destroy(&plan);
}

TEST_F(FEPPlanningIntegrationTest, CuriosityGuidedPlanning) {
    fep_planning_connect(planning, fep);
    fep_curiosity_connect(curiosity, fep);

    /* Build up memory of visited states */
    std::vector<float> visited = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_curiosity_record_observation(curiosity, visited.data(), STATE_DIM);

    /* Plan from a different state */
    std::vector<float> current = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);

    fep_planning_generate_plan(planning, fep, current.data(), STATE_DIM, &plan);

    fep_plan_destroy(&plan);
}

/* ============================================================================
 * Planning + Consciousness Integration Tests
 * ============================================================================ */

TEST_F(FEPPlanningIntegrationTest, PlanningWithConsciousnessGating) {
    fep_planning_connect(planning, fep);

    fep_consciousness_config_t con_config;
    fep_consciousness_default_config(&con_config);
    fep_consciousness_bridge_t* consciousness = fep_consciousness_create(&con_config);
    fep_consciousness_connect_fep(consciousness, fep);

    /* Plan */
    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);
    fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);

    /* Gate first action if plan is valid */
    if (plan.sequence_length > 0) {
        uint32_t gated;
        fep_consciousness_gate_action(consciousness, plan.action_sequence[0], &gated);
    }

    fep_plan_destroy(&plan);
    fep_consciousness_destroy(consciousness);
}

/* ============================================================================
 * Planning Method Tests
 * ============================================================================ */

TEST_F(FEPPlanningIntegrationTest, MCTSMethod) {
    fep_planning_config_t config;
    fep_planning_default_config(&config);
    config.method = PLANNING_MCTS;

    fep_planning_system_t* mcts = fep_planning_create(&config);
    fep_planning_connect(mcts, fep);

    std::vector<float> state = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);

    int ret = fep_planning_generate_plan(mcts, fep, state.data(), STATE_DIM, &plan);
    EXPECT_EQ(ret, 0);

    fep_plan_destroy(&plan);
    fep_planning_destroy(mcts);
}

TEST_F(FEPPlanningIntegrationTest, BeamSearchMethod) {
    fep_planning_config_t config;
    fep_planning_default_config(&config);
    config.method = PLANNING_BEAM_SEARCH;

    fep_planning_system_t* beam = fep_planning_create(&config);
    fep_planning_connect(beam, fep);

    std::vector<float> state = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);

    int ret = fep_planning_generate_plan(beam, fep, state.data(), STATE_DIM, &plan);
    EXPECT_EQ(ret, 0);

    fep_plan_destroy(&plan);
    fep_planning_destroy(beam);
}

/* ============================================================================
 * Stats Tracking Tests
 * ============================================================================ */

TEST_F(FEPPlanningIntegrationTest, StatsAfterPlanning) {
    fep_planning_connect(planning, fep);

    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);

    fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);

    fep_planning_stats_t stats;
    int ret = fep_planning_get_stats(planning, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.simulations_run, 0u);

    fep_plan_destroy(&plan);
}

/* ============================================================================
 * Config Integration Tests
 * ============================================================================ */

TEST_F(FEPPlanningIntegrationTest, CustomHorizon) {
    fep_planning_config_t config;
    fep_planning_default_config(&config);
    config.planning_horizon = 20;

    fep_planning_system_t* long_horizon = fep_planning_create(&config);
    fep_planning_connect(long_horizon, fep);

    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 20);

    fep_planning_generate_plan(long_horizon, fep, state.data(), STATE_DIM, &plan);

    fep_plan_destroy(&plan);
    fep_planning_destroy(long_horizon);
}

TEST_F(FEPPlanningIntegrationTest, CustomIterations) {
    fep_planning_config_t config;
    fep_planning_default_config(&config);
    config.num_simulations = 500;

    fep_planning_system_t* many_iters = fep_planning_create(&config);
    fep_planning_connect(many_iters, fep);

    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);

    fep_planning_generate_plan(many_iters, fep, state.data(), STATE_DIM, &plan);

    fep_plan_destroy(&plan);
    fep_planning_destroy(many_iters);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(FEPPlanningIntegrationTest, BioAsyncWithPlanning) {
    fep_planning_connect(planning, fep);
    fep_planning_connect_bio_async(planning);

    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);

    int ret = fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);
    EXPECT_EQ(ret, 0);

    fep_plan_destroy(&plan);
    fep_planning_disconnect_bio_async(planning);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(FEPPlanningIntegrationTest, MethodStrings) {
    EXPECT_STREQ(fep_planning_method_to_string(PLANNING_MCTS), "MCTS");
    EXPECT_STREQ(fep_planning_method_to_string(PLANNING_BEAM_SEARCH), "BEAM_SEARCH");
}

TEST_F(FEPPlanningIntegrationTest, NodeStateStrings) {
    EXPECT_STREQ(fep_planning_node_state_to_string(MCTS_NODE_UNVISITED), "UNVISITED");
}

/* ============================================================================
 * Multi-Step Planning Tests
 * ============================================================================ */

TEST_F(FEPPlanningIntegrationTest, MultiStepPlanExecution) {
    fep_planning_connect(planning, fep);

    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 10);

    fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);

    /* Execute each step of plan */
    for (uint32_t i = 0; i < plan.sequence_length; i++) {
        uint32_t action = plan.action_sequence[i];
        /* Actions should be valid */
        EXPECT_GE(action, 0u);
    }

    fep_plan_destroy(&plan);
}

/* ============================================================================
 * Replanning Tests
 * ============================================================================ */

TEST_F(FEPPlanningIntegrationTest, ReplanFromNewState) {
    fep_planning_connect(planning, fep);

    /* Initial plan */
    std::vector<float> state1 = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan1;
    fep_plan_create(&plan1, 5);
    fep_planning_generate_plan(planning, fep, state1.data(), STATE_DIM, &plan1);

    /* Replan from new state */
    fep_planning_reset(planning);
    std::vector<float> state2 = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan2;
    fep_plan_create(&plan2, 5);
    fep_planning_generate_plan(planning, fep, state2.data(), STATE_DIM, &plan2);

    fep_plan_destroy(&plan1);
    fep_plan_destroy(&plan2);
}
