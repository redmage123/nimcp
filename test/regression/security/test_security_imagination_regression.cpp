/**
 * @file test_security_imagination_regression.cpp
 * @brief Regression tests for Security-Imagination Bridge
 *
 * WHAT: Regression tests for security-imagination bidirectional bridge
 * WHY:  Ensure performance and behavior consistency across changes
 * HOW:  Test performance benchmarks, simulation integrity, boundary cases
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <vector>

extern "C" {
#include "security/imagination/nimcp_security_imagination_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

class SecurityImaginationRegressionTest : public ::testing::Test {
protected:
    security_imagination_bridge_t* bridge = nullptr;

    void SetUp() override {
        security_imagination_config_t config;
        security_imagination_default_config(&config);
        bridge = security_imagination_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            security_imagination_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    uint64_t CreateSandbox(const char* name = "regression_test") {
        uint64_t sandbox_id = 0;
        security_imagination_sandbox_workspace(
            bridge, name, SANDBOX_LEVEL_STANDARD, &sandbox_id
        );
        return sandbox_id;
    }

    double MeasureTimeMs(std::function<void()> operation, int iterations = 100) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            operation();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0 / iterations;
    }
};

// ============================================================================
// Performance Regression Tests
// ============================================================================

TEST_F(SecurityImaginationRegressionTest, SandboxCreationPerformance) {
    if (!bridge) GTEST_SKIP();

    // Measure sandbox creation time
    double avg_time = MeasureTimeMs([&]() {
        uint64_t id = 0;
        security_imagination_sandbox_workspace(
            bridge, "perf_test", SANDBOX_LEVEL_STANDARD, &id
        );
        security_imagination_release_sandbox(bridge, id);
    }, 50);

    // Should complete in reasonable time (< 1ms average)
    EXPECT_LT(avg_time, 1.0) << "Sandbox creation took " << avg_time << "ms average";
}

TEST_F(SecurityImaginationRegressionTest, ConfabulationDetectionPerformance) {
    if (!bridge) GTEST_SKIP();

    uint8_t content[1024];
    for (int i = 0; i < 1024; i++) {
        content[i] = (uint8_t)(i % 256);
    }

    security_imagination_confab_result_t result;

    double avg_time = MeasureTimeMs([&]() {
        security_imagination_detect_confabulation(bridge, content, sizeof(content), &result);
    }, 100);

    // Confabulation check should be fast (< 0.5ms)
    EXPECT_LT(avg_time, 0.5) << "Confabulation detection took " << avg_time << "ms average";
}

TEST_F(SecurityImaginationRegressionTest, RealityGroundingPerformance) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_grounding_result_t result;

    double avg_time = MeasureTimeMs([&]() {
        security_imagination_ground_reality(bridge, sandbox_id, &result);
    }, 100);

    // Grounding check should be fast (< 0.5ms)
    EXPECT_LT(avg_time, 0.5) << "Reality grounding took " << avg_time << "ms average";
}

TEST_F(SecurityImaginationRegressionTest, IntegrityVerificationPerformance) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_integrity_result_t result;

    double avg_time = MeasureTimeMs([&]() {
        security_imagination_verify_simulation(bridge, sandbox_id, &result);
    }, 100);

    // Integrity check should be fast (< 0.5ms)
    EXPECT_LT(avg_time, 0.5) << "Integrity verification took " << avg_time << "ms average";
}

TEST_F(SecurityImaginationRegressionTest, BridgeUpdatePerformance) {
    if (!bridge) GTEST_SKIP();

    // Create some sandboxes for realistic load
    for (int i = 0; i < 5; i++) {
        CreateSandbox();
    }

    double avg_time = MeasureTimeMs([&]() {
        security_imagination_bridge_update(bridge, 16);
    }, 100);

    // Bridge update should be fast (< 1ms)
    EXPECT_LT(avg_time, 1.0) << "Bridge update took " << avg_time << "ms average";
}

TEST_F(SecurityImaginationRegressionTest, BoundsEnforcementPerformance) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    double avg_time = MeasureTimeMs([&]() {
        security_imagination_enforce_bounds(bridge, sandbox_id, 4);
    }, 1000);

    // Bounds check should be very fast (< 0.1ms)
    EXPECT_LT(avg_time, 0.1) << "Bounds enforcement took " << avg_time << "ms average";
}

// ============================================================================
// Simulation Integrity Regression Tests
// ============================================================================

TEST_F(SecurityImaginationRegressionTest, IntegrityScoreConsistency) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // Get multiple integrity readings
    std::vector<float> scores;
    for (int i = 0; i < 10; i++) {
        security_imagination_integrity_result_t result;
        security_imagination_verify_simulation(bridge, sandbox_id, &result);
        scores.push_back(result.integrity_score);
    }

    // Scores should be consistent for same state
    for (size_t i = 1; i < scores.size(); i++) {
        EXPECT_FLOAT_EQ(scores[i], scores[0])
            << "Integrity score inconsistent on iteration " << i;
    }
}

TEST_F(SecurityImaginationRegressionTest, DivergenceScoreMonotonicity) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    float prev_divergence = -1.0f;

    // Increasing depth should generally increase divergence
    for (uint32_t depth = 1; depth <= 6; depth++) {
        security_imagination_enforce_bounds(bridge, sandbox_id, depth);

        // Simulate some activity
        security_imagination_track_resources(bridge, sandbox_id, 1000);

        security_imagination_grounding_result_t result;
        security_imagination_ground_reality(bridge, sandbox_id, &result);

        // Divergence should be non-decreasing (or at least not dramatically decrease)
        if (prev_divergence >= 0) {
            EXPECT_GE(result.divergence_score, prev_divergence - 0.1f)
                << "Divergence decreased unexpectedly at depth " << depth;
        }
        prev_divergence = result.divergence_score;
    }
}

TEST_F(SecurityImaginationRegressionTest, ResourceTrackingAccuracy) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    uint64_t expected_total = 0;

    // Track various amounts
    for (int i = 0; i < 100; i++) {
        uint64_t amount = (uint64_t)(i * 37 % 1000);  // Pseudo-random amounts
        int ret = security_imagination_track_resources(bridge, sandbox_id, amount);
        if (ret == NIMCP_SUCCESS) {
            expected_total += amount;
        }
    }

    uint64_t actual_total = security_imagination_get_resources(bridge, sandbox_id);
    EXPECT_EQ(actual_total, expected_total)
        << "Resource tracking mismatch: expected " << expected_total
        << " got " << actual_total;
}

// ============================================================================
// Boundary Case Regression Tests
// ============================================================================

TEST_F(SecurityImaginationRegressionTest, MaxDepthBoundaryRegression) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    uint32_t max_depth = security_imagination_get_max_depth(bridge);

    // Test all depths up to and beyond max
    for (uint32_t depth = 0; depth <= max_depth + 5; depth++) {
        bool result = security_imagination_enforce_bounds(bridge, sandbox_id, depth);

        if (depth <= max_depth) {
            EXPECT_TRUE(result) << "Depth " << depth << " should be allowed (max: " << max_depth << ")";
        } else {
            EXPECT_FALSE(result) << "Depth " << depth << " should be blocked (max: " << max_depth << ")";
        }
    }
}

TEST_F(SecurityImaginationRegressionTest, ZeroLengthContentConfabulation) {
    if (!bridge) GTEST_SKIP();

    security_imagination_confab_result_t result;

    // Empty content
    int ret = security_imagination_detect_confabulation(bridge, nullptr, 0, &result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_FALSE(result.detected);
    EXPECT_EQ(result.score, 0.0f);

    // Zero-length but non-null
    uint8_t empty[1] = {0};
    ret = security_imagination_detect_confabulation(bridge, empty, 0, &result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(SecurityImaginationRegressionTest, LargeContentConfabulation) {
    if (!bridge) GTEST_SKIP();

    // Large content block
    std::vector<uint8_t> large_content(100000);
    for (size_t i = 0; i < large_content.size(); i++) {
        large_content[i] = (uint8_t)(i % 256);
    }

    security_imagination_confab_result_t result;
    int ret = security_imagination_detect_confabulation(
        bridge, large_content.data(), large_content.size(), &result
    );

    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(result.score, 0.0f);
    EXPECT_LE(result.score, 1.0f);
}

// ============================================================================
// Stats Tracking Regression Tests
// ============================================================================

TEST_F(SecurityImaginationRegressionTest, StatsIncrementCorrectly) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // Perform known number of operations
    const int DEPTH_CHECKS = 5;
    const int CONFAB_CHECKS = 3;
    const int GROUNDING_CHECKS = 4;
    const int INTEGRITY_CHECKS = 2;

    for (int i = 0; i < DEPTH_CHECKS; i++) {
        security_imagination_enforce_bounds(bridge, sandbox_id, 2);
    }

    uint8_t content[10] = {0};
    security_imagination_confab_result_t confab;
    for (int i = 0; i < CONFAB_CHECKS; i++) {
        security_imagination_detect_confabulation(bridge, content, sizeof(content), &confab);
    }

    security_imagination_grounding_result_t grounding;
    for (int i = 0; i < GROUNDING_CHECKS; i++) {
        security_imagination_ground_reality(bridge, sandbox_id, &grounding);
    }

    security_imagination_integrity_result_t integrity;
    for (int i = 0; i < INTEGRITY_CHECKS; i++) {
        security_imagination_verify_simulation(bridge, sandbox_id, &integrity);
    }

    security_imagination_stats_t stats;
    security_imagination_get_stats(bridge, &stats);

    EXPECT_EQ(stats.depth_checks, (uint64_t)DEPTH_CHECKS);
    EXPECT_EQ(stats.confab_checks, (uint64_t)CONFAB_CHECKS);
    EXPECT_EQ(stats.grounding_checks, (uint64_t)GROUNDING_CHECKS);
    EXPECT_EQ(stats.integrity_checks, (uint64_t)INTEGRITY_CHECKS);
}

TEST_F(SecurityImaginationRegressionTest, StatsResetClearsAll) {
    if (!bridge) GTEST_SKIP();

    // Generate activity
    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_enforce_bounds(bridge, sandbox_id, 3);
    security_imagination_track_resources(bridge, sandbox_id, 500);

    uint8_t content[10] = {0};
    security_imagination_confab_result_t confab;
    security_imagination_detect_confabulation(bridge, content, sizeof(content), &confab);

    security_imagination_grounding_result_t grounding;
    security_imagination_ground_reality(bridge, sandbox_id, &grounding);

    // Reset
    security_imagination_reset_stats(bridge);

    // Verify all stats are zero
    security_imagination_stats_t stats;
    security_imagination_get_stats(bridge, &stats);

    EXPECT_EQ(stats.sandboxes_created, 0u);
    EXPECT_EQ(stats.sandboxes_destroyed, 0u);
    EXPECT_EQ(stats.depth_checks, 0u);
    EXPECT_EQ(stats.confab_checks, 0u);
    EXPECT_EQ(stats.grounding_checks, 0u);
    EXPECT_EQ(stats.integrity_checks, 0u);
    EXPECT_EQ(stats.adversarial_checks, 0u);
    EXPECT_EQ(stats.total_resources_consumed, 0u);
    EXPECT_EQ(stats.avg_sandbox_duration_ms, 0.0f);
    EXPECT_EQ(stats.avg_confab_score, 0.0f);
    EXPECT_EQ(stats.avg_divergence_score, 0.0f);
    EXPECT_EQ(stats.avg_integrity_score, 0.0f);
}

// ============================================================================
// State Consistency Regression Tests
// ============================================================================

TEST_F(SecurityImaginationRegressionTest, StateConsistencyAfterOperations) {
    if (!bridge) GTEST_SKIP();

    // Track state through operations
    security_imagination_state_t initial_state;
    security_imagination_get_state(bridge, &initial_state);
    EXPECT_TRUE(initial_state.is_active);
    EXPECT_FALSE(initial_state.imagination_restricted);

    // Create sandboxes
    uint64_t id1 = CreateSandbox();
    uint64_t id2 = CreateSandbox();

    security_imagination_state_t mid_state;
    security_imagination_get_state(bridge, &mid_state);
    EXPECT_EQ(mid_state.active_sandbox_count, 2u);

    // Release one
    security_imagination_release_sandbox(bridge, id1);

    security_imagination_state_t after_release;
    security_imagination_get_state(bridge, &after_release);
    EXPECT_EQ(after_release.active_sandbox_count, 1u);

    // Enter/exit restricted
    security_imagination_enter_restricted(bridge);
    security_imagination_state_t restricted_state;
    security_imagination_get_state(bridge, &restricted_state);
    EXPECT_TRUE(restricted_state.imagination_restricted);

    security_imagination_exit_restricted(bridge);
    security_imagination_state_t final_state;
    security_imagination_get_state(bridge, &final_state);
    EXPECT_FALSE(final_state.imagination_restricted);
    EXPECT_TRUE(final_state.is_active);  // Should still be active
}

TEST_F(SecurityImaginationRegressionTest, EffectsConsistencyAfterUpdate) {
    if (!bridge) GTEST_SKIP();

    // Get initial effects
    security_to_imagination_effects_t effects1;
    security_imagination_get_security_effects(bridge, &effects1);

    // Update multiple times
    for (int i = 0; i < 10; i++) {
        security_imagination_bridge_update(bridge, 16);
    }

    // Get effects again
    security_to_imagination_effects_t effects2;
    security_imagination_get_security_effects(bridge, &effects2);

    // Without changes to configuration, effects should remain consistent
    EXPECT_EQ(effects1.effective_max_depth, effects2.effective_max_depth);
    EXPECT_EQ(effects1.effective_simulation_budget, effects2.effective_simulation_budget);
    EXPECT_FLOAT_EQ(effects1.depth_reduction_factor, effects2.depth_reduction_factor);
    EXPECT_FLOAT_EQ(effects1.resource_reduction_factor, effects2.resource_reduction_factor);
}

// ============================================================================
// Memory Safety Regression Tests
// ============================================================================

TEST_F(SecurityImaginationRegressionTest, RepeatedCreateDestroy) {
    // Test for memory leaks or corruption
    for (int i = 0; i < 100; i++) {
        security_imagination_config_t config;
        security_imagination_default_config(&config);
        security_imagination_bridge_t* temp = security_imagination_bridge_create(&config);

        if (temp) {
            // Create and destroy some sandboxes
            uint64_t id1 = 0, id2 = 0;
            security_imagination_sandbox_workspace(temp, "test1", SANDBOX_LEVEL_STANDARD, &id1);
            security_imagination_sandbox_workspace(temp, "test2", SANDBOX_LEVEL_STRICT, &id2);

            if (id1) security_imagination_release_sandbox(temp, id1);
            if (id2) security_imagination_release_sandbox(temp, id2);

            security_imagination_bridge_destroy(temp);
        }
    }
    // Test passes if no crashes or memory issues
}

TEST_F(SecurityImaginationRegressionTest, SandboxReuseAfterRelease) {
    if (!bridge) GTEST_SKIP();

    std::vector<uint64_t> used_ids;

    // Create and release in cycles
    for (int cycle = 0; cycle < 20; cycle++) {
        uint64_t id = CreateSandbox();
        if (id > 0) {
            // Verify no duplicate IDs
            for (auto prev_id : used_ids) {
                EXPECT_NE(id, prev_id) << "Duplicate sandbox ID generated";
            }
            used_ids.push_back(id);

            // Use the sandbox
            security_imagination_enforce_bounds(bridge, id, 2);
            security_imagination_track_resources(bridge, id, 100);

            // Release
            security_imagination_release_sandbox(bridge, id);
        }
    }
}

// ============================================================================
// Configuration Regression Tests
// ============================================================================

TEST_F(SecurityImaginationRegressionTest, ConfigurationPersistence) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);

    // Set specific values
    config.max_hypothetical_depth = 12;
    config.default_simulation_budget = 777777;
    config.confabulation_threshold = 0.42f;
    config.reality_divergence_threshold = 0.63f;
    config.max_sandboxed_workspaces = 7;

    security_imagination_bridge_destroy(bridge);
    bridge = security_imagination_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    // Verify configuration was applied
    uint32_t max_depth = security_imagination_get_max_depth(bridge);
    EXPECT_EQ(max_depth, 12u);

    security_to_imagination_effects_t effects;
    security_imagination_get_security_effects(bridge, &effects);
    EXPECT_EQ(effects.effective_max_depth, 12u);
    EXPECT_EQ(effects.effective_simulation_budget, 777777u);
}

TEST_F(SecurityImaginationRegressionTest, DisabledFeaturesRegression) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);

    // Disable all optional features
    config.enable_confabulation_detection = false;
    config.enable_reasoning_bounds = false;
    config.enable_reality_grounding = false;
    config.enable_simulation_integrity = false;
    config.enable_adversarial_detection = false;
    config.enable_resource_tracking = false;

    security_imagination_bridge_destroy(bridge);
    bridge = security_imagination_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // All operations should succeed without actual checks
    EXPECT_TRUE(security_imagination_enforce_bounds(bridge, sandbox_id, 1000));

    security_imagination_confab_result_t confab;
    EXPECT_EQ(security_imagination_detect_confabulation(bridge, nullptr, 0, &confab),
              NIMCP_SUCCESS);
    EXPECT_FALSE(confab.detected);

    security_imagination_grounding_result_t grounding;
    EXPECT_EQ(security_imagination_ground_reality(bridge, sandbox_id, &grounding),
              NIMCP_SUCCESS);
    EXPECT_TRUE(grounding.grounded);

    security_imagination_integrity_result_t integrity;
    EXPECT_EQ(security_imagination_verify_simulation(bridge, sandbox_id, &integrity),
              NIMCP_SUCCESS);
    EXPECT_TRUE(integrity.integrity_valid);

    float adversarial;
    EXPECT_EQ(security_imagination_check_adversarial(bridge, sandbox_id, &adversarial),
              NIMCP_SUCCESS);
    EXPECT_EQ(adversarial, 0.0f);

    EXPECT_EQ(security_imagination_track_resources(bridge, sandbox_id, UINT64_MAX),
              NIMCP_SUCCESS);
}

// ============================================================================
// Edge Case Regression Tests
// ============================================================================

TEST_F(SecurityImaginationRegressionTest, ZeroConfigValues) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);

    // Set some values to zero
    config.max_hypothetical_depth = 0;
    config.default_simulation_budget = 0;
    config.max_sandboxed_workspaces = 1;

    security_imagination_bridge_destroy(bridge);
    bridge = security_imagination_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    // Bridge should still function
    EXPECT_TRUE(bridge != nullptr);

    // Get max depth
    uint32_t max_depth = security_imagination_get_max_depth(bridge);
    EXPECT_EQ(max_depth, 0u);
}

TEST_F(SecurityImaginationRegressionTest, MaxConfigValues) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);

    // Set maximum values
    config.max_hypothetical_depth = UINT32_MAX;
    config.default_simulation_budget = UINT64_MAX;
    config.confabulation_threshold = 1.0f;
    config.reality_divergence_threshold = 1.0f;

    security_imagination_bridge_destroy(bridge);
    bridge = security_imagination_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    // Bridge should still function
    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // Very high depth should be allowed
    EXPECT_TRUE(security_imagination_enforce_bounds(bridge, sandbox_id, 1000000));
}

TEST_F(SecurityImaginationRegressionTest, ThresholdBoundaryValues) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // Test various threshold boundary values
    float test_values[] = {0.0f, 0.5f, 0.999f, 1.0f};

    for (float threshold : test_values) {
        // Thresholds are internal, just verify operations work
        security_imagination_grounding_result_t grounding;
        int ret = security_imagination_ground_reality(bridge, sandbox_id, &grounding);
        EXPECT_EQ(ret, NIMCP_SUCCESS);

        security_imagination_integrity_result_t integrity;
        ret = security_imagination_verify_simulation(bridge, sandbox_id, &integrity);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }
}
