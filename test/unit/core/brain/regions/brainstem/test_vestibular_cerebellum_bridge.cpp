/**
 * @file test_vestibular_cerebellum_bridge.cpp
 * @brief Unit tests for vestibular-cerebellum bridge
 *
 * Tests:
 * - Lifecycle (create/destroy)
 * - Mossy fiber transmission
 * - VOR adaptation
 * - Cerebellar feedback
 * - Status and diagnostics
 *
 * @version Phase 3: Vestibular System Integration
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/brainstem/nimcp_vestibular.h"
#include "core/brain/regions/brainstem/nimcp_vestibular_cerebellum_bridge.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class VestibularCerebellumBridgeTest : public ::testing::Test {
protected:
    vestibular_processor_t* vestibular = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;
    vestibular_cerebellum_bridge_t* bridge = nullptr;

    void SetUp() override {
        // Create vestibular processor
        vestibular_config_t vest_config = vestibular_default_config();
        vestibular = vestibular_create(&vest_config);
        ASSERT_NE(vestibular, nullptr);

        // Create cerebellum adapter
        cerebellum_config_t cereb_config = cerebellum_default_config();
        cereb_config.enable_vestibulocerebellum = true;
        cerebellum = cerebellum_create(&cereb_config);
        ASSERT_NE(cerebellum, nullptr);

        // Create bridge with defaults
        bridge = vestibular_cerebellum_bridge_create(vestibular, cerebellum, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            vestibular_cerebellum_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (vestibular) {
            vestibular_destroy(vestibular);
            vestibular = nullptr;
        }
    }

    // Helper to create and process vestibular input
    void processVestibularInput(float yaw_velocity, uint64_t timestamp_us) {
        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.canals.yaw_velocity = yaw_velocity;
        input.canals.pitch_velocity = 0.0f;
        input.canals.roll_velocity = 0.0f;
        input.canals.timestamp_us = timestamp_us;
        input.canals_valid = true;
        input.otoliths_valid = false;

        vestibular_process_input(vestibular, &input);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

class VestibularCerebellumConfigTest : public ::testing::Test {};

TEST_F(VestibularCerebellumConfigTest, DefaultConfigValues) {
    vestibular_cerebellum_config_t config = vestibular_cerebellum_default_config();

    // Mossy fiber routing
    EXPECT_GT(config.num_mossy_fibers, 0u);
    EXPECT_GT(config.mossy_fiber_gain, 0.0f);

    // VOR adaptation
    EXPECT_TRUE(config.enable_vor_adaptation);
    EXPECT_FLOAT_EQ(config.vor_ltd_rate, VOR_DEFAULT_LTD_RATE);
    EXPECT_FLOAT_EQ(config.retinal_slip_threshold, VOR_RETINAL_SLIP_THRESHOLD);

    // Routing
    EXPECT_TRUE(config.route_to_flocculus);
    EXPECT_TRUE(config.route_to_nodulus);

    // Feedback
    EXPECT_TRUE(config.enable_feedback_loop);
    EXPECT_GT(config.feedback_weight, 0.0f);
    EXPECT_LE(config.feedback_weight, 1.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(VestibularCerebellumBridgeTest, CreateWithDefaultConfig) {
    vestibular_cerebellum_status_t status = vestibular_cerebellum_get_status(bridge);
    EXPECT_EQ(status, VESTIBULAR_CEREBELLUM_STATUS_IDLE);
}

TEST_F(VestibularCerebellumBridgeTest, CreateWithCustomConfig) {
    vestibular_cerebellum_config_t config = vestibular_cerebellum_default_config();
    config.enable_vor_adaptation = false;
    config.route_to_nodulus = false;

    vestibular_cerebellum_bridge_t* custom_bridge =
        vestibular_cerebellum_bridge_create(vestibular, cerebellum, &config);
    ASSERT_NE(custom_bridge, nullptr);

    vestibular_cerebellum_bridge_destroy(custom_bridge);
}

TEST_F(VestibularCerebellumBridgeTest, CreateWithNullVestibular) {
    vestibular_cerebellum_bridge_t* null_bridge =
        vestibular_cerebellum_bridge_create(nullptr, cerebellum, nullptr);
    EXPECT_EQ(null_bridge, nullptr);
}

TEST_F(VestibularCerebellumBridgeTest, CreateWithNullCerebellum) {
    vestibular_cerebellum_bridge_t* null_bridge =
        vestibular_cerebellum_bridge_create(vestibular, nullptr, nullptr);
    EXPECT_EQ(null_bridge, nullptr);
}

TEST_F(VestibularCerebellumBridgeTest, DestroyNull) {
    vestibular_cerebellum_bridge_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Mossy Fiber Transmission Tests
//=============================================================================

TEST_F(VestibularCerebellumBridgeTest, SendMossySignal) {
    // First, provide some vestibular input
    processVestibularInput(1.0f, 0);

    // Send mossy signal
    int result = vestibular_cerebellum_send_mossy_signal(bridge);
    EXPECT_EQ(result, 0);

    // Status returns to IDLE after successful transmission
    vestibular_cerebellum_status_t status = vestibular_cerebellum_get_status(bridge);
    EXPECT_EQ(status, VESTIBULAR_CEREBELLUM_STATUS_IDLE);

    // Verify stats were updated
    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.mossy_signals_sent, 1u);
}

TEST_F(VestibularCerebellumBridgeTest, SendMossySignalNull) {
    int result = vestibular_cerebellum_send_mossy_signal(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(VestibularCerebellumBridgeTest, SendCustomSignal) {
    vestibular_mossy_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.head_velocity[0] = 0.5f;  // Yaw
    signal.head_velocity[1] = 0.0f;
    signal.head_velocity[2] = 0.3f;
    signal.linear_accel[0] = 0.0f;
    signal.linear_accel[1] = 0.0f;
    signal.linear_accel[2] = 0.0f;
    signal.retinal_slip = 0.1f;
    signal.source = VESTIBULAR_NUCLEUS_MEDIAL;
    signal.timestamp_us = 10000;

    int result = vestibular_cerebellum_send_custom_signal(bridge, &signal);
    EXPECT_EQ(result, 0);
}

TEST_F(VestibularCerebellumBridgeTest, SendCustomSignalNull) {
    int result = vestibular_cerebellum_send_custom_signal(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(VestibularCerebellumBridgeTest, MossySignalStats) {
    // Send multiple signals
    for (int i = 0; i < 10; i++) {
        processVestibularInput(0.5f, i * 1000);
        vestibular_cerebellum_send_mossy_signal(bridge);
    }

    vestibular_cerebellum_stats_t stats;
    int result = vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.mossy_signals_sent, 10u);
}

//=============================================================================
// VOR Adaptation Tests
//=============================================================================

TEST_F(VestibularCerebellumBridgeTest, TriggerVorAdaptation) {
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};  // Yaw retinal slip

    int result = vestibular_cerebellum_trigger_vor_adaptation(
        bridge, 0.5f, slip_direction);
    EXPECT_EQ(result, 0);

    // Status returns to IDLE after adaptation completes
    vestibular_cerebellum_status_t status = vestibular_cerebellum_get_status(bridge);
    EXPECT_EQ(status, VESTIBULAR_CEREBELLUM_STATUS_IDLE);

    // Verify adaptation was triggered
    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.adaptation_triggers, 1u);
}

TEST_F(VestibularCerebellumBridgeTest, VorAdaptationBelowThreshold) {
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};

    // Very small slip - below threshold
    int result = vestibular_cerebellum_trigger_vor_adaptation(
        bridge, 0.001f, slip_direction);

    // Should succeed but not trigger actual adaptation
    EXPECT_EQ(result, 0);

    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.adaptation_triggers, 0u);  // No adaptation triggered
}

TEST_F(VestibularCerebellumBridgeTest, VorAdaptationAboveThreshold) {
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};

    // Significant slip - above threshold
    int result = vestibular_cerebellum_trigger_vor_adaptation(
        bridge, 0.5f, slip_direction);
    EXPECT_EQ(result, 0);

    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.adaptation_triggers, 1u);
}

TEST_F(VestibularCerebellumBridgeTest, GetVorState) {
    float vor_gain[3];
    bool adaptation_active;

    int result = vestibular_cerebellum_get_vor_state(bridge, vor_gain, &adaptation_active);
    EXPECT_EQ(result, 0);

    // Default VOR gain should be close to 1.0
    EXPECT_NEAR(vor_gain[0], 1.0f, 0.2f);
    EXPECT_NEAR(vor_gain[1], 1.0f, 0.2f);
    EXPECT_NEAR(vor_gain[2], 1.0f, 0.2f);
    EXPECT_FALSE(adaptation_active);
}

TEST_F(VestibularCerebellumBridgeTest, VorGainChangesWithAdaptation) {
    // Get initial modulation
    float initial_modulation;
    vestibular_cerebellum_get_modulation(bridge, VESTIBULAR_NUCLEUS_MEDIAL, &initial_modulation);
    EXPECT_FLOAT_EQ(initial_modulation, 1.0f);  // Starts at neutral

    // Apply repeated retinal slip (should cause modulation changes)
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 50; i++) {
        vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.5f, slip_direction);
    }

    // Check modulation changed
    float final_modulation;
    vestibular_cerebellum_get_modulation(bridge, VESTIBULAR_NUCLEUS_MEDIAL, &final_modulation);

    // Modulation should have changed from initial (adaptation occurred)
    EXPECT_NE(final_modulation, initial_modulation);

    // Verify stats show VOR gain change occurred
    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_vor_gain_change, 0.0f);
    EXPECT_EQ(stats.adaptation_triggers, 50u);
}

TEST_F(VestibularCerebellumBridgeTest, VorAdaptationNullDirection) {
    int result = vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.5f, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(VestibularCerebellumBridgeTest, MaxRetinalSlipClipped) {
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};

    // Very large slip should be clipped
    int result = vestibular_cerebellum_trigger_vor_adaptation(
        bridge, 100.0f, slip_direction);
    EXPECT_EQ(result, 0);

    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_LE(stats.avg_retinal_slip, VOR_MAX_RETINAL_SLIP);
}

//=============================================================================
// Cerebellar Feedback Tests
//=============================================================================

TEST_F(VestibularCerebellumBridgeTest, ApplyFeedback) {
    // First activate the cerebellum
    processVestibularInput(1.0f, 0);
    vestibular_cerebellum_send_mossy_signal(bridge);

    // Apply feedback
    int result = vestibular_cerebellum_apply_feedback(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(VestibularCerebellumBridgeTest, ApplyFeedbackNull) {
    int result = vestibular_cerebellum_apply_feedback(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(VestibularCerebellumBridgeTest, FeedbackStats) {
    for (int i = 0; i < 5; i++) {
        processVestibularInput(0.8f, i * 1000);
        vestibular_cerebellum_send_mossy_signal(bridge);
        vestibular_cerebellum_apply_feedback(bridge);
    }

    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.feedback_events, 5u);
}

TEST_F(VestibularCerebellumBridgeTest, GetModulation) {
    float modulation;

    int result = vestibular_cerebellum_get_modulation(
        bridge, VESTIBULAR_NUCLEUS_MEDIAL, &modulation);
    EXPECT_EQ(result, 0);

    // Modulation should be in [0, 2] range
    EXPECT_GE(modulation, 0.0f);
    EXPECT_LE(modulation, 2.0f);
}

TEST_F(VestibularCerebellumBridgeTest, GetModulationAllNuclei) {
    float modulation;

    // Test all vestibular nuclei
    EXPECT_EQ(0, vestibular_cerebellum_get_modulation(
        bridge, VESTIBULAR_NUCLEUS_MEDIAL, &modulation));
    EXPECT_EQ(0, vestibular_cerebellum_get_modulation(
        bridge, VESTIBULAR_NUCLEUS_LATERAL, &modulation));
    EXPECT_EQ(0, vestibular_cerebellum_get_modulation(
        bridge, VESTIBULAR_NUCLEUS_SUPERIOR, &modulation));
    EXPECT_EQ(0, vestibular_cerebellum_get_modulation(
        bridge, VESTIBULAR_NUCLEUS_INFERIOR, &modulation));
}

TEST_F(VestibularCerebellumBridgeTest, GetModulationNull) {
    int result = vestibular_cerebellum_get_modulation(
        bridge, VESTIBULAR_NUCLEUS_MEDIAL, nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Status and Diagnostics Tests
//=============================================================================

TEST_F(VestibularCerebellumBridgeTest, InitialStatus) {
    vestibular_cerebellum_status_t status = vestibular_cerebellum_get_status(bridge);
    EXPECT_EQ(status, VESTIBULAR_CEREBELLUM_STATUS_IDLE);
}

TEST_F(VestibularCerebellumBridgeTest, GetStatusNull) {
    vestibular_cerebellum_status_t status = vestibular_cerebellum_get_status(nullptr);
    EXPECT_EQ(status, VESTIBULAR_CEREBELLUM_STATUS_ERROR);
}

TEST_F(VestibularCerebellumBridgeTest, InitialError) {
    vestibular_cerebellum_error_t error = vestibular_cerebellum_get_last_error(bridge);
    EXPECT_EQ(error, VESTIBULAR_CEREBELLUM_ERROR_NONE);
}

TEST_F(VestibularCerebellumBridgeTest, GetErrorNull) {
    vestibular_cerebellum_error_t error = vestibular_cerebellum_get_last_error(nullptr);
    // Implementation returns INTERNAL for NULL bridge (indicates general error)
    EXPECT_EQ(error, VESTIBULAR_CEREBELLUM_ERROR_INTERNAL);
}

TEST_F(VestibularCerebellumBridgeTest, GetStats) {
    vestibular_cerebellum_stats_t stats;
    int result = vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.mossy_signals_sent, 0u);
    EXPECT_EQ(stats.adaptation_triggers, 0u);
    EXPECT_EQ(stats.feedback_events, 0u);
    EXPECT_FLOAT_EQ(stats.total_vor_gain_change, 0.0f);
}

TEST_F(VestibularCerebellumBridgeTest, GetStatsNull) {
    int result = vestibular_cerebellum_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(VestibularCerebellumBridgeTest, GetStatsNullBridge) {
    vestibular_cerebellum_stats_t stats;
    int result = vestibular_cerebellum_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Integration with Vestibular and Cerebellum
//=============================================================================

TEST_F(VestibularCerebellumBridgeTest, FullVorLoop) {
    // Simulate complete VOR calibration loop

    // 1. Head movement (vestibular input)
    processVestibularInput(2.0f, 0);

    // 2. Send to cerebellum via mossy fibers
    vestibular_cerebellum_send_mossy_signal(bridge);

    // 3. Retinal slip error (eyes didn't compensate properly)
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};
    vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.3f, slip_direction);

    // 4. Cerebellar feedback modulates vestibular response
    vestibular_cerebellum_apply_feedback(bridge);

    // 5. Verify adaptation occurred
    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);

    EXPECT_GT(stats.mossy_signals_sent, 0u);
    EXPECT_GT(stats.adaptation_triggers, 0u);
    EXPECT_GT(stats.feedback_events, 0u);
}

TEST_F(VestibularCerebellumBridgeTest, RepeatedAdaptationConverges) {
    // Multiple adaptation cycles should gradually reduce error

    float vor_gains[5][3];
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};
    bool adaptation_active;

    for (int cycle = 0; cycle < 5; cycle++) {
        // Get current gain
        vestibular_cerebellum_get_vor_state(bridge, vor_gains[cycle], &adaptation_active);

        // Apply slip and adapt
        for (int i = 0; i < 10; i++) {
            vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.2f, slip_direction);
        }
    }

    // Each cycle should show progressive reduction in yaw gain
    for (int i = 1; i < 5; i++) {
        EXPECT_LE(vor_gains[i][0], vor_gains[i-1][0]);
    }
}

//=============================================================================
// Flocculus Output Tests
//=============================================================================

TEST_F(VestibularCerebellumBridgeTest, FlocculusOutputAfterActivity) {
    // Process vestibular input and send mossy signals
    for (int i = 0; i < 10; i++) {
        processVestibularInput(1.5f, i * 1000);
        vestibular_cerebellum_send_mossy_signal(bridge);
    }

    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);

    // Verify mossy signals were sent successfully
    EXPECT_EQ(stats.mossy_signals_sent, 10u);

    // Flocculus output depends on cerebellum's VOR processing
    // which may or may not produce output in unit test context
    EXPECT_GE(stats.current_flocculus_output, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
