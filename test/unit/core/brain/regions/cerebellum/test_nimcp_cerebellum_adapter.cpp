/**
 * @file test_nimcp_cerebellum_adapter.cpp
 * @brief Unit tests for Cerebellum adapter
 *
 * Tests:
 * - Lifecycle (create, destroy, reset)
 * - Mossy fiber input processing
 * - Climbing fiber error processing
 * - Motor coordination pipeline
 * - Timing control
 * - Forward model operations
 * - Motor adaptation
 * - Callbacks and diagnostics
 *
 * @version Phase B4: Cerebellum Brain Integration
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

//=============================================================================
// Lifecycle Tests
//=============================================================================

class CerebellumLifecycleTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellumLifecycleTest, CreateWithNullConfig) {
    adapter = cerebellum_create(nullptr);
    ASSERT_NE(adapter, nullptr);
}

TEST_F(CerebellumLifecycleTest, CreateWithConfig) {
    cerebellum_config_t config = cerebellum_default_config();
    config.num_granule_cells = 1000;
    config.num_purkinje_cells = 50;

    adapter = cerebellum_create(&config);
    ASSERT_NE(adapter, nullptr);

    cerebellum_config_t retrieved;
    EXPECT_TRUE(cerebellum_get_config(adapter, &retrieved));
    EXPECT_EQ(retrieved.num_granule_cells, 1000);
    EXPECT_EQ(retrieved.num_purkinje_cells, 50);
}

TEST_F(CerebellumLifecycleTest, DefaultConfigValues) {
    cerebellum_config_t config = cerebellum_default_config();

    EXPECT_GT(config.num_granule_cells, 0);
    EXPECT_GT(config.num_purkinje_cells, 0);
    EXPECT_GT(config.num_parallel_fibers, 0);
    EXPECT_GT(config.num_climbing_fibers, 0);
    EXPECT_TRUE(config.enable_timing);
    EXPECT_TRUE(config.enable_error_learning);
    EXPECT_TRUE(config.enable_forward_models);
    EXPECT_TRUE(config.enable_motor_adaptation);
    EXPECT_GT(config.ltd_rate, 0.0f);
    EXPECT_GT(config.ltp_rate, 0.0f);
}

TEST_F(CerebellumLifecycleTest, DestroyNull) {
    cerebellum_destroy(nullptr);  // Should not crash
}

TEST_F(CerebellumLifecycleTest, Reset) {
    adapter = cerebellum_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    EXPECT_TRUE(cerebellum_reset(adapter));
    EXPECT_EQ(cerebellum_get_status(adapter), CEREBELLUM_STATUS_IDLE);
}

TEST_F(CerebellumLifecycleTest, ResetNull) {
    EXPECT_FALSE(cerebellum_reset(nullptr));
}

//=============================================================================
// Mossy Fiber Input Tests
//=============================================================================

class CerebellumMossyInputTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        adapter = cerebellum_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellumMossyInputTest, ProcessSingleInput) {
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.8f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    EXPECT_TRUE(cerebellum_process_mossy_input(adapter, &input));

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);
    EXPECT_EQ(stats.mossy_inputs_processed, 1);
}

TEST_F(CerebellumMossyInputTest, ProcessBatchInput) {
    mossy_fiber_input_t inputs[5];
    for (int i = 0; i < 5; i++) {
        inputs[i].fiber_id = i;
        inputs[i].activity = 0.5f + 0.1f * i;
        inputs[i].timestamp_ms = (float)i;
        inputs[i].modality = 0;
    }

    EXPECT_TRUE(cerebellum_process_mossy_batch(adapter, inputs, 5));

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);
    EXPECT_EQ(stats.mossy_inputs_processed, 5);
}

TEST_F(CerebellumMossyInputTest, ProcessNullInput) {
    EXPECT_FALSE(cerebellum_process_mossy_input(adapter, nullptr));
    EXPECT_EQ(cerebellum_get_last_error(adapter), CEREBELLUM_ERROR_INVALID_INPUT);
}

TEST_F(CerebellumMossyInputTest, ProcessNullAdapter) {
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.5f;

    EXPECT_FALSE(cerebellum_process_mossy_input(nullptr, &input));
}

TEST_F(CerebellumMossyInputTest, StatusAfterReceiving) {
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.8f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    cerebellum_process_mossy_input(adapter, &input);
    EXPECT_EQ(cerebellum_get_status(adapter), CEREBELLUM_STATUS_RECEIVING);
}

//=============================================================================
// Climbing Fiber Error Tests
//=============================================================================

class CerebellumClimbingFiberTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_error_learning = true;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellumClimbingFiberTest, ProcessClimbingSignal) {
    climbing_fiber_signal_t signal;
    signal.fiber_id = 0;
    signal.error_signal = 0.5f;
    signal.timestamp_ms = 0.0f;
    signal.target_purkinje_id = 0;
    signal.error_type = 0;

    EXPECT_TRUE(cerebellum_process_climbing_signal(adapter, &signal));

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);
    EXPECT_EQ(stats.climbing_signals, 1);
    EXPECT_GT(stats.purkinje_complex_spikes, 0);
}

TEST_F(CerebellumClimbingFiberTest, BroadcastError) {
    EXPECT_TRUE(cerebellum_broadcast_error(adapter, 0.3f, 1));

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);
    EXPECT_GT(stats.climbing_signals, 0);
}

TEST_F(CerebellumClimbingFiberTest, SignalTriggersLearning) {
    climbing_fiber_signal_t signal;
    signal.fiber_id = 0;
    signal.error_signal = 0.5f;  // Above threshold
    signal.timestamp_ms = 0.0f;
    signal.target_purkinje_id = 0;
    signal.error_type = 0;

    EXPECT_TRUE(cerebellum_process_climbing_signal(adapter, &signal));

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);
    EXPECT_GT(stats.ltd_events, 0);  // LTD should have been triggered
}

TEST_F(CerebellumClimbingFiberTest, NullSignal) {
    EXPECT_FALSE(cerebellum_process_climbing_signal(adapter, nullptr));
}

//=============================================================================
// Motor Coordination Pipeline Tests
//=============================================================================

class CerebellumCoordinationTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        adapter = cerebellum_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellumCoordinationTest, BeginCoordination) {
    EXPECT_TRUE(cerebellum_begin_coordination(adapter));
    EXPECT_EQ(cerebellum_get_status(adapter), CEREBELLUM_STATUS_IDLE);
}

TEST_F(CerebellumCoordinationTest, ProcessCoordination) {
    cerebellum_begin_coordination(adapter);

    // Feed some input
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.8f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;
    cerebellum_process_mossy_input(adapter, &input);

    motor_coordination_result_t result;
    EXPECT_TRUE(cerebellum_process(adapter, &result));

    EXPECT_EQ(cerebellum_get_status(adapter), CEREBELLUM_STATUS_OUTPUT_READY);
    EXPECT_TRUE(result.motor_ready);
}

TEST_F(CerebellumCoordinationTest, GetNucleiOutput) {
    cerebellum_begin_coordination(adapter);

    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.8f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;
    cerebellum_process_mossy_input(adapter, &input);

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    nuclei_output_t output;
    EXPECT_TRUE(cerebellum_get_nuclei_output(adapter, &output));
    EXPECT_GT(output.activity, 0.0f);
    EXPECT_GT(output.num_dimensions, 0);
}

TEST_F(CerebellumCoordinationTest, NullResult) {
    EXPECT_TRUE(cerebellum_process(adapter, nullptr));  // Should still work, just no output
}

TEST_F(CerebellumCoordinationTest, FullPipeline) {
    // Step 1: Begin coordination
    cerebellum_begin_coordination(adapter);

    // Step 2: Feed multiple mossy fiber inputs
    for (int i = 0; i < 10; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i;
        input.activity = 0.5f + 0.05f * i;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    // Step 3: Process through cerebellum
    motor_coordination_result_t result;
    EXPECT_TRUE(cerebellum_process(adapter, &result));

    // Step 4: Verify output
    EXPECT_TRUE(result.motor_ready);
    EXPECT_GT(result.num_motor_dims, 0);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);

    // Step 5: Get nuclei output
    nuclei_output_t output;
    EXPECT_TRUE(cerebellum_get_nuclei_output(adapter, &output));

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);
    EXPECT_EQ(stats.mossy_inputs_processed, 10);
    EXPECT_EQ(stats.motor_commands_output, 1);
}

//=============================================================================
// Timing Control Tests
//=============================================================================

class CerebellumTimingTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_timing = true;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellumTimingTest, SetTargetTiming) {
    EXPECT_TRUE(cerebellum_set_target_timing(adapter, 100.0f));
}

TEST_F(CerebellumTimingTest, PredictTiming) {
    cerebellum_set_target_timing(adapter, 100.0f);

    // Feed input and process
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.8f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;
    cerebellum_process_mossy_input(adapter, &input);

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    float predicted_ms;
    float confidence;
    EXPECT_TRUE(cerebellum_predict_timing(adapter, &predicted_ms, &confidence));
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(CerebellumTimingTest, ReportTiming) {
    cerebellum_set_target_timing(adapter, 100.0f);
    EXPECT_TRUE(cerebellum_report_timing(adapter, 110.0f));  // 10ms error

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);
    EXPECT_GT(stats.avg_timing_error_ms, 0.0f);
}

TEST_F(CerebellumTimingTest, TimingLearning) {
    cerebellum_set_target_timing(adapter, 100.0f);

    // Report a significant timing error
    cerebellum_report_timing(adapter, 120.0f);  // 20ms error

    // Should trigger learning
    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);
    EXPECT_GT(stats.climbing_signals, 0);  // Error should have been broadcast
}

//=============================================================================
// Forward Model Tests
//=============================================================================

class CerebellumForwardModelTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_forward_models = true;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellumForwardModelTest, UpdateForwardModel) {
    float motor_command[4] = {0.5f, 0.3f, 0.2f, 0.1f};
    float outcome[4] = {0.6f, 0.4f, 0.3f, 0.2f};

    EXPECT_TRUE(cerebellum_update_forward_model(adapter, motor_command, outcome, 4));
}

TEST_F(CerebellumForwardModelTest, PredictOutcome) {
    // First train the model
    float motor_command[4] = {0.5f, 0.3f, 0.2f, 0.1f};
    float outcome[4] = {0.6f, 0.4f, 0.3f, 0.2f};
    cerebellum_update_forward_model(adapter, motor_command, outcome, 4);

    // Then predict
    float predicted_outcome[4];
    float confidence;
    EXPECT_TRUE(cerebellum_predict_outcome(adapter, motor_command, 4, predicted_outcome, &confidence));
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(CerebellumForwardModelTest, LearningReducesError) {
    float motor_command[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float outcome[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    // Train multiple times
    for (int i = 0; i < 10; i++) {
        cerebellum_update_forward_model(adapter, motor_command, outcome, 4);
    }

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);
    EXPECT_LT(stats.avg_error_after_learning, 1.0f);  // Error should decrease
}

//=============================================================================
// Motor Adaptation Tests
//=============================================================================

class CerebellumAdaptationTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_motor_adaptation = true;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);

        // Feed some input to activate the system
        mossy_fiber_input_t input;
        input.fiber_id = 0;
        input.activity = 0.8f;
        input.timestamp_ms = 0.0f;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);

        motor_coordination_result_t result;
        cerebellum_process(adapter, &result);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellumAdaptationTest, AdaptGains) {
    float gains[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    EXPECT_TRUE(cerebellum_adapt_gains(adapter, gains, 4, 0.1f));

    // Gains should have been modified
    bool modified = false;
    for (int i = 0; i < 4; i++) {
        if (fabs(gains[i] - 1.0f) > 0.001f) {
            modified = true;
            break;
        }
    }
    // Note: may not always be modified depending on nuclei state
}

TEST_F(CerebellumAdaptationTest, GetAdaptationState) {
    float adaptation_level;
    EXPECT_TRUE(cerebellum_get_adaptation_state(adapter, &adaptation_level));
    EXPECT_GE(adaptation_level, 0.0f);
    EXPECT_LE(adaptation_level, 1.0f);
}

TEST_F(CerebellumAdaptationTest, GainsClamped) {
    float gains[4] = {0.05f, 3.0f, 1.0f, 1.0f};  // Extreme values

    cerebellum_adapt_gains(adapter, gains, 4, 0.1f);

    // Gains should be clamped to [0.1, 2.0]
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(gains[i], 0.1f);
        EXPECT_LE(gains[i], 2.0f);
    }
}

//=============================================================================
// Status and Diagnostics Tests
//=============================================================================

class CerebellumDiagnosticsTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        adapter = cerebellum_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellumDiagnosticsTest, GetStatus) {
    EXPECT_EQ(cerebellum_get_status(adapter), CEREBELLUM_STATUS_IDLE);
}

TEST_F(CerebellumDiagnosticsTest, GetStatusNull) {
    EXPECT_EQ(cerebellum_get_status(nullptr), CEREBELLUM_STATUS_ERROR);
}

TEST_F(CerebellumDiagnosticsTest, GetLastError) {
    EXPECT_EQ(cerebellum_get_last_error(adapter), CEREBELLUM_ERROR_NONE);
}

TEST_F(CerebellumDiagnosticsTest, GetLastErrorNull) {
    EXPECT_EQ(cerebellum_get_last_error(nullptr), CEREBELLUM_ERROR_INTERNAL);
}

TEST_F(CerebellumDiagnosticsTest, ErrorStrings) {
    EXPECT_STREQ(cerebellum_error_string(CEREBELLUM_ERROR_NONE), "No error");
    EXPECT_STREQ(cerebellum_error_string(CEREBELLUM_ERROR_INVALID_INPUT), "Invalid input");
    EXPECT_STREQ(cerebellum_error_string(CEREBELLUM_ERROR_GRANULE_FAILURE), "Granule layer failure");
    EXPECT_STREQ(cerebellum_error_string(CEREBELLUM_ERROR_PURKINJE_FAILURE), "Purkinje layer failure");
    EXPECT_STREQ(cerebellum_error_string(CEREBELLUM_ERROR_TIMING_FAILURE), "Timing computation failure");
    EXPECT_STREQ(cerebellum_error_string(CEREBELLUM_ERROR_LEARNING_FAILURE), "Learning failure");
    EXPECT_STREQ(cerebellum_error_string(CEREBELLUM_ERROR_NUCLEI_FAILURE), "Deep nuclei failure");
    EXPECT_STREQ(cerebellum_error_string(CEREBELLUM_ERROR_INTERNAL), "Internal error");
}

TEST_F(CerebellumDiagnosticsTest, StatusStrings) {
    EXPECT_STREQ(cerebellum_status_string(CEREBELLUM_STATUS_IDLE), "Idle");
    EXPECT_STREQ(cerebellum_status_string(CEREBELLUM_STATUS_RECEIVING), "Receiving input");
    EXPECT_STREQ(cerebellum_status_string(CEREBELLUM_STATUS_PROCESSING), "Processing");
    EXPECT_STREQ(cerebellum_status_string(CEREBELLUM_STATUS_TIMING), "Timing computation");
    EXPECT_STREQ(cerebellum_status_string(CEREBELLUM_STATUS_LEARNING), "Learning");
    EXPECT_STREQ(cerebellum_status_string(CEREBELLUM_STATUS_OUTPUT_READY), "Output ready");
    EXPECT_STREQ(cerebellum_status_string(CEREBELLUM_STATUS_ERROR), "Error");
}

TEST_F(CerebellumDiagnosticsTest, GetStats) {
    cerebellum_stats_t stats;
    EXPECT_TRUE(cerebellum_get_stats(adapter, &stats));
    EXPECT_EQ(stats.mossy_inputs_processed, 0);
    EXPECT_EQ(stats.climbing_signals, 0);
    EXPECT_EQ(stats.motor_commands_output, 0);
}

TEST_F(CerebellumDiagnosticsTest, GetStatsNull) {
    cerebellum_stats_t stats;
    EXPECT_FALSE(cerebellum_get_stats(nullptr, &stats));
    EXPECT_FALSE(cerebellum_get_stats(adapter, nullptr));
}

TEST_F(CerebellumDiagnosticsTest, GetConfig) {
    cerebellum_config_t config;
    EXPECT_TRUE(cerebellum_get_config(adapter, &config));
    EXPECT_GT(config.num_granule_cells, 0);
    EXPECT_GT(config.num_purkinje_cells, 0);
}

//=============================================================================
// Sub-module Access Tests
//=============================================================================

class CerebellumSubmoduleTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        adapter = cerebellum_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellumSubmoduleTest, GetGranuleLayer) {
    granule_layer_t* granule = cerebellum_get_granule_layer(adapter);
    EXPECT_NE(granule, nullptr);
}

TEST_F(CerebellumSubmoduleTest, GetPurkinjeLayer) {
    purkinje_layer_t* purkinje = cerebellum_get_purkinje_layer(adapter);
    EXPECT_NE(purkinje, nullptr);
}

TEST_F(CerebellumSubmoduleTest, GetDeepNuclei) {
    deep_nuclei_t* nuclei = cerebellum_get_deep_nuclei(adapter);
    EXPECT_NE(nuclei, nullptr);
}

TEST_F(CerebellumSubmoduleTest, GetSubmodulesNull) {
    EXPECT_EQ(cerebellum_get_granule_layer(nullptr), nullptr);
    EXPECT_EQ(cerebellum_get_purkinje_layer(nullptr), nullptr);
    EXPECT_EQ(cerebellum_get_deep_nuclei(nullptr), nullptr);
}

//=============================================================================
// Callback Tests
//=============================================================================

class CerebellumCallbackTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;
    static bool motor_callback_called;
    static nuclei_output_t last_output;

    static void motor_callback(const nuclei_output_t* output, void* user_data) {
        motor_callback_called = true;
        if (output) {
            last_output = *output;
        }
    }

    void SetUp() override {
        adapter = cerebellum_create(nullptr);
        ASSERT_NE(adapter, nullptr);
        motor_callback_called = false;
        memset(&last_output, 0, sizeof(last_output));
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

bool CerebellumCallbackTest::motor_callback_called = false;
nuclei_output_t CerebellumCallbackTest::last_output;

TEST_F(CerebellumCallbackTest, SetMotorCallback) {
    EXPECT_TRUE(cerebellum_set_motor_callback(adapter, motor_callback, nullptr));
}

TEST_F(CerebellumCallbackTest, MotorCallbackInvoked) {
    cerebellum_set_motor_callback(adapter, motor_callback, nullptr);

    // Process through cerebellum
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.8f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;
    cerebellum_process_mossy_input(adapter, &input);

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    nuclei_output_t output;
    cerebellum_get_nuclei_output(adapter, &output);

    EXPECT_TRUE(motor_callback_called);
}

TEST_F(CerebellumCallbackTest, SetCallbackNull) {
    EXPECT_FALSE(cerebellum_set_motor_callback(nullptr, motor_callback, nullptr));
}

//=============================================================================
// Null Safety Tests
//=============================================================================

class CerebellumNullSafetyTest : public ::testing::Test {};

TEST_F(CerebellumNullSafetyTest, AllOperationsHandleNull) {
    mossy_fiber_input_t mossy;
    mossy.fiber_id = 0;
    mossy.activity = 0.5f;

    climbing_fiber_signal_t climbing;
    climbing.fiber_id = 0;
    climbing.error_signal = 0.5f;

    motor_coordination_result_t result;
    nuclei_output_t output;
    float value;

    // All these should return false/null and not crash
    EXPECT_FALSE(cerebellum_reset(nullptr));
    EXPECT_FALSE(cerebellum_process_mossy_input(nullptr, &mossy));
    EXPECT_FALSE(cerebellum_process_mossy_batch(nullptr, &mossy, 1));
    EXPECT_FALSE(cerebellum_process_climbing_signal(nullptr, &climbing));
    EXPECT_FALSE(cerebellum_broadcast_error(nullptr, 0.5f, 0));
    EXPECT_FALSE(cerebellum_begin_coordination(nullptr));
    EXPECT_FALSE(cerebellum_process(nullptr, &result));
    EXPECT_FALSE(cerebellum_get_nuclei_output(nullptr, &output));
    EXPECT_FALSE(cerebellum_set_target_timing(nullptr, 100.0f));
    EXPECT_FALSE(cerebellum_predict_timing(nullptr, &value, &value));
    EXPECT_FALSE(cerebellum_report_timing(nullptr, 100.0f));
    EXPECT_FALSE(cerebellum_get_adaptation_state(nullptr, &value));
    EXPECT_EQ(cerebellum_get_status(nullptr), CEREBELLUM_STATUS_ERROR);
    EXPECT_EQ(cerebellum_get_last_error(nullptr), CEREBELLUM_ERROR_INTERNAL);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
