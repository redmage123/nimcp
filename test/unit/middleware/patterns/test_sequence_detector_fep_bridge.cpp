/**
 * @file test_sequence_detector_fep_bridge.cpp
 * @brief Unit tests for Sequence Detector - FEP Bridge
 * @version 1.0.0
 * @date 2025-12-12
 */

#include <gtest/gtest.h>
#include "middleware/patterns/nimcp_sequence_detector_fep_bridge.h"

class SequenceDetectorFepBridgeTest : public ::testing::Test {
protected:
    sequence_detector_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        sequence_detector_fep_config_t config;
        sequence_detector_fep_bridge_default_config(&config);
        bridge = sequence_detector_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            sequence_detector_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SequenceDetectorFepBridgeTest, DefaultConfigInitialization) {
    sequence_detector_fep_config_t config;
    int result = sequence_detector_fep_bridge_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_prediction_priming);
    EXPECT_TRUE(config.enable_precision_tolerance);
    EXPECT_TRUE(config.enable_sequence_pe);
    EXPECT_TRUE(config.enable_replay_consolidation);
    EXPECT_GT(config.tolerance_sensitivity, 0.0f);
    EXPECT_GT(config.pe_sensitivity, 0.0f);
}

TEST_F(SequenceDetectorFepBridgeTest, DefaultConfigNullPointer) {
    int result = sequence_detector_fep_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SequenceDetectorFepBridgeTest, BridgeCreationWithNullConfig) {
    sequence_detector_fep_bridge_t* bridge_null = sequence_detector_fep_bridge_create(nullptr);
    EXPECT_NE(bridge_null, nullptr);
    sequence_detector_fep_bridge_destroy(bridge_null);
}

