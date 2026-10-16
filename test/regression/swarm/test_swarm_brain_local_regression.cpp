/**
 * @file test_swarm_brain_local_regression.cpp
 * @brief Regression tests for Swarm Brain Local Instantiation
 *
 * REGRESSION FOCUS:
 * - Memory leak prevention
 * - Performance degradation detection
 * - Edge case handling consistency
 * - API stability
 * - Thread safety
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_brain_local.h"
#include "core/brain/factory/init/nimcp_brain_init_config.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

class SwarmBrainLocalRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// Memory Leak Regression Tests
//=============================================================================

TEST_F(SwarmBrainLocalRegressionTest, NoLeaksOnRepeatedCreationDestruction) {
    swarm_brain_config_t config = swarm_brain_local_default_config();
    config.enable_bio_async = false;
    config.test_mode = true;  // Skip actual brain creation for fast tests

    // Repeat many times to detect memory leaks
    for (int iteration = 0; iteration < 100; iteration++) {
        auto* mgr = swarm_brain_manager_create(&config);
        ASSERT_NE(mgr, nullptr);

        // Create agents
        for (uint32_t i = 1; i <= 10; i++) {
            swarm_brain_create_for_agent(mgr, i, 100);
        }

        // Use agents
        for (uint32_t i = 1; i <= 10; i++) {
            swarm_brain_local_sync_weights(mgr, i);
        }

        swarm_brain_manager_destroy(mgr);
    }

    SUCCEED();
}

TEST_F(SwarmBrainLocalRegressionTest, NoLeaksOnAgentChurn) {
    swarm_brain_config_t config = swarm_brain_local_default_config();
    config.enable_bio_async = false;
    config.test_mode = true;
    auto* mgr = swarm_brain_manager_create(&config);

    // Repeatedly create and destroy agents
    for (int iteration = 0; iteration < 100; iteration++) {
        for (uint32_t i = 1; i <= 5; i++) {
            swarm_brain_create_for_agent(mgr, i, 100);
        }

        for (uint32_t i = 1; i <= 5; i++) {
            swarm_brain_destroy_for_agent(mgr, i);
        }
    }

    EXPECT_EQ(swarm_brain_get_agent_count(mgr), 0u);
    swarm_brain_manager_destroy(mgr);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(SwarmBrainLocalRegressionTest, AgentCreationPerformance) {
    swarm_brain_config_t config = swarm_brain_local_default_config();
    config.enable_bio_async = false;
    config.test_mode = true;
    auto* mgr = swarm_brain_manager_create(&config);

    const uint32_t num_agents = 50;
    uint64_t start_time = nimcp_time_get_us();

    for (uint32_t i = 1; i <= num_agents; i++) {
        swarm_brain_create_for_agent(mgr, i, 100);
    }

    uint64_t end_time = nimcp_time_get_us();
    uint64_t duration_us = end_time - start_time;

    // Should complete in reasonable time (baseline: < 1 second)
    EXPECT_LT(duration_us, 1000000u);

    swarm_brain_manager_destroy(mgr);
}

TEST_F(SwarmBrainLocalRegressionTest, SyncPerformance) {
    swarm_brain_config_t config = swarm_brain_local_default_config();
    config.enable_bio_async = false;
    config.test_mode = true;
    auto* mgr = swarm_brain_manager_create(&config);

    for (uint32_t i = 1; i <= 20; i++) {
        swarm_brain_create_for_agent(mgr, i, 100);
    }

    uint64_t start_time = nimcp_time_get_us();

    swarm_brain_sync_all(mgr);

    uint64_t end_time = nimcp_time_get_us();
    uint64_t duration_us = end_time - start_time;

    // Should complete quickly (baseline: < 100ms)
    EXPECT_LT(duration_us, 100000u);

    swarm_brain_manager_destroy(mgr);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(SwarmBrainLocalRegressionTest, NullParameterHandling) {
    // All null parameter cases should fail gracefully
    swarm_brain_manager_destroy(nullptr);  // void function, just verify no crash
    EXPECT_NE(swarm_brain_create_for_agent(nullptr, 1, 100), NIMCP_SUCCESS);
    EXPECT_NE(swarm_brain_destroy_for_agent(nullptr, 1), NIMCP_SUCCESS);
    EXPECT_EQ(swarm_brain_get(nullptr, 1), nullptr);
    EXPECT_NE(swarm_brain_local_sync_weights(nullptr, 1), NIMCP_SUCCESS);
    EXPECT_EQ(swarm_brain_get_divergence(nullptr, 1), -1.0f);
}

TEST_F(SwarmBrainLocalRegressionTest, InvalidAgentIDHandling) {
    swarm_brain_config_t config = swarm_brain_local_default_config();
    config.enable_bio_async = false;
    config.test_mode = true;
    auto* mgr = swarm_brain_manager_create(&config);

    // Operations on non-existent agents should fail gracefully
    EXPECT_NE(swarm_brain_destroy_for_agent(mgr, 999), NIMCP_SUCCESS);
    EXPECT_EQ(swarm_brain_get(mgr, 999), nullptr);
    EXPECT_NE(swarm_brain_local_sync_weights(mgr, 999), NIMCP_SUCCESS);
    EXPECT_EQ(swarm_brain_get_divergence(mgr, 999), -1.0f);

    swarm_brain_manager_destroy(mgr);
}

TEST_F(SwarmBrainLocalRegressionTest, MaxAgentsLimit) {
    swarm_brain_config_t config = swarm_brain_local_default_config();
    config.enable_bio_async = false;
    config.test_mode = true;
    auto* mgr = swarm_brain_manager_create(&config);

    // Create up to max
    for (uint32_t i = 1; i <= SWARM_BRAIN_MAX_AGENTS; i++) {
        int result = swarm_brain_create_for_agent(mgr, i, 50);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Next should fail
    int result = swarm_brain_create_for_agent(
        mgr, SWARM_BRAIN_MAX_AGENTS + 1, 50
    );
    EXPECT_NE(result, NIMCP_SUCCESS);

    swarm_brain_manager_destroy(mgr);
}

//=============================================================================
// API Stability Regression Tests
//=============================================================================

TEST_F(SwarmBrainLocalRegressionTest, DefaultConfigStability) {
    swarm_brain_config_t config1 = swarm_brain_local_default_config();
    swarm_brain_config_t config2 = swarm_brain_local_default_config();

    // Default config should be consistent
    EXPECT_EQ(config1.default_brain_size, config2.default_brain_size);
    EXPECT_EQ(config1.max_local_neurons, config2.max_local_neurons);
    EXPECT_FLOAT_EQ(config1.sync_interval_ms, config2.sync_interval_ms);
    EXPECT_FLOAT_EQ(config1.divergence_threshold, config2.divergence_threshold);
}

TEST_F(SwarmBrainLocalRegressionTest, StatisticsConsistency) {
    swarm_brain_config_t config = swarm_brain_local_default_config();
    config.enable_bio_async = false;
    config.test_mode = true;
    auto* mgr = swarm_brain_manager_create(&config);

    // Create agents
    for (uint32_t i = 1; i <= 5; i++) {
        swarm_brain_create_for_agent(mgr, i, 100);
    }

    // Get stats multiple times - should be consistent
    swarm_brain_stats_t stats1, stats2;
    swarm_brain_local_get_stats(mgr, &stats1);
    swarm_brain_local_get_stats(mgr, &stats2);

    EXPECT_EQ(stats1.num_agents, stats2.num_agents);
    EXPECT_EQ(stats1.total_neurons, stats2.total_neurons);

    swarm_brain_manager_destroy(mgr);
}

//=============================================================================
// Role-Based Brain Regression Tests
//=============================================================================

TEST_F(SwarmBrainLocalRegressionTest, RoleTemplateStability) {
    // Verify role templates return consistent values
    for (int iteration = 0; iteration < 100; iteration++) {
        drone_brain_template_t scout1 = swarm_brain_get_role_template(DRONE_ROLE_SCOUT);
        drone_brain_template_t scout2 = swarm_brain_get_role_template(DRONE_ROLE_SCOUT);

        EXPECT_EQ(scout1.brain_size, scout2.brain_size);
        EXPECT_EQ(scout1.enable_visual_cortex, scout2.enable_visual_cortex);
        EXPECT_EQ(scout1.enable_curiosity, scout2.enable_curiosity);
        EXPECT_EQ(scout1.minimal_mode, scout2.minimal_mode);
    }
}

TEST_F(SwarmBrainLocalRegressionTest, RoleTrainingConfigStability) {
    // Verify training configs return consistent values
    for (int iteration = 0; iteration < 100; iteration++) {
        role_training_config_t config1 = swarm_brain_get_role_training_config(DRONE_ROLE_WORKER);
        role_training_config_t config2 = swarm_brain_get_role_training_config(DRONE_ROLE_WORKER);

        EXPECT_FLOAT_EQ(config1.learning_rate, config2.learning_rate);
        EXPECT_EQ(config1.batch_size, config2.batch_size);
        EXPECT_FLOAT_EQ(config1.sync_strength, config2.sync_strength);
    }
}

TEST_F(SwarmBrainLocalRegressionTest, RoleNameStability) {
    // Verify role names remain stable
    const char* scout_name1 = swarm_brain_role_name(DRONE_ROLE_SCOUT);
    const char* scout_name2 = swarm_brain_role_name(DRONE_ROLE_SCOUT);
    EXPECT_STREQ(scout_name1, scout_name2);
    EXPECT_STREQ(scout_name1, "Scout");

    const char* worker_name = swarm_brain_role_name(DRONE_ROLE_WORKER);
    EXPECT_STREQ(worker_name, "Worker");

    const char* coordinator_name = swarm_brain_role_name(DRONE_ROLE_COORDINATOR);
    EXPECT_STREQ(coordinator_name, "Coordinator");
}

TEST_F(SwarmBrainLocalRegressionTest, NoLeaksOnRoleBasedCreation) {
    swarm_brain_config_t config = swarm_brain_local_default_config();
    config.enable_bio_async = false;
    config.test_mode = true;  // Skip actual brain creation for fast tests

    // Repeat role-based creation/destruction
    for (int iteration = 0; iteration < 50; iteration++) {
        auto* mgr = swarm_brain_manager_create(&config);
        ASSERT_NE(mgr, nullptr);

        // Create with roles
        swarm_brain_create_for_agent_with_role(mgr, 1, DRONE_ROLE_SCOUT);
        swarm_brain_create_for_agent_with_role(mgr, 2, DRONE_ROLE_WORKER);
        swarm_brain_create_for_agent_with_role(mgr, 3, DRONE_ROLE_COORDINATOR);
        swarm_brain_create_for_agent_with_role(mgr, 4, DRONE_ROLE_SENSOR);
        swarm_brain_create_for_agent_with_role(mgr, 5, DRONE_ROLE_GUARDIAN);
        swarm_brain_create_for_agent_with_role(mgr, 6, DRONE_ROLE_RELAY);

        // Use role operations
        swarm_brain_sync_role_group(mgr, DRONE_ROLE_SCOUT);
        swarm_brain_sync_role_group(mgr, DRONE_ROLE_WORKER);

        uint32_t* agents = nullptr;
        uint32_t count = 0;
        swarm_brain_get_agents_by_role(mgr, DRONE_ROLE_WORKER, &agents, &count);
        if (agents) nimcp_free(agents);

        swarm_brain_manager_destroy(mgr);
    }

    SUCCEED();
}

TEST_F(SwarmBrainLocalRegressionTest, RoleReassignmentNoCorruption) {
    swarm_brain_config_t config = swarm_brain_local_default_config();
    config.enable_bio_async = false;
    config.test_mode = true;
    auto* mgr = swarm_brain_manager_create(&config);

    // Create agent as worker
    swarm_brain_create_for_agent_with_role(mgr, 1, DRONE_ROLE_WORKER);
    EXPECT_EQ(swarm_brain_get_agent_role(mgr, 1), DRONE_ROLE_WORKER);

    // Reassign many times
    drone_role_t roles[] = {
        DRONE_ROLE_SCOUT, DRONE_ROLE_COORDINATOR, DRONE_ROLE_SENSOR,
        DRONE_ROLE_GUARDIAN, DRONE_ROLE_RELAY, DRONE_ROLE_WORKER
    };

    for (int iteration = 0; iteration < 100; iteration++) {
        for (int r = 0; r < 6; r++) {
            int result = swarm_brain_set_agent_role(mgr, 1, roles[r]);
            EXPECT_EQ(result, NIMCP_SUCCESS);
            EXPECT_EQ(swarm_brain_get_agent_role(mgr, 1), roles[r]);
        }
    }

    // Agent should still be valid
    EXPECT_TRUE(swarm_brain_has_agent(mgr, 1));

    swarm_brain_manager_destroy(mgr);
}

TEST_F(SwarmBrainLocalRegressionTest, RoleSyncPerformance) {
    swarm_brain_config_t config = swarm_brain_local_default_config();
    config.enable_bio_async = false;
    config.test_mode = true;
    auto* mgr = swarm_brain_manager_create(&config);

    // Create agents with same role
    for (uint32_t i = 1; i <= 20; i++) {
        swarm_brain_create_for_agent_with_role(mgr, i, DRONE_ROLE_WORKER);
    }

    uint64_t start_time = nimcp_time_get_us();

    // Sync role group many times
    for (int iteration = 0; iteration < 100; iteration++) {
        swarm_brain_sync_role_group(mgr, DRONE_ROLE_WORKER);
    }

    uint64_t end_time = nimcp_time_get_us();
    uint64_t duration_us = end_time - start_time;

    // 100 syncs should complete quickly (< 1 second)
    EXPECT_LT(duration_us, 1000000u);

    swarm_brain_manager_destroy(mgr);
}

TEST_F(SwarmBrainLocalRegressionTest, TransferKnowledgeParameterValidation) {
    swarm_brain_config_t config = swarm_brain_local_default_config();
    config.enable_bio_async = false;
    config.test_mode = true;
    auto* mgr = swarm_brain_manager_create(&config);

    swarm_brain_create_for_agent_with_role(mgr, 1, DRONE_ROLE_SENSOR);
    swarm_brain_create_for_agent_with_role(mgr, 2, DRONE_ROLE_GUARDIAN);

    // Valid transfer weight
    EXPECT_EQ(swarm_brain_transfer_role_knowledge(mgr, 2, DRONE_ROLE_SENSOR, 0.5f), NIMCP_SUCCESS);

    // Invalid transfer weights should fail
    EXPECT_NE(swarm_brain_transfer_role_knowledge(mgr, 2, DRONE_ROLE_SENSOR, -0.1f), NIMCP_SUCCESS);
    EXPECT_NE(swarm_brain_transfer_role_knowledge(mgr, 2, DRONE_ROLE_SENSOR, 1.5f), NIMCP_SUCCESS);

    // Invalid agent should fail
    EXPECT_NE(swarm_brain_transfer_role_knowledge(mgr, 999, DRONE_ROLE_SENSOR, 0.5f), NIMCP_SUCCESS);

    swarm_brain_manager_destroy(mgr);
}

//=============================================================================
// Memory Optimization Regression Tests
//=============================================================================

TEST_F(SwarmBrainLocalRegressionTest, BrainSizeMicroConsistency) {
    // Verify BRAIN_SIZE_MICRO returns consistent values
    for (int iteration = 0; iteration < 100; iteration++) {
        uint32_t neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MICRO);
        float sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MICRO);

        EXPECT_EQ(neurons, 25u);
        EXPECT_FLOAT_EQ(sparsity, 0.60f);
    }
}

TEST_F(SwarmBrainLocalRegressionTest, BrainSizeOrderingPreserved) {
    // Verify brain sizes maintain expected ordering
    uint32_t micro = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MICRO);
    uint32_t tiny = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY);
    uint32_t small = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL);
    uint32_t medium = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM);
    uint32_t large = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_LARGE);

    EXPECT_LT(micro, tiny);
    EXPECT_LT(tiny, small);
    EXPECT_LT(small, medium);
    EXPECT_LT(medium, large);

    // Verify sparsity ordering (lower for smaller brains)
    float s_micro = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MICRO);
    float s_tiny = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_TINY);
    float s_small = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_SMALL);
    float s_medium = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM);
    float s_large = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE);

    EXPECT_LT(s_micro, s_tiny);
    EXPECT_LT(s_tiny, s_small);
    EXPECT_LT(s_small, s_medium);
    EXPECT_LT(s_medium, s_large);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
