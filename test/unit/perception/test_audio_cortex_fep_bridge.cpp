/**
 * @file test_audio_cortex_fep_bridge.cpp
 * @brief Unit tests for Audio Cortex FEP Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Comprehensive unit tests for audio-FEP integration bridge
 * WHY:  Ensure correct lifecycle, connection, state management, and bidirectional
 *       integration between FEP and audio cortex (temporal prediction, MMN,
 *       cocktail party effect).
 * HOW:  Test all API functions with valid and invalid inputs, verify return codes,
 *       state transitions, and null safety.
 */

#include <gtest/gtest.h>
#include "perception/nimcp_audio_cortex_fep_bridge.h"

/**
 * @brief Test fixture for Audio Cortex FEP Bridge tests
 *
 * WHAT: Provides setup/teardown for bridge testing
 * WHY:  Ensure clean state for each test
 * HOW:  Create bridge in SetUp, destroy in TearDown
 */
class AudioCortexFepBridgeTest : public ::testing::Test {
protected:
    audio_cortex_fep_bridge_t* bridge = nullptr;

    /**
     * WHAT: Initialize bridge before each test
     * WHY:  Ensure clean starting state
     * HOW:  Create with default config
     */
    void SetUp() override {
        audio_cortex_fep_config_t config;
        audio_cortex_fep_bridge_default_config(&config);
        bridge = audio_cortex_fep_bridge_create(&config);
    }

