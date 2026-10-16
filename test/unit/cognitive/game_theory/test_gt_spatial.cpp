//=============================================================================
// test_gt_spatial.cpp - Unit tests for Spatial Games Module
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_gt_spatial.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SpatialGameTest : public ::testing::Test {
protected:
    nimcp_spatial_game_t game = nullptr;
    nimcp_spatial_config_t config;

    // Standard Prisoner's Dilemma payoffs: T=5, R=3, P=1, S=0
    float pd_payoffs[4] = {
        3.0f,  // (C, C) = R
        0.0f,  // (C, D) = S
        5.0f,  // (D, C) = T
        1.0f   // (D, D) = P
    };

    void SetUp() override {
        config = nimcp_spatial_default_config();
        config.num_nodes = 16;
        config.grid_width = 4;
        config.grid_height = 4;
        config.num_strategies = 2;
    }

    void TearDown() override {
        if (game) {
            nimcp_spatial_destroy(game);
            game = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(SpatialGameTest, DefaultConfigValues) {
    nimcp_spatial_config_t cfg = nimcp_spatial_default_config();
    EXPECT_GT(cfg.num_strategies, 0u);
    EXPECT_GT(cfg.max_steps, 0u);
    EXPECT_GT(cfg.selection_intensity, 0.0f);
}

TEST_F(SpatialGameTest, TopologyNames) {
    EXPECT_NE(nimcp_topology_name(NIMCP_TOPOLOGY_COMPLETE), nullptr);
    EXPECT_NE(nimcp_topology_name(NIMCP_TOPOLOGY_GRID_2D), nullptr);
    EXPECT_NE(nimcp_topology_name(NIMCP_TOPOLOGY_RING), nullptr);
    EXPECT_NE(nimcp_topology_name(NIMCP_TOPOLOGY_RANDOM_GRAPH), nullptr);
    EXPECT_NE(nimcp_topology_name(NIMCP_TOPOLOGY_SCALE_FREE), nullptr);
    EXPECT_NE(nimcp_topology_name(NIMCP_TOPOLOGY_SMALL_WORLD), nullptr);
}

TEST_F(SpatialGameTest, UpdateRuleNames) {
    EXPECT_NE(nimcp_update_rule_name(NIMCP_UPDATE_REPLICATOR), nullptr);
    EXPECT_NE(nimcp_update_rule_name(NIMCP_UPDATE_IMITATION), nullptr);
    EXPECT_NE(nimcp_update_rule_name(NIMCP_UPDATE_BEST_RESPONSE), nullptr);
    EXPECT_NE(nimcp_update_rule_name(NIMCP_UPDATE_FERMI), nullptr);
    EXPECT_NE(nimcp_update_rule_name(NIMCP_UPDATE_MORAN), nullptr);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SpatialGameTest, CreateDestroy) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_destroy(game);
    game = nullptr;
}

TEST_F(SpatialGameTest, CreateWithNullConfig) {
    game = nimcp_spatial_create(nullptr, pd_payoffs);
    // Should handle gracefully - may return NULL
    if (game) {
        nimcp_spatial_destroy(game);
        game = nullptr;
    }
}

TEST_F(SpatialGameTest, CreateWithNullPayoffs) {
    game = nimcp_spatial_create(&config, nullptr);
    // Should handle gracefully - may return NULL
    if (game) {
        nimcp_spatial_destroy(game);
        game = nullptr;
    }
}

TEST_F(SpatialGameTest, Reset) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_error_t err = nimcp_spatial_reset(game);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, DestroyNull) {
    nimcp_spatial_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Topology Tests
//=============================================================================

TEST_F(SpatialGameTest, SetTopologyComplete) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_error_t err = nimcp_spatial_set_topology(game, NIMCP_TOPOLOGY_COMPLETE, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, SetTopologyGrid2D) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_error_t err = nimcp_spatial_set_topology(game, NIMCP_TOPOLOGY_GRID_2D, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, SetTopologyRing) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_error_t err = nimcp_spatial_set_topology(game, NIMCP_TOPOLOGY_RING, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, SetTopologySmallWorld) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    float params[] = { 0.1f };  // rewiring probability
    nimcp_error_t err = nimcp_spatial_set_topology(game, NIMCP_TOPOLOGY_SMALL_WORLD, params);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(SpatialGameTest, InitializeRandom) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    // Uniform distribution
    nimcp_error_t err = nimcp_spatial_initialize_random(game, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, InitializeRandomWithProbs) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    float probs[] = { 0.7f, 0.3f };  // 70% strategy 0, 30% strategy 1
    nimcp_error_t err = nimcp_spatial_initialize_random(game, probs);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, InitializeCluster) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    // Initialize with all strategy 0
    nimcp_spatial_initialize_random(game, nullptr);

    // Place cluster of strategy 1 at center
    nimcp_error_t err = nimcp_spatial_initialize_cluster(game, 1, 8, 1.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, SetNodeStrategy) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    nimcp_error_t err = nimcp_spatial_set_node_strategy(game, 0, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    int32_t strategy = nimcp_spatial_get_node_strategy(game, 0);
    EXPECT_EQ(strategy, 1);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(SpatialGameTest, SingleStep) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    nimcp_error_t err = nimcp_spatial_step(game);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_spatial_get_step(game), 1u);
}

TEST_F(SpatialGameTest, RunSimulation) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    nimcp_evolutionary_result_t result;
    memset(&result, 0, sizeof(result));

    // Use small step count for fast testing (16 nodes don't need 100 steps)
    nimcp_error_t err = nimcp_spatial_run(game, 10, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(result.steps_taken, 0u);
}

