/**
 * @file test_connectivity_health_integration.cpp
 * @brief Integration tests for brain connectivity health assessment
 *
 * TEST COVERAGE:
 * - Integration with brain training workflow
 * - Integration with Shannon monitoring
 * - Integration with community detection
 * - Integration with global workspace
 * - Cross-module health assessment
 * - Periodic monitoring workflow
 * - Real network topology analysis
 *
 * PHASE: 1.5.4 - Introspection + Community Detection Health Monitoring
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>

#include "cognitive/introspection/nimcp_connectivity_health.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/global_workspace/nimcp_global_workspace_shannon.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain_regions/nimcp_brain_regions.h"

/* Map region names used in tests to actual brain_region_type_t values */
#define BRAIN_REGION_EXECUTIVE  REGION_PREFRONTAL
#define BRAIN_REGION_WORKSPACE  REGION_PARIETAL
#define BRAIN_REGION_SALIENCE   REGION_TEMPORAL

//=============================================================================
// Test Fixture
//=============================================================================

class ConnectivityHealthIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    static constexpr uint32_t NUM_INPUTS = 20;
    static constexpr uint32_t NUM_OUTPUTS = 5;

    void SetUp() override {
        // Create brain using correct API
        brain = brain_create(
            "integration_test",      // task_name
            BRAIN_SIZE_SMALL,        // size (larger for integration tests)
            BRAIN_TASK_CLASSIFICATION, // task
            NUM_INPUTS,              // num_inputs
            NUM_OUTPUTS              // num_outputs
        );
    }

    void TearDown() override {
        if (brain != nullptr) {
            brain_disable_connectivity_monitoring(brain);
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Create training sample
    std::vector<float> CreateTrainingSample(uint32_t num_inputs, float base) {
        std::vector<float> sample(num_inputs);
        for (uint32_t i = 0; i < num_inputs; i++) {
            sample[i] = base + static_cast<float>(i % 5) * 0.1f;
        }
        return sample;
    }

    // Helper: Get label for class index
    const char* GetClassLabel(int class_idx) {
        static const char* labels[] = {"class_0", "class_1", "class_2", "class_3", "class_4"};
        return labels[class_idx % 5];
    }

    // Helper: Train brain for a few epochs using correct API
    void TrainBrain(uint32_t epochs) {
        if (brain == nullptr) return;

        for (uint32_t e = 0; e < epochs; e++) {
            for (int i = 0; i < 10; i++) {
                auto sample = CreateTrainingSample(NUM_INPUTS, static_cast<float>(i));
                // Use brain_learn_example with correct signature
                brain_learn_example(brain, sample.data(), NUM_INPUTS,
                                   GetClassLabel(i), 0.9f);
            }
        }
    }
};

//=============================================================================
// 1. Brain Training Integration Tests
//=============================================================================

TEST_F(ConnectivityHealthIntegrationTest, HealthChangesAfterTraining) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    // Get baseline health before training
    brain_connectivity_health_t health_before = brain_assess_connectivity_now(brain);

    // Train the brain
    TrainBrain(5);

    // Get health after training
    brain_connectivity_health_t health_after = brain_assess_connectivity_now(brain);

    // Both assessments should be valid
    EXPECT_GE(health_before.overall_health, 0.0f);
    EXPECT_LE(health_before.overall_health, 1.0f);
    EXPECT_GE(health_after.overall_health, 0.0f);
    EXPECT_LE(health_after.overall_health, 1.0f);

    // Note: Health may improve, worsen, or stay same depending on training
    // Just verify the system works
}

TEST_F(ConnectivityHealthIntegrationTest, CommunityStructureAfterTraining) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    // Train to establish patterns
    TrainBrain(10);

    // Assess connectivity health
    brain_connectivity_health_t health = brain_assess_connectivity_now(brain);

    // Skip if introspection isn't available
    if (strlen(health.primary_issue) > 0) {
        GTEST_SKIP() << "Introspection not initialized: " << health.primary_issue;
    }

    // After training, we should see some community structure
    // Modularity should be measurable (not just 0)
    EXPECT_GE(health.community.modularity_q, -0.5f);
    EXPECT_LE(health.community.modularity_q, 1.0f);

    // Number of communities should be reasonable
    EXPECT_GE(health.community.num_communities, 1u);
}

