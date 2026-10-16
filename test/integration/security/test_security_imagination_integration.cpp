/**
 * @file test_security_imagination_integration.cpp
 * @brief Integration tests for Security-Imagination Bridge
 *
 * WHAT: Integration tests for security-imagination bidirectional bridge
 * WHY:  Verify security module integrates correctly with imagination system
 * HOW:  Test multi-sandbox scenarios, boundary conditions, combined operations
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>

extern "C" {
#include "security/imagination/nimcp_security_imagination_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

class SecurityImaginationIntegrationTest : public ::testing::Test {
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

    uint64_t CreateSandbox(const char* name = "integration_test") {
        uint64_t sandbox_id = 0;
        security_imagination_sandbox_workspace(
            bridge, name, SANDBOX_LEVEL_STANDARD, &sandbox_id
        );
        return sandbox_id;
    }
};

// ============================================================================
// Multi-Sandbox Integration Tests
// ============================================================================

TEST_F(SecurityImaginationIntegrationTest, MultiSandboxParallelOperations) {
    if (!bridge) GTEST_SKIP();

    // Create multiple sandboxes
    std::vector<uint64_t> sandbox_ids;
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "parallel_%d", i);
        uint64_t id = 0;
        ASSERT_EQ(security_imagination_sandbox_workspace(
            bridge, name, SANDBOX_LEVEL_STANDARD, &id), NIMCP_SUCCESS);
        sandbox_ids.push_back(id);
    }

    // Perform operations on all sandboxes
    for (auto id : sandbox_ids) {
        EXPECT_TRUE(security_imagination_enforce_bounds(bridge, id, 3));
        EXPECT_EQ(security_imagination_track_resources(bridge, id, 100), NIMCP_SUCCESS);

        security_imagination_grounding_result_t grounding;
        EXPECT_EQ(security_imagination_ground_reality(bridge, id, &grounding), NIMCP_SUCCESS);

        security_imagination_integrity_result_t integrity;
        EXPECT_EQ(security_imagination_verify_simulation(bridge, id, &integrity), NIMCP_SUCCESS);
    }

    // Release all sandboxes
    for (auto id : sandbox_ids) {
        EXPECT_EQ(security_imagination_release_sandbox(bridge, id), NIMCP_SUCCESS);
    }
}

TEST_F(SecurityImaginationIntegrationTest, MultiSandboxResourceTracking) {
    if (!bridge) GTEST_SKIP();

    // Create sandboxes with different resource usage
    uint64_t id1 = CreateSandbox("resource_heavy");
    uint64_t id2 = CreateSandbox("resource_light");
    uint64_t id3 = CreateSandbox("resource_medium");

    if (id1 == 0 || id2 == 0 || id3 == 0) GTEST_SKIP();

    // Track different amounts
    security_imagination_track_resources(bridge, id1, 10000);
    security_imagination_track_resources(bridge, id2, 100);
    security_imagination_track_resources(bridge, id3, 5000);

    // Verify individual tracking
    EXPECT_EQ(security_imagination_get_resources(bridge, id1), 10000u);
    EXPECT_EQ(security_imagination_get_resources(bridge, id2), 100u);
    EXPECT_EQ(security_imagination_get_resources(bridge, id3), 5000u);

    // Update and check effects
    security_imagination_update_imagination_effects(bridge);
    imagination_to_security_effects_t effects;
    security_imagination_get_imagination_effects(bridge, &effects);

    EXPECT_EQ(effects.total_resources_used, 15100u);
}

TEST_F(SecurityImaginationIntegrationTest, MultiSandboxDepthTracking) {
    if (!bridge) GTEST_SKIP();

    uint64_t id1 = CreateSandbox("shallow");
    uint64_t id2 = CreateSandbox("deep");
    uint64_t id3 = CreateSandbox("medium");

    if (id1 == 0 || id2 == 0 || id3 == 0) GTEST_SKIP();

    // Set different depths
    security_imagination_enforce_bounds(bridge, id1, 2);
    security_imagination_enforce_bounds(bridge, id2, 6);
    security_imagination_enforce_bounds(bridge, id3, 4);

    // Verify depths via sandbox info
    security_imagination_sandbox_t sandbox;

    security_imagination_get_sandbox(bridge, id1, &sandbox);
    EXPECT_EQ(sandbox.max_depth_reached, 2u);

    security_imagination_get_sandbox(bridge, id2, &sandbox);
    EXPECT_EQ(sandbox.max_depth_reached, 6u);

    security_imagination_get_sandbox(bridge, id3, &sandbox);
    EXPECT_EQ(sandbox.max_depth_reached, 4u);
}

// ============================================================================
// Combined Security Check Integration Tests
// ============================================================================

TEST_F(SecurityImaginationIntegrationTest, FullSecurityCheckCycle) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox("full_check");
    if (sandbox_id == 0) GTEST_SKIP();

    // 1. Enforce bounds
    EXPECT_TRUE(security_imagination_enforce_bounds(bridge, sandbox_id, 4));

    // 2. Check confabulation
    uint8_t content[50];
    for (int i = 0; i < 50; i++) content[i] = (uint8_t)(i * 3);
    security_imagination_confab_result_t confab;
    EXPECT_EQ(security_imagination_detect_confabulation(bridge, content, 50, &confab),
              NIMCP_SUCCESS);

    // 3. Ground reality
    security_imagination_grounding_result_t grounding;
    EXPECT_EQ(security_imagination_ground_reality(bridge, sandbox_id, &grounding),
              NIMCP_SUCCESS);

    // 4. Verify integrity
    security_imagination_integrity_result_t integrity;
    EXPECT_EQ(security_imagination_verify_simulation(bridge, sandbox_id, &integrity),
              NIMCP_SUCCESS);

    // 5. Check adversarial
    float adversarial_score;
    EXPECT_EQ(security_imagination_check_adversarial(bridge, sandbox_id, &adversarial_score),
              NIMCP_SUCCESS);

    // 6. Track resources
    EXPECT_EQ(security_imagination_track_resources(bridge, sandbox_id, 1000),
              NIMCP_SUCCESS);

    // Update bridge state
    EXPECT_EQ(security_imagination_bridge_update(bridge, 16), NIMCP_SUCCESS);

    // Verify stats reflect all operations
    security_imagination_stats_t stats;
    security_imagination_get_stats(bridge, &stats);

    EXPECT_GT(stats.depth_checks, 0u);
    EXPECT_GT(stats.grounding_checks, 0u);
    EXPECT_GT(stats.integrity_checks, 0u);
    EXPECT_GT(stats.adversarial_checks, 0u);
}

TEST_F(SecurityImaginationIntegrationTest, SecurityEscalationScenario) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox("escalation");
    if (sandbox_id == 0) GTEST_SKIP();

    // Initial state: not restricted
    EXPECT_FALSE(security_imagination_is_restricted(bridge));

    // Simulate escalating concerning activity
    for (int i = 0; i < 5; i++) {
        // Track increasing resource usage
        security_imagination_track_resources(bridge, sandbox_id, 50000);

        // Go to deeper depth each iteration
        security_imagination_enforce_bounds(bridge, sandbox_id, (uint32_t)(i + 2));

        // Update effects
        security_imagination_bridge_update(bridge, 16);
    }

    // Now enter restricted mode due to high activity
    security_imagination_enter_restricted(bridge);
    EXPECT_TRUE(security_imagination_is_restricted(bridge));

    // Verify depth is now more limited
    uint32_t restricted_depth = security_imagination_get_max_depth(bridge);
    EXPECT_LT(restricted_depth, SECURITY_IMAGINATION_DEFAULT_MAX_DEPTH);
}

// ============================================================================
// Boundary Testing Integration
// ============================================================================

TEST_F(SecurityImaginationIntegrationTest, DepthBoundaryExact) {
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox("depth_boundary");
    if (sandbox_id == 0) GTEST_SKIP();

    uint32_t max_depth = security_imagination_get_max_depth(bridge);

    // Test exact boundary
    EXPECT_TRUE(security_imagination_enforce_bounds(bridge, sandbox_id, max_depth));

    // Test one over boundary
    EXPECT_FALSE(security_imagination_enforce_bounds(bridge, sandbox_id, max_depth + 1));
}

TEST_F(SecurityImaginationIntegrationTest, ResourceBoundaryExact) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.default_simulation_budget = 10000;
    security_imagination_bridge_destroy(bridge);
    bridge = security_imagination_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox("resource_boundary");
    if (sandbox_id == 0) GTEST_SKIP();

    // Use exactly the budget
    EXPECT_EQ(security_imagination_track_resources(bridge, sandbox_id, 10000),
              NIMCP_SUCCESS);

    // Any more should fail
    EXPECT_EQ(security_imagination_track_resources(bridge, sandbox_id, 1),
              NIMCP_ERROR_OUT_OF_RANGE);
}

TEST_F(SecurityImaginationIntegrationTest, SandboxCapacityBoundary) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.max_sandboxed_workspaces = 5;
    security_imagination_bridge_destroy(bridge);
    bridge = security_imagination_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    std::vector<uint64_t> ids;

    // Fill to capacity
    for (int i = 0; i < 5; i++) {
        uint64_t id = 0;
        EXPECT_EQ(security_imagination_sandbox_workspace(
            bridge, "fill", SANDBOX_LEVEL_STANDARD, &id), NIMCP_SUCCESS);
        ids.push_back(id);
    }

    // Next should fail
    uint64_t overflow_id = 0;
    EXPECT_EQ(security_imagination_sandbox_workspace(
        bridge, "overflow", SANDBOX_LEVEL_STANDARD, &overflow_id),
        NIMCP_ERROR_OUT_OF_RANGE);

    // Release one
    security_imagination_release_sandbox(bridge, ids[0]);

    // Now one more should succeed
    EXPECT_EQ(security_imagination_sandbox_workspace(
        bridge, "reuse", SANDBOX_LEVEL_STANDARD, &overflow_id), NIMCP_SUCCESS);
}

// ============================================================================
// Scenario Blocking Integration Tests
// ============================================================================

TEST_F(SecurityImaginationIntegrationTest, BlockUnblockScenarios) {
    if (!bridge) GTEST_SKIP();

    // Create initial sandbox
    uint64_t id1 = CreateSandbox("before_block");
    EXPECT_GT(id1, 0u);

    // Block new scenarios
    security_imagination_block_new_scenarios(bridge);

    // Attempt to create should fail
    uint64_t id2 = 0;
    EXPECT_EQ(security_imagination_sandbox_workspace(
        bridge, "blocked", SANDBOX_LEVEL_STANDARD, &id2),
        NIMCP_ERROR_INVALID_STATE);

    // Existing sandbox should still work
    EXPECT_EQ(security_imagination_track_resources(bridge, id1, 100), NIMCP_SUCCESS);

    // Unblock
    security_imagination_allow_new_scenarios(bridge);

    // Now creation should work
    EXPECT_EQ(security_imagination_sandbox_workspace(
        bridge, "unblocked", SANDBOX_LEVEL_STANDARD, &id2), NIMCP_SUCCESS);
    EXPECT_GT(id2, 0u);
}

// ============================================================================
// Restriction Mode Integration Tests
// ============================================================================

TEST_F(SecurityImaginationIntegrationTest, RestrictionModeEffects) {
    if (!bridge) GTEST_SKIP();

    // Get baseline effects
    security_to_imagination_effects_t baseline;
    security_imagination_get_security_effects(bridge, &baseline);

    // Enter restricted mode
    security_imagination_enter_restricted(bridge);
    security_imagination_update_security_effects(bridge);

    // Get restricted effects
    security_to_imagination_effects_t restricted;
    security_imagination_get_security_effects(bridge, &restricted);

    // Verify restrictions applied
    EXPECT_TRUE(restricted.imagination_restricted);
    EXPECT_LE(restricted.effective_max_depth, baseline.effective_max_depth);
    EXPECT_LE(restricted.effective_simulation_budget, baseline.effective_simulation_budget);
    EXPECT_LT(restricted.depth_reduction_factor, 1.0f);
    EXPECT_LT(restricted.resource_reduction_factor, 1.0f);

    // Exit restricted mode
    security_imagination_exit_restricted(bridge);
    security_imagination_update_security_effects(bridge);

    // Get restored effects
    security_to_imagination_effects_t restored;
    security_imagination_get_security_effects(bridge, &restored);

    // Verify restoration
    EXPECT_FALSE(restored.imagination_restricted);
    EXPECT_EQ(restored.effective_max_depth, baseline.effective_max_depth);
    EXPECT_EQ(restored.effective_simulation_budget, baseline.effective_simulation_budget);
}

// ============================================================================
// Statistics Integration Tests
// ============================================================================

TEST_F(SecurityImaginationIntegrationTest, ComprehensiveStatsTracking) {
    if (!bridge) GTEST_SKIP();

    // Perform various operations
    uint64_t id = CreateSandbox("stats_test");
    if (id == 0) GTEST_SKIP();

    // Multiple depth checks
    for (int i = 0; i < 5; i++) {
        security_imagination_enforce_bounds(bridge, id, (uint32_t)(i + 1));
    }

    // Multiple confabulation checks
    uint8_t content[20] = {0};
    security_imagination_confab_result_t confab;
    for (int i = 0; i < 3; i++) {
        security_imagination_detect_confabulation(bridge, content, sizeof(content), &confab);
    }

    // Multiple grounding checks
    security_imagination_grounding_result_t grounding;
    for (int i = 0; i < 4; i++) {
        security_imagination_ground_reality(bridge, id, &grounding);
    }

    // Multiple integrity checks
    security_imagination_integrity_result_t integrity;
    for (int i = 0; i < 2; i++) {
        security_imagination_verify_simulation(bridge, id, &integrity);
    }

    // Multiple adversarial checks
    float adversarial;
    for (int i = 0; i < 6; i++) {
        security_imagination_check_adversarial(bridge, id, &adversarial);
    }

    // Verify stats
    security_imagination_stats_t stats;
    security_imagination_get_stats(bridge, &stats);

    EXPECT_EQ(stats.depth_checks, 5u);
    EXPECT_EQ(stats.confab_checks, 3u);
    EXPECT_EQ(stats.grounding_checks, 4u);
    EXPECT_EQ(stats.integrity_checks, 2u);
    EXPECT_EQ(stats.adversarial_checks, 6u);
}

TEST_F(SecurityImaginationIntegrationTest, StatsResetIntegration) {
    if (!bridge) GTEST_SKIP();

    // Generate activity
    uint64_t id = CreateSandbox("reset_test");
    if (id == 0) GTEST_SKIP();

    security_imagination_enforce_bounds(bridge, id, 3);
    security_imagination_track_resources(bridge, id, 500);

    // Verify stats exist
    security_imagination_stats_t stats;
    security_imagination_get_stats(bridge, &stats);
    EXPECT_GT(stats.sandboxes_created, 0u);
    EXPECT_GT(stats.depth_checks, 0u);

    // Reset stats
    security_imagination_reset_stats(bridge);

    // Verify reset
    security_imagination_get_stats(bridge, &stats);
    EXPECT_EQ(stats.sandboxes_created, 0u);
    EXPECT_EQ(stats.depth_checks, 0u);
    EXPECT_EQ(stats.total_resources_consumed, 0u);
}

// ============================================================================
// Bidirectional Effects Integration Tests
// ============================================================================

TEST_F(SecurityImaginationIntegrationTest, BidirectionalEffectsFlow) {
    if (!bridge) GTEST_SKIP();

    // Create sandbox and perform activities
    uint64_t id = CreateSandbox("bidirectional");
    if (id == 0) GTEST_SKIP();

    // Simulate heavy imagination usage
    security_imagination_enforce_bounds(bridge, id, 7);  // Deep
    security_imagination_track_resources(bridge, id, 300000);  // High resource

    // Trigger suspicious content
    uint8_t suspicious[100];
    memset(suspicious, 0xFF, sizeof(suspicious));  // High-value bytes
    security_imagination_confab_result_t confab;
    security_imagination_detect_confabulation(bridge, suspicious, sizeof(suspicious), &confab);

    // Update bridge
    security_imagination_bridge_update(bridge, 16);

    // Get imagination effects on security
    imagination_to_security_effects_t imag_effects;
    security_imagination_get_imagination_effects(bridge, &imag_effects);

    EXPECT_GT(imag_effects.total_simulation_steps, 0u);
    EXPECT_GT(imag_effects.total_resources_used, 0u);
    EXPECT_GE(imag_effects.avg_scenario_depth, 0.0f);

    // Get security effects on imagination
    security_to_imagination_effects_t sec_effects;
    security_imagination_get_security_effects(bridge, &sec_effects);

    EXPECT_GT(sec_effects.active_sandboxes, 0u);
}

// ============================================================================
// Concurrent-Like Operations Test
// ============================================================================

TEST_F(SecurityImaginationIntegrationTest, RapidSandboxCycling) {
    if (!bridge) GTEST_SKIP();

    // Rapidly create and destroy sandboxes
    for (int cycle = 0; cycle < 10; cycle++) {
        std::vector<uint64_t> ids;

        // Create batch
        for (int i = 0; i < 3; i++) {
            uint64_t id = 0;
            int ret = security_imagination_sandbox_workspace(
                bridge, "cycle", SANDBOX_LEVEL_STANDARD, &id
            );
            if (ret == NIMCP_SUCCESS) {
                ids.push_back(id);
            }
        }

        // Quick operations on all
        for (auto id : ids) {
            security_imagination_enforce_bounds(bridge, id, 2);
            security_imagination_track_resources(bridge, id, 50);
        }

        // Destroy batch
        for (auto id : ids) {
            security_imagination_release_sandbox(bridge, id);
        }
    }

    // Verify bridge still functional
    uint64_t final_id = CreateSandbox("final");
    EXPECT_GT(final_id, 0u);

    // Update and check state
    security_imagination_bridge_update(bridge, 16);

    security_imagination_state_t state;
    security_imagination_get_state(bridge, &state);
    EXPECT_TRUE(state.is_active);
}

// ============================================================================
// Error Recovery Integration Tests
// ============================================================================

TEST_F(SecurityImaginationIntegrationTest, RecoveryFromResourceExhaustion) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.default_simulation_budget = 1000;
    security_imagination_bridge_destroy(bridge);
    bridge = security_imagination_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t id = CreateSandbox("exhaustion");
    if (id == 0) GTEST_SKIP();

    // Exhaust resources
    security_imagination_track_resources(bridge, id, 1000);
    EXPECT_EQ(security_imagination_track_resources(bridge, id, 100),
              NIMCP_ERROR_OUT_OF_RANGE);

    // Release sandbox
    security_imagination_release_sandbox(bridge, id);

    // Create new sandbox - should work with fresh budget
    uint64_t id2 = CreateSandbox("fresh");
    EXPECT_GT(id2, 0u);

    // Should be able to track resources again
    EXPECT_EQ(security_imagination_track_resources(bridge, id2, 500), NIMCP_SUCCESS);
}

TEST_F(SecurityImaginationIntegrationTest, RecoveryFromSandboxQuota) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.max_sandboxed_workspaces = 2;
    security_imagination_bridge_destroy(bridge);
    bridge = security_imagination_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    // Fill quota
    uint64_t id1 = CreateSandbox("quota1");
    uint64_t id2 = CreateSandbox("quota2");
    EXPECT_GT(id1, 0u);
    EXPECT_GT(id2, 0u);

    // Verify quota reached
    security_to_imagination_effects_t effects;
    security_imagination_get_security_effects(bridge, &effects);
    EXPECT_TRUE(effects.sandbox_quota_reached);

    // Release one
    security_imagination_release_sandbox(bridge, id1);

    // Update effects
    security_imagination_update_security_effects(bridge);
    security_imagination_get_security_effects(bridge, &effects);
    EXPECT_FALSE(effects.sandbox_quota_reached);

    // Should be able to create again
    uint64_t id3 = CreateSandbox("recovery");
    EXPECT_GT(id3, 0u);
}

// ============================================================================
// Complete Workflow Integration Test
// ============================================================================

TEST_F(SecurityImaginationIntegrationTest, CompleteImaginationWorkflow) {
    if (!bridge) GTEST_SKIP();

    // 1. Create sandbox for imagination scenario
    uint64_t scenario_id = 0;
    ASSERT_EQ(security_imagination_sandbox_workspace(
        bridge, "complete_workflow", SANDBOX_LEVEL_STRICT, &scenario_id),
        NIMCP_SUCCESS);
    ASSERT_GT(scenario_id, 0u);

    // 2. Initialize scenario at depth 1
    EXPECT_TRUE(security_imagination_enforce_bounds(bridge, scenario_id, 1));

    // 3. Simulate imagination steps with increasing depth
    for (uint32_t depth = 2; depth <= 5; depth++) {
        // Check bounds before going deeper
        EXPECT_TRUE(security_imagination_check_depth(bridge, scenario_id, depth));

        // Enforce bounds
        EXPECT_TRUE(security_imagination_enforce_bounds(bridge, scenario_id, depth));

        // Track resources for this step
        EXPECT_EQ(security_imagination_track_resources(bridge, scenario_id, 1000),
                  NIMCP_SUCCESS);

        // Check reality grounding periodically
        if (depth % 2 == 0) {
            security_imagination_grounding_result_t grounding;
            security_imagination_ground_reality(bridge, scenario_id, &grounding);
        }
    }

    // 4. Generate some content and check for confabulation
    uint8_t generated_content[64];
    for (int i = 0; i < 64; i++) {
        generated_content[i] = (uint8_t)((i * 7) % 200);
    }
    security_imagination_confab_result_t confab;
    security_imagination_detect_confabulation(
        bridge, generated_content, sizeof(generated_content), &confab
    );

    // 5. Final integrity check
    security_imagination_integrity_result_t integrity;
    security_imagination_verify_simulation(bridge, scenario_id, &integrity);

    // 6. Update bridge state
    security_imagination_bridge_update(bridge, 100);

    // 7. Get final sandbox state
    security_imagination_sandbox_t sandbox;
    security_imagination_get_sandbox(bridge, scenario_id, &sandbox);

    EXPECT_EQ(sandbox.max_depth_reached, 5u);
    EXPECT_EQ(sandbox.simulation_steps, 4u);  // 4 resource tracking calls
    EXPECT_GT(sandbox.resources_used, 0u);
    EXPECT_TRUE(sandbox.flags & SECURITY_IMAGINATION_FLAG_SANDBOXED);
    EXPECT_TRUE(sandbox.flags & SECURITY_IMAGINATION_FLAG_BOUNDED);

    // 8. Release scenario
    EXPECT_EQ(security_imagination_release_sandbox(bridge, scenario_id), NIMCP_SUCCESS);

    // 9. Verify final stats
    security_imagination_stats_t stats;
    security_imagination_get_stats(bridge, &stats);

    EXPECT_EQ(stats.sandboxes_created, 1u);
    EXPECT_EQ(stats.sandboxes_destroyed, 1u);
    EXPECT_GT(stats.depth_checks, 0u);
    EXPECT_GT(stats.grounding_checks, 0u);
    EXPECT_GT(stats.integrity_checks, 0u);
}
