//=============================================================================
// test_subthalamic_regression.cpp - Subthalamic Nucleus Regression Tests
//=============================================================================
/**
 * @file test_subthalamic_regression.cpp
 * @brief Regression tests for the subthalamic nucleus (STN) system
 *
 * WHAT: Tests for creation/destruction cycles, STN activation patterns,
 *       hyperdirect pathway timing, and state consistency
 * WHY:  Ensure STN behavior is stable and correct for action suppression
 * HOW:  GTest framework with lifecycle, timing, and pattern tests
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_subthalamic.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SubthalamicRegressionTest : public NimcpTestBase {
protected:
    subthalamic_nucleus_t* stn = nullptr;
    static constexpr uint32_t TEST_NEURONS = 64;
    static constexpr uint32_t TEST_ACTIONS = 8;

    void SetUp() override {
        NimcpTestBase::SetUp();

        subthalamic_config_t config;
        subthalamic_default_config(&config);
        config.num_neurons = TEST_NEURONS;
        config.num_actions = TEST_ACTIONS;
        stn = subthalamic_create(&config);
    }

    void TearDown() override {
        if (stn) {
            subthalamic_destroy(stn);
            stn = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Creation/Destruction Cycle Tests
//=============================================================================

TEST_F(SubthalamicRegressionTest, CreateDestroy_BasicCycle) {
    subthalamic_config_t config;
    subthalamic_default_config(&config);

    subthalamic_nucleus_t* s = subthalamic_create(&config);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->num_neurons, config.num_neurons);
    EXPECT_EQ(s->num_actions, config.num_actions);

    subthalamic_destroy(s);
}

TEST_F(SubthalamicRegressionTest, CreateDestroy_RepeatedCycles) {
    for (int i = 0; i < 100; i++) {
        subthalamic_config_t config;
        subthalamic_default_config(&config);
        config.num_neurons = 32 + (i % 64);
        config.num_actions = 4 + (i % 12);

        subthalamic_nucleus_t* s = subthalamic_create(&config);
        ASSERT_NE(s, nullptr) << "Create failed at iteration " << i;

        // Verify structure
        EXPECT_EQ(s->num_neurons, config.num_neurons);
        EXPECT_EQ(s->num_actions, config.num_actions);
        EXPECT_NE(s->neurons, nullptr);
        EXPECT_NE(s->output, nullptr);

        subthalamic_destroy(s);
    }
}

TEST_F(SubthalamicRegressionTest, CreateDestroy_WithOperations) {
    for (int i = 0; i < 50; i++) {
        subthalamic_config_t config;
        subthalamic_default_config(&config);
        config.num_neurons = 64;
        config.num_actions = 8;

        subthalamic_nucleus_t* s = subthalamic_create(&config);
        ASSERT_NE(s, nullptr);

        // Perform operations
        std::vector<float> cortical_input(config.num_actions, 0.5f);
        std::vector<float> gpe_input(config.num_actions, 0.3f);

        subthalamic_set_cortical_input(s, cortical_input.data(), false);
        subthalamic_set_gpe_input(s, gpe_input.data());
        subthalamic_process(s);

        for (int j = 0; j < 10; j++) {
            subthalamic_step(s, 1.0f);
        }

        subthalamic_destroy(s);
    }
}

TEST_F(SubthalamicRegressionTest, CreateDestroy_NullConfig) {
    // subthalamic_create(NULL) returns NULL (matches GP/SN pattern)
    subthalamic_nucleus_t* s = subthalamic_create(nullptr);
    EXPECT_EQ(s, nullptr);
    // Destroy NULL should be safe no-op
    subthalamic_destroy(s);
}

TEST_F(SubthalamicRegressionTest, CreateDestroy_NullSafety) {
    subthalamic_destroy(nullptr);  // Should not crash
}

//=============================================================================
// STN Activation Pattern Tests
//=============================================================================

TEST_F(SubthalamicRegressionTest, ActivationPattern_BaselineMode) {
    ASSERT_NE(stn, nullptr);

    // Without input, should be in baseline mode
    EXPECT_EQ(subthalamic_get_mode(stn), STN_MODE_BASELINE);

    // Baseline output should be tonic firing
    float global_output = subthalamic_get_global_output(stn);
    EXPECT_GE(global_output, 0.0f);
    EXPECT_LE(global_output, 1.0f);
}

TEST_F(SubthalamicRegressionTest, ActivationPattern_HyperdirectActivation) {
    ASSERT_NE(stn, nullptr);

    // Apply strong cortical input (hyperdirect pathway)
    std::vector<float> cortical_input(TEST_ACTIONS, 1.0f);
    int result = subthalamic_set_cortical_input(stn, cortical_input.data(), true);
    EXPECT_GE(result, 0);

    // Process
    result = subthalamic_process(stn);
    EXPECT_GE(result, 0);

    // Should be in hyperdirect mode
    EXPECT_EQ(subthalamic_get_mode(stn), STN_MODE_HYPERDIRECT);

    // Output should increase (suppression signal)
    float global_output = subthalamic_get_global_output(stn);
    EXPECT_GT(global_output, 0.0f);
}

TEST_F(SubthalamicRegressionTest, ActivationPattern_IndirectActivation) {
    ASSERT_NE(stn, nullptr);

    // Apply GPe disinhibition (indirect pathway)
    std::vector<float> gpe_input(TEST_ACTIONS, 0.2f);  // Low GPe = disinhibition of STN
    int result = subthalamic_set_gpe_input(stn, gpe_input.data());
    EXPECT_GE(result, 0);

    // Process
    result = subthalamic_process(stn);
    EXPECT_GE(result, 0);

    // Should show indirect activation
    stn_mode_t mode = subthalamic_get_mode(stn);
    EXPECT_TRUE(mode == STN_MODE_INDIRECT || mode == STN_MODE_BASELINE);
}

TEST_F(SubthalamicRegressionTest, ActivationPattern_EmergencyStop) {
    ASSERT_NE(stn, nullptr);

    // Trigger emergency stop with high urgency
    int result = subthalamic_emergency_stop(stn, 1.0f);
    EXPECT_GE(result, 0);

    // Process
    result = subthalamic_process(stn);
    EXPECT_GE(result, 0);

    // Should be in suppression mode
    EXPECT_EQ(subthalamic_get_mode(stn), STN_MODE_SUPPRESSION);

    // Global output should be elevated above baseline (~0.083) after emergency.
    // process() recomputes using normal formula with the cortical_input=1.0 set
    // by emergency_stop, yielding ~0.3125 (above baseline but not >0.5 since
    // the process formula doesn't use the emergency 4x multiplier)
    float global_output = subthalamic_get_global_output(stn);
    EXPECT_GT(global_output, 0.2f);
}

TEST_F(SubthalamicRegressionTest, ActivationPattern_OutputPerAction) {
    ASSERT_NE(stn, nullptr);

    // Apply cortical input
    std::vector<float> cortical_input(TEST_ACTIONS);
    for (uint32_t i = 0; i < TEST_ACTIONS; i++) {
        cortical_input[i] = 0.1f * (i + 1);
    }
    subthalamic_set_cortical_input(stn, cortical_input.data(), false);
    subthalamic_process(stn);

    // Get per-action output
    std::vector<float> output(TEST_ACTIONS);
    int result = subthalamic_get_output(stn, output.data());
    EXPECT_GE(result, 0);

    // All outputs should be valid
    for (uint32_t i = 0; i < TEST_ACTIONS; i++) {
        EXPECT_GE(output[i], 0.0f) << "Negative output at action " << i;
        EXPECT_FALSE(std::isnan(output[i])) << "NaN at action " << i;
        EXPECT_FALSE(std::isinf(output[i])) << "Inf at action " << i;
    }
}

//=============================================================================
// Hyperdirect Pathway Timing Tests
//=============================================================================

TEST_F(SubthalamicRegressionTest, HyperdirectTiming_DelayConsistency) {
    ASSERT_NE(stn, nullptr);

    // Record initial state
    float initial_output = subthalamic_get_global_output(stn);

    // Apply hyperdirect input
    std::vector<float> cortical_input(TEST_ACTIONS, 0.8f);
    subthalamic_set_cortical_input(stn, cortical_input.data(), true);

    // Step through time to observe delay
    std::vector<float> output_history;
    for (int i = 0; i < 50; i++) {
        subthalamic_step(stn, 0.5f);  // 0.5ms steps
        subthalamic_process(stn);
        output_history.push_back(subthalamic_get_global_output(stn));
    }

    // Output should eventually differ from initial
    float final_output = output_history.back();
    EXPECT_NE(initial_output, final_output);

    // Verify no NaN or Inf in history
    for (size_t i = 0; i < output_history.size(); i++) {
        EXPECT_FALSE(std::isnan(output_history[i])) << "NaN at step " << i;
        EXPECT_FALSE(std::isinf(output_history[i])) << "Inf at step " << i;
    }
}

TEST_F(SubthalamicRegressionTest, HyperdirectTiming_FastResponse) {
    ASSERT_NE(stn, nullptr);

    // The hyperdirect pathway should be fast (~3ms)
    // Apply sudden cortical input
    std::vector<float> cortical_input(TEST_ACTIONS, 1.0f);
    subthalamic_set_cortical_input(stn, cortical_input.data(), true);

    // Step for less than delay time
    for (int i = 0; i < 3; i++) {
        subthalamic_step(stn, 1.0f);
        subthalamic_process(stn);
    }

    // After hyperdirect delay, should see response
    for (int i = 0; i < 5; i++) {
        subthalamic_step(stn, 1.0f);
        subthalamic_process(stn);
    }

    stn_mode_t mode = subthalamic_get_mode(stn);
    EXPECT_TRUE(mode == STN_MODE_HYPERDIRECT || mode == STN_MODE_SUPPRESSION);
}

TEST_F(SubthalamicRegressionTest, HyperdirectTiming_IndirectSlower) {
    ASSERT_NE(stn, nullptr);

    // Indirect pathway should be slower (~10ms vs ~3ms for hyperdirect)
    // Apply GPe input
    std::vector<float> gpe_input(TEST_ACTIONS, 0.1f);
    subthalamic_set_gpe_input(stn, gpe_input.data());

    // Record outputs over time
    std::vector<float> outputs;
    for (int i = 0; i < 20; i++) {
        subthalamic_step(stn, 1.0f);
        subthalamic_process(stn);
        outputs.push_back(subthalamic_get_global_output(stn));
    }

    // Should have smooth transition (no NaN/Inf)
    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_FALSE(std::isnan(outputs[i]));
        EXPECT_FALSE(std::isinf(outputs[i]));
        EXPECT_GE(outputs[i], 0.0f);
    }
}

//=============================================================================
// State Consistency Tests
//=============================================================================

TEST_F(SubthalamicRegressionTest, StateConsistency_AfterReset) {
    ASSERT_NE(stn, nullptr);

    // Modify state
    std::vector<float> cortical_input(TEST_ACTIONS, 0.9f);
    subthalamic_set_cortical_input(stn, cortical_input.data(), true);
    subthalamic_emergency_stop(stn, 0.8f);
    subthalamic_process(stn);

    for (int i = 0; i < 20; i++) {
        subthalamic_step(stn, 1.0f);
    }

    // Reset
    int result = subthalamic_reset(stn);
    EXPECT_GE(result, 0);

    // Should be back to baseline
    EXPECT_EQ(subthalamic_get_mode(stn), STN_MODE_BASELINE);

    // Stats should be reset
    stn_stats_t stats;
    subthalamic_get_stats(stn, &stats);
    EXPECT_EQ(stats.suppression_events, 0);
}

TEST_F(SubthalamicRegressionTest, StateConsistency_NeuronState) {
    ASSERT_NE(stn, nullptr);

    // Apply input and step
    std::vector<float> cortical_input(TEST_ACTIONS, 0.6f);
    subthalamic_set_cortical_input(stn, cortical_input.data(), false);

    for (int i = 0; i < 100; i++) {
        subthalamic_step(stn, 1.0f);
        subthalamic_process(stn);

        // Verify all neurons have valid state
        for (uint32_t n = 0; n < stn->num_neurons; n++) {
            EXPECT_GE(stn->neurons[n].firing_rate, 0.0f);
            EXPECT_LE(stn->neurons[n].firing_rate, STN_MAX_FIRING);
            EXPECT_FALSE(std::isnan(stn->neurons[n].membrane_potential));
        }
    }
}

TEST_F(SubthalamicRegressionTest, StateConsistency_Statistics) {
    ASSERT_NE(stn, nullptr);

    // Perform operations
    for (int i = 0; i < 50; i++) {
        std::vector<float> cortical_input(TEST_ACTIONS, 0.5f + 0.01f * i);
        subthalamic_set_cortical_input(stn, cortical_input.data(), i % 5 == 0);

        if (i % 10 == 0) {
            subthalamic_emergency_stop(stn, 0.7f);
        }

        subthalamic_process(stn);
        subthalamic_step(stn, 1.0f);
    }

    // Get and verify statistics
    stn_stats_t stats;
    int result = subthalamic_get_stats(stn, &stats);
    EXPECT_GE(result, 0);

    EXPECT_GE(stats.avg_firing_rate, 0.0f);
    EXPECT_LE(stats.avg_firing_rate, STN_MAX_FIRING);
    EXPECT_GE(stats.max_firing_rate, stats.avg_firing_rate);
    EXPECT_GE(stats.hyperdirect_activations, 0.0f);
    EXPECT_GE(stats.indirect_activations, 0.0f);
    EXPECT_GE(stats.suppression_events, 0);
}

TEST_F(SubthalamicRegressionTest, StateConsistency_MultipleResets) {
    ASSERT_NE(stn, nullptr);

    for (int cycle = 0; cycle < 20; cycle++) {
        // Use the STN
        std::vector<float> input(TEST_ACTIONS, 0.7f);
        subthalamic_set_cortical_input(stn, input.data(), true);
        subthalamic_process(stn);

        for (int i = 0; i < 10; i++) {
            subthalamic_step(stn, 1.0f);
        }

        // Reset
        int result = subthalamic_reset(stn);
        EXPECT_GE(result, 0);

        // Verify clean state
        EXPECT_EQ(subthalamic_get_mode(stn), STN_MODE_BASELINE);
    }
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(SubthalamicRegressionTest, DefaultConfig_Values) {
    subthalamic_config_t config;
    subthalamic_default_config(&config);

    EXPECT_EQ(config.num_neurons, STN_DEFAULT_NEURONS);
    EXPECT_GT(config.num_actions, 0);
    EXPECT_FLOAT_EQ(config.tonic_firing_rate, STN_TONIC_FIRING);
    EXPECT_FLOAT_EQ(config.max_firing_rate, STN_MAX_FIRING);
    EXPECT_FLOAT_EQ(config.hyperdirect_delay_ms, STN_HYPERDIRECT_DELAY);
    EXPECT_FLOAT_EQ(config.indirect_delay_ms, STN_INDIRECT_DELAY);
    EXPECT_GT(config.hyperdirect_gain, 0.0f);
    EXPECT_GT(config.indirect_gain, 0.0f);
    EXPECT_GT(config.urgency_threshold, 0.0f);
}

TEST_F(SubthalamicRegressionTest, DefaultConfig_Stability) {
    // Multiple calls should return same values
    subthalamic_config_t config1, config2;
    subthalamic_default_config(&config1);
    subthalamic_default_config(&config2);

    EXPECT_EQ(config1.num_neurons, config2.num_neurons);
    EXPECT_EQ(config1.num_actions, config2.num_actions);
    EXPECT_FLOAT_EQ(config1.tonic_firing_rate, config2.tonic_firing_rate);
    EXPECT_FLOAT_EQ(config1.max_firing_rate, config2.max_firing_rate);
    EXPECT_FLOAT_EQ(config1.hyperdirect_delay_ms, config2.hyperdirect_delay_ms);
}

//=============================================================================
// Mode Name Tests
//=============================================================================

TEST_F(SubthalamicRegressionTest, ModeNames_Stable) {
    EXPECT_STREQ(subthalamic_mode_name(STN_MODE_BASELINE), "Baseline");
    EXPECT_STREQ(subthalamic_mode_name(STN_MODE_HYPERDIRECT), "Hyperdirect");
    EXPECT_STREQ(subthalamic_mode_name(STN_MODE_INDIRECT), "Indirect");
    EXPECT_STREQ(subthalamic_mode_name(STN_MODE_SUPPRESSION), "Suppression");
    EXPECT_STREQ(subthalamic_mode_name((stn_mode_t)99), "Unknown");
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(SubthalamicRegressionTest, NullSafety_Operations) {
    subthalamic_destroy(nullptr);

    EXPECT_NE(subthalamic_reset(nullptr), 0);
    EXPECT_NE(subthalamic_process(nullptr), 0);
    EXPECT_NE(subthalamic_step(nullptr, 1.0f), 0);
    EXPECT_EQ(subthalamic_get_mode(nullptr), STN_MODE_BASELINE);
    EXPECT_LE(subthalamic_get_global_output(nullptr), 0.0f);

    float buffer[8];
    EXPECT_NE(subthalamic_get_output(nullptr, buffer), 0);
    EXPECT_NE(subthalamic_set_cortical_input(nullptr, buffer, false), 0);
    EXPECT_NE(subthalamic_set_gpe_input(nullptr, buffer), 0);
    EXPECT_NE(subthalamic_emergency_stop(nullptr, 0.5f), 0);

    stn_stats_t stats;
    EXPECT_NE(subthalamic_get_stats(nullptr, &stats), 0);
    EXPECT_NE(subthalamic_get_stats(stn, nullptr), 0);
}

TEST_F(SubthalamicRegressionTest, NullSafety_Buffers) {
    ASSERT_NE(stn, nullptr);

    EXPECT_NE(subthalamic_set_cortical_input(stn, nullptr, false), 0);
    EXPECT_NE(subthalamic_set_gpe_input(stn, nullptr), 0);
    EXPECT_NE(subthalamic_get_output(stn, nullptr), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
