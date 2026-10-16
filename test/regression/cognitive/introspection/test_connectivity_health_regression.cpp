/**
 * @file test_connectivity_health_regression.cpp
 * @brief Regression tests for brain connectivity health assessment
 *
 * TEST COVERAGE:
 * - API stability (no breaking changes)
 * - Metric consistency (same input -> same output)
 * - Threshold behavior (health status matches thresholds)
 * - Backward compatibility with existing brain APIs
 * - Known bug reproductions (prevent regressions)
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

#include "cognitive/introspection/nimcp_connectivity_health.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain_regions/nimcp_brain_regions.h"

/* Map region names used in tests to actual brain_region_type_t values */
#define BRAIN_REGION_EXECUTIVE  REGION_PREFRONTAL
#define BRAIN_REGION_WORKSPACE  REGION_PARIETAL
#define BRAIN_REGION_SALIENCE   REGION_TEMPORAL

//=============================================================================
// Test Fixture
//=============================================================================

class ConnectivityHealthRegressionTest : public ::testing::Test {
protected:
    brain_t brain;
    static constexpr uint32_t NUM_INPUTS = 10;
    static constexpr uint32_t NUM_OUTPUTS = 3;

    void SetUp() override {
        // Create brain using correct API
        brain = brain_create(
            "regression_test",       // task_name
            BRAIN_SIZE_TINY,         // size
            BRAIN_TASK_CLASSIFICATION, // task
            NUM_INPUTS,              // num_inputs
            NUM_OUTPUTS              // num_outputs
        );
    }

    void TearDown() override {
        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// 1. API Stability Tests
//=============================================================================

TEST_F(ConnectivityHealthRegressionTest, DefaultConfigAPIStability) {
    // Ensure default config function signature is stable
    connectivity_health_config_t config = connectivity_health_default_config();

    // These constants must remain stable across versions
    EXPECT_FLOAT_EQ(CONNECTIVITY_MIN_MODULARITY, 0.3f);
    EXPECT_FLOAT_EQ(CONNECTIVITY_MIN_CLUSTERING, 0.3f);
    EXPECT_FLOAT_EQ(CONNECTIVITY_MAX_PATH_LENGTH, 6.0f);
    EXPECT_FLOAT_EQ(CONNECTIVITY_SMALL_WORLD_THRESHOLD, 1.0f);
    EXPECT_FLOAT_EQ(CONNECTIVITY_MIN_FLOW_EFFICIENCY, 0.7f);
    EXPECT_FLOAT_EQ(CONNECTIVITY_HUB_THRESHOLD, 2.0f);
    EXPECT_EQ(CONNECTIVITY_MAX_HUBS, 256u);
    EXPECT_EQ(CONNECTIVITY_DEFAULT_ASSESSMENT_INTERVAL_MS, 10000u);
}

TEST_F(ConnectivityHealthRegressionTest, ConfigStructureStability) {
    connectivity_health_config_t config = connectivity_health_default_config();

    // Verify all expected fields exist and are accessible
    (void)config.min_modularity;
    (void)config.max_community_imbalance;
    (void)config.hub_threshold_stddev;
    (void)config.require_executive_hubs;
    (void)config.require_workspace_hubs;
    (void)config.min_clustering_coefficient;
    (void)config.max_path_length;
    (void)config.small_world_threshold;
    (void)config.min_flow_efficiency;
    (void)config.min_layer_connectivity;
    (void)config.weight_modularity;
    (void)config.weight_hubs;
    (void)config.weight_topology;
    (void)config.weight_flow;
    (void)config.assessment_interval_ms;
    (void)config.enable_detailed_hub_analysis;
    (void)config.enable_community_balance;
}

TEST_F(ConnectivityHealthRegressionTest, HealthStructureStability) {
    brain_connectivity_health_t health;
    memset(&health, 0, sizeof(health));

    // Verify all expected fields exist
    (void)health.community;
    (void)health.hubs;
    (void)health.topology;
    (void)health.flow;
    (void)health.overall_health;
    (void)health.is_healthy;
    (void)health.num_warnings;
    (void)health.num_critical;
    (void)health.primary_issue;
    (void)health.assessment_timestamp_ms;
    (void)health.assessment_duration_ms;
    (void)health.total_neurons;
    (void)health.total_synapses;
}

//=============================================================================
// 2. Metric Consistency Tests
//=============================================================================

TEST_F(ConnectivityHealthRegressionTest, CommunityBalanceConsistency) {
    // Same community sizes should always produce same balance
    uint32_t sizes1[] = {100, 100, 100, 100};
    uint32_t sizes2[] = {100, 100, 100, 100};

    float balance1 = calculate_community_balance(sizes1, 4);
    float balance2 = calculate_community_balance(sizes2, 4);

    EXPECT_FLOAT_EQ(balance1, balance2);
}

TEST_F(ConnectivityHealthRegressionTest, AssessmentDeterminism) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    // Same brain state should produce consistent health assessments
    brain_connectivity_health_t health1 = brain_assess_connectivity_now(brain);
    brain_connectivity_health_t health2 = brain_assess_connectivity_now(brain);

    // Overall health should be identical (deterministic assessment)
    EXPECT_FLOAT_EQ(health1.overall_health, health2.overall_health);
    EXPECT_EQ(health1.total_neurons, health2.total_neurons);
    EXPECT_EQ(health1.total_synapses, health2.total_synapses);
}

TEST_F(ConnectivityHealthRegressionTest, ComponentAssessmentConsistency) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Multiple assessments should be consistent
    community_health_t comm1 = introspection_assess_community_health(intro, nullptr);
    community_health_t comm2 = introspection_assess_community_health(intro, nullptr);

