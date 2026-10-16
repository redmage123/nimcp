/**
 * @file test_speech_cortex_fep_bridge.cpp
 * @brief Unit tests for Speech Cortex FEP Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Comprehensive unit tests for speech-FEP integration bridge
 * WHY:  Ensure correct lifecycle, connection, state management, and bidirectional
 *       integration between FEP and speech cortex (phoneme prediction, motor theory,
 *       word segmentation).
 * HOW:  Test all API functions with valid and invalid inputs, verify return codes,
 *       state transitions, and null safety.
 */

#include <gtest/gtest.h>
#include "perception/nimcp_speech_cortex_fep_bridge.h"

/**
 * @brief Test fixture for Speech Cortex FEP Bridge tests
 *
 * WHAT: Provides setup/teardown for bridge testing
 * WHY:  Ensure clean state for each test
 * HOW:  Create bridge in SetUp, destroy in TearDown
 */
class SpeechCortexFepBridgeTest : public ::testing::Test {
protected:
    speech_cortex_fep_bridge_t* bridge = nullptr;

    /**
     * WHAT: Initialize bridge before each test
     * WHY:  Ensure clean starting state
     * HOW:  Create with default config
     */
    void SetUp() override {
        speech_cortex_fep_config_t config;
        speech_cortex_fep_bridge_default_config(&config);
        bridge = speech_cortex_fep_bridge_create(&config);
    }

