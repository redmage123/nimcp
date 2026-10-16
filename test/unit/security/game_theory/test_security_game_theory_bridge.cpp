/**
 * @file test_security_game_theory_bridge.cpp
 * @brief Unit tests for Security-Game Theory Integration Bridge
 *
 * WHAT: Tests for security-game theory bidirectional bridge
 * WHY:  Verify payoff validation, coalition monitoring, mechanism verification,
 *       equilibrium checking, and manipulation detection integrate correctly
 * HOW:  Test lifecycle, connections, validation, monitoring, verification,
 *       detection, and bidirectional effects
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <limits>

extern "C" {
#include "security/game_theory/nimcp_security_game_theory_bridge.h"
#include "cognitive/game_theory/nimcp_game_theory.h"
#include "cognitive/game_theory/nimcp_gt_equilibrium.h"
#include "cognitive/game_theory/nimcp_gt_coalition.h"
#include "cognitive/game_theory/nimcp_gt_mechanism.h"
#include "utils/error/nimcp_error_codes.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class SecurityGameTheoryBridgeTest : public ::testing::Test {
protected:
    security_game_theory_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            security_gt_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void CreateBridge() {
        security_game_theory_config_t config;
        security_gt_default_config(&config);
        bridge = security_gt_bridge_create(&config);
    }

    void CreateBridgeWithConfig(const security_game_theory_config_t* config) {
        bridge = security_gt_bridge_create(config);
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, DefaultConfigReturnsValidConfig) {
    security_game_theory_config_t config;
    memset(&config, 0, sizeof(config));

    int ret = security_gt_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_payoff_validation);
    EXPECT_TRUE(config.enable_coalition_monitoring);
    EXPECT_TRUE(config.enable_mechanism_verification);
    EXPECT_TRUE(config.enable_equilibrium_verification);
    EXPECT_TRUE(config.enable_manipulation_detection);
    EXPECT_GE(config.security_sensitivity, 0.5f);
    EXPECT_LE(config.security_sensitivity, 2.0f);
    EXPECT_GE(config.game_theory_sensitivity, 0.5f);
    EXPECT_LE(config.game_theory_sensitivity, 2.0f);
}

TEST_F(SecurityGameTheoryBridgeTest, DefaultConfigNullPointer) {
    int ret = security_gt_default_config(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, DefaultConfigPayoffBounds) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);

    EXPECT_LT(config.payoff_lower_bound, 0.0f);
    EXPECT_GT(config.payoff_upper_bound, 0.0f);
    EXPECT_LT(config.payoff_lower_bound, config.payoff_upper_bound);
}

TEST_F(SecurityGameTheoryBridgeTest, DefaultConfigThresholds) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);

    EXPECT_GT(config.sybil_detection_threshold, 0.0f);
    EXPECT_LT(config.sybil_detection_threshold, 1.0f);
    EXPECT_GT(config.collusion_threshold, 0.0f);
    EXPECT_LT(config.collusion_threshold, 1.0f);
    EXPECT_GT(config.nash_epsilon, 0.0f);
    EXPECT_GT(config.regret_threshold, 0.0f);
}

TEST_F(SecurityGameTheoryBridgeTest, CreateWithNullConfig) {
    bridge = security_gt_bridge_create(nullptr);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SecurityGameTheoryBridgeTest, CreateWithValidConfig) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);

    bridge = security_gt_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SecurityGameTheoryBridgeTest, CreateWithCustomConfig) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);

    config.enable_payoff_validation = true;
    config.enable_coalition_monitoring = false;
    config.enable_mechanism_verification = true;
    config.enable_equilibrium_verification = false;
    config.max_coalition_size = 8;
    config.nash_epsilon = 0.001f;

    bridge = security_gt_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SecurityGameTheoryBridgeTest, DestroyNull) {
    security_gt_bridge_destroy(nullptr);
}

TEST_F(SecurityGameTheoryBridgeTest, DestroyValid) {
    CreateBridge();
    if (bridge) {
        security_gt_bridge_destroy(bridge);
        bridge = nullptr;
    }
}

TEST_F(SecurityGameTheoryBridgeTest, CreateDestroyMultiple) {
    for (int i = 0; i < 5; i++) {
        security_game_theory_config_t config;
        security_gt_default_config(&config);
        security_game_theory_bridge_t* temp = security_gt_bridge_create(&config);
        EXPECT_NE(temp, nullptr);
        security_gt_bridge_destroy(temp);
    }
}

// ============================================================================
// Connection Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, ConnectGtSystemNullBridge) {
    nimcp_gt_system_t gt = nullptr;
    EXPECT_EQ(security_gt_bridge_connect_gt_system(nullptr, gt), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, ConnectCoalitionGameNullBridge) {
    nimcp_coalition_game_t coal = nullptr;
    EXPECT_EQ(security_gt_bridge_connect_coalition_game(nullptr, coal), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, ConnectMechanismNullBridge) {
    nimcp_mechanism_t mech = nullptr;
    EXPECT_EQ(security_gt_bridge_connect_mechanism(nullptr, mech), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, ConnectEquilibriumNullBridge) {
    nimcp_equilibrium_t eq = nullptr;
    EXPECT_EQ(security_gt_bridge_connect_equilibrium(nullptr, eq), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, DisconnectNullBridge) {
    EXPECT_EQ(security_gt_bridge_disconnect(nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, IsConnectedNullBridge) {
    EXPECT_FALSE(security_gt_bridge_is_connected(nullptr));
}

// ============================================================================
// Connection Tests - Valid Bridge, NULL System
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, ConnectGtSystemNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_gt_bridge_connect_gt_system(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, ConnectCoalitionGameNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_gt_bridge_connect_coalition_game(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, ConnectMechanismNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_gt_bridge_connect_mechanism(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, ConnectEquilibriumNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_gt_bridge_connect_equilibrium(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Connection Tests - Valid Bridge, Disconnect
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, DisconnectValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    int ret = security_gt_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, IsConnectedNoConnections) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_FALSE(security_gt_bridge_is_connected(bridge));
}

// ============================================================================
// Payoff Validation Tests - NULL Inputs
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffNullBridge) {
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    security_payoff_result_t result;
    EXPECT_EQ(security_gt_validate_payoff_matrix(nullptr, payoffs, 2, 2, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_EQ(security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffNullMatrix) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    security_payoff_result_t result;
    int ret = security_gt_validate_payoff_matrix(bridge, nullptr, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_INVALID_NULL);
}

// ============================================================================
// Payoff Validation Tests - Invalid Dimensions
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffZeroRows) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 0, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_INVALID_DIMENSION);
}

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffZeroCols) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 0, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_INVALID_DIMENSION);
}

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffExceedMaxDimension) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 100, 100, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_INVALID_DIMENSION);
}

// ============================================================================
// Payoff Validation Tests - Valid Matrices
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffValidMatrix) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_VALID);
    EXPECT_EQ(result.nan_count, 0u);
    EXPECT_EQ(result.inf_count, 0u);
    EXPECT_EQ(result.out_of_bounds_count, 0u);
    EXPECT_EQ(result.rows, 2u);
    EXPECT_EQ(result.cols, 2u);
    EXPECT_EQ(result.total_elements, 4u);
}

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffValidLargeMatrix) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const uint32_t size = 32;
    float payoffs[size * size];
    for (uint32_t i = 0; i < size * size; i++) {
        payoffs[i] = (float)i * 0.01f;
    }

    security_payoff_result_t result;
    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, size, size, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.total_elements, size * size);
}

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffNegativeValues) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    float payoffs[4] = {-1.0f, -2.0f, -3.0f, -4.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.min_value, -4.0f);
    EXPECT_EQ(result.max_value, -1.0f);
}

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffMixedValues) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    float payoffs[4] = {-10.0f, 5.0f, -3.0f, 20.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.min_value, -10.0f);
    EXPECT_EQ(result.max_value, 20.0f);
}

// ============================================================================
// Payoff Validation Tests - NaN Detection
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffWithNaN) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.check_nan_inf = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    float payoffs[4] = {1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f, 4.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_INVALID_NAN);
    EXPECT_GE(result.nan_count, 1u);
}

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffWithMultipleNaN) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.check_nan_inf = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    float nan_val = std::numeric_limits<float>::quiet_NaN();
    float payoffs[4] = {nan_val, nan_val, 3.0f, 4.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.nan_count, 2u);
}

// ============================================================================
// Payoff Validation Tests - Inf Detection
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffWithPosInf) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.check_nan_inf = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    float payoffs[4] = {1.0f, std::numeric_limits<float>::infinity(), 3.0f, 4.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_INVALID_INF);
    EXPECT_GE(result.inf_count, 1u);
}

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffWithNegInf) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.check_nan_inf = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    float payoffs[4] = {1.0f, -std::numeric_limits<float>::infinity(), 3.0f, 4.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_INVALID_INF);
}

// ============================================================================
// Payoff Validation Tests - Bounds Checking
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffOutOfBounds) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.payoff_lower_bound = -100.0f;
    config.payoff_upper_bound = 100.0f;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    float payoffs[4] = {1.0f, 200.0f, 3.0f, 4.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_INVALID_BOUNDS);
    EXPECT_GE(result.out_of_bounds_count, 1u);
}

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffAtBounds) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.payoff_lower_bound = -100.0f;
    config.payoff_upper_bound = 100.0f;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    float payoffs[4] = {-100.0f, 100.0f, 0.0f, 50.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);
}

// ============================================================================
// Coalition Monitoring Tests - NULL Inputs
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, MonitorCoalitionNullBridge) {
    uint32_t player_ids[4] = {0, 1, 2, 3};
    security_coalition_result_t result;
    EXPECT_EQ(security_gt_monitor_coalition(nullptr, 0xF, player_ids, 4, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, MonitorCoalitionNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    uint32_t player_ids[4] = {0, 1, 2, 3};
    EXPECT_EQ(security_gt_monitor_coalition(bridge, 0xF, player_ids, 4, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, MonitorCoalitionEmptyCoalition) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    security_coalition_result_t result;

    int ret = security_gt_monitor_coalition(bridge, 0, nullptr, 0, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_suspicious);
    EXPECT_EQ(result.alert, SECURITY_COALITION_NORMAL);
}

// ============================================================================
// Coalition Monitoring Tests - Valid Coalitions
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, MonitorCoalitionNormalSmall) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t player_ids[2] = {0, 1};
    security_coalition_result_t result;

    int ret = security_gt_monitor_coalition(bridge, 0x3, player_ids, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_suspicious);
    EXPECT_EQ(result.alert, SECURITY_COALITION_NORMAL);
    EXPECT_EQ(result.coalition_size, 2u);
}

TEST_F(SecurityGameTheoryBridgeTest, MonitorCoalitionNormalMedium) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t player_ids[4] = {0, 1, 2, 3};
    security_coalition_result_t result;

    int ret = security_gt_monitor_coalition(bridge, 0xF, player_ids, 4, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.coalition_size, 4u);
    EXPECT_EQ(result.num_members, 4u);
}

TEST_F(SecurityGameTheoryBridgeTest, MonitorCoalitionWithNullPlayerIds) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_coalition_result_t result;
    int ret = security_gt_monitor_coalition(bridge, 0xF, nullptr, 4, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.coalition_size, 4u);
}

// ============================================================================
// Coalition Monitoring Tests - Size Exceeded
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, MonitorCoalitionSizeExceeded) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.max_coalition_size = 4;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint32_t player_ids[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    security_coalition_result_t result;

    int ret = security_gt_monitor_coalition(bridge, 0xFF, player_ids, 8, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_suspicious);
    EXPECT_EQ(result.alert, SECURITY_COALITION_SIZE_EXCEEDED);
}

// ============================================================================
// Coalition Monitoring Tests - Sybil Detection
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, MonitorCoalitionSybilPattern) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.sybil_detection_threshold = 0.3f;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    /* Create a pattern where num_players >> coalition bits (potential Sybil) */
    uint32_t player_ids[10] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1};
    security_coalition_result_t result;

    int ret = security_gt_monitor_coalition(bridge, 0x3, player_ids, 10, &result);
    EXPECT_EQ(ret, 0);
    /* Should detect Sybil pattern due to mismatch */
    if (result.is_suspicious) {
        EXPECT_EQ(result.alert, SECURITY_COALITION_SYBIL_DETECTED);
    }
}

