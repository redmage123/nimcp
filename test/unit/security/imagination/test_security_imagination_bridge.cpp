/**
 * @file test_security_imagination_bridge.cpp
 * @brief Unit tests for Security-Imagination Bridge
 *
 * WHAT: Tests for security-imagination bidirectional integration bridge
 * WHY:  Verify security module integrates correctly with imagination system
 * HOW:  Test lifecycle, sandbox, confabulation, bounds, grounding, integrity
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "security/imagination/nimcp_security_imagination_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

class SecurityImaginationBridgeTest : public ::testing::Test {
protected:
    security_imagination_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            security_imagination_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void CreateBridge() {
        security_imagination_config_t config;
        security_imagination_default_config(&config);
        bridge = security_imagination_bridge_create(&config);
    }

    void CreateBridgeWithConfig(const security_imagination_config_t* config) {
        bridge = security_imagination_bridge_create(config);
    }

    uint64_t CreateSandbox(const char* name = "test_scenario") {
        uint64_t sandbox_id = 0;
        security_imagination_sandbox_workspace(
            bridge, name, SANDBOX_LEVEL_STANDARD, &sandbox_id
        );
        return sandbox_id;
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, DefaultConfigReturnsValidConfig) {
    security_imagination_config_t config;
    int ret = security_imagination_default_config(&config);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Security features should be enabled by default
    EXPECT_TRUE(config.enable_workspace_sandboxing);
    EXPECT_TRUE(config.enable_confabulation_detection);
    EXPECT_TRUE(config.enable_reasoning_bounds);
    EXPECT_TRUE(config.enable_reality_grounding);
    EXPECT_TRUE(config.enable_simulation_integrity);
    EXPECT_TRUE(config.enable_adversarial_detection);
    EXPECT_TRUE(config.enable_resource_tracking);

    // Limits should have sensible defaults
    EXPECT_GT(config.max_hypothetical_depth, 0u);
    EXPECT_GT(config.default_simulation_budget, 0u);
    EXPECT_GT(config.confabulation_threshold, 0.0f);
    EXPECT_LE(config.confabulation_threshold, 1.0f);
}

TEST_F(SecurityImaginationBridgeTest, DefaultConfigNullPointer) {
    int ret = security_imagination_default_config(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, CreateWithNullConfig) {
    bridge = security_imagination_bridge_create(nullptr);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SecurityImaginationBridgeTest, CreateWithValidConfig) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    bridge = security_imagination_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SecurityImaginationBridgeTest, CreateWithCustomConfig) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);

    config.max_hypothetical_depth = 4;
    config.default_simulation_budget = 100000;
    config.confabulation_threshold = 0.5f;

    bridge = security_imagination_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SecurityImaginationBridgeTest, DestroyNull) {
    security_imagination_bridge_destroy(nullptr);
    // Should not crash
}

TEST_F(SecurityImaginationBridgeTest, DestroyValid) {
    CreateBridge();
    if (bridge) {
        security_imagination_bridge_destroy(bridge);
        bridge = nullptr;
    }
}

// ============================================================================
// Connection Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, ConnectEngineNullBridge) {
    struct imagination_engine* engine = nullptr;
    EXPECT_EQ(security_imagination_connect_engine(nullptr, engine),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, ConnectWorkspaceNullBridge) {
    struct imagination_workspace* workspace = nullptr;
    EXPECT_EQ(security_imagination_connect_workspace(nullptr, workspace),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, IsConnectedNullBridge) {
    EXPECT_FALSE(security_imagination_is_connected(nullptr));
}

// ============================================================================
// Connection Tests - Valid Bridge, NULL System
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, ConnectEngineNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_imagination_connect_engine(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, ConnectWorkspaceNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_imagination_connect_workspace(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, NotConnectedInitially) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_FALSE(security_imagination_is_connected(bridge));
}

// ============================================================================
// Sandbox Management Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, SandboxWorkspaceNullBridge) {
    uint64_t sandbox_id = 0;
    EXPECT_EQ(security_imagination_sandbox_workspace(nullptr, "test",
              SANDBOX_LEVEL_STANDARD, &sandbox_id), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, ReleaseSandboxNullBridge) {
    EXPECT_EQ(security_imagination_release_sandbox(nullptr, 1),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, GetSandboxNullBridge) {
    security_imagination_sandbox_t sandbox;
    EXPECT_EQ(security_imagination_get_sandbox(nullptr, 1, &sandbox),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Sandbox Management Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, SandboxWorkspaceSuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = 0;
    int ret = security_imagination_sandbox_workspace(
        bridge, "test_scenario", SANDBOX_LEVEL_STANDARD, &sandbox_id
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(sandbox_id, 0u);
}

TEST_F(SecurityImaginationBridgeTest, SandboxWorkspaceNullName) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = 0;
    int ret = security_imagination_sandbox_workspace(
        bridge, nullptr, SANDBOX_LEVEL_STANDARD, &sandbox_id
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(sandbox_id, 0u);
}

TEST_F(SecurityImaginationBridgeTest, SandboxWorkspaceNullId) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_imagination_sandbox_workspace(
        bridge, "test", SANDBOX_LEVEL_STANDARD, nullptr
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(SecurityImaginationBridgeTest, SandboxWorkspaceDifferentLevels) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t id1 = 0, id2 = 0, id3 = 0;

    EXPECT_EQ(security_imagination_sandbox_workspace(
        bridge, "minimal", SANDBOX_LEVEL_MINIMAL, &id1), NIMCP_SUCCESS);
    EXPECT_EQ(security_imagination_sandbox_workspace(
        bridge, "strict", SANDBOX_LEVEL_STRICT, &id2), NIMCP_SUCCESS);
    EXPECT_EQ(security_imagination_sandbox_workspace(
        bridge, "maximum", SANDBOX_LEVEL_MAXIMUM, &id3), NIMCP_SUCCESS);

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
}

TEST_F(SecurityImaginationBridgeTest, ReleaseSandboxSuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    int ret = security_imagination_release_sandbox(bridge, sandbox_id);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(SecurityImaginationBridgeTest, ReleaseSandboxNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_imagination_release_sandbox(bridge, 999999);
    EXPECT_EQ(ret, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityImaginationBridgeTest, GetSandboxSuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox("my_scenario");
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_sandbox_t sandbox;
    int ret = security_imagination_get_sandbox(bridge, sandbox_id, &sandbox);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(sandbox.sandbox_id, sandbox_id);
    EXPECT_TRUE(sandbox.is_active);
    EXPECT_STREQ(sandbox.scenario_name, "my_scenario");
}

TEST_F(SecurityImaginationBridgeTest, GetSandboxNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_imagination_sandbox_t sandbox;
    int ret = security_imagination_get_sandbox(bridge, 999999, &sandbox);
    EXPECT_EQ(ret, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityImaginationBridgeTest, GetSandboxNullOutput) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    int ret = security_imagination_get_sandbox(bridge, sandbox_id, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, MultipleSandboxes) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t ids[5];
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "scenario_%d", i);
        int ret = security_imagination_sandbox_workspace(
            bridge, name, SANDBOX_LEVEL_STANDARD, &ids[i]
        );
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_GT(ids[i], 0u);
    }

    // All IDs should be unique
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST_F(SecurityImaginationBridgeTest, SandboxQuotaExceeded) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.max_sandboxed_workspaces = 3;
    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t id;
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(security_imagination_sandbox_workspace(
            bridge, "test", SANDBOX_LEVEL_STANDARD, &id), NIMCP_SUCCESS);
    }

    // Fourth should fail
    int ret = security_imagination_sandbox_workspace(
        bridge, "test", SANDBOX_LEVEL_STANDARD, &id
    );
    EXPECT_EQ(ret, NIMCP_ERROR_OUT_OF_RANGE);
}

// ============================================================================
// Confabulation Detection Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, DetectConfabulationNullBridge) {
    char content[] = "test content";
    security_imagination_confab_result_t result;
    EXPECT_EQ(security_imagination_detect_confabulation(
        nullptr, content, sizeof(content), &result), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, CheckScenarioConfabNullBridge) {
    security_imagination_confab_result_t result;
    EXPECT_EQ(security_imagination_check_scenario_confabulation(
        nullptr, 1, &result), NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Confabulation Detection Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, DetectConfabulationNormalContent) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Normal varied content
    uint8_t content[100];
    for (int i = 0; i < 100; i++) {
        content[i] = (uint8_t)(i % 128);
    }

    security_imagination_confab_result_t result;
    int ret = security_imagination_detect_confabulation(
        bridge, content, sizeof(content), &result
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(result.score, 0.0f);
    EXPECT_LE(result.score, 1.0f);
}

TEST_F(SecurityImaginationBridgeTest, DetectConfabulationRepetitiveContent) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Highly repetitive content (potential hallucination)
    uint8_t content[100];
    memset(content, 'A', sizeof(content));

    security_imagination_confab_result_t result;
    int ret = security_imagination_detect_confabulation(
        bridge, content, sizeof(content), &result
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    // High repetition should result in higher score
    EXPECT_GT(result.score, 0.0f);
}

TEST_F(SecurityImaginationBridgeTest, DetectConfabulationNullContent) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_imagination_confab_result_t result;
    int ret = security_imagination_detect_confabulation(
        bridge, nullptr, 0, &result
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_FALSE(result.detected);
    EXPECT_EQ(result.score, 0.0f);
}

TEST_F(SecurityImaginationBridgeTest, DetectConfabulationNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    char content[] = "test";
    int ret = security_imagination_detect_confabulation(
        bridge, content, sizeof(content), nullptr
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, DetectConfabulationDisabled) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.enable_confabulation_detection = false;
    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    // High-anomaly content
    uint8_t content[100];
    memset(content, 0xFF, sizeof(content));

    security_imagination_confab_result_t result;
    int ret = security_imagination_detect_confabulation(
        bridge, content, sizeof(content), &result
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_FALSE(result.detected);
    EXPECT_EQ(result.score, 0.0f);
}

TEST_F(SecurityImaginationBridgeTest, CheckScenarioConfabulation) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_confab_result_t result;
    int ret = security_imagination_check_scenario_confabulation(
        bridge, sandbox_id, &result
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(SecurityImaginationBridgeTest, CheckScenarioConfabNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_imagination_confab_result_t result;
    int ret = security_imagination_check_scenario_confabulation(
        bridge, 999999, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NOT_FOUND);
}

// ============================================================================
// Reasoning Bounds Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, EnforceBoundsNullBridge) {
    EXPECT_FALSE(security_imagination_enforce_bounds(nullptr, 1, 5));
}

TEST_F(SecurityImaginationBridgeTest, CheckDepthNullBridge) {
    EXPECT_FALSE(security_imagination_check_depth(nullptr, 1, 5));
}

TEST_F(SecurityImaginationBridgeTest, GetMaxDepthNullBridge) {
    EXPECT_EQ(security_imagination_get_max_depth(nullptr), 0u);
}

// ============================================================================
// Reasoning Bounds Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, EnforceBoundsWithinLimit) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // Depth 1 should always be allowed
    EXPECT_TRUE(security_imagination_enforce_bounds(bridge, sandbox_id, 1));

    // Default max is 8, so 5 should be fine
    EXPECT_TRUE(security_imagination_enforce_bounds(bridge, sandbox_id, 5));
}

TEST_F(SecurityImaginationBridgeTest, EnforceBoundsExceedsLimit) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // Default max is 8
    EXPECT_FALSE(security_imagination_enforce_bounds(bridge, sandbox_id, 100));
}

TEST_F(SecurityImaginationBridgeTest, EnforceBoundsNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_FALSE(security_imagination_enforce_bounds(bridge, 999999, 1));
}

TEST_F(SecurityImaginationBridgeTest, EnforceBoundsDisabled) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.enable_reasoning_bounds = false;
    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // With bounds disabled, any depth should be allowed
    EXPECT_TRUE(security_imagination_enforce_bounds(bridge, sandbox_id, 1000));
}

TEST_F(SecurityImaginationBridgeTest, CheckDepthWithinLimit) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    EXPECT_TRUE(security_imagination_check_depth(bridge, sandbox_id, 1));
    EXPECT_TRUE(security_imagination_check_depth(bridge, sandbox_id, 5));
}

TEST_F(SecurityImaginationBridgeTest, CheckDepthExceedsLimit) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    EXPECT_FALSE(security_imagination_check_depth(bridge, sandbox_id, 100));
}

TEST_F(SecurityImaginationBridgeTest, GetMaxDepthValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t max_depth = security_imagination_get_max_depth(bridge);
    EXPECT_EQ(max_depth, SECURITY_IMAGINATION_DEFAULT_MAX_DEPTH);
}

TEST_F(SecurityImaginationBridgeTest, CustomMaxDepth) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.max_hypothetical_depth = 4;
    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint32_t max_depth = security_imagination_get_max_depth(bridge);
    EXPECT_EQ(max_depth, 4u);
}

// ============================================================================
// Reality Grounding Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, GroundRealityNullBridge) {
    security_imagination_grounding_result_t result;
    EXPECT_EQ(security_imagination_ground_reality(nullptr, 1, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, GetDivergenceNullBridge) {
    EXPECT_LT(security_imagination_get_divergence(nullptr, 1), 0.0f);
}

// ============================================================================
// Reality Grounding Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, GroundRealitySuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_grounding_result_t result;
    int ret = security_imagination_ground_reality(bridge, sandbox_id, &result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(result.divergence_score, 0.0f);
    EXPECT_LE(result.divergence_score, 1.0f);
}

TEST_F(SecurityImaginationBridgeTest, GroundRealityNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    int ret = security_imagination_ground_reality(bridge, sandbox_id, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, GroundRealityNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_imagination_grounding_result_t result;
    int ret = security_imagination_ground_reality(bridge, 999999, &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityImaginationBridgeTest, GroundRealityDisabled) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.enable_reality_grounding = false;
    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_grounding_result_t result;
    int ret = security_imagination_ground_reality(bridge, sandbox_id, &result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_TRUE(result.grounded);
    EXPECT_EQ(result.divergence_score, 0.0f);
}

TEST_F(SecurityImaginationBridgeTest, GetDivergenceValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // First call ground_reality to set divergence
    security_imagination_grounding_result_t result;
    security_imagination_ground_reality(bridge, sandbox_id, &result);

    float divergence = security_imagination_get_divergence(bridge, sandbox_id);
    EXPECT_GE(divergence, 0.0f);
    EXPECT_LE(divergence, 1.0f);
}

TEST_F(SecurityImaginationBridgeTest, GetDivergenceNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    float divergence = security_imagination_get_divergence(bridge, 999999);
    EXPECT_LT(divergence, 0.0f);
}

// ============================================================================
// Simulation Integrity Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, VerifySimulationNullBridge) {
    security_imagination_integrity_result_t result;
    EXPECT_EQ(security_imagination_verify_simulation(nullptr, 1, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, CheckAdversarialNullBridge) {
    float score;
    EXPECT_EQ(security_imagination_check_adversarial(nullptr, 1, &score),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Simulation Integrity Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, VerifySimulationSuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_integrity_result_t result;
    int ret = security_imagination_verify_simulation(bridge, sandbox_id, &result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(result.integrity_score, 0.0f);
    EXPECT_LE(result.integrity_score, 1.0f);
}

TEST_F(SecurityImaginationBridgeTest, VerifySimulationNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    int ret = security_imagination_verify_simulation(bridge, sandbox_id, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, VerifySimulationNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_imagination_integrity_result_t result;
    int ret = security_imagination_verify_simulation(bridge, 999999, &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityImaginationBridgeTest, VerifySimulationDisabled) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.enable_simulation_integrity = false;
    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_integrity_result_t result;
    int ret = security_imagination_verify_simulation(bridge, sandbox_id, &result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_TRUE(result.integrity_valid);
    EXPECT_EQ(result.integrity_score, 1.0f);
}

TEST_F(SecurityImaginationBridgeTest, CheckAdversarialSuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    float score;
    int ret = security_imagination_check_adversarial(bridge, sandbox_id, &score);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(SecurityImaginationBridgeTest, CheckAdversarialNullScore) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    int ret = security_imagination_check_adversarial(bridge, sandbox_id, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, CheckAdversarialNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    float score;
    int ret = security_imagination_check_adversarial(bridge, 999999, &score);
    EXPECT_EQ(ret, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityImaginationBridgeTest, CheckAdversarialDisabled) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.enable_adversarial_detection = false;
    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    float score;
    int ret = security_imagination_check_adversarial(bridge, sandbox_id, &score);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(score, 0.0f);
}

// ============================================================================
// Resource Tracking Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, TrackResourcesNullBridge) {
    EXPECT_EQ(security_imagination_track_resources(nullptr, 1, 100),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, GetResourcesNullBridge) {
    EXPECT_EQ(security_imagination_get_resources(nullptr, 1), 0u);
}

// ============================================================================
// Resource Tracking Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, TrackResourcesSuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    int ret = security_imagination_track_resources(bridge, sandbox_id, 100);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    ret = security_imagination_track_resources(bridge, sandbox_id, 200);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(SecurityImaginationBridgeTest, TrackResourcesNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_imagination_track_resources(bridge, 999999, 100);
    EXPECT_EQ(ret, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityImaginationBridgeTest, TrackResourcesExceedsBudget) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.default_simulation_budget = 1000;
    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // First call within budget
    int ret = security_imagination_track_resources(bridge, sandbox_id, 500);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Second call exceeds budget
    ret = security_imagination_track_resources(bridge, sandbox_id, 600);
    EXPECT_EQ(ret, NIMCP_ERROR_OUT_OF_RANGE);
}

TEST_F(SecurityImaginationBridgeTest, TrackResourcesDisabled) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.enable_resource_tracking = false;
    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // Should succeed regardless of amount
    int ret = security_imagination_track_resources(bridge, sandbox_id, UINT64_MAX);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(SecurityImaginationBridgeTest, GetResourcesValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_track_resources(bridge, sandbox_id, 150);

    uint64_t resources = security_imagination_get_resources(bridge, sandbox_id);
    EXPECT_EQ(resources, 150u);
}

TEST_F(SecurityImaginationBridgeTest, GetResourcesAccumulates) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_track_resources(bridge, sandbox_id, 100);
    security_imagination_track_resources(bridge, sandbox_id, 200);
    security_imagination_track_resources(bridge, sandbox_id, 50);

    uint64_t resources = security_imagination_get_resources(bridge, sandbox_id);
    EXPECT_EQ(resources, 350u);
}

TEST_F(SecurityImaginationBridgeTest, GetResourcesNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t resources = security_imagination_get_resources(bridge, 999999);
    EXPECT_EQ(resources, 0u);
}

// ============================================================================
// Bidirectional Update Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, UpdateSecurityEffectsNullBridge) {
    EXPECT_EQ(security_imagination_update_security_effects(nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, UpdateImaginationEffectsNullBridge) {
    EXPECT_EQ(security_imagination_update_imagination_effects(nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, BridgeUpdateNullBridge) {
    EXPECT_EQ(security_imagination_bridge_update(nullptr, 16),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Bidirectional Update Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, UpdateSecurityEffectsValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_imagination_update_security_effects(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(SecurityImaginationBridgeTest, UpdateImaginationEffectsValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_imagination_update_imagination_effects(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(SecurityImaginationBridgeTest, BridgeUpdateValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_imagination_bridge_update(bridge, 16);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(SecurityImaginationBridgeTest, BridgeUpdateWithActivity) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Add some activity
    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id > 0) {
        security_imagination_track_resources(bridge, sandbox_id, 100);
        security_imagination_enforce_bounds(bridge, sandbox_id, 3);
    }

    int ret = security_imagination_bridge_update(bridge, 16);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

// ============================================================================
// Query Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, GetSecurityEffectsNullBridge) {
    security_to_imagination_effects_t effects;
    EXPECT_EQ(security_imagination_get_security_effects(nullptr, &effects),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, GetImaginationEffectsNullBridge) {
    imagination_to_security_effects_t effects;
    EXPECT_EQ(security_imagination_get_imagination_effects(nullptr, &effects),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, GetStateNullBridge) {
    security_imagination_state_t state;
    EXPECT_EQ(security_imagination_get_state(nullptr, &state),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, GetStatsNullBridge) {
    security_imagination_stats_t stats;
    EXPECT_EQ(security_imagination_get_stats(nullptr, &stats),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, ResetStatsNullBridge) {
    EXPECT_EQ(security_imagination_reset_stats(nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Query Tests - Valid Bridge, NULL Output
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, GetSecurityEffectsNullOutput) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_imagination_get_security_effects(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, GetImaginationEffectsNullOutput) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_imagination_get_imagination_effects(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, GetStateNullOutput) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_imagination_get_state(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, GetStatsNullOutput) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_imagination_get_stats(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Query Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, GetSecurityEffectsValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_to_imagination_effects_t effects;
    int ret = security_imagination_get_security_effects(bridge, &effects);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(effects.effective_max_depth, 0u);
    EXPECT_GE(effects.depth_reduction_factor, 0.0f);
    EXPECT_LE(effects.depth_reduction_factor, 1.0f);
}

TEST_F(SecurityImaginationBridgeTest, GetImaginationEffectsValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    imagination_to_security_effects_t effects;
    int ret = security_imagination_get_imagination_effects(bridge, &effects);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(effects.total_scenarios, 0u);
}

TEST_F(SecurityImaginationBridgeTest, GetStateValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_imagination_state_t state;
    int ret = security_imagination_get_state(bridge, &state);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_TRUE(state.is_active);
    EXPECT_FALSE(state.imagination_restricted);
}

TEST_F(SecurityImaginationBridgeTest, GetStatsValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_imagination_stats_t stats;
    int ret = security_imagination_get_stats(bridge, &stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(stats.sandboxes_created, 0u);
}

TEST_F(SecurityImaginationBridgeTest, ResetStatsValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Generate some activity
    CreateSandbox();
    CreateSandbox();

    // Reset stats
    int ret = security_imagination_reset_stats(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Check stats are zero
    security_imagination_stats_t stats;
    security_imagination_get_stats(bridge, &stats);
    EXPECT_EQ(stats.sandboxes_created, 0u);
    EXPECT_EQ(stats.confab_checks, 0u);
}

// ============================================================================
// Restriction Mode Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, EnterRestrictedNullBridge) {
    EXPECT_EQ(security_imagination_enter_restricted(nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, ExitRestrictedNullBridge) {
    EXPECT_EQ(security_imagination_exit_restricted(nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, IsRestrictedNullBridge) {
    EXPECT_FALSE(security_imagination_is_restricted(nullptr));
}

TEST_F(SecurityImaginationBridgeTest, BlockNewScenariosNullBridge) {
    EXPECT_EQ(security_imagination_block_new_scenarios(nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityImaginationBridgeTest, AllowNewScenariosNullBridge) {
    EXPECT_EQ(security_imagination_allow_new_scenarios(nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Restriction Mode Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, NotRestrictedInitially) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_FALSE(security_imagination_is_restricted(bridge));
}

TEST_F(SecurityImaginationBridgeTest, EnterRestrictedSuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_imagination_enter_restricted(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_TRUE(security_imagination_is_restricted(bridge));
}

TEST_F(SecurityImaginationBridgeTest, ExitRestrictedSuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Enter restricted first
    security_imagination_enter_restricted(bridge);
    EXPECT_TRUE(security_imagination_is_restricted(bridge));

    // Exit restricted
    int ret = security_imagination_exit_restricted(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_FALSE(security_imagination_is_restricted(bridge));
}

TEST_F(SecurityImaginationBridgeTest, RestrictedReducesDepth) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Get initial max depth
    uint32_t initial_depth = security_imagination_get_max_depth(bridge);

    // Enter restricted mode
    security_imagination_enter_restricted(bridge);
    security_imagination_update_security_effects(bridge);

    // Max depth should be reduced
    uint32_t restricted_depth = security_imagination_get_max_depth(bridge);
    EXPECT_LT(restricted_depth, initial_depth);

    // Exit restricted
    security_imagination_exit_restricted(bridge);
    security_imagination_update_security_effects(bridge);

    // Depth should be restored
    uint32_t restored_depth = security_imagination_get_max_depth(bridge);
    EXPECT_EQ(restored_depth, initial_depth);
}

TEST_F(SecurityImaginationBridgeTest, BlockNewScenariosSuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_imagination_block_new_scenarios(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Try to create a new sandbox
    uint64_t sandbox_id = 0;
    ret = security_imagination_sandbox_workspace(
        bridge, "blocked", SANDBOX_LEVEL_STANDARD, &sandbox_id
    );
    EXPECT_EQ(ret, NIMCP_ERROR_INVALID_STATE);
}

TEST_F(SecurityImaginationBridgeTest, AllowNewScenariosSuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Block first
    security_imagination_block_new_scenarios(bridge);

    // Allow again
    int ret = security_imagination_allow_new_scenarios(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Should be able to create sandbox now
    uint64_t sandbox_id = 0;
    ret = security_imagination_sandbox_workspace(
        bridge, "allowed", SANDBOX_LEVEL_STANDARD, &sandbox_id
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(sandbox_id, 0u);
}

// ============================================================================
// State Consistency Tests
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, StateAfterRestriction) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_imagination_enter_restricted(bridge);

    security_imagination_state_t state;
    int ret = security_imagination_get_state(bridge, &state);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_TRUE(state.imagination_restricted);

    security_imagination_exit_restricted(bridge);

    ret = security_imagination_get_state(bridge, &state);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_FALSE(state.imagination_restricted);
}

TEST_F(SecurityImaginationBridgeTest, EffectsAfterSandboxCreation) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Get initial effects
    security_to_imagination_effects_t effects_before;
    security_imagination_get_security_effects(bridge, &effects_before);

    // Create sandboxes
    for (int i = 0; i < 3; i++) {
        CreateSandbox();
    }

    // Update and get effects
    security_imagination_update_security_effects(bridge);
    security_to_imagination_effects_t effects_after;
    security_imagination_get_security_effects(bridge, &effects_after);

    EXPECT_EQ(effects_after.active_sandboxes, 3u);
}

TEST_F(SecurityImaginationBridgeTest, StatsAfterActivity) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Create sandbox and perform activities
    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    security_imagination_enforce_bounds(bridge, sandbox_id, 3);
    security_imagination_track_resources(bridge, sandbox_id, 100);

    security_imagination_grounding_result_t grounding;
    security_imagination_ground_reality(bridge, sandbox_id, &grounding);

    security_imagination_integrity_result_t integrity;
    security_imagination_verify_simulation(bridge, sandbox_id, &integrity);

    // Check stats
    security_imagination_stats_t stats;
    security_imagination_get_stats(bridge, &stats);

    EXPECT_GT(stats.sandboxes_created, 0u);
    EXPECT_GT(stats.depth_checks, 0u);
    EXPECT_GT(stats.grounding_checks, 0u);
    EXPECT_GT(stats.integrity_checks, 0u);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(SecurityImaginationBridgeTest, EmptyScenarioName) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = 0;
    int ret = security_imagination_sandbox_workspace(
        bridge, "", SANDBOX_LEVEL_STANDARD, &sandbox_id
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(SecurityImaginationBridgeTest, LongScenarioName) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    char long_name[256];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    uint64_t sandbox_id = 0;
    int ret = security_imagination_sandbox_workspace(
        bridge, long_name, SANDBOX_LEVEL_STANDARD, &sandbox_id
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Verify name is truncated
    security_imagination_sandbox_t sandbox;
    security_imagination_get_sandbox(bridge, sandbox_id, &sandbox);
    EXPECT_LT(strlen(sandbox.scenario_name), 256u);
}

TEST_F(SecurityImaginationBridgeTest, ZeroDepthConfig) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.max_hypothetical_depth = 0;
    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // With max depth 0, any depth should fail
    EXPECT_FALSE(security_imagination_enforce_bounds(bridge, sandbox_id, 1));
}

TEST_F(SecurityImaginationBridgeTest, ZeroResourceBudget) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);
    config.default_simulation_budget = 0;
    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint64_t sandbox_id = CreateSandbox();
    if (sandbox_id == 0) GTEST_SKIP();

    // With zero budget, any usage should fail (or be unlimited)
    int ret = security_imagination_track_resources(bridge, sandbox_id, 1);
    EXPECT_TRUE(ret == NIMCP_SUCCESS || ret == NIMCP_ERROR_OUT_OF_RANGE);
}

TEST_F(SecurityImaginationBridgeTest, DoubleEnterRestricted) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_imagination_enter_restricted(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Double enter should be idempotent
    ret = security_imagination_enter_restricted(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    EXPECT_TRUE(security_imagination_is_restricted(bridge));
}

TEST_F(SecurityImaginationBridgeTest, ExitRestrictedWithoutEnter) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_imagination_exit_restricted(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_FALSE(security_imagination_is_restricted(bridge));
}

TEST_F(SecurityImaginationBridgeTest, DisabledFeatures) {
    security_imagination_config_t config;
    security_imagination_default_config(&config);

    // Disable all features
    config.enable_workspace_sandboxing = false;
    config.enable_confabulation_detection = false;
    config.enable_reasoning_bounds = false;
    config.enable_reality_grounding = false;
    config.enable_simulation_integrity = false;
    config.enable_adversarial_detection = false;
    config.enable_resource_tracking = false;

    CreateBridgeWithConfig(&config);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SecurityImaginationBridgeTest, SandboxAllIsolationLevels) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    sandbox_isolation_level_t levels[] = {
        SANDBOX_LEVEL_NONE,
        SANDBOX_LEVEL_MINIMAL,
        SANDBOX_LEVEL_STANDARD,
        SANDBOX_LEVEL_STRICT,
        SANDBOX_LEVEL_MAXIMUM
    };

    for (auto level : levels) {
        uint64_t sandbox_id = 0;
        int ret = security_imagination_sandbox_workspace(
            bridge, "test", level, &sandbox_id
        );
        EXPECT_EQ(ret, NIMCP_SUCCESS);

        security_imagination_sandbox_t sandbox;
        security_imagination_get_sandbox(bridge, sandbox_id, &sandbox);
        EXPECT_EQ(sandbox.isolation_level, level);
    }
}

TEST_F(SecurityImaginationBridgeTest, HighValueContent) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Content with high-value bytes
    uint8_t content[100];
    memset(content, 0xFF, sizeof(content));

    security_imagination_confab_result_t result;
    int ret = security_imagination_detect_confabulation(
        bridge, content, sizeof(content), &result
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    // High-value bytes indicate potential anomaly
    EXPECT_GT(result.score, 0.0f);
}

TEST_F(SecurityImaginationBridgeTest, ReleaseThenReuse) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Create and release a sandbox
    uint64_t id1 = CreateSandbox();
    EXPECT_GT(id1, 0u);
    security_imagination_release_sandbox(bridge, id1);

    // Create another - should reuse the slot
    uint64_t id2 = CreateSandbox();
    EXPECT_GT(id2, 0u);

    // IDs should be different
    EXPECT_NE(id1, id2);
}
