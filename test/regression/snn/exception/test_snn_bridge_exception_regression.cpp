/**
 * @file test_snn_bridge_exception_regression.cpp
 * @brief Regression tests for SNN bridge exception handling API contracts
 *
 * WHAT: Test API contract stability for SNN bridge exception handling
 * WHY:  Ensure exception behavior remains consistent across versions
 * HOW:  Test error codes, return values, and message formats remain stable
 *
 * BRIDGES TESTED:
 * - nimcp_snn_curiosity_bridge
 * - nimcp_snn_empathetic_bridge
 * - nimcp_snn_free_energy_bridge
 * - nimcp_snn_grief_bridge
 * - nimcp_snn_joy_bridge
 * - nimcp_snn_love_bridge
 * - nimcp_snn_medulla_bridge
 * - nimcp_snn_reasoning_bridge
 *
 * @author NIMCP Development Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <cstring>

#include "utils/nimcp_test_base.h"

extern "C" {
#include "snn/bridges/nimcp_snn_curiosity_bridge.h"
#include "snn/bridges/nimcp_snn_empathetic_bridge.h"
#include "snn/bridges/nimcp_snn_free_energy_bridge.h"
#include "snn/bridges/nimcp_snn_grief_bridge.h"
#include "snn/bridges/nimcp_snn_joy_bridge.h"
#include "snn/bridges/nimcp_snn_love_bridge.h"
#include "snn/bridges/nimcp_snn_medulla_bridge.h"
#include "snn/bridges/nimcp_snn_reasoning_bridge.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class SnnBridgeExceptionRegressionTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// API Contract: Return Value Stability Tests
//=============================================================================

TEST_F(SnnBridgeExceptionRegressionTest, DisconnectBioAsync_NullReturnsNonZero) {
    // Contract: disconnect_bio_async(NULL) must return non-zero error
    EXPECT_NE(snn_curiosity_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_empathetic_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_free_energy_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_grief_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_joy_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_love_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_medulla_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_reasoning_bridge_disconnect_bio_async(nullptr), 0);
}

TEST_F(SnnBridgeExceptionRegressionTest, IsBioAsyncConnected_NullReturnsFalse) {
    // Contract: is_bio_async_connected(NULL) must return false
    EXPECT_FALSE(snn_curiosity_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_empathetic_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_free_energy_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_grief_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_joy_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_love_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_medulla_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_reasoning_bridge_is_bio_async_connected(nullptr));
}

TEST_F(SnnBridgeExceptionRegressionTest, ConnectBioAsync_NullReturnsNonZero) {
    // Contract: connect_bio_async(NULL) must return non-zero error
    EXPECT_NE(snn_curiosity_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_empathetic_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_free_energy_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_grief_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_joy_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_love_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_medulla_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_reasoning_bridge_connect_bio_async(nullptr), 0);
}

TEST_F(SnnBridgeExceptionRegressionTest, Update_NullReturnsNonZero) {
    // Contract: update(NULL) must return non-zero error
    EXPECT_NE(snn_curiosity_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_empathetic_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_free_energy_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_grief_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_joy_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_love_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_medulla_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_reasoning_bridge_update(nullptr, 0.016f), 0);
}

//=============================================================================
// API Contract: Destroy Null Safety Tests
//=============================================================================

TEST_F(SnnBridgeExceptionRegressionTest, Destroy_NullIsSafe) {
    // Contract: destroy(NULL) must be safe (no crash, no exception)
    snn_curiosity_bridge_destroy(nullptr);
    snn_empathetic_bridge_destroy(nullptr);
    snn_free_energy_bridge_destroy(nullptr);
    snn_grief_bridge_destroy(nullptr);
    snn_joy_bridge_destroy(nullptr);
    snn_love_bridge_destroy(nullptr);
    snn_medulla_bridge_destroy(nullptr);
    snn_reasoning_bridge_destroy(nullptr);
    // Test passes if no crash
}

//=============================================================================
// API Contract: Config Default Null Safety Tests
//=============================================================================

TEST_F(SnnBridgeExceptionRegressionTest, ConfigDefault_NullIsSafe) {
    // Contract: config_default(NULL) must be safe
    snn_curiosity_config_default(nullptr);
    snn_empathetic_config_default(nullptr);
    snn_free_energy_config_default(nullptr);
    snn_grief_config_default(nullptr);
    snn_joy_config_default(nullptr);
    snn_love_config_default(nullptr);
    snn_medulla_config_default(nullptr);
    snn_reasoning_config_default(nullptr);
    // Test passes if no crash
}

//=============================================================================
// API Contract: Consistency Across Bridges
//=============================================================================

TEST_F(SnnBridgeExceptionRegressionTest, AllBridges_SameReturnValuePattern) {
    // Contract: All bridges should return consistent values for same error condition
    int results[8];

    results[0] = snn_curiosity_bridge_disconnect_bio_async(nullptr);
    results[1] = snn_empathetic_bridge_disconnect_bio_async(nullptr);
    results[2] = snn_free_energy_bridge_disconnect_bio_async(nullptr);
    results[3] = snn_grief_bridge_disconnect_bio_async(nullptr);
    results[4] = snn_joy_bridge_disconnect_bio_async(nullptr);
    results[5] = snn_love_bridge_disconnect_bio_async(nullptr);
    results[6] = snn_medulla_bridge_disconnect_bio_async(nullptr);
    results[7] = snn_reasoning_bridge_disconnect_bio_async(nullptr);

    // All should return the same error value (typically SNN_ERROR_NULL_POINTER)
    for (int i = 1; i < 8; i++) {
        EXPECT_EQ(results[0], results[i])
            << "Bridge " << i << " returned different value than bridge 0";
    }
}

TEST_F(SnnBridgeExceptionRegressionTest, AllBridges_UpdateReturnsConsistentError) {
    int results[8];

    results[0] = snn_curiosity_bridge_update(nullptr, 0.016f);
    results[1] = snn_empathetic_bridge_update(nullptr, 0.016f);
    results[2] = snn_free_energy_bridge_update(nullptr, 0.016f);
    results[3] = snn_grief_bridge_update(nullptr, 0.016f);
    results[4] = snn_joy_bridge_update(nullptr, 0.016f);
    results[5] = snn_love_bridge_update(nullptr, 0.016f);
    results[6] = snn_medulla_bridge_update(nullptr, 0.016f);
    results[7] = snn_reasoning_bridge_update(nullptr, 0.016f);

    // All should return the same error value
    for (int i = 1; i < 8; i++) {
        EXPECT_EQ(results[0], results[i])
            << "Bridge " << i << " returned different value than bridge 0 for update";
    }
}

TEST_F(SnnBridgeExceptionRegressionTest, AllBridges_ConnectReturnsConsistentError) {
    int results[8];

    results[0] = snn_curiosity_bridge_connect_bio_async(nullptr);
    results[1] = snn_empathetic_bridge_connect_bio_async(nullptr);
    results[2] = snn_free_energy_bridge_connect_bio_async(nullptr);
    results[3] = snn_grief_bridge_connect_bio_async(nullptr);
    results[4] = snn_joy_bridge_connect_bio_async(nullptr);
    results[5] = snn_love_bridge_connect_bio_async(nullptr);
    results[6] = snn_medulla_bridge_connect_bio_async(nullptr);
    results[7] = snn_reasoning_bridge_connect_bio_async(nullptr);

    // All should return the same error value
    for (int i = 1; i < 8; i++) {
        EXPECT_EQ(results[0], results[i])
            << "Bridge " << i << " returned different value than bridge 0 for connect";
    }
}

//=============================================================================
// Repeated Operations Tests
//=============================================================================

TEST_F(SnnBridgeExceptionRegressionTest, RepeatedCalls_SameResult) {
    // Contract: Repeated calls should return consistent results
    int first_result = snn_curiosity_bridge_disconnect_bio_async(nullptr);

    for (int i = 0; i < 10; i++) {
        int result = snn_curiosity_bridge_disconnect_bio_async(nullptr);
        EXPECT_EQ(first_result, result) << "Result changed on iteration " << i;
    }
}

TEST_F(SnnBridgeExceptionRegressionTest, IsBioAsyncConnected_RepeatedCalls_SameResult) {
    // Contract: Repeated calls should return consistent results
    bool first_result = snn_curiosity_bridge_is_bio_async_connected(nullptr);

    for (int i = 0; i < 10; i++) {
        bool result = snn_curiosity_bridge_is_bio_async_connected(nullptr);
        EXPECT_EQ(first_result, result) << "Result changed on iteration " << i;
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