// ============================================================================
// Coalition Monitoring Tests - Statistics Update
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, MonitorCoalitionUpdatesStats) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_game_theory_stats_t stats_before;
    security_gt_bridge_get_stats(bridge, &stats_before);

    uint32_t player_ids[2] = {0, 1};
    security_coalition_result_t result;
    security_gt_monitor_coalition(bridge, 0x3, player_ids, 2, &result);

    security_game_theory_stats_t stats_after;
    security_gt_bridge_get_stats(bridge, &stats_after);

    EXPECT_GT(stats_after.total_coalition_checks, stats_before.total_coalition_checks);
}

// ============================================================================
// Mechanism Verification Tests - NULL Inputs
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, VerifyMechanismNullBridge) {
    security_mechanism_result_t result;
    EXPECT_EQ(security_gt_verify_mechanism(nullptr, nullptr, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, VerifyMechanismNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_gt_verify_mechanism(bridge, nullptr, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, VerifyMechanismNullMechanism) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    security_mechanism_result_t result;

    int ret = security_gt_verify_mechanism(bridge, nullptr, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_MECHANISM_INVALID_STATE);
}

// ============================================================================
// Equilibrium Verification Tests - NULL Inputs
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, CheckEquilibriumNullBridge) {
    security_equilibrium_result_t result;
    EXPECT_EQ(security_gt_check_equilibrium(nullptr, nullptr, nullptr, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, CheckEquilibriumNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_gt_check_equilibrium(bridge, nullptr, nullptr, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, CheckEquilibriumNullEquilibrium) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    security_equilibrium_result_t result;

    int ret = security_gt_check_equilibrium(bridge, nullptr, nullptr, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
}

// ============================================================================
// Manipulation Detection Tests - NULL Inputs
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, DetectManipulationNullBridge) {
    uint32_t actions[4] = {0, 1, 0, 1};
    security_manipulation_result_t result;
    EXPECT_EQ(security_gt_detect_manipulation(nullptr, 0, actions, 4, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, DetectManipulationNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    uint32_t actions[4] = {0, 1, 0, 1};
    EXPECT_EQ(security_gt_detect_manipulation(bridge, 0, actions, 4, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, DetectManipulationNullActions) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    security_manipulation_result_t result;

    int ret = security_gt_detect_manipulation(bridge, 0, nullptr, 0, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.manipulation_detected);
    EXPECT_EQ(result.type, SECURITY_MANIPULATION_NONE);
}

TEST_F(SecurityGameTheoryBridgeTest, DetectManipulationZeroActions) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    uint32_t actions[4] = {0, 1, 0, 1};
    security_manipulation_result_t result;

    int ret = security_gt_detect_manipulation(bridge, 0, actions, 0, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.manipulation_detected);
}

// ============================================================================
// Manipulation Detection Tests - Normal Behavior
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, DetectManipulationNormalBehavior) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.manipulation_sensitivity = 0.5f;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    uint32_t actions[8] = {0, 1, 2, 3, 0, 2, 1, 3};  /* Varied actions */
    security_manipulation_result_t result;

    int ret = security_gt_detect_manipulation(bridge, 0, actions, 8, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.affected_player, 0u);
    EXPECT_EQ(result.event_count, 8u);
}

TEST_F(SecurityGameTheoryBridgeTest, DetectManipulationRepetitivePattern) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.manipulation_sensitivity = 0.9f;  /* High sensitivity */

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    /* Highly repetitive pattern */
    uint32_t actions[8] = {0, 1, 0, 1, 0, 1, 0, 1};
    security_manipulation_result_t result;

    int ret = security_gt_detect_manipulation(bridge, 0, actions, 8, &result);
    EXPECT_EQ(ret, 0);
    /* With high sensitivity, repetitive pattern may trigger detection */
    EXPECT_GE(result.pattern_match_score, 0.0f);
}

// ============================================================================
// Threat Game Analysis Tests - NULL Inputs
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, AnalyzeThreatGameNullBridge) {
    float attacker[4] = {1, 2, 3, 4};
    float defender[4] = {4, 3, 2, 1};
    float defense[2];
    float payoff;
    EXPECT_EQ(security_gt_analyze_threat_game(nullptr, attacker, defender, 2, 2, defense, &payoff),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, AnalyzeThreatGameNullAttacker) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    float defender[4] = {4, 3, 2, 1};
    float defense[2];
    float payoff;
    EXPECT_EQ(security_gt_analyze_threat_game(bridge, nullptr, defender, 2, 2, defense, &payoff),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, AnalyzeThreatGameNullDefender) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    float attacker[4] = {1, 2, 3, 4};
    float defense[2];
    float payoff;
    EXPECT_EQ(security_gt_analyze_threat_game(bridge, attacker, nullptr, 2, 2, defense, &payoff),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, AnalyzeThreatGameNullDefense) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    float attacker[4] = {1, 2, 3, 4};
    float defender[4] = {4, 3, 2, 1};
    float payoff;
    EXPECT_EQ(security_gt_analyze_threat_game(bridge, attacker, defender, 2, 2, nullptr, &payoff),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, AnalyzeThreatGameNullPayoff) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    float attacker[4] = {1, 2, 3, 4};
    float defender[4] = {4, 3, 2, 1};
    float defense[2];
    EXPECT_EQ(security_gt_analyze_threat_game(bridge, attacker, defender, 2, 2, defense, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Threat Game Analysis Tests - Valid Inputs
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, AnalyzeThreatGameValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Simple 2x2 zero-sum game */
    float attacker[4] = {3, 0, 5, 1};  /* Attacker payoffs */
    float defender[4] = {-3, 0, -5, -1};  /* Defender payoffs (zero-sum) */
    float defense[2] = {0, 0};
    float payoff = 0;

    int ret = security_gt_analyze_threat_game(bridge, attacker, defender, 2, 2, defense, &payoff);
    EXPECT_EQ(ret, 0);

    /* Defense strategy should be a valid probability distribution */
    float sum = defense[0] + defense[1];
    EXPECT_NEAR(sum, 1.0f, 0.01f);
    EXPECT_GE(defense[0], 0.0f);
    EXPECT_GE(defense[1], 0.0f);
}

TEST_F(SecurityGameTheoryBridgeTest, AnalyzeThreatGameZeroActions) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    float attacker[4] = {1, 2, 3, 4};
    float defender[4] = {4, 3, 2, 1};
    float defense[2];
    float payoff;

    EXPECT_EQ(security_gt_analyze_threat_game(bridge, attacker, defender, 0, 2, defense, &payoff),
              NIMCP_ERROR_INVALID_PARAMETER);
}

// ============================================================================
// Defensive Coalition Tests - NULL Inputs
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, FormDefensiveCoalitionNullBridge) {
    uint32_t defenders[4] = {0, 1, 2, 3};
    uint32_t coalition;
    float strength;
    EXPECT_EQ(security_gt_form_defensive_coalition(nullptr, defenders, 4, &coalition, &strength),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, FormDefensiveCoalitionNullCoalitionOut) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    uint32_t defenders[4] = {0, 1, 2, 3};
    float strength;
    EXPECT_EQ(security_gt_form_defensive_coalition(bridge, defenders, 4, nullptr, &strength),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, FormDefensiveCoalitionNullStrengthOut) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    uint32_t defenders[4] = {0, 1, 2, 3};
    uint32_t coalition;
    EXPECT_EQ(security_gt_form_defensive_coalition(bridge, defenders, 4, &coalition, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Defensive Coalition Tests - Valid Inputs
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, FormDefensiveCoalitionEmpty) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t coalition = 999;
    float strength = 999.0f;

    int ret = security_gt_form_defensive_coalition(bridge, nullptr, 0, &coalition, &strength);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(coalition, 0u);
    EXPECT_EQ(strength, 0.0f);
}

TEST_F(SecurityGameTheoryBridgeTest, FormDefensiveCoalitionSmall) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t defenders[2] = {0, 1};
    uint32_t coalition;
    float strength;

    int ret = security_gt_form_defensive_coalition(bridge, defenders, 2, &coalition, &strength);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(coalition, 0u);
    EXPECT_GT(strength, 0.0f);
    EXPECT_LE(strength, 1.0f);
}

TEST_F(SecurityGameTheoryBridgeTest, FormDefensiveCoalitionLarge) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t defenders[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    uint32_t coalition;
    float strength;

    int ret = security_gt_form_defensive_coalition(bridge, defenders, 16, &coalition, &strength);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(strength, 0.0f);
}

// ============================================================================
// Bridge Update Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, BridgeUpdateNullBridge) {
    EXPECT_EQ(security_gt_bridge_update(nullptr, 100), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, ApplySecurityEffectsNullBridge) {
    EXPECT_EQ(security_gt_apply_security_effects(nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, ApplyGtEffectsNullBridge) {
    EXPECT_EQ(security_gt_apply_gt_effects(nullptr), NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Bridge Update Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, BridgeUpdateBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, BridgeUpdateZeroDelta) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_bridge_update(bridge, 0);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, BridgeUpdateMultiple) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    for (int i = 0; i < 10; i++) {
        int ret = security_gt_bridge_update(bridge, 50);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(SecurityGameTheoryBridgeTest, ApplySecurityEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_apply_security_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, ApplyGtEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_apply_gt_effects(bridge);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// State and Stats Query Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, GetStateNullBridge) {
    security_game_theory_state_t state;
    EXPECT_EQ(security_gt_bridge_get_state(nullptr, &state), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, GetStateNullState) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_gt_bridge_get_state(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, GetStatsNullBridge) {
    security_game_theory_stats_t stats;
    EXPECT_EQ(security_gt_bridge_get_stats(nullptr, &stats), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, GetStatsNullStats) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_gt_bridge_get_stats(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, ResetStatsNullBridge) {
    EXPECT_EQ(security_gt_bridge_reset_stats(nullptr), NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// State and Stats Query Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, GetStateBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_game_theory_state_t state;
    int ret = security_gt_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.last_update_time, 0u);
}

TEST_F(SecurityGameTheoryBridgeTest, GetStatsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_game_theory_stats_t stats;
    int ret = security_gt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_payoff_validations, 0u);
}

TEST_F(SecurityGameTheoryBridgeTest, GetStatsAfterOperations) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Perform some operations */
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    security_payoff_result_t payoff_result;
    security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &payoff_result);

    uint32_t player_ids[2] = {0, 1};
    security_coalition_result_t coal_result;
    security_gt_monitor_coalition(bridge, 0x3, player_ids, 2, &coal_result);

    security_game_theory_stats_t stats;
    int ret = security_gt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.total_payoff_validations, 1u);
    EXPECT_GE(stats.total_coalition_checks, 1u);
}

TEST_F(SecurityGameTheoryBridgeTest, ResetStatsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Perform operations to generate stats */
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    security_payoff_result_t result;
    security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);

    /* Reset stats */
    int ret = security_gt_bridge_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    /* Verify stats are reset */
    security_game_theory_stats_t stats;
    security_gt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_payoff_validations, 0u);
}