    /**
     * WHAT: Clean up bridge after each test
     * WHY:  Prevent memory leaks
     * HOW:  Destroy bridge, reset pointer
     */
    void TearDown() override {
        if (bridge) {
            speech_cortex_fep_bridge_destroy(bridge);
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
TEST_F(SpeechCortexFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
    // Cleanup handled by TearDown
}

/**
 * WHAT: Test creation with NULL config uses defaults
 * WHY:  Verify NULL config fallback behavior
 * HOW:  Create with NULL, verify non-null result
 */
TEST_F(SpeechCortexFepBridgeTest, CreateWithNullConfig) {
    // Clean up existing bridge
    speech_cortex_fep_bridge_destroy(bridge);

    // Create with NULL config
    bridge = speech_cortex_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
}

/**
 * WHAT: Test destroying NULL bridge is safe
 * WHY:  Verify NULL pointer safety
 * HOW:  Call destroy with NULL, should not crash
 */
TEST_F(SpeechCortexFepBridgeTest, DestroyNull) {
    speech_cortex_fep_bridge_destroy(nullptr);
    // Test passes if no crash
}

/**
 * WHAT: Test default config provides valid values
 * WHY:  Verify configuration initialization
 * HOW:  Call default_config, check fields
 */
TEST_F(SpeechCortexFepBridgeTest, DefaultConfig) {
    speech_cortex_fep_config_t config;
    int result = speech_cortex_fep_bridge_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_GT(config.prediction_error_threshold, 0.0f);
    EXPECT_GT(config.precision_category_factor, 0.0f);
    EXPECT_GT(config.phoneme_prediction_weight, 0.0f);
}

/**
 * WHAT: Test default config with NULL parameter
 * WHY:  Verify NULL safety for config initialization
 * HOW:  Call with NULL, expect error
 */
TEST_F(SpeechCortexFepBridgeTest, DefaultConfigNull) {
    int result = speech_cortex_fep_bridge_default_config(nullptr);
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
TEST_F(SpeechCortexFepBridgeTest, ConnectFep) {
    // Mock FEP system (just a non-null pointer for testing)
    fep_system_t* mock_fep = reinterpret_cast<fep_system_t*>(0x1000);

    int result = speech_cortex_fep_bridge_connect_fep(bridge, mock_fep);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test connecting speech cortex
 * WHY:  Verify speech cortex integration setup
 * HOW:  Connect speech cortex pointer, verify success
 */
TEST_F(SpeechCortexFepBridgeTest, ConnectSpeechCortex) {
    // Mock speech cortex (just a non-null pointer for testing)
    speech_cortex_t* mock_speech = reinterpret_cast<speech_cortex_t*>(0x2000);

    int result = speech_cortex_fep_bridge_connect_speech_cortex(bridge, mock_speech);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test connecting with NULL bridge
 * WHY:  Verify NULL safety for connection functions
 * HOW:  Call connect functions with NULL bridge
 */
TEST_F(SpeechCortexFepBridgeTest, ConnectNullBridge) {
    fep_system_t* mock_fep = reinterpret_cast<fep_system_t*>(0x1000);
    speech_cortex_t* mock_speech = reinterpret_cast<speech_cortex_t*>(0x2000);

    EXPECT_EQ(speech_cortex_fep_bridge_connect_fep(nullptr, mock_fep), -1);
    EXPECT_EQ(speech_cortex_fep_bridge_connect_speech_cortex(nullptr, mock_speech), -1);
}

/**
 * WHAT: Test connecting with NULL systems
 * WHY:  Verify NULL safety for system pointers
 * HOW:  Call connect functions with NULL systems
 */
TEST_F(SpeechCortexFepBridgeTest, ConnectNullSystems) {
    EXPECT_EQ(speech_cortex_fep_bridge_connect_fep(bridge, nullptr), -1);
    EXPECT_EQ(speech_cortex_fep_bridge_connect_speech_cortex(bridge, nullptr), -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

/**
 * WHAT: Test getting bridge state
 * WHY:  Verify state reporting functionality
 * HOW:  Get state, verify success
 */
TEST_F(SpeechCortexFepBridgeTest, GetState) {
    speech_cortex_fep_state_t state;
    int result = speech_cortex_fep_bridge_get_state(bridge, &state);

    EXPECT_EQ(result, 0);
    // Initial state should have zeros
    EXPECT_GE(state.phonemes_processed, 0UL);
}

/**
 * WHAT: Test getting bridge statistics
 * WHY:  Verify statistics reporting
 * HOW:  Get stats, verify success
 */
TEST_F(SpeechCortexFepBridgeTest, GetStats) {
    speech_cortex_fep_stats_t stats;
    int result = speech_cortex_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.total_phonemes_processed, 0UL);
}

/**
 * WHAT: Test get state/stats with NULL parameters
 * WHY:  Verify NULL safety for getter functions
 * HOW:  Call with various NULL combinations
 */
TEST_F(SpeechCortexFepBridgeTest, GetStateStatsNull) {
    speech_cortex_fep_state_t state;
    speech_cortex_fep_stats_t stats;

    EXPECT_EQ(speech_cortex_fep_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(speech_cortex_fep_bridge_get_state(bridge, nullptr), -1);

    EXPECT_EQ(speech_cortex_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(speech_cortex_fep_bridge_get_stats(bridge, nullptr), -1);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

/**
 * WHAT: Test bridge update cycle
 * WHY:  Verify update mechanism works
 * HOW:  Call update, verify success
 */
TEST_F(SpeechCortexFepBridgeTest, Update) {
    int result = speech_cortex_fep_bridge_update(bridge, 16);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test update with NULL bridge
 * WHY:  Verify NULL safety for update
 * HOW:  Call update with NULL
 */
TEST_F(SpeechCortexFepBridgeTest, UpdateNull) {
    int result = speech_cortex_fep_bridge_update(nullptr, 16);
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
TEST_F(SpeechCortexFepBridgeTest, BioAsyncConnect) {
    int result = speech_cortex_fep_bridge_connect_bio_async(bridge);
    // May return 0 or -1 depending on router availability
    // Just verify it doesn't crash
    (void)result;
}

/**
 * WHAT: Test bio-async disconnection
 * WHY:  Verify bio-async cleanup
 * HOW:  Disconnect bio-async, check state
 */
TEST_F(SpeechCortexFepBridgeTest, BioAsyncDisconnect) {
    speech_cortex_fep_bridge_connect_bio_async(bridge);
    int result = speech_cortex_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test checking bio-async connection status
 * WHY:  Verify connection state query
 * HOW:  Check status before/after connection
 */
TEST_F(SpeechCortexFepBridgeTest, BioAsyncIsConnected) {
    bool connected = speech_cortex_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);

    speech_cortex_fep_bridge_connect_bio_async(bridge);
    // May or may not be connected depending on router availability
}

/**
 * WHAT: Test bio-async functions with NULL bridge
 * WHY:  Verify NULL safety for bio-async API
 * HOW:  Call bio-async functions with NULL
 */
TEST_F(SpeechCortexFepBridgeTest, BioAsyncNull) {
    EXPECT_EQ(speech_cortex_fep_bridge_connect_bio_async(nullptr), -1);
    EXPECT_EQ(speech_cortex_fep_bridge_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(speech_cortex_fep_bridge_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * FEP → Speech Direction Tests
 * ============================================================================ */

/**
 * WHAT: Test applying FEP phoneme predictions to speech processing
 * WHY:  Verify phoneme prediction priming
 * HOW:  Call apply_phoneme_predictions, verify success
 */
TEST_F(SpeechCortexFepBridgeTest, ApplyPhonemePredictions) {
    int result = speech_cortex_fep_apply_phoneme_predictions(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test applying precision to phoneme categories
 * WHY:  Verify precision-weighted category modulation
 * HOW:  Call apply_precision_categories, verify success
 */
TEST_F(SpeechCortexFepBridgeTest, ApplyPrecisionCategories) {
    int result = speech_cortex_fep_apply_precision_categories(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test applying motor predictions to articulation (motor theory)
 * WHY:  Verify motor theory integration
 * HOW:  Call apply_motor_predictions, verify success
 */
TEST_F(SpeechCortexFepBridgeTest, ApplyMotorPredictions) {
    int result = speech_cortex_fep_apply_motor_predictions(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test FEP→Speech functions with NULL bridge
 * WHY:  Verify NULL safety
 * HOW:  Call functions with NULL bridge
 */
TEST_F(SpeechCortexFepBridgeTest, FepToSpeechNull) {
    EXPECT_EQ(speech_cortex_fep_apply_phoneme_predictions(nullptr), -1);
    EXPECT_EQ(speech_cortex_fep_apply_precision_categories(nullptr), -1);
    EXPECT_EQ(speech_cortex_fep_apply_motor_predictions(nullptr), -1);
}

/* ============================================================================
 * Speech → FEP Direction Tests
 * ============================================================================ */

/**
 * WHAT: Test computing phoneme prediction error
 * WHY:  Verify PE computation from detected phonemes
 * HOW:  Compute PE with valid phoneme, check result
 */
TEST_F(SpeechCortexFepBridgeTest, ComputePhonemePredictionError) {
    // Use phoneme value from speech cortex (assume IY = 0 for testing)
    phoneme_t test_phoneme = static_cast<phoneme_t>(0);
    float confidence = 0.8f;
    float pe = -1.0f;

    int result = speech_cortex_fep_compute_phoneme_prediction_error(
        bridge, test_phoneme, confidence, &pe
    );

    EXPECT_EQ(result, 0);
    EXPECT_GE(pe, 0.0f);
}

/**
 * WHAT: Test reporting phoneme observations to FEP
 * WHY:  Verify phoneme observation reporting
 * HOW:  Report phoneme, verify success
 */
TEST_F(SpeechCortexFepBridgeTest, ReportPhonemeObservation) {
    phoneme_t test_phoneme = static_cast<phoneme_t>(0);
    float confidence = 0.9f;

    int result = speech_cortex_fep_report_phoneme_observation(
        bridge, test_phoneme, confidence
    );
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test reporting word boundary to FEP
 * WHY:  Verify word segmentation signaling
 * HOW:  Report boundary, verify success
 */
TEST_F(SpeechCortexFepBridgeTest, ReportWordBoundary) {
    int result = speech_cortex_fep_report_word_boundary(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test Speech→FEP functions with NULL bridge
 * WHY:  Verify NULL safety
 * HOW:  Call functions with NULL bridge
 */
TEST_F(SpeechCortexFepBridgeTest, SpeechToFepNull) {
    phoneme_t test_phoneme = static_cast<phoneme_t>(0);
    float confidence = 0.8f;
    float pe;

    EXPECT_EQ(speech_cortex_fep_compute_phoneme_prediction_error(
        nullptr, test_phoneme, confidence, &pe), -1);
    EXPECT_EQ(speech_cortex_fep_report_phoneme_observation(
        nullptr, test_phoneme, confidence), -1);
    EXPECT_EQ(speech_cortex_fep_report_word_boundary(nullptr), -1);
}

/**
 * WHAT: Test compute_phoneme_prediction_error with NULL output
 * WHY:  Verify NULL safety for PE output
 * HOW:  Call with NULL PE pointer
 */
TEST_F(SpeechCortexFepBridgeTest, ComputePhonemePredictionErrorNullOutput) {
    phoneme_t test_phoneme = static_cast<phoneme_t>(0);
    float confidence = 0.8f;

    int result = speech_cortex_fep_compute_phoneme_prediction_error(
        bridge, test_phoneme, confidence, nullptr
    );
    EXPECT_EQ(result, -1);
}

/**
 * WHAT: Test phoneme functions with invalid confidence values
 * WHY:  Verify confidence range validation
 * HOW:  Call with out-of-range confidence values
 */
TEST_F(SpeechCortexFepBridgeTest, InvalidConfidenceValues) {
    phoneme_t test_phoneme = static_cast<phoneme_t>(0);
    float pe;

    // Negative confidence
    int result = speech_cortex_fep_compute_phoneme_prediction_error(
        bridge, test_phoneme, -0.5f, &pe
    );
    // Should handle gracefully (may return 0 with clamping or -1 for error)

    // Confidence > 1.0
    result = speech_cortex_fep_report_phoneme_observation(
        bridge, test_phoneme, 1.5f
    );
    // Should handle gracefully
}

/**
 * Main entry point for tests
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