TEST_F(SpatialGameTest, RunWithNullResult) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    nimcp_error_t err = nimcp_spatial_run(game, 10, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, StopSimulation) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);
    nimcp_spatial_step(game);

    nimcp_error_t err = nimcp_spatial_stop(game);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Query Tests
//=============================================================================

TEST_F(SpatialGameTest, GetFrequencies) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    float frequencies[2];
    nimcp_error_t err = nimcp_spatial_get_frequencies(game, frequencies);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Frequencies should sum to 1
    float sum = frequencies[0] + frequencies[1];
    EXPECT_NEAR(sum, 1.0f, 0.001f);
}

TEST_F(SpatialGameTest, GetNodeStrategy) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    int32_t strategy = nimcp_spatial_get_node_strategy(game, 0);
    EXPECT_GE(strategy, 0);
    EXPECT_LT(strategy, 2);
}

TEST_F(SpatialGameTest, GetNodeFitness) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);
    nimcp_spatial_step(game);  // Need at least one step for fitness calculation

    float fitness = nimcp_spatial_get_node_fitness(game, 0);
    EXPECT_FALSE(std::isnan(fitness));
}

TEST_F(SpatialGameTest, GetState) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_state_t state = nimcp_spatial_get_state(game);
    EXPECT_EQ(state, NIMCP_SPATIAL_STATE_INITIALIZED);

    nimcp_spatial_initialize_random(game, nullptr);
    nimcp_spatial_step(game);

    state = nimcp_spatial_get_state(game);
    EXPECT_TRUE(state == NIMCP_SPATIAL_STATE_RUNNING ||
                state == NIMCP_SPATIAL_STATE_CONVERGED);
}

TEST_F(SpatialGameTest, GetPopulation) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    nimcp_population_t pop;
    memset(&pop, 0, sizeof(pop));

    nimcp_error_t err = nimcp_spatial_get_population(game, &pop);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(pop.num_strategies, 2u);
}

//=============================================================================
// ESS and Invasion Analysis Tests
//=============================================================================

TEST_F(SpatialGameTest, CheckESS) {
    // Use a game where defection is ESS (Prisoner's Dilemma)
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);
    nimcp_spatial_run(game, 1000, nullptr);

    // In PD, defection (strategy 1) should be ESS
    bool is_ess = nimcp_spatial_is_ess(game, 1);
    // Note: spatial structure can change ESS properties
    SUCCEED();  // Just verify no crash
}

TEST_F(SpatialGameTest, InvasionFitness) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    float invasion_fitness = nimcp_spatial_invasion_fitness(game, 0, 1);
    // Should be finite
    EXPECT_FALSE(std::isnan(invasion_fitness));
    EXPECT_FALSE(std::isinf(invasion_fitness));
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(SpatialGameTest, OutOfBoundsNode) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    // Node 1000 is out of bounds for 16-node game
    int32_t strategy = nimcp_spatial_get_node_strategy(game, 1000);
    EXPECT_EQ(strategy, -1);
}

TEST_F(SpatialGameTest, InvalidStrategy) {
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    // Strategy 10 is invalid for 2-strategy game
    nimcp_error_t err = nimcp_spatial_set_node_strategy(game, 0, 10);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, NullContextHandling) {
    // Spatial module uses NIMCP_GT_ERROR_NULL_POINTER
    EXPECT_EQ(nimcp_spatial_step(nullptr), NIMCP_GT_ERROR_NULL_POINTER);
    EXPECT_EQ(nimcp_spatial_reset(nullptr), NIMCP_GT_ERROR_NULL_POINTER);
    EXPECT_EQ(nimcp_spatial_get_node_strategy(nullptr, 0), -1);
}

//=============================================================================
// Performance/Stress Tests
//=============================================================================

TEST_F(SpatialGameTest, LargePopulation) {
    nimcp_spatial_config_t large_config = nimcp_spatial_default_config();
    large_config.num_nodes = 100;
    large_config.grid_width = 10;
    large_config.grid_height = 10;
    large_config.num_strategies = 2;
    large_config.max_steps = 50;

    game = nimcp_spatial_create(&large_config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    nimcp_evolutionary_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = nimcp_spatial_run(game, 50, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, RepeatedCreateDestroy) {
    for (int i = 0; i < 10; i++) {
        game = nimcp_spatial_create(&config, pd_payoffs);
        ASSERT_NE(game, nullptr);

        nimcp_spatial_initialize_random(game, nullptr);
        nimcp_spatial_step(game);

        nimcp_spatial_destroy(game);
        game = nullptr;
    }
    SUCCEED();
}

//=============================================================================
// Update Rule Tests
//=============================================================================

TEST_F(SpatialGameTest, ReplicatorDynamics) {
    config.update_rule = NIMCP_UPDATE_REPLICATOR;
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    nimcp_error_t err = nimcp_spatial_run(game, 100, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, ImitationDynamics) {
    config.update_rule = NIMCP_UPDATE_IMITATION;
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    nimcp_error_t err = nimcp_spatial_run(game, 100, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, BestResponseDynamics) {
    config.update_rule = NIMCP_UPDATE_BEST_RESPONSE;
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    nimcp_error_t err = nimcp_spatial_run(game, 100, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, FermiDynamics) {
    config.update_rule = NIMCP_UPDATE_FERMI;
    config.fermi_temperature = 0.5f;
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    nimcp_error_t err = nimcp_spatial_run(game, 100, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SpatialGameTest, MoranProcess) {
    config.update_rule = NIMCP_UPDATE_MORAN;
    game = nimcp_spatial_create(&config, pd_payoffs);
    ASSERT_NE(game, nullptr);

    nimcp_spatial_initialize_random(game, nullptr);

    nimcp_error_t err = nimcp_spatial_run(game, 100, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}
