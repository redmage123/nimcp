/**
 * @file test_snn_bridge_exception.cpp
 * @brief Unit tests for SNN bridge exception handling with NIMCP_THROW_TO_IMMUNE
 *
 * WHAT: Test NIMCP_THROW_TO_IMMUNE exception handling for SNN bridge bio_async functions
 * WHY:  Ensure proper error reporting to immune system for NULL pointer conditions
 * HOW:  Test disconnect_bio_async and is_bio_async_connected functions for each bridge
 *
 * BRIDGES TESTED:
 * - nimcp_snn_curiosity_bridge.h
 * - nimcp_snn_empathetic_bridge.h
 * - nimcp_snn_free_energy_bridge.h
 * - nimcp_snn_grief_bridge.h
 * - nimcp_snn_joy_bridge.h
 * - nimcp_snn_love_bridge.h
 * - nimcp_snn_medulla_bridge.h
 * - nimcp_snn_reasoning_bridge.h
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

class SnnBridgeExceptionTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Curiosity Bridge Exception Tests
//=============================================================================

TEST_F(SnnBridgeExceptionTest, CuriosityBridge_DisconnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_curiosity_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, CuriosityBridge_IsBioAsyncConnected_NullBridge_ReturnsFalse) {
    bool result = snn_curiosity_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SnnBridgeExceptionTest, CuriosityBridge_ConnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_curiosity_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, CuriosityBridge_Update_NullBridge_ReturnsError) {
    int result = snn_curiosity_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Empathetic Bridge Exception Tests
//=============================================================================

TEST_F(SnnBridgeExceptionTest, EmpatheticBridge_DisconnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_empathetic_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, EmpatheticBridge_IsBioAsyncConnected_NullBridge_ReturnsFalse) {
    bool result = snn_empathetic_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SnnBridgeExceptionTest, EmpatheticBridge_ConnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_empathetic_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, EmpatheticBridge_Update_NullBridge_ReturnsError) {
    int result = snn_empathetic_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Free Energy Bridge Exception Tests
//=============================================================================

TEST_F(SnnBridgeExceptionTest, FreeEnergyBridge_DisconnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_free_energy_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, FreeEnergyBridge_IsBioAsyncConnected_NullBridge_ReturnsFalse) {
    bool result = snn_free_energy_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SnnBridgeExceptionTest, FreeEnergyBridge_ConnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_free_energy_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, FreeEnergyBridge_Update_NullBridge_ReturnsError) {
    int result = snn_free_energy_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Grief Bridge Exception Tests
//=============================================================================

TEST_F(SnnBridgeExceptionTest, GriefBridge_DisconnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_grief_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, GriefBridge_IsBioAsyncConnected_NullBridge_ReturnsFalse) {
    bool result = snn_grief_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SnnBridgeExceptionTest, GriefBridge_ConnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_grief_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, GriefBridge_Update_NullBridge_ReturnsError) {
    int result = snn_grief_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Joy Bridge Exception Tests
//=============================================================================

TEST_F(SnnBridgeExceptionTest, JoyBridge_DisconnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_joy_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, JoyBridge_IsBioAsyncConnected_NullBridge_ReturnsFalse) {
    bool result = snn_joy_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SnnBridgeExceptionTest, JoyBridge_ConnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_joy_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, JoyBridge_Update_NullBridge_ReturnsError) {
    int result = snn_joy_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Love Bridge Exception Tests
//=============================================================================

TEST_F(SnnBridgeExceptionTest, LoveBridge_DisconnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_love_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, LoveBridge_IsBioAsyncConnected_NullBridge_ReturnsFalse) {
    bool result = snn_love_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SnnBridgeExceptionTest, LoveBridge_ConnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_love_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, LoveBridge_Update_NullBridge_ReturnsError) {
    int result = snn_love_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Medulla Bridge Exception Tests
//=============================================================================

TEST_F(SnnBridgeExceptionTest, MedullaBridge_DisconnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_medulla_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, MedullaBridge_IsBioAsyncConnected_NullBridge_ReturnsFalse) {
    bool result = snn_medulla_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SnnBridgeExceptionTest, MedullaBridge_ConnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_medulla_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, MedullaBridge_Update_NullBridge_ReturnsError) {
    int result = snn_medulla_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Reasoning Bridge Exception Tests
//=============================================================================

TEST_F(SnnBridgeExceptionTest, ReasoningBridge_DisconnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_reasoning_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, ReasoningBridge_IsBioAsyncConnected_NullBridge_ReturnsFalse) {
    bool result = snn_reasoning_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SnnBridgeExceptionTest, ReasoningBridge_ConnectBioAsync_NullBridge_ReturnsError) {
    int result = snn_reasoning_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SnnBridgeExceptionTest, ReasoningBridge_Update_NullBridge_ReturnsError) {
    int result = snn_reasoning_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Destroy NULL Safety Tests
//=============================================================================

TEST_F(SnnBridgeExceptionTest, CuriosityBridge_Destroy_NullSafe) {
    // Should not crash
    snn_curiosity_bridge_destroy(nullptr);
}

TEST_F(SnnBridgeExceptionTest, EmpatheticBridge_Destroy_NullSafe) {
    snn_empathetic_bridge_destroy(nullptr);
}

TEST_F(SnnBridgeExceptionTest, FreeEnergyBridge_Destroy_NullSafe) {
    snn_free_energy_bridge_destroy(nullptr);
}

TEST_F(SnnBridgeExceptionTest, GriefBridge_Destroy_NullSafe) {
    snn_grief_bridge_destroy(nullptr);
}

TEST_F(SnnBridgeExceptionTest, JoyBridge_Destroy_NullSafe) {
    snn_joy_bridge_destroy(nullptr);
}

TEST_F(SnnBridgeExceptionTest, LoveBridge_Destroy_NullSafe) {
    snn_love_bridge_destroy(nullptr);
}

TEST_F(SnnBridgeExceptionTest, MedullaBridge_Destroy_NullSafe) {
    snn_medulla_bridge_destroy(nullptr);
}

TEST_F(SnnBridgeExceptionTest, ReasoningBridge_Destroy_NullSafe) {
    snn_reasoning_bridge_destroy(nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
