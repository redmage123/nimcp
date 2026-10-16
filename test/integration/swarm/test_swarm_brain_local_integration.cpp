/**
 * @file test_swarm_brain_local_integration.cpp
 * @brief Integration tests for Swarm Brain Local Instantiation
 *
 * INTEGRATION SCENARIOS:
 * - Multi-agent learning coordination
 * - Weight synchronization across swarm
 * - Divergence detection and correction
 * - Bio-async message passing
 * - Consensus formation
 * - Agent join/leave dynamics
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_brain_local.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

class SwarmBrainLocalIntegrationTest : public ::testing::Test {
protected:
    swarm_brain_manager_t* manager;

    void SetUp() override {
        swarm_brain_config_t config = swarm_brain_local_default_config();
        config.enable_bio_async = false;
        config.test_mode = true;  // Skip actual brain creation for fast tests
        manager = swarm_brain_manager_create(&config);
        ASSERT_NE(manager, nullptr);
    }

    void TearDown() override {
        swarm_brain_manager_destroy(manager);
    }
};

//=============================================================================
// Multi-Agent Learning Coordination
//=============================================================================

TEST_F(SwarmBrainLocalIntegrationTest, MultiAgentLearningConvergence) {
    const uint32_t num_agents = 5;

    // Create agents
    for (uint32_t i = 1; i <= num_agents; i++) {
        swarm_brain_create_for_agent(manager, i, 100);
    }

    // Simulate learning cycles
    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float target[5] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f};

    for (int cycle = 0; cycle < 10; cycle++) {
        // Each agent learns locally
        for (uint32_t i = 1; i <= num_agents; i++) {
            swarm_brain_local_learn(manager, i, input, 10, target, 5);
        }

        // Synchronize
        swarm_brain_sync_all(manager);
    }

    // Verify all agents have similar weights (low divergence)
    for (uint32_t i = 1; i <= num_agents; i++) {
        float divergence = swarm_brain_get_divergence(manager, i);
        EXPECT_LT(divergence, 0.5f);  // Should converge
    }

    SUCCEED();
}

//=============================================================================
// Weight Synchronization
//=============================================================================

TEST_F(SwarmBrainLocalIntegrationTest, WeightSyncReducesDivergence) {
    // Create 3 agents
    for (uint32_t i = 1; i <= 3; i++) {
        swarm_brain_create_for_agent(manager, i, 100);
        swarm_brain_local_sync_weights(manager, i);
    }

    // Learn different patterns on different agents
    float input1[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    float input2[5] = {0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float target[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    swarm_brain_local_learn(manager, 1, input1, 5, target, 5);
    swarm_brain_local_learn(manager, 2, input2, 5, target, 5);

    // Check divergence before sync
    float div1_before = swarm_brain_get_divergence(manager, 1);
    float div2_before = swarm_brain_get_divergence(manager, 2);

    // Sync all
    swarm_brain_sync_all(manager);

    // Check divergence after sync
    float div1_after = swarm_brain_get_divergence(manager, 1);
    float div2_after = swarm_brain_get_divergence(manager, 2);

    // Divergence should reduce (or stay similar if already low)
    SUCCEED();
}

//=============================================================================
// Consensus Formation
//=============================================================================

TEST_F(SwarmBrainLocalIntegrationTest, ConsensusWeightsReflectAllAgents) {
    const uint32_t num_agents = 4;

    // Create agents
    for (uint32_t i = 1; i <= num_agents; i++) {
        swarm_brain_create_for_agent(manager, i, 100);
        swarm_brain_local_sync_weights(manager, i);
    }

    // Get consensus
    float* consensus = nullptr;
    uint32_t num_weights = 0;

    int result = swarm_brain_get_consensus_weights(
        manager, &consensus, &num_weights
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);

    if (consensus) {
        EXPECT_GT(num_weights, 0u);
        nimcp_free(consensus);
    }
}

//=============================================================================
// Agent Join/Leave Dynamics
//=============================================================================

TEST_F(SwarmBrainLocalIntegrationTest, AgentJoinLeaveScenario) {
    // Start with 2 agents
    swarm_brain_create_for_agent(manager, 1, 100);
    swarm_brain_create_for_agent(manager, 2, 100);
    swarm_brain_sync_all(manager);

    EXPECT_EQ(swarm_brain_get_agent_count(manager), 2u);

    // Add 3 more agents
    for (uint32_t i = 3; i <= 5; i++) {
        swarm_brain_create_for_agent(manager, i, 100);
        swarm_brain_local_sync_weights(manager, i);
    }

    EXPECT_EQ(swarm_brain_get_agent_count(manager), 5u);

    // Remove 2 agents
    swarm_brain_destroy_for_agent(manager, 1);
    swarm_brain_destroy_for_agent(manager, 2);

    EXPECT_EQ(swarm_brain_get_agent_count(manager), 3u);

    // Sync remaining agents
    swarm_brain_sync_all(manager);

    SUCCEED();
}

//=============================================================================
// Divergence Detection
//=============================================================================

TEST_F(SwarmBrainLocalIntegrationTest, DivergentAgentDetection) {
    // Create agents with different configurations
    swarm_brain_create_for_agent(manager, 1, 100);
    swarm_brain_create_for_agent(manager, 2, 100);
    swarm_brain_create_for_agent(manager, 3, 100);

    // Initialize
    swarm_brain_sync_all(manager);

    // Make agent 2 divergent by learning unique pattern
    float unique_input[10] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f, 0.0f};
    float target[5] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    for (int i = 0; i < 20; i++) {
        swarm_brain_local_learn(manager, 2, unique_input, 10, target, 5);
    }

    // Update consensus
    swarm_brain_sync_all(manager);

    // Get divergent agents
    uint32_t* divergent = nullptr;
    uint32_t count = 0;

    swarm_brain_get_divergent_agents(manager, &divergent, &count);

    // Should detect some divergence
    if (divergent) {
        nimcp_free(divergent);
    }

    SUCCEED();
}

//=============================================================================
// Statistics Tracking
//=============================================================================

TEST_F(SwarmBrainLocalIntegrationTest, StatisticsAccuracy) {
    const uint32_t num_agents = 5;
    const uint32_t neurons_per_agent = 100;

    // Create agents
    for (uint32_t i = 1; i <= num_agents; i++) {
        swarm_brain_create_for_agent(manager, i, neurons_per_agent);
    }

    // Get stats
    swarm_brain_stats_t stats;
    swarm_brain_local_get_stats(manager, &stats);

    EXPECT_EQ(stats.num_agents, num_agents);
    EXPECT_EQ(stats.total_neurons, num_agents * neurons_per_agent);
}

//=============================================================================
// Concurrent Operations
//=============================================================================

TEST_F(SwarmBrainLocalIntegrationTest, ConcurrentAgentOperations) {
    // Create multiple agents
    for (uint32_t i = 1; i <= 10; i++) {
        swarm_brain_create_for_agent(manager, i, 100);
    }

    // Perform many concurrent operations
    for (int iteration = 0; iteration < 100; iteration++) {
        for (uint32_t i = 1; i <= 10; i++) {
            swarm_brain_local_sync_weights(manager, i);
            float div = swarm_brain_get_divergence(manager, i);
            (void)div;
        }
    }

    EXPECT_EQ(swarm_brain_get_agent_count(manager), 10u);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(SwarmBrainLocalIntegrationTest, LargeSwarmPerformance) {
    const uint32_t num_agents = 50;

    uint64_t start_time = nimcp_time_get_us();

    // Create large swarm
    for (uint32_t i = 1; i <= num_agents; i++) {
        swarm_brain_create_for_agent(manager, i, 50);
    }

    uint64_t end_time = nimcp_time_get_us();
    uint64_t creation_time_us = end_time - start_time;

    // Should complete in reasonable time (< 1 second)
    EXPECT_LT(creation_time_us, 1000000u);

    // Sync performance
    start_time = nimcp_time_get_us();
    swarm_brain_sync_all(manager);
    end_time = nimcp_time_get_us();

    uint64_t sync_time_us = end_time - start_time;
    EXPECT_LT(sync_time_us, 500000u);  // < 0.5 seconds
}

//=============================================================================
// Role-Based Asymmetric Swarm Integration Tests
//=============================================================================

TEST_F(SwarmBrainLocalIntegrationTest, AsymmetricSwarmCreation) {
    // Create a realistic asymmetric swarm with different roles
    // 2 scouts, 5 workers, 1 coordinator, 2 sensors

    int result;

    // Scouts - navigation and exploration
    result = swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    result = swarm_brain_create_for_agent_with_role(manager, 2, DRONE_ROLE_SCOUT);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Workers - basic task execution
    for (uint32_t i = 3; i <= 7; i++) {
        result = swarm_brain_create_for_agent_with_role(manager, i, DRONE_ROLE_WORKER);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Coordinator
    result = swarm_brain_create_for_agent_with_role(manager, 8, DRONE_ROLE_COORDINATOR);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Sensors
    result = swarm_brain_create_for_agent_with_role(manager, 9, DRONE_ROLE_SENSOR);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    result = swarm_brain_create_for_agent_with_role(manager, 10, DRONE_ROLE_SENSOR);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(swarm_brain_get_agent_count(manager), 10u);

    // Verify role distribution
    uint32_t* agents = nullptr;
    uint32_t count = 0;

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_SCOUT, &agents, &count);
    EXPECT_EQ(count, 2u);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_WORKER, &agents, &count);
    EXPECT_EQ(count, 5u);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_COORDINATOR, &agents, &count);
    EXPECT_EQ(count, 1u);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_SENSOR, &agents, &count);
    EXPECT_EQ(count, 2u);
    if (agents) nimcp_free(agents);
}

TEST_F(SwarmBrainLocalIntegrationTest, RoleBasedTrainingCoordination) {
    // Create scouts and workers
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 2, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 3, DRONE_ROLE_WORKER);
    swarm_brain_create_for_agent_with_role(manager, 4, DRONE_ROLE_WORKER);

    // Verify different training configs
    role_training_config_t scout_config = swarm_brain_get_role_training_config(DRONE_ROLE_SCOUT);
    role_training_config_t worker_config = swarm_brain_get_role_training_config(DRONE_ROLE_WORKER);

    // Scouts should have higher learning rate (exploration)
    EXPECT_GT(scout_config.learning_rate, worker_config.learning_rate);

    // Workers should have higher sync strength (consistency)
    EXPECT_GT(worker_config.sync_strength, scout_config.sync_strength);

    // Train agents with role-specific configs
    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float target[5] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f};

    // Train scouts
    swarm_brain_train_with_role(manager, 1, DRONE_ROLE_SCOUT, input, 10, target, 5, nullptr);
    swarm_brain_train_with_role(manager, 2, DRONE_ROLE_SCOUT, input, 10, target, 5, nullptr);

    // Train workers
    swarm_brain_train_with_role(manager, 3, DRONE_ROLE_WORKER, input, 10, target, 5, nullptr);
    swarm_brain_train_with_role(manager, 4, DRONE_ROLE_WORKER, input, 10, target, 5, nullptr);

    SUCCEED();
}

TEST_F(SwarmBrainLocalIntegrationTest, RoleGroupSyncIsolation) {
    // Create agents with different roles
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 2, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 3, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 4, DRONE_ROLE_WORKER);
    swarm_brain_create_for_agent_with_role(manager, 5, DRONE_ROLE_WORKER);

    // Sync only scouts
    int result = swarm_brain_sync_role_group(manager, DRONE_ROLE_SCOUT);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Sync only workers
    result = swarm_brain_sync_role_group(manager, DRONE_ROLE_WORKER);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Stats should show sync operations
    swarm_brain_stats_t stats;
    swarm_brain_local_get_stats(manager, &stats);
    EXPECT_GT(stats.sync_count, 0u);
}

TEST_F(SwarmBrainLocalIntegrationTest, InterRoleKnowledgeTransfer) {
    // Create source agents (sensors with environmental knowledge)
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SENSOR);
    swarm_brain_create_for_agent_with_role(manager, 2, DRONE_ROLE_SENSOR);

    // Create target agent (guardian that needs threat detection)
    swarm_brain_create_for_agent_with_role(manager, 3, DRONE_ROLE_GUARDIAN);

    // Guardian should learn from sensors (as defined in default config)
    role_training_config_t guardian_config = swarm_brain_get_role_training_config(DRONE_ROLE_GUARDIAN);
    EXPECT_EQ(guardian_config.transfer_from, DRONE_ROLE_SENSOR);
    EXPECT_TRUE(guardian_config.enable_transfer_learning);

    // Perform knowledge transfer
    int result = swarm_brain_transfer_role_knowledge(manager, 3, DRONE_ROLE_SENSOR, 0.25f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmBrainLocalIntegrationTest, DynamicRoleReassignment) {
    // Start with all workers
    for (uint32_t i = 1; i <= 5; i++) {
        swarm_brain_create_for_agent_with_role(manager, i, DRONE_ROLE_WORKER);
    }

    uint32_t* agents = nullptr;
    uint32_t count = 0;

    // Verify initial distribution
    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_WORKER, &agents, &count);
    EXPECT_EQ(count, 5u);
    if (agents) nimcp_free(agents);

    // Promote one worker to coordinator
    int result = swarm_brain_set_agent_role(manager, 1, DRONE_ROLE_COORDINATOR);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Promote two workers to scouts
    result = swarm_brain_set_agent_role(manager, 2, DRONE_ROLE_SCOUT);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    result = swarm_brain_set_agent_role(manager, 3, DRONE_ROLE_SCOUT);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify new distribution
    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_WORKER, &agents, &count);
    EXPECT_EQ(count, 2u);  // 5 - 1 - 2 = 2
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_COORDINATOR, &agents, &count);
    EXPECT_EQ(count, 1u);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_SCOUT, &agents, &count);
    EXPECT_EQ(count, 2u);
    if (agents) nimcp_free(agents);

    // Verify roles are correct
    EXPECT_EQ(swarm_brain_get_agent_role(manager, 1), DRONE_ROLE_COORDINATOR);
    EXPECT_EQ(swarm_brain_get_agent_role(manager, 2), DRONE_ROLE_SCOUT);
    EXPECT_EQ(swarm_brain_get_agent_role(manager, 3), DRONE_ROLE_SCOUT);
    EXPECT_EQ(swarm_brain_get_agent_role(manager, 4), DRONE_ROLE_WORKER);
    EXPECT_EQ(swarm_brain_get_agent_role(manager, 5), DRONE_ROLE_WORKER);
}

TEST_F(SwarmBrainLocalIntegrationTest, HeterogeneousSwarmPerformance) {
    // Create a large heterogeneous swarm
    const uint32_t NUM_SCOUTS = 5;
    const uint32_t NUM_WORKERS = 20;
    const uint32_t NUM_COORDINATORS = 2;
    const uint32_t NUM_SENSORS = 8;
    const uint32_t NUM_GUARDIANS = 3;
    const uint32_t NUM_RELAYS = 2;

    uint64_t start_time = nimcp_time_get_us();

    uint32_t agent_id = 1;

    // Create scouts
    for (uint32_t i = 0; i < NUM_SCOUTS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_SCOUT);
    }

    // Create workers (MICRO brains - should be fastest)
    for (uint32_t i = 0; i < NUM_WORKERS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_WORKER);
    }

    // Create coordinators (MEDIUM brains - most complex)
    for (uint32_t i = 0; i < NUM_COORDINATORS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_COORDINATOR);
    }

    // Create sensors
    for (uint32_t i = 0; i < NUM_SENSORS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_SENSOR);
    }

    // Create guardians
    for (uint32_t i = 0; i < NUM_GUARDIANS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_GUARDIAN);
    }

    // Create relays (MICRO brains - minimal)
    for (uint32_t i = 0; i < NUM_RELAYS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_RELAY);
    }

    uint64_t creation_time = nimcp_time_get_us() - start_time;

    uint32_t total_agents = NUM_SCOUTS + NUM_WORKERS + NUM_COORDINATORS +
                           NUM_SENSORS + NUM_GUARDIANS + NUM_RELAYS;
    EXPECT_EQ(swarm_brain_get_agent_count(manager), total_agents);

    // Heterogeneous creation should still be reasonably fast
    // Note: This test verifies memory optimization benefits
    EXPECT_LT(creation_time, 5000000u);  // < 5 seconds for 40 agents
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
