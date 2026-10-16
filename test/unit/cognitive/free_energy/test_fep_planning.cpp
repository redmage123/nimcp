/**
 * @file test_fep_planning.cpp
 * @brief Unit tests for FEP Planning module
 */

#include <gtest/gtest.h>
#include "cognitive/free_energy/nimcp_fep_planning.h"

class FEPPlanningTest : public ::testing::Test {
protected:
    fep_planning_system_t* planning = nullptr;

    void SetUp() override {
        fep_planning_config_t config;
        fep_planning_default_config(&config);
        planning = fep_planning_create(&config);
    }

    void TearDown() override {
        if (planning) {
            fep_planning_destroy(planning);
            planning = nullptr;
        }
    }
};

TEST_F(FEPPlanningTest, CreateDestroy) {
    ASSERT_NE(planning, nullptr);
}

TEST_F(FEPPlanningTest, CreateWithNullConfig) {
    fep_planning_system_t* sys = fep_planning_create(nullptr);
    ASSERT_NE(sys, nullptr);
    fep_planning_destroy(sys);
}

TEST_F(FEPPlanningTest, DefaultConfig) {
    fep_planning_config_t config;
    fep_planning_default_config(&config);
    EXPECT_EQ(config.method, PLANNING_MCTS);
    EXPECT_GT(config.planning_horizon, 0u);
}

TEST_F(FEPPlanningTest, Reset) {
    int ret = fep_planning_reset(planning);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPPlanningTest, GetTreeSize) {
    uint32_t size = fep_planning_get_tree_size(planning);
    EXPECT_GE(size, 1u);  /* At least root */
}

TEST_F(FEPPlanningTest, GetTreeDepth) {
    uint32_t depth = fep_planning_get_tree_depth(planning);
    EXPECT_GE(depth, 0u);
}

TEST_F(FEPPlanningTest, GetStats) {
    fep_planning_stats_t stats;
    int ret = fep_planning_get_stats(planning, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPPlanningTest, PlanCreate) {
    fep_plan_t plan;
    int ret = fep_plan_create(&plan, 10);
    EXPECT_EQ(ret, 0);
    fep_plan_destroy(&plan);
}

TEST_F(FEPPlanningTest, PlanIsValid) {
    fep_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    EXPECT_FALSE(fep_plan_is_valid(&plan));

    fep_plan_create(&plan, 10);
    plan.sequence_length = 0;
    EXPECT_FALSE(fep_plan_is_valid(&plan));
    fep_plan_destroy(&plan);
}

TEST_F(FEPPlanningTest, MethodToString) {
    EXPECT_STREQ(fep_planning_method_to_string(PLANNING_MCTS), "MCTS");
    EXPECT_STREQ(fep_planning_method_to_string(PLANNING_BEAM_SEARCH), "BEAM_SEARCH");
}

TEST_F(FEPPlanningTest, NodeStateToString) {
    EXPECT_STREQ(fep_planning_node_state_to_string(MCTS_NODE_UNVISITED), "UNVISITED");
}

TEST_F(FEPPlanningTest, BioAsyncConnectDisconnect) {
    EXPECT_FALSE(fep_planning_is_bio_async_connected(planning));
    fep_planning_connect_bio_async(planning);
    fep_planning_disconnect_bio_async(planning);
}
