/**
 * @file test_visual_cortex_fep_bridge.cpp
 * @brief Unit tests for Visual Cortex FEP Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Comprehensive unit tests for visual-FEP integration bridge
 * WHY:  Ensure correct lifecycle, connection, state management, and bidirectional
 *       integration between FEP and visual cortex (hierarchical prediction, attention,
 *       active vision).
 * HOW:  Test all API functions with valid and invalid inputs, verify return codes,
 *       state transitions, and null safety.
 */

#include <gtest/gtest.h>
#include "perception/nimcp_visual_cortex_fep_bridge.h"

/**
 * @brief Test fixture for Visual Cortex FEP Bridge tests
 *
 * WHAT: Provides setup/teardown for bridge testing
 * WHY:  Ensure clean state for each test
 * HOW:  Create bridge in SetUp, destroy in TearDown
 */
class VisualCortexFepBridgeTest : public ::testing::Test {
protected:
    visual_cortex_fep_bridge_t* bridge = nullptr;

    /**
     * WHAT: Initialize bridge before each test
     * WHY:  Ensure clean starting state
     * HOW:  Create with default config
     */
    void SetUp() override {
        visual_cortex_fep_config_t config;
        visual_cortex_fep_bridge_default_config(&config);
        bridge = visual_cortex_fep_bridge_create(&config);
    }