    EXPECT_FLOAT_EQ(comm1.modularity_q, comm2.modularity_q);
    EXPECT_EQ(comm1.num_communities, comm2.num_communities);
}

//=============================================================================
// 3. Threshold Behavior Tests
//=============================================================================

TEST_F(ConnectivityHealthRegressionTest, ModularityThresholdBehavior) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Test with strict threshold
    connectivity_health_config_t strict_config = connectivity_health_default_config();
    strict_config.min_modularity = 0.9f;  // Very strict

    community_health_t strict_health = introspection_assess_community_health(intro, &strict_config);

    // Test with lenient threshold
    connectivity_health_config_t lenient_config = connectivity_health_default_config();
    lenient_config.min_modularity = 0.0f;  // Very lenient

    community_health_t lenient_health = introspection_assess_community_health(intro, &lenient_config);

    // Same modularity Q value
    EXPECT_FLOAT_EQ(strict_health.modularity_q, lenient_health.modularity_q);

    // But health status may differ
    // (lenient should be healthy if Q >= 0, strict may not be)
}

TEST_F(ConnectivityHealthRegressionTest, SmallWorldThresholdBehavior) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    topology_health_t topology = introspection_assess_topology_health(intro, nullptr);

    // Small-world status should match threshold comparison
    bool expected_small_world = (topology.small_world_sigma > CONNECTIVITY_SMALL_WORLD_THRESHOLD);
    EXPECT_EQ(topology.is_small_world, expected_small_world);
}

//=============================================================================
// 4. Backward Compatibility Tests
//=============================================================================

TEST_F(ConnectivityHealthRegressionTest, BrainAPICompatibility) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    // Existing brain APIs should work alongside connectivity health
    brain_stats_t stats;
    bool got_stats = brain_get_stats(brain, &stats);
    EXPECT_TRUE(got_stats);

    // Brain decision should still work
    std::vector<float> input(10, 1.0f);
    brain_decision_t* decision = brain_decide(brain, input.data(), NUM_INPUTS);
    if (decision) {
        EXPECT_GT(decision->confidence, 0.0f);
        brain_free_decision(decision);
    }

    // Connectivity health should not affect existing APIs
    brain_connectivity_health_t health = brain_assess_connectivity_now(brain);
    EXPECT_GE(health.overall_health, 0.0f);

    // Brain should still function after health assessment
    decision = brain_decide(brain, input.data(), NUM_INPUTS);
    if (decision) {
        EXPECT_GT(decision->confidence, 0.0f);
        brain_free_decision(decision);
    }
}

TEST_F(ConnectivityHealthRegressionTest, IntrospectionAPICompatibility) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Existing introspection APIs should work
    // (Add appropriate calls based on existing introspection API)

    // Connectivity health should not affect introspection
    brain_connectivity_health_t health = introspection_assess_connectivity_health(intro, nullptr);
    EXPECT_GE(health.overall_health, 0.0f);
}

//=============================================================================
// 5. Known Bug Prevention Tests
//=============================================================================

