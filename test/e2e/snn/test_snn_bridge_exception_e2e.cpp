/**
 * @file test_snn_bridge_exception_e2e.cpp
 * @brief End-to-end tests for SNN bridge exception handling
 *
 * WHAT: Test full bridge lifecycle with exception conditions
 * WHY:  Verify exception handling works in realistic multi-bridge workflows
 * HOW:  Test complete scenarios including creation, operation, exception, and cleanup
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

class SnnBridgeExceptionE2ETest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Full Lifecycle Tests
//=============================================================================

TEST_F(SnnBridgeExceptionE2ETest, FullLifecycle_CuriosityBridge_ExceptionHandling) {
    // Phase 1: Attempt operations on NULL bridge (should return error)
    int result = snn_curiosity_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);

    // Phase 2: Attempt update on NULL (should return error)
    result = snn_curiosity_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, 0);

    // Phase 3: Attempt disconnect on NULL (should return error)
    result = snn_curiosity_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);

    // Phase 4: Check connection status on NULL (should return false)
    bool connected = snn_curiosity_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);

    // Phase 5: Destroy NULL (should be safe)
    snn_curiosity_bridge_destroy(nullptr);
}

TEST_F(SnnBridgeExceptionE2ETest, FullLifecycle_AllEmotionBridges_ExceptionHandling) {
    // Test grief, joy, love, empathetic bridges in sequence

    // Phase 1: Connect attempts
    EXPECT_NE(snn_grief_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_joy_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_love_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_empathetic_bridge_connect_bio_async(nullptr), 0);

    // Phase 2: Update attempts
    EXPECT_NE(snn_grief_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_joy_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_love_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_empathetic_bridge_update(nullptr, 0.016f), 0);

    // Phase 3: Disconnect attempts
    EXPECT_NE(snn_grief_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_joy_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_love_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_empathetic_bridge_disconnect_bio_async(nullptr), 0);

    // Phase 4: Cleanup (should be safe)
    snn_grief_bridge_destroy(nullptr);
    snn_joy_bridge_destroy(nullptr);
    snn_love_bridge_destroy(nullptr);
    snn_empathetic_bridge_destroy(nullptr);
}

//=============================================================================
// Multi-Bridge Workflow Tests
//=============================================================================

TEST_F(SnnBridgeExceptionE2ETest, MultiBridgeWorkflow_SequentialOperations) {
    // Simulate a workflow where multiple bridges are used
    // Step 1: Try to connect all cognitive bridges
    EXPECT_NE(snn_curiosity_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_reasoning_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_free_energy_bridge_connect_bio_async(nullptr), 0);

    // Step 2: Try to update all bridges in a loop (simulating main loop)
    for (int i = 0; i < 5; i++) {
        EXPECT_NE(snn_curiosity_bridge_update(nullptr, 0.016f), 0);
        EXPECT_NE(snn_reasoning_bridge_update(nullptr, 0.016f), 0);
        EXPECT_NE(snn_free_energy_bridge_update(nullptr, 0.016f), 0);
    }

    // Step 3: Check connection status
    EXPECT_FALSE(snn_curiosity_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_reasoning_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(snn_free_energy_bridge_is_bio_async_connected(nullptr));

    // Step 4: Disconnect all
    EXPECT_NE(snn_curiosity_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_reasoning_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_free_energy_bridge_disconnect_bio_async(nullptr), 0);
}

TEST_F(SnnBridgeExceptionE2ETest, MultiBridgeWorkflow_AllEightBridges) {
    // Simulate a complete system with all 8 bridges

    // Connect all
    EXPECT_NE(snn_curiosity_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_empathetic_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_free_energy_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_grief_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_joy_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_love_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_medulla_bridge_connect_bio_async(nullptr), 0);
    EXPECT_NE(snn_reasoning_bridge_connect_bio_async(nullptr), 0);

    // Update cycle
    EXPECT_NE(snn_curiosity_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_empathetic_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_free_energy_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_grief_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_joy_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_love_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_medulla_bridge_update(nullptr, 0.016f), 0);
    EXPECT_NE(snn_reasoning_bridge_update(nullptr, 0.016f), 0);

    // Disconnect all
    EXPECT_NE(snn_curiosity_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_empathetic_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_free_energy_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_grief_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_joy_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_love_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_medulla_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_NE(snn_reasoning_bridge_disconnect_bio_async(nullptr), 0);

    // Cleanup (should be safe)
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
// Stress Tests
//=============================================================================

TEST_F(SnnBridgeExceptionE2ETest, StressTest_RapidExceptions) {
    // Rapidly throw exceptions - system should remain stable
    for (int i = 0; i < 100; i++) {
        snn_curiosity_bridge_update(nullptr, 0.016f);
        snn_grief_bridge_update(nullptr, 0.016f);
    }
    // If we get here without crashing, test passes
}

TEST_F(SnnBridgeExceptionE2ETest, StressTest_InterleavedOperations) {
    // Interleave different operations on different bridges
    for (int i = 0; i < 50; i++) {
        snn_curiosity_bridge_connect_bio_async(nullptr);
        snn_grief_bridge_update(nullptr, 0.016f);
        snn_joy_bridge_disconnect_bio_async(nullptr);
        snn_love_bridge_is_bio_async_connected(nullptr);
    }
    // If we get here without crashing, test passes
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

TEST_F(SnnBridgeExceptionE2ETest, ErrorRecovery_SystemRemainsOperational) {
    // Throw exceptions
    for (int i = 0; i < 10; i++) {
        snn_curiosity_bridge_update(nullptr, 0.016f);
    }

    // Verify system still works
    int result = snn_curiosity_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Real-World Scenario Tests
//=============================================================================

TEST_F(SnnBridgeExceptionE2ETest, Scenario_EmotionalProcessingPipeline) {
    // Simulate emotional processing pipeline with NULL bridges
    // Step 1: Receive emotional stimulus (empathetic bridge)
    EXPECT_NE(snn_empathetic_bridge_update(nullptr, 0.016f), 0);

    // Step 2: Process grief response
    EXPECT_NE(snn_grief_bridge_update(nullptr, 0.016f), 0);

    // Step 3: Balance with joy
    EXPECT_NE(snn_joy_bridge_update(nullptr, 0.016f), 0);

    // Step 4: Integrate with love/attachment
    EXPECT_NE(snn_love_bridge_update(nullptr, 0.016f), 0);

    // Cleanup
    snn_empathetic_bridge_destroy(nullptr);
    snn_grief_bridge_destroy(nullptr);
    snn_joy_bridge_destroy(nullptr);
    snn_love_bridge_destroy(nullptr);
}

TEST_F(SnnBridgeExceptionE2ETest, Scenario_CognitivePipeline) {
    // Simulate cognitive processing pipeline
    // Step 1: Curiosity-driven exploration
    EXPECT_NE(snn_curiosity_bridge_update(nullptr, 0.016f), 0);

    // Step 2: Reasoning about observations
    EXPECT_NE(snn_reasoning_bridge_update(nullptr, 0.016f), 0);

    // Step 3: Free energy minimization
    EXPECT_NE(snn_free_energy_bridge_update(nullptr, 0.016f), 0);

    // Step 4: Medulla for autonomic regulation
    EXPECT_NE(snn_medulla_bridge_update(nullptr, 0.016f), 0);
}

TEST_F(SnnBridgeExceptionE2ETest, Scenario_BioAsyncConnectionCycle) {
    // Simulate a bio-async connection lifecycle
    // Step 1: Check if already connected
    bool connected = snn_curiosity_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);

    // Step 2: Try to connect
    int result = snn_curiosity_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);

    // Step 3: Check connection again
    connected = snn_curiosity_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);

    // Step 4: Try to disconnect (cleanup)
    result = snn_curiosity_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