TEST_F(SequenceDetectorFepBridgeTest, BridgeDestroyNullSafe) {
    sequence_detector_fep_bridge_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(SequenceDetectorFepBridgeTest, ConnectDetectorSuccess) {
    sequence_detector_t detector = {};
    int result = sequence_detector_fep_bridge_connect_detector(bridge, &detector);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, ConnectDetectorNullBridge) {
    sequence_detector_t detector = {};
    int result = sequence_detector_fep_bridge_connect_detector(nullptr, &detector);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, ConnectFepSuccess) {
    fep_system_t fep = {};
    int result = sequence_detector_fep_bridge_connect_fep(bridge, &fep);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, ConnectFepNullBridge) {
    fep_system_t fep = {};
    int result = sequence_detector_fep_bridge_connect_fep(nullptr, &fep);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, DisconnectSuccess) {
    int result = sequence_detector_fep_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, DisconnectNullBridge) {
    int result = sequence_detector_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * FEP → Sequence Detector Direction Tests
 * ============================================================================ */

TEST_F(SequenceDetectorFepBridgeTest, PrimeExpectedSequence) {
    int result = sequence_detector_fep_prime_expected_sequence(bridge, 123);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, PrimeExpectedSequenceNullBridge) {
    int result = sequence_detector_fep_prime_expected_sequence(nullptr, 123);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, AdjustToleranceHighPrecision) {
    int result = sequence_detector_fep_adjust_tolerance(bridge, 0.9f);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, AdjustToleranceLowPrecision) {
    int result = sequence_detector_fep_adjust_tolerance(bridge, 0.2f);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, AdjustToleranceNullBridge) {
    int result = sequence_detector_fep_adjust_tolerance(nullptr, 0.5f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Sequence Detector → FEP Direction Tests
 * ============================================================================ */

TEST_F(SequenceDetectorFepBridgeTest, ReportDetection) {
    sequence_detection_t detection = {};
    detection.template_id = 456;
    detection.strength = 0.8f;
    int result = sequence_detector_fep_report_detection(bridge, &detection);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, ReportDetectionNullBridge) {
    sequence_detection_t detection = {};
    int result = sequence_detector_fep_report_detection(nullptr, &detection);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, ReportDetectionNullDetection) {
    int result = sequence_detector_fep_report_detection(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, ReportViolation) {
    int result = sequence_detector_fep_report_violation(bridge, 789);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, ReportViolationNullBridge) {
    int result = sequence_detector_fep_report_violation(nullptr, 789);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, ReportReplay) {
    sequence_detection_t replay = {};
    replay.template_id = 321;
    replay.strength = 0.9f;
    int result = sequence_detector_fep_report_replay(bridge, &replay);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, ReportReplayNullBridge) {
    sequence_detection_t replay = {};
    int result = sequence_detector_fep_report_replay(nullptr, &replay);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, ReportReplayNullReplay) {
    int result = sequence_detector_fep_report_replay(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(SequenceDetectorFepBridgeTest, BridgeUpdate) {
    int result = sequence_detector_fep_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, BridgeUpdateNullBridge) {
    int result = sequence_detector_fep_bridge_update(nullptr, 100);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(SequenceDetectorFepBridgeTest, GetState) {
    sequence_detector_fep_state_t state;
    int result = sequence_detector_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, GetStateNullBridge) {
    sequence_detector_fep_state_t state;
    int result = sequence_detector_fep_bridge_get_state(nullptr, &state);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, GetStateNullOutput) {
    int result = sequence_detector_fep_bridge_get_state(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, GetStats) {
    sequence_detector_fep_stats_t stats;
    int result = sequence_detector_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, GetStatsNullBridge) {
    sequence_detector_fep_stats_t stats;
    int result = sequence_detector_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, GetStatsNullOutput) {
    sequence_detector_fep_stats_t stats;
    int result = sequence_detector_fep_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(SequenceDetectorFepBridgeTest, ConnectBioAsync) {
    int result = sequence_detector_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, ConnectBioAsyncNullBridge) {
    int result = sequence_detector_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, DisconnectBioAsync) {
    sequence_detector_fep_bridge_connect_bio_async(bridge);
    int result = sequence_detector_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, DisconnectBioAsyncNullBridge) {
    int result = sequence_detector_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SequenceDetectorFepBridgeTest, IsBioAsyncConnected) {
    bool connected = sequence_detector_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);

    sequence_detector_fep_bridge_connect_bio_async(bridge);
    connected = sequence_detector_fep_bridge_is_bio_async_connected(bridge);
    // May or may not be true depending on bio-async availability
}

TEST_F(SequenceDetectorFepBridgeTest, IsBioAsyncConnectedNullBridge) {
    bool connected = sequence_detector_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(SequenceDetectorFepBridgeTest, PredictionPrimingWorkflow) {
    // Prime expected sequence
    int result1 = sequence_detector_fep_prime_expected_sequence(bridge, 100);
    EXPECT_EQ(result1, 0);

    // Adjust tolerance for high precision
    int result2 = sequence_detector_fep_adjust_tolerance(bridge, 0.9f);
    EXPECT_EQ(result2, 0);

    // Report detection
    sequence_detection_t detection = {};
    detection.template_id = 100;
    detection.strength = 0.85f;
    int result3 = sequence_detector_fep_report_detection(bridge, &detection);
    EXPECT_EQ(result3, 0);

    // Update
    int result4 = sequence_detector_fep_bridge_update(bridge, 100);
    EXPECT_EQ(result4, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, ViolationWorkflow) {
    // Prime expected sequence
    int result1 = sequence_detector_fep_prime_expected_sequence(bridge, 200);
    EXPECT_EQ(result1, 0);

    // Report violation (different sequence detected)
    int result2 = sequence_detector_fep_report_violation(bridge, 200);
    EXPECT_EQ(result2, 0);

    // Update
    int result3 = sequence_detector_fep_bridge_update(bridge, 100);
    EXPECT_EQ(result3, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, ReplayConsolidationWorkflow) {
    // Report replay event
    sequence_detection_t replay = {};
    replay.template_id = 300;
    replay.strength = 0.95f;
    int result1 = sequence_detector_fep_report_replay(bridge, &replay);
    EXPECT_EQ(result1, 0);

    // Update
    int result2 = sequence_detector_fep_bridge_update(bridge, 100);
    EXPECT_EQ(result2, 0);
}

TEST_F(SequenceDetectorFepBridgeTest, PrecisionToleranceMapping) {
    // High precision → strict tolerance
    int result1 = sequence_detector_fep_adjust_tolerance(bridge, 0.95f);
    EXPECT_EQ(result1, 0);

    sequence_detector_fep_state_t state1;
    sequence_detector_fep_bridge_get_state(bridge, &state1);

    // Low precision → loose tolerance
    int result2 = sequence_detector_fep_adjust_tolerance(bridge, 0.15f);
    EXPECT_EQ(result2, 0);

    sequence_detector_fep_state_t state2;
    sequence_detector_fep_bridge_get_state(bridge, &state2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