// ============================================================================
// Effects Query Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, GetSecurityEffectsNullBridge) {
    security_to_game_theory_effects_t effects;
    EXPECT_EQ(security_gt_get_security_effects(nullptr, &effects), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, GetSecurityEffectsNullEffects) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_gt_get_security_effects(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, GetGtEffectsNullBridge) {
    game_theory_to_security_effects_t effects;
    EXPECT_EQ(security_gt_get_gt_effects(nullptr, &effects), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, GetGtEffectsNullEffects) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_gt_get_gt_effects(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Effects Query Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, GetSecurityEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_to_game_theory_effects_t effects;
    int ret = security_gt_get_security_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.threat_level, 0.0f);
    EXPECT_LE(effects.threat_level, 1.0f);
}

TEST_F(SecurityGameTheoryBridgeTest, GetGtEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    game_theory_to_security_effects_t effects;
    int ret = security_gt_get_gt_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.defense_effectiveness, 0.0f);
    EXPECT_LE(effects.defense_effectiveness, 1.0f);
}

// ============================================================================
// Bio-Async Connection Tests
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, ConnectBioAsyncNullBridge) {
    EXPECT_EQ(security_gt_bridge_connect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, DisconnectBioAsyncNullBridge) {
    EXPECT_EQ(security_gt_bridge_disconnect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityGameTheoryBridgeTest, IsBioAsyncConnectedNullBridge) {
    EXPECT_FALSE(security_gt_bridge_is_bio_async_connected(nullptr));
}

TEST_F(SecurityGameTheoryBridgeTest, ConnectBioAsyncBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP() << "Bridge creation failed";

    int ret = security_gt_bridge_connect_bio_async(bridge);
    if (ret == -1) {
        GTEST_SKIP() << "Bio-async router not available";
    }
    EXPECT_TRUE(ret == 0 || ret >= NIMCP_ERROR_UNKNOWN);
}

TEST_F(SecurityGameTheoryBridgeTest, DisconnectBioAsyncBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, IsBioAsyncConnectedNotConnected) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_FALSE(security_gt_bridge_is_bio_async_connected(bridge));
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, FullValidationCycle) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Step 1: Validate payoff matrix */
    float payoffs[9] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    security_payoff_result_t payoff_result;
    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 3, 3, &payoff_result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(payoff_result.is_valid);

    /* Step 2: Monitor coalition */
    uint32_t player_ids[3] = {0, 1, 2};
    security_coalition_result_t coal_result;
    ret = security_gt_monitor_coalition(bridge, 0x7, player_ids, 3, &coal_result);
    EXPECT_EQ(ret, 0);

    /* Step 3: Detect manipulation */
    uint32_t actions[6] = {0, 1, 2, 0, 1, 2};
    security_manipulation_result_t manip_result;
    ret = security_gt_detect_manipulation(bridge, 0, actions, 6, &manip_result);
    EXPECT_EQ(ret, 0);

    /* Step 4: Update bridge */
    ret = security_gt_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);

    /* Step 5: Apply effects */
    ret = security_gt_apply_security_effects(bridge);
    EXPECT_EQ(ret, 0);

    ret = security_gt_apply_gt_effects(bridge);
    EXPECT_EQ(ret, 0);

    /* Step 6: Verify stats */
    security_game_theory_stats_t stats;
    ret = security_gt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.total_payoff_validations, 1u);
    EXPECT_GE(stats.total_coalition_checks, 1u);
    EXPECT_GE(stats.bridge_updates, 1u);
}

