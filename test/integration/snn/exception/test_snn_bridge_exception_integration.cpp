/**
 * @file test_snn_bridge_exception_integration.cpp
 * @brief Integration tests for SNN bridge exception handling across multiple bridges
 *
 * WHAT: Test cross-bridge exception propagation and recovery
 * WHY:  Verify exception handling works correctly when bridges interact
 * HOW:  Test sequences of operations across multiple bridges with exception conditions
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

class SnnBridgeExceptionIntegrationTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Cross-Bridge Exception Propagation Tests
//=============================================================================

TEST_F(SnnBridgeExceptionIntegrationTest, MultipleBridges_SequentialNullDisconnects_AllReturnError) {
    // All bridges should return error for NULL disconnect
    EXPECT_NE(snn_curiosity_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_empathetic_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_free_energy_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_grief_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_joy_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_love_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_medulla_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_reasoning_bridge_disconnect_bio_async(nullptr), 0);
}

TEST_F(SnnBridgeExceptionIntegrationTest, MultipleBridges_IsBioAsyncConnected_AllReturnFalse) {
    // All bridges should return false for NULL connection check
    EXPECT_FALSE(snn_curiosity_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_empathetic_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_free_energy_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_grief_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_joy_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_love_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_medulla_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_reasoning_bridge_is_bio_async_connected(nullptr));
}

TEST_F(SnnBridgeExceptionIntegrationTest, MixedOperations_SequentialCalls_AllHandleGracefully) {
    // Mix of different operations on different bridges
    EXPECT_NE(snn_curiosity_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_curiosity_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_FALSE(snn_curiosity_bridge_is_bio_async_connected(nullptr));

    EXPECT_NE(snn_empathetic_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_empathetic_bridge_update(nullptr, 0.016f), 0);

    EXPECT_NE(snn_free_energy_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_grief_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_joy_bridge_connect_bio_async(nullptr), 0);
}

//=============================================================================
// Recovery After Exception Tests
//=============================================================================

TEST_F(SnnBridgeExceptionIntegrationTest, MultipleExceptions_SystemRemainsStable) {
    // Throw multiple exceptions - system should remain stable
    for (int i = 0; i < 10; i++) {
        snn_curiosity_bridge_disconnect_bio_async(nullptr);
        snn_empathetic_bridge_disconnect_bio_async(nullptr);
    }
    // If we get here without crashing, test passes
}

TEST_F(SnnBridgeExceptionIntegrationTest, RapidExceptions_NoSystemCrash) {
    // Rapidly throw exceptions
    for (int i = 0; i < 100; i++) {
        snn_joy_bridge_is_bio_async_connected(nullptr);
        snn_love_bridge_is_bio_async_connected(nullptr);
    }
    // If we get here without crashing, test passes
}

//=============================================================================
// Destroy Safety Tests
//=============================================================================

TEST_F(SnnBridgeExceptionIntegrationTest, AllBridges_NullDestroy_Safe) {
    // Destroy with NULL should be safe (no crash)
    snn_curiosity_bridge_destroy(nullptr);
    snn_empathetic_bridge_destroy(nullptr);
    snn_free_energy_bridge_destroy(nullptr);
    snn_grief_bridge_destroy(nullptr);
    snn_joy_bridge_destroy(nullptr);
    snn_love_bridge_destroy(nullptr);
    snn_medulla_bridge_destroy(nullptr);
    snn_reasoning_bridge_destroy(nullptr);
}

//=============================================================================
// Update Operation Tests
//=============================================================================

TEST_F(SnnBridgeExceptionIntegrationTest, AllBridges_UpdateNull_ReturnsError) {
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
// Connect Operation Tests
//=============================================================================

TEST_F(SnnBridgeExceptionIntegrationTest, AllBridges_ConnectNull_ReturnsError) {
    EXPECT_NE(snn_curiosity_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_empathetic_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_free_energy_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_grief_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_joy_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_love_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_medulla_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_reasoning_bridge_connect_bio_async(nullptr), 0);
}

//=============================================================================
// Emotion Bridge Group Tests (Grief, Joy, Love, Empathetic)
//=============================================================================

TEST_F(SnnBridgeExceptionIntegrationTest, EmotionBridges_AllNullOperations_ConsistentBehavior) {
    // All emotion bridges should behave consistently
    int results[4];
    results[0] = snn_grief_bridge_disconnect_bio_async(nullptr);
    results[1] = snn_joy_bridge_disconnect_bio_async(nullptr);
    results[2] = snn_love_bridge_disconnect_bio_async(nullptr);
    results[3] = snn_empathetic_bridge_disconnect_bio_async(nullptr);

    // All should return error
    for (int i = 0; i < 4; i++) {
        EXPECT_NE(results[i], 0) << "Emotion bridge " << i << " returned success for NULL";
    }
}

//=============================================================================
// Cognitive Bridge Group Tests (Curiosity, Reasoning, Free Energy)
//=============================================================================

TEST_F(SnnBridgeExceptionIntegrationTest, CognitiveBridges_AllNullOperations_ConsistentBehavior) {
    int results[3];
    results[0] = snn_curiosity_bridge_disconnect_bio_async(nullptr);
    results[1] = snn_reasoning_bridge_disconnect_bio_async(nullptr);
    results[2] = snn_free_energy_bridge_disconnect_bio_async(nullptr);

    // All should return error
    for (int i = 0; i < 3; i++) {
        EXPECT_NE(results[i], 0) << "Cognitive bridge " << i << " returned success for NULL";
    }
}

//=============================================================================
// Consistency Across Bridges
//=============================================================================

TEST_F(SnnBridgeExceptionIntegrationTest, AllBridges_SameReturnPattern) {
    // All bridges should return consistent non-zero values for NULL
    int results[8];
    results[0] = snn_curiosity_bridge_disconnect_bio_async(nullptr);
    results[1] = snn_empathetic_bridge_disconnect_bio_async(nullptr);
    results[2] = snn_free_energy_bridge_disconnect_bio_async(nullptr);
    results[3] = snn_grief_bridge_disconnect_bio_async(nullptr);
    results[4] = snn_joy_bridge_disconnect_bio_async(nullptr);
    results[5] = snn_love_bridge_disconnect_bio_async(nullptr);
    results[6] = snn_medulla_bridge_disconnect_bio_async(nullptr);
    results[7] = snn_reasoning_bridge_disconnect_bio_async(nullptr);

    // All should return the same error value
    for (int i = 1; i < 8; i++) {
        EXPECT_EQ(results[0], results[i])
            << "Bridge " << i << " returned different value than bridge 0";
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