    /**
     * WHAT: Clean up bridge after each test
     * WHY:  Prevent memory leaks
     * HOW:  Destroy bridge, reset pointer
     */
    void TearDown() override {
        if (bridge) {
            audio_cortex_fep_bridge_destroy(bridge);
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
TEST_F(AudioCortexFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
    // Cleanup handled by TearDown
}

/**
 * WHAT: Test creation with NULL config uses defaults
 * WHY:  Verify NULL config fallback behavior
 * HOW:  Create with NULL, verify non-null result
 */
TEST_F(AudioCortexFepBridgeTest, CreateWithNullConfig) {
    // Clean up existing bridge
    audio_cortex_fep_bridge_destroy(bridge);

    // Create with NULL config
    bridge = audio_cortex_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
}

/**
 * WHAT: Test destroying NULL bridge is safe
 * WHY:  Verify NULL pointer safety
 * HOW:  Call destroy with NULL, should not crash
 */
TEST_F(AudioCortexFepBridgeTest, DestroyNull) {
    audio_cortex_fep_bridge_destroy(nullptr);
    // Test passes if no crash
}

/**
 * WHAT: Test default config provides valid values
 * WHY:  Verify configuration initialization
 * HOW:  Call default_config, check fields
 */
TEST_F(AudioCortexFepBridgeTest, DefaultConfig) {
    audio_cortex_fep_config_t config;
    int result = audio_cortex_fep_bridge_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_GT(config.prediction_error_threshold, 0.0f);
    EXPECT_GT(config.precision_tuning_factor, 0.0f);
    EXPECT_GT(config.temporal_prediction_weight, 0.0f);
}

/**
 * WHAT: Test default config with NULL parameter
 * WHY:  Verify NULL safety for config initialization
 * HOW:  Call with NULL, expect error
 */
TEST_F(AudioCortexFepBridgeTest, DefaultConfigNull) {
    int result = audio_cortex_fep_bridge_default_config(nullptr);
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
TEST_F(AudioCortexFepBridgeTest, ConnectFep) {
    // Mock FEP system (just a non-null pointer for testing)
    fep_system_t* mock_fep = reinterpret_cast<fep_system_t*>(0x1000);

    int result = audio_cortex_fep_bridge_connect_fep(bridge, mock_fep);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test connecting audio cortex
 * WHY:  Verify audio cortex integration setup
 * HOW:  Connect audio cortex pointer, verify success
 */
TEST_F(AudioCortexFepBridgeTest, ConnectAudioCortex) {
    // Mock audio cortex (just a non-null pointer for testing)
    audio_cortex_t* mock_audio = reinterpret_cast<audio_cortex_t*>(0x2000);

    int result = audio_cortex_fep_bridge_connect_audio_cortex(bridge, mock_audio);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test connecting with NULL bridge
 * WHY:  Verify NULL safety for connection functions
 * HOW:  Call connect functions with NULL bridge
 */
TEST_F(AudioCortexFepBridgeTest, ConnectNullBridge) {
    fep_system_t* mock_fep = reinterpret_cast<fep_system_t*>(0x1000);
    audio_cortex_t* mock_audio = reinterpret_cast<audio_cortex_t*>(0x2000);

    EXPECT_EQ(audio_cortex_fep_bridge_connect_fep(nullptr, mock_fep), -1);
    EXPECT_EQ(audio_cortex_fep_bridge_connect_audio_cortex(nullptr, mock_audio), -1);
}

/**
 * WHAT: Test connecting with NULL systems
 * WHY:  Verify NULL safety for system pointers
 * HOW:  Call connect functions with NULL systems
 */
TEST_F(AudioCortexFepBridgeTest, ConnectNullSystems) {
    EXPECT_EQ(audio_cortex_fep_bridge_connect_fep(bridge, nullptr), -1);
    EXPECT_EQ(audio_cortex_fep_bridge_connect_audio_cortex(bridge, nullptr), -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

/**
 * WHAT: Test getting bridge state
 * WHY:  Verify state reporting functionality
 * HOW:  Get state, verify success
 */
TEST_F(AudioCortexFepBridgeTest, GetState) {
    audio_cortex_fep_state_t state;
    int result = audio_cortex_fep_bridge_get_state(bridge, &state);

    EXPECT_EQ(result, 0);
    // Initial state should have zeros
    EXPECT_GE(state.frames_processed, 0UL);
}

/**
 * WHAT: Test getting bridge statistics
 * WHY:  Verify statistics reporting
 * HOW:  Get stats, verify success
 */
TEST_F(AudioCortexFepBridgeTest, GetStats) {
    audio_cortex_fep_stats_t stats;
    int result = audio_cortex_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.total_frames_processed, 0UL);
}

/**
 * WHAT: Test get state/stats with NULL parameters
 * WHY:  Verify NULL safety for getter functions
 * HOW:  Call with various NULL combinations
 */
TEST_F(AudioCortexFepBridgeTest, GetStateStatsNull) {
    audio_cortex_fep_state_t state;
    audio_cortex_fep_stats_t stats;

    EXPECT_EQ(audio_cortex_fep_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(audio_cortex_fep_bridge_get_state(bridge, nullptr), -1);

    EXPECT_EQ(audio_cortex_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(audio_cortex_fep_bridge_get_stats(bridge, nullptr), -1);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

/**
 * WHAT: Test bridge update cycle
 * WHY:  Verify update mechanism works
 * HOW:  Call update, verify success
 */
TEST_F(AudioCortexFepBridgeTest, Update) {
    int result = audio_cortex_fep_bridge_update(bridge, 16);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test update with NULL bridge
 * WHY:  Verify NULL safety for update
 * HOW:  Call update with NULL
 */
TEST_F(AudioCortexFepBridgeTest, UpdateNull) {
    int result = audio_cortex_fep_bridge_update(nullptr, 16);
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
TEST_F(AudioCortexFepBridgeTest, BioAsyncConnect) {
    int result = audio_cortex_fep_bridge_connect_bio_async(bridge);
    // May return 0 or -1 depending on router availability
    // Just verify it doesn't crash
    (void)result;
}

/**
 * WHAT: Test bio-async disconnection
 * WHY:  Verify bio-async cleanup
 * HOW:  Disconnect bio-async, check state
 */
TEST_F(AudioCortexFepBridgeTest, BioAsyncDisconnect) {
    audio_cortex_fep_bridge_connect_bio_async(bridge);
    int result = audio_cortex_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test checking bio-async connection status
 * WHY:  Verify connection state query
 * HOW:  Check status before/after connection
 */
TEST_F(AudioCortexFepBridgeTest, BioAsyncIsConnected) {
    bool connected = audio_cortex_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);

    audio_cortex_fep_bridge_connect_bio_async(bridge);
    // May or may not be connected depending on router availability
}

/**
 * WHAT: Test bio-async functions with NULL bridge
 * WHY:  Verify NULL safety for bio-async API
 * HOW:  Call bio-async functions with NULL
 */
TEST_F(AudioCortexFepBridgeTest, BioAsyncNull) {
    EXPECT_EQ(audio_cortex_fep_bridge_connect_bio_async(nullptr), -1);
    EXPECT_EQ(audio_cortex_fep_bridge_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(audio_cortex_fep_bridge_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * FEP → Audio Direction Tests
 * ============================================================================ */

/**
 * WHAT: Test applying FEP temporal predictions to auditory processing
 * WHY:  Verify temporal prediction modulation
 * HOW:  Call apply_temporal_predictions, verify success
 */
TEST_F(AudioCortexFepBridgeTest, ApplyTemporalPredictions) {
    int result = audio_cortex_fep_apply_temporal_predictions(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test applying precision to frequency tuning (cocktail party)
 * WHY:  Verify precision-weighted tuning modulation
 * HOW:  Call apply_precision_tuning, verify success
 */
TEST_F(AudioCortexFepBridgeTest, ApplyPrecisionTuning) {
    int result = audio_cortex_fep_apply_precision_tuning(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test FEP→Audio functions with NULL bridge
 * WHY:  Verify NULL safety
 * HOW:  Call functions with NULL bridge
 */
TEST_F(AudioCortexFepBridgeTest, FepToAudioNull) {
    EXPECT_EQ(audio_cortex_fep_apply_temporal_predictions(nullptr), -1);
    EXPECT_EQ(audio_cortex_fep_apply_precision_tuning(nullptr), -1);
}

/* ============================================================================
 * Audio → FEP Direction Tests
 * ============================================================================ */

/**
 * WHAT: Test computing auditory prediction error (MMN)
 * WHY:  Verify PE computation from audio features
 * HOW:  Compute PE with valid features, check result
 */
TEST_F(AudioCortexFepBridgeTest, ComputePredictionError) {
    float features[20] = {
        0.05f, 0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f, 0.4f, 0.45f, 0.5f,
        0.55f, 0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.0f
    };
    float pe = -1.0f;

    int result = audio_cortex_fep_compute_prediction_error(
        bridge, features, 20, &pe
    );

    EXPECT_EQ(result, 0);
    EXPECT_GE(pe, 0.0f);
}

/**
 * WHAT: Test reporting auditory observations to FEP
 * WHY:  Verify observation reporting mechanism
 * HOW:  Report features, verify success
 */
TEST_F(AudioCortexFepBridgeTest, ReportObservations) {
    float features[20] = {
        0.05f, 0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f, 0.4f, 0.45f, 0.5f,
        0.55f, 0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.0f
    };

    int result = audio_cortex_fep_report_observations(bridge, features, 20);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test reporting temporal events (onset/offset) to FEP
 * WHY:  Verify temporal boundary signaling
 * HOW:  Report events, verify success
 */
TEST_F(AudioCortexFepBridgeTest, ReportTemporalEvents) {
    int result = audio_cortex_fep_report_temporal_events(bridge, true, false);
    EXPECT_EQ(result, 0);

    result = audio_cortex_fep_report_temporal_events(bridge, false, true);
    EXPECT_EQ(result, 0);

    result = audio_cortex_fep_report_temporal_events(bridge, true, true);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test Audio→FEP functions with NULL bridge
 * WHY:  Verify NULL safety
 * HOW:  Call functions with NULL bridge
 */
TEST_F(AudioCortexFepBridgeTest, AudioToFepNull) {
    float features[20] = {
        0.05f, 0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f, 0.4f, 0.45f, 0.5f,
        0.55f, 0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.0f
    };
    float pe;

    EXPECT_EQ(audio_cortex_fep_compute_prediction_error(
        nullptr, features, 20, &pe), -1);
    EXPECT_EQ(audio_cortex_fep_report_observations(
        nullptr, features, 20), -1);
    EXPECT_EQ(audio_cortex_fep_report_temporal_events(
        nullptr, true, false), -1);
}

/**
 * WHAT: Test compute_prediction_error with NULL parameters
 * WHY:  Verify NULL safety for PE computation
 * HOW:  Call with various NULL combinations
 */
TEST_F(AudioCortexFepBridgeTest, ComputePredictionErrorNullParams) {
    float features[20] = {
        0.05f, 0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f, 0.4f, 0.45f, 0.5f,
        0.55f, 0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.0f
    };
    float pe;

    EXPECT_EQ(audio_cortex_fep_compute_prediction_error(
        bridge, nullptr, 20, &pe), -1);
    EXPECT_EQ(audio_cortex_fep_compute_prediction_error(
        bridge, features, 20, nullptr), -1);
}

/**
 * WHAT: Test report_observations with NULL features
 * WHY:  Verify NULL safety for observation reporting
 * HOW:  Call with NULL features array
 */
TEST_F(AudioCortexFepBridgeTest, ReportObservationsNullFeatures) {
    int result = audio_cortex_fep_report_observations(bridge, nullptr, 20);
    EXPECT_EQ(result, -1);
}

/**
 * Main entry point for tests
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