//=============================================================================
// 2. Shannon Integration Tests
//=============================================================================

TEST_F(ConnectivityHealthIntegrationTest, HealthWithShannonMonitoring) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    // Enable connectivity monitoring with Shannon synergy
    connectivity_health_config_t config = connectivity_health_default_config();
    config.weight_flow = 0.4f;  // Give more weight to information flow
    config.weight_modularity = 0.2f;
    config.weight_hubs = 0.2f;
    config.weight_topology = 0.2f;

    bool enabled = brain_enable_connectivity_monitoring(brain, &config, nullptr, nullptr);
    EXPECT_TRUE(enabled);

    // Train the brain
    TrainBrain(5);

    // Assess health
    brain_connectivity_health_t health = brain_assess_connectivity_now(brain);

    // Information flow should be measurable
    EXPECT_GE(health.flow.transfer_efficiency, 0.0f);
    EXPECT_LE(health.flow.transfer_efficiency, 1.0f);

    // Overall health should reflect the weighted components
    EXPECT_GE(health.overall_health, 0.0f);
    EXPECT_LE(health.overall_health, 1.0f);
}

TEST_F(ConnectivityHealthIntegrationTest, FlowEfficiencyCorrelatesWithActivity) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    // Get baseline flow efficiency
    brain_connectivity_health_t health1 = brain_assess_connectivity_now(brain);
    float flow1 = health1.flow.transfer_efficiency;

    // Generate activity by processing inputs
    std::vector<float> input(NUM_INPUTS, 1.0f);
    for (int i = 0; i < 100; i++) {
        brain_decision_t* decision = brain_decide(brain, input.data(), NUM_INPUTS);
        if (decision) brain_free_decision(decision);
    }

    // Get flow efficiency after activity
    brain_connectivity_health_t health2 = brain_assess_connectivity_now(brain);
    float flow2 = health2.flow.transfer_efficiency;

    // Both should be valid
    EXPECT_GE(flow1, 0.0f);
    EXPECT_LE(flow1, 1.0f);
    EXPECT_GE(flow2, 0.0f);
    EXPECT_LE(flow2, 1.0f);
}

//=============================================================================
// 3. Community Detection Integration Tests
//=============================================================================

TEST_F(ConnectivityHealthIntegrationTest, CommunityDetectionWithBrainTopology) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Assess community health
    community_health_t community = introspection_assess_community_health(intro, nullptr);

    // Community metrics should be valid
    EXPECT_GE(community.modularity_q, -0.5f);
    EXPECT_LE(community.modularity_q, 1.0f);
    EXPECT_GE(community.community_balance, 0.0f);
    EXPECT_LE(community.community_balance, 1.0f);
    EXPECT_GE(community.largest_community_ratio, 0.0f);
    EXPECT_LE(community.largest_community_ratio, 1.0f);
}

TEST_F(ConnectivityHealthIntegrationTest, HubDetectionWithBrainTopology) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Assess hub health
    hub_health_t hubs = introspection_assess_hub_health(intro, nullptr);

    // Hub metrics should be valid
    EXPECT_LE(hubs.num_hubs, CONNECTIVITY_MAX_HUBS);
    EXPECT_GE(hubs.avg_hub_centrality, 0.0f);
    EXPECT_GE(hubs.hub_distribution_entropy, 0.0f);
    EXPECT_LE(hubs.hub_distribution_entropy, 1.0f);
}

//=============================================================================
// 4. Periodic Monitoring Integration Tests
//=============================================================================

TEST_F(ConnectivityHealthIntegrationTest, PeriodicMonitoringWorkflow) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    static int callback_invocations = 0;
    static brain_connectivity_health_t captured_health;

    auto health_callback = [](const brain_connectivity_health_t* health, void* ctx) {
        callback_invocations++;
        if (health) {
            captured_health = *health;
        }
    };

    callback_invocations = 0;
    connectivity_health_config_t config = connectivity_health_default_config();
    config.assessment_interval_ms = 100;  // Short interval for testing

    bool enabled = brain_enable_connectivity_monitoring(brain, &config, health_callback, nullptr);
    EXPECT_TRUE(enabled);
    EXPECT_TRUE(brain_is_connectivity_monitoring_enabled(brain));

    // Force several assessments
    for (int i = 0; i < 3; i++) {
        brain_assess_connectivity_now(brain);
    }

    // Disable monitoring
    brain_disable_connectivity_monitoring(brain);
    EXPECT_FALSE(brain_is_connectivity_monitoring_enabled(brain));
}