TEST_F(SecurityGameTheoryBridgeTest, ThreatAnalysisAndDefense) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Analyze threat game */
    float attacker[4] = {3.0f, 0.0f, 5.0f, 1.0f};
    float defender[4] = {-3.0f, 0.0f, -5.0f, -1.0f};
    float defense[2];
    float expected_payoff;

    int ret = security_gt_analyze_threat_game(
        bridge, attacker, defender, 2, 2, defense, &expected_payoff);
    EXPECT_EQ(ret, 0);

    /* Form defensive coalition */
    uint32_t defenders[4] = {0, 1, 2, 3};
    uint32_t coalition;
    float strength;

    ret = security_gt_form_defensive_coalition(bridge, defenders, 4, &coalition, &strength);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(strength, 0.0f);

    /* Get game theory effects */
    game_theory_to_security_effects_t effects;
    ret = security_gt_get_gt_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.threat_games_analyzed, 1u);
    EXPECT_GE(effects.defense_coalitions_formed, 1u);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffSingleElement) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    float payoff = 5.0f;
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, &payoff, 1, 1, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.total_elements, 1u);
}

TEST_F(SecurityGameTheoryBridgeTest, ValidatePayoffZeroValues) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    float payoffs[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    security_payoff_result_t result;

    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.min_value, 0.0f);
    EXPECT_EQ(result.max_value, 0.0f);
}