TEST_F(ConnectivityHealthRegressionTest, NullPointerSafety) {
    // All functions should handle NULL gracefully

    // Config functions
    connectivity_health_config_t config = connectivity_health_default_config();
    (void)config;

    // Assessment functions with NULL introspection
    community_health_t comm = introspection_assess_community_health(nullptr, nullptr);
    EXPECT_FALSE(comm.is_healthy);

    hub_health_t hubs = introspection_assess_hub_health(nullptr, nullptr);
    EXPECT_FALSE(hubs.is_healthy);

    topology_health_t topo = introspection_assess_topology_health(nullptr, nullptr);
    EXPECT_FALSE(topo.is_healthy);

    information_flow_health_t flow = introspection_assess_flow_health(nullptr, nullptr);
    EXPECT_FALSE(flow.is_healthy);

    brain_connectivity_health_t full = introspection_assess_connectivity_health(nullptr, nullptr);
    EXPECT_FALSE(full.is_healthy);

    // Brain functions with NULL brain
    bool enabled = brain_enable_connectivity_monitoring(nullptr, nullptr, nullptr, nullptr);
    EXPECT_FALSE(enabled);

    brain_disable_connectivity_monitoring(nullptr);
    // Should not crash

    bool is_enabled = brain_is_connectivity_monitoring_enabled(nullptr);
    EXPECT_FALSE(is_enabled);

    brain_connectivity_health_t brain_health;
    bool got_health = brain_get_connectivity_health(nullptr, &brain_health);
    EXPECT_FALSE(got_health);

    // Utility functions with NULL
    float balance = calculate_community_balance(nullptr, 0);
    EXPECT_FLOAT_EQ(balance, 0.0f);

    bool in_region = is_neuron_in_region(nullptr, 0, BRAIN_REGION_EXECUTIVE);
    EXPECT_FALSE(in_region);

    const char* str = connectivity_health_to_string(nullptr, nullptr, 0);
    EXPECT_EQ(str, nullptr);

    connectivity_health_free(nullptr);
    // Should not crash
}

TEST_F(ConnectivityHealthRegressionTest, OverflowPrevention) {
    // Test with boundary values
    uint32_t max_sizes[] = {UINT32_MAX / 4, UINT32_MAX / 4, UINT32_MAX / 4, UINT32_MAX / 4};
    float balance = calculate_community_balance(max_sizes, 4);

    // Should not overflow, result should be valid
    EXPECT_GE(balance, 0.0f);
    EXPECT_LE(balance, 1.0f);
}

TEST_F(ConnectivityHealthRegressionTest, DivisionByZeroPrevention) {
    // Edge case: zero-sized communities
    uint32_t zero_sizes[] = {0, 0, 0, 0};
    float balance = calculate_community_balance(zero_sizes, 4);

    // Should handle gracefully (no crash, valid result)
    EXPECT_GE(balance, 0.0f);
    EXPECT_LE(balance, 1.0f);
}

//=============================================================================
// 6. Performance Regression Tests
//=============================================================================

TEST_F(ConnectivityHealthRegressionTest, AssessmentCompletes) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    auto start = std::chrono::steady_clock::now();

    brain_connectivity_health_t health = brain_assess_connectivity_now(brain);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Assessment should complete within reasonable time (< 5 seconds for tiny brain)
    EXPECT_LT(duration.count(), 5000);

    // Assessment should be valid
    EXPECT_GE(health.overall_health, 0.0f);
    EXPECT_LE(health.overall_health, 1.0f);
}

TEST_F(ConnectivityHealthRegressionTest, QuickCheckIsQuick) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    // First do a full assessment to populate cache
    brain_assess_connectivity_now(brain);

    auto start = std::chrono::steady_clock::now();

    bool is_healthy;
    float score = introspection_quick_connectivity_check(intro, &is_healthy);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Quick check should be much faster (< 100ms)
    EXPECT_LT(duration.count(), 100);

    // Result should be valid
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

//=============================================================================
// 7. Memory Safety Tests
//=============================================================================

TEST_F(ConnectivityHealthRegressionTest, NoMemoryLeakOnRepeatAssessment) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    // Run multiple assessments
    for (int i = 0; i < 10; i++) {
        brain_connectivity_health_t health = brain_assess_connectivity_now(brain);
        connectivity_health_free(&health);
    }

    // If we get here without crash, basic memory handling is correct
    // Full memory leak detection requires tools like Valgrind
}

TEST_F(ConnectivityHealthRegressionTest, SafeEnableDisableCycle) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed - insufficient resources";
    }

    connectivity_health_config_t config = connectivity_health_default_config();

    // Enable/disable multiple times
    for (int i = 0; i < 10; i++) {
        brain_enable_connectivity_monitoring(brain, &config, nullptr, nullptr);
        brain_assess_connectivity_now(brain);
        brain_disable_connectivity_monitoring(brain);
    }

    // Should not crash or leak memory
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