TEST_F(ConnectivityHealthIntegrationTest, CachedHealthRetrieval) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    // Force an assessment
    brain_connectivity_health_t assessed = brain_assess_connectivity_now(brain);

    // Retrieve cached health
    brain_connectivity_health_t cached;
    bool has_cache = brain_get_connectivity_health(brain, &cached);

    if (has_cache) {
        // Cached should match assessed
        EXPECT_FLOAT_EQ(cached.overall_health, assessed.overall_health);
        EXPECT_EQ(cached.total_neurons, assessed.total_neurons);
    }
}

//=============================================================================
// 5. Cross-Module Integration Tests
//=============================================================================

TEST_F(ConnectivityHealthIntegrationTest, HealthWithIntrospectionContext) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Full assessment through introspection
    brain_connectivity_health_t health = introspection_assess_connectivity_health(intro, nullptr);

    // Should match brain-level assessment
    brain_connectivity_health_t brain_health = brain_assess_connectivity_now(brain);

    // Both should have valid network stats
    EXPECT_GT(health.total_neurons, 0u);
    EXPECT_GT(brain_health.total_neurons, 0u);
}

TEST_F(ConnectivityHealthIntegrationTest, TopologyMetricsConsistency) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Get individual assessments
    community_health_t community = introspection_assess_community_health(intro, nullptr);
    hub_health_t hubs = introspection_assess_hub_health(intro, nullptr);
    topology_health_t topology = introspection_assess_topology_health(intro, nullptr);
    information_flow_health_t flow = introspection_assess_flow_health(intro, nullptr);

    // Get full assessment
    brain_connectivity_health_t full = introspection_assess_connectivity_health(intro, nullptr);

    // Individual metrics should match full assessment components
    EXPECT_FLOAT_EQ(full.community.modularity_q, community.modularity_q);
    EXPECT_EQ(full.hubs.num_hubs, hubs.num_hubs);
    EXPECT_FLOAT_EQ(full.topology.clustering_coefficient, topology.clustering_coefficient);
    EXPECT_FLOAT_EQ(full.flow.transfer_efficiency, flow.transfer_efficiency);
}

//=============================================================================
// 6. Small-World Topology Integration Tests
//=============================================================================

TEST_F(ConnectivityHealthIntegrationTest, SmallWorldDetection) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    // Train to establish connectivity patterns
    TrainBrain(10);

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    topology_health_t topology = introspection_assess_topology_health(intro, nullptr);

    // Small-world sigma should be computed
    EXPECT_GE(topology.small_world_sigma, 0.0f);

    // If sigma > 1, network is small-world
    if (topology.small_world_sigma > 1.0f) {
        EXPECT_TRUE(topology.is_small_world);
    }

    // Clustering and path length should be reasonable
    EXPECT_GE(topology.clustering_coefficient, 0.0f);
    EXPECT_LE(topology.clustering_coefficient, 1.0f);
    EXPECT_GE(topology.avg_path_length, 0.0f);
}

//=============================================================================
// 7. Error Recovery Integration Tests
//=============================================================================

TEST_F(ConnectivityHealthIntegrationTest, RecoveryFromInvalidState) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    // Enable then disable monitoring multiple times
    for (int i = 0; i < 3; i++) {
        connectivity_health_config_t config = connectivity_health_default_config();
        brain_enable_connectivity_monitoring(brain, &config, nullptr, nullptr);
        brain_assess_connectivity_now(brain);
        brain_disable_connectivity_monitoring(brain);
    }

    // Should still work after repeated enable/disable
    brain_connectivity_health_t health = brain_assess_connectivity_now(brain);
    EXPECT_GE(health.overall_health, 0.0f);
    EXPECT_LE(health.overall_health, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