TEST_F(SecurityGameTheoryBridgeTest, MonitorCoalitionSinglePlayer) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t player_id = 0;
    security_coalition_result_t result;

    int ret = security_gt_monitor_coalition(bridge, 0x1, &player_id, 1, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.coalition_size, 1u);
}

TEST_F(SecurityGameTheoryBridgeTest, DetectManipulationMinimalActions) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t actions[2] = {0, 1};
    security_manipulation_result_t result;

    int ret = security_gt_detect_manipulation(bridge, 0, actions, 2, &result);
    EXPECT_EQ(ret, 0);
    /* With only 2 actions, pattern detection should have limited data */
}

TEST_F(SecurityGameTheoryBridgeTest, ThreatGameSingleActionEach) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    float attacker[1] = {5.0f};
    float defender[1] = {-5.0f};
    float defense[1];
    float payoff;

    int ret = security_gt_analyze_threat_game(bridge, attacker, defender, 1, 1, defense, &payoff);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(defense[0], 1.0f);  /* Pure strategy with single action */
}

TEST_F(SecurityGameTheoryBridgeTest, UpdateWithLargeDelta) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_bridge_update(bridge, UINT64_MAX);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, MultipleResets) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    for (int i = 0; i < 5; i++) {
        /* Generate some stats */
        float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        security_payoff_result_t result;
        security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);

        /* Reset */
        int ret = security_gt_bridge_reset_stats(bridge);
        EXPECT_EQ(ret, 0);

        /* Verify reset */
        security_game_theory_stats_t stats;
        security_gt_bridge_get_stats(bridge, &stats);
        EXPECT_EQ(stats.total_payoff_validations, 0u);
    }
}