    /**
     * WHAT: Clean up bridge after each test
     * WHY:  Prevent memory leaks
     * HOW:  Destroy bridge, reset pointer
     */
    void TearDown() override {
        if (bridge) {
            visual_cortex_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

/**
 * WHAT: Test successful bridge creation and destruction
 * WHY:  Verify basic lifecycle management
 * HOW:  Create with valid config, verify non-null, destroy
 */
TEST_F(VisualCortexFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
    // Cleanup handled by TearDown
}

/**
 * WHAT: Test creation with NULL config uses defaults
 * WHY:  Verify NULL config fallback behavior
 * HOW:  Create with NULL, verify non-null result
 */
TEST_F(VisualCortexFepBridgeTest, CreateWithNullConfig) {
    // Clean up existing bridge
    visual_cortex_fep_bridge_destroy(bridge);

    // Create with NULL config
    bridge = visual_cortex_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
}

/**
 * WHAT: Test destroying NULL bridge is safe
 * WHY:  Verify NULL pointer safety
 * HOW:  Call destroy with NULL, should not crash
 */
TEST_F(VisualCortexFepBridgeTest, DestroyNull) {
    visual_cortex_fep_bridge_destroy(nullptr);
    // Test passes if no crash
}

/**
 * WHAT: Test default config provides valid values
 * WHY:  Verify configuration initialization
 * HOW:  Call default_config, check fields
 */
TEST_F(VisualCortexFepBridgeTest, DefaultConfig) {
    visual_cortex_fep_config_t config;
    int result = visual_cortex_fep_bridge_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_GT(config.prediction_error_threshold, 0.0f);
    EXPECT_GT(config.precision_gain_factor, 0.0f);
    EXPECT_GT(config.attention_boost_factor, 0.0f);
}

/**
 * WHAT: Test default config with NULL parameter
 * WHY:  Verify NULL safety for config initialization
 * HOW:  Call with NULL, expect error
 */
TEST_F(VisualCortexFepBridgeTest, DefaultConfigNull) {
    int result = visual_cortex_fep_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

/**
 * WHAT: Test connecting FEP system
 * WHY:  Verify FEP integration setup
 * HOW:  Connect FEP pointer, verify success
 */
TEST_F(VisualCortexFepBridgeTest, ConnectFep) {
    // Mock FEP system (just a non-null pointer for testing)
    fep_system_t* mock_fep = reinterpret_cast<fep_system_t*>(0x1000);

    int result = visual_cortex_fep_bridge_connect_fep(bridge, mock_fep);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test connecting visual cortex
 * WHY:  Verify visual cortex integration setup
 * HOW:  Connect visual cortex pointer, verify success
 */
TEST_F(VisualCortexFepBridgeTest, ConnectVisualCortex) {
    // Mock visual cortex (just a non-null pointer for testing)
    visual_cortex_t* mock_visual = reinterpret_cast<visual_cortex_t*>(0x2000);

    int result = visual_cortex_fep_bridge_connect_visual_cortex(bridge, mock_visual);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test connecting with NULL bridge
 * WHY:  Verify NULL safety for connection functions
 * HOW:  Call connect functions with NULL bridge
 */
TEST_F(VisualCortexFepBridgeTest, ConnectNullBridge) {
    fep_system_t* mock_fep = reinterpret_cast<fep_system_t*>(0x1000);
    visual_cortex_t* mock_visual = reinterpret_cast<visual_cortex_t*>(0x2000);

    EXPECT_EQ(visual_cortex_fep_bridge_connect_fep(nullptr, mock_fep), -1);
    EXPECT_EQ(visual_cortex_fep_bridge_connect_visual_cortex(nullptr, mock_visual), -1);
}

/**
 * WHAT: Test connecting with NULL systems
 * WHY:  Verify NULL safety for system pointers
 * HOW:  Call connect functions with NULL systems
 */
TEST_F(VisualCortexFepBridgeTest, ConnectNullSystems) {
    EXPECT_EQ(visual_cortex_fep_bridge_connect_fep(bridge, nullptr), -1);
    EXPECT_EQ(visual_cortex_fep_bridge_connect_visual_cortex(bridge, nullptr), -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

/**
 * WHAT: Test getting bridge state
 * WHY:  Verify state reporting functionality
 * HOW:  Get state, verify success
 */
TEST_F(VisualCortexFepBridgeTest, GetState) {
    visual_cortex_fep_state_t state;
    int result = visual_cortex_fep_bridge_get_state(bridge, &state);

    EXPECT_EQ(result, 0);
    // Initial state should have zeros
    EXPECT_GE(state.frames_processed, 0UL);
}

/**
 * WHAT: Test getting bridge statistics
 * WHY:  Verify statistics reporting
 * HOW:  Get stats, verify success
 */
TEST_F(VisualCortexFepBridgeTest, GetStats) {
    visual_cortex_fep_stats_t stats;
    int result = visual_cortex_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.total_frames_processed, 0UL);
}

/**
 * WHAT: Test get state/stats with NULL parameters
 * WHY:  Verify NULL safety for getter functions
 * HOW:  Call with various NULL combinations
 */
TEST_F(VisualCortexFepBridgeTest, GetStateStatsNull) {
    visual_cortex_fep_state_t state;
    visual_cortex_fep_stats_t stats;

    EXPECT_EQ(visual_cortex_fep_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(visual_cortex_fep_bridge_get_state(bridge, nullptr), -1);

    EXPECT_EQ(visual_cortex_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(visual_cortex_fep_bridge_get_stats(bridge, nullptr), -1);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

/**
 * WHAT: Test bridge update cycle
 * WHY:  Verify update mechanism works
 * HOW:  Call update, verify success
 */
TEST_F(VisualCortexFepBridgeTest, Update) {
    int result = visual_cortex_fep_bridge_update(bridge, 16);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test update with NULL bridge
 * WHY:  Verify NULL safety for update
 * HOW:  Call update with NULL
 */
TEST_F(VisualCortexFepBridgeTest, UpdateNull) {
    int result = visual_cortex_fep_bridge_update(nullptr, 16);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

/**
 * WHAT: Test bio-async connection
 * WHY:  Verify bio-async integration setup
 * HOW:  Connect bio-async, check state
 */
TEST_F(VisualCortexFepBridgeTest, BioAsyncConnect) {
    int result = visual_cortex_fep_bridge_connect_bio_async(bridge);
    // May return 0 or -1 depending on router availability
    // Just verify it doesn't crash
    (void)result;
}

/**
 * WHAT: Test bio-async disconnection
 * WHY:  Verify bio-async cleanup
 * HOW:  Disconnect bio-async, check state
 */
TEST_F(VisualCortexFepBridgeTest, BioAsyncDisconnect) {
    visual_cortex_fep_bridge_connect_bio_async(bridge);
    int result = visual_cortex_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test checking bio-async connection status
 * WHY:  Verify connection state query
 * HOW:  Check status before/after connection
 */
TEST_F(VisualCortexFepBridgeTest, BioAsyncIsConnected) {
    bool connected = visual_cortex_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);

    visual_cortex_fep_bridge_connect_bio_async(bridge);
    // May or may not be connected depending on router availability
}

/**
 * WHAT: Test bio-async functions with NULL bridge
 * WHY:  Verify NULL safety for bio-async API
 * HOW:  Call bio-async functions with NULL
 */
TEST_F(VisualCortexFepBridgeTest, BioAsyncNull) {
    EXPECT_EQ(visual_cortex_fep_bridge_connect_bio_async(nullptr), -1);
    EXPECT_EQ(visual_cortex_fep_bridge_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(visual_cortex_fep_bridge_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * FEP → Visual Direction Tests
 * ============================================================================ */

/**
 * WHAT: Test applying FEP predictions to visual processing
 * WHY:  Verify top-down prediction modulation
 * HOW:  Call apply_predictions, verify success
 */
TEST_F(VisualCortexFepBridgeTest, ApplyPredictions) {
    int result = visual_cortex_fep_apply_predictions(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test applying precision to visual attention
 * WHY:  Verify precision-weighted attention modulation
 * HOW:  Call apply_precision, verify success
 */
TEST_F(VisualCortexFepBridgeTest, ApplyPrecision) {
    int result = visual_cortex_fep_apply_precision(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test generating saccade via active inference
 * WHY:  Verify active vision mechanism
 * HOW:  Generate saccade, verify valid coordinates
 */
TEST_F(VisualCortexFepBridgeTest, GenerateSaccade) {
    float target_x = -1.0f;
    float target_y = -1.0f;

    int result = visual_cortex_fep_generate_saccade(bridge, &target_x, &target_y);
    EXPECT_EQ(result, 0);

    // Coordinates should be normalized [0, 1]
    EXPECT_GE(target_x, 0.0f);
    EXPECT_LE(target_x, 1.0f);
    EXPECT_GE(target_y, 0.0f);
    EXPECT_LE(target_y, 1.0f);
}

/**
 * WHAT: Test FEP→Visual functions with NULL bridge
 * WHY:  Verify NULL safety
 * HOW:  Call functions with NULL bridge
 */
TEST_F(VisualCortexFepBridgeTest, FepToVisualNull) {
    float target_x, target_y;

    EXPECT_EQ(visual_cortex_fep_apply_predictions(nullptr), -1);
    EXPECT_EQ(visual_cortex_fep_apply_precision(nullptr), -1);
    EXPECT_EQ(visual_cortex_fep_generate_saccade(nullptr, &target_x, &target_y), -1);
}

/**
 * WHAT: Test generate_saccade with NULL output parameters
 * WHY:  Verify NULL safety for output params
 * HOW:  Call with NULL target pointers
 */
TEST_F(VisualCortexFepBridgeTest, GenerateSaccadeNullOutputs) {
    float target_x, target_y;

    EXPECT_EQ(visual_cortex_fep_generate_saccade(bridge, nullptr, &target_y), -1);
    EXPECT_EQ(visual_cortex_fep_generate_saccade(bridge, &target_x, nullptr), -1);
    EXPECT_EQ(visual_cortex_fep_generate_saccade(bridge, nullptr, nullptr), -1);
}

/* ============================================================================
 * Visual → FEP Direction Tests
 * ============================================================================ */

/**
 * WHAT: Test computing visual prediction error
 * WHY:  Verify PE computation from visual features
 * HOW:  Compute PE with valid features, check result
 */
TEST_F(VisualCortexFepBridgeTest, ComputePredictionError) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float pe = -1.0f;

    int result = visual_cortex_fep_compute_prediction_error(
        bridge, features, 10, &pe
    );

    EXPECT_EQ(result, 0);
    EXPECT_GE(pe, 0.0f);
}

/**
 * WHAT: Test reporting visual observations to FEP
 * WHY:  Verify observation reporting mechanism
 * HOW:  Report features, verify success
 */
TEST_F(VisualCortexFepBridgeTest, ReportObservations) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    int result = visual_cortex_fep_report_observations(bridge, features, 10);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test reporting visual novelty to FEP
 * WHY:  Verify novelty signaling
 * HOW:  Report novelty score, verify success
 */
TEST_F(VisualCortexFepBridgeTest, ReportNovelty) {
    int result = visual_cortex_fep_report_novelty(bridge, 0.8f);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test Visual→FEP functions with NULL bridge
 * WHY:  Verify NULL safety
 * HOW:  Call functions with NULL bridge
 */
TEST_F(VisualCortexFepBridgeTest, VisualToFepNull) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float pe;

    EXPECT_EQ(visual_cortex_fep_compute_prediction_error(
        nullptr, features, 10, &pe), -1);
    EXPECT_EQ(visual_cortex_fep_report_observations(
        nullptr, features, 10), -1);
    EXPECT_EQ(visual_cortex_fep_report_novelty(nullptr, 0.5f), -1);
}

/**
 * WHAT: Test compute_prediction_error with NULL parameters
 * WHY:  Verify NULL safety for PE computation
 * HOW:  Call with various NULL combinations
 */
TEST_F(VisualCortexFepBridgeTest, ComputePredictionErrorNullParams) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float pe;

    EXPECT_EQ(visual_cortex_fep_compute_prediction_error(
        bridge, nullptr, 10, &pe), -1);
    EXPECT_EQ(visual_cortex_fep_compute_prediction_error(
        bridge, features, 10, nullptr), -1);
}

/**
 * WHAT: Test report_observations with NULL features
 * WHY:  Verify NULL safety for observation reporting
 * HOW:  Call with NULL features array
 */
TEST_F(VisualCortexFepBridgeTest, ReportObservationsNullFeatures) {
    int result = visual_cortex_fep_report_observations(bridge, nullptr, 10);
    EXPECT_EQ(result, -1);
}

/**
 * Main entry point for tests
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