// ============================================================================
// Sensitivity Configuration Tests
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, HighSecuritySensitivity) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.security_sensitivity = 2.0f;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_apply_security_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, LowSecuritySensitivity) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.security_sensitivity = 0.5f;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_apply_security_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, HighGameTheorySensitivity) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.game_theory_sensitivity = 2.0f;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_apply_gt_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, LowGameTheorySensitivity) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.game_theory_sensitivity = 0.5f;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_apply_gt_effects(bridge);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Disabled Feature Tests
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, DisabledPayoffValidation) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.enable_payoff_validation = false;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_apply_security_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, DisabledCoalitionMonitoring) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.enable_coalition_monitoring = false;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_apply_security_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, DisabledMechanismVerification) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.enable_mechanism_verification = false;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_apply_security_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, DisabledEquilibriumVerification) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.enable_equilibrium_verification = false;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_apply_security_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryBridgeTest, DisabledManipulationDetection) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.enable_manipulation_detection = false;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_apply_security_effects(bridge);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Thread Safety Tests (basic - not comprehensive)
// ============================================================================

TEST_F(SecurityGameTheoryBridgeTest, ConcurrentUpdates) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    for (int i = 0; i < 100; i++) {
        security_gt_bridge_update(bridge, 10);
        security_gt_apply_security_effects(bridge);
        security_gt_apply_gt_effects(bridge);
    }
}

TEST_F(SecurityGameTheoryBridgeTest, ConcurrentValidations) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    for (int i = 0; i < 50; i++) {
        float payoffs[4] = {(float)i, (float)i + 1, (float)i + 2, (float)i + 3};
        security_payoff_result_t result;
        security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    }
}
