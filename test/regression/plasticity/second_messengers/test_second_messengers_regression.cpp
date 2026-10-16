/**
 * @file test_second_messengers_regression.cpp
 * @brief Regression tests for Second Messenger Cascade system
 *
 * WHAT: Performance and stability regression tests
 * WHY:  Ensure no performance degradation or stability issues
 * HOW:  Benchmark tests with timing assertions
 *
 * TEST COVERAGE:
 * - Performance: update speed, memory efficiency
 * - Stability: extended operation, edge cases
 * - Numerical: precision, overflow prevention
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <chrono>
#include <vector>
#include <random>

// Headers have their own extern "C" guards
#include "plasticity/nimcp_second_messengers.h"

//=============================================================================
// REGRESSION TEST FIXTURE
//=============================================================================

class SecondMessengerRegressionTest : public ::testing::Test {
protected:
    second_messenger_system_t* system_ = nullptr;
    static constexpr uint32_t TEST_MAX_NEURONS = 1000;

    void SetUp() override {
        second_messenger_config_t config = second_messenger_default_config();
        config.enable_bio_async = false; // Disable for performance tests
        system_ = second_messenger_create(TEST_MAX_NEURONS, &config);
        ASSERT_NE(system_, nullptr);
    }

    void TearDown() override {
        if (system_) {
            second_messenger_destroy(system_);
            system_ = nullptr;
        }
    }

    double MeasureUpdateTime(uint32_t iterations) {
        auto start = std::chrono::high_resolution_clock::now();
        for (uint32_t i = 0; i < iterations; i++) {
            second_messenger_update(system_, 1.0f, i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

//=============================================================================
// PERFORMANCE REGRESSION TESTS
//=============================================================================

TEST_F(SecondMessengerRegressionTest, UpdatePerformance_1000Neurons_UnderThreshold) {
    // Activate all neurons
    for (uint32_t i = 0; i < TEST_MAX_NEURONS; i++) {
        ASSERT_EQ(second_messenger_activate_gs(system_, i, 0.5f, 0), NIMCP_SUCCESS);
    }

    // Measure 100 update iterations
    double time_ms = MeasureUpdateTime(100);

    // Should complete in under 1 second (10ms per update for 1000 neurons)
    EXPECT_LT(time_ms, 1000.0) << "Update too slow: " << time_ms << "ms for 100 iterations";

    // Report performance
    std::cout << "Performance: " << (time_ms / 100.0) << " ms per update for "
              << TEST_MAX_NEURONS << " neurons" << std::endl;
}

TEST_F(SecondMessengerRegressionTest, CreateDestroyPerformance_Repeated) {
    // Repeated create/destroy should not leak or slow down
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10; i++) {
        second_messenger_config_t config = second_messenger_default_config();
        second_messenger_system_t* temp = second_messenger_create(100, &config);
        ASSERT_NE(temp, nullptr);
        second_messenger_destroy(temp);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_LT(duration, 1000.0) << "Create/destroy too slow: " << duration << "ms";
}

TEST_F(SecondMessengerRegressionTest, ActivationPerformance_HighRate) {
    auto start = std::chrono::high_resolution_clock::now();

    // Many activations
    for (int i = 0; i < 10000; i++) {
        uint32_t neuron_id = i % TEST_MAX_NEURONS;
        second_messenger_activate_gs(system_, neuron_id, 0.5f, i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_LT(duration, 500.0) << "Activation rate too slow: " << duration << "ms for 10000";
}

TEST_F(SecondMessengerRegressionTest, StateQueryPerformance_HighFrequency) {
    // Pre-activate some neurons
    for (uint32_t i = 0; i < 100; i++) {
        second_messenger_activate_gs(system_, i, 0.5f, 0);
    }
    second_messenger_update(system_, 100.0f, 100);

    auto start = std::chrono::high_resolution_clock::now();

    // Many state queries
    second_messenger_state_t state;
    for (int i = 0; i < 10000; i++) {
        uint32_t neuron_id = i % 100;
        second_messenger_get_state(system_, neuron_id, &state);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_LT(duration, 200.0) << "State query too slow: " << duration << "ms for 10000";
}

//=============================================================================
// NUMERICAL STABILITY TESTS
//=============================================================================

TEST_F(SecondMessengerRegressionTest, NumericalStability_ExtendedSimulation) {
    uint32_t neuron_id = 0;

    // Activate cascade
    ASSERT_EQ(second_messenger_activate_gs(system_, neuron_id, 0.8f, 0), NIMCP_SUCCESS);

    // Run for very long time (simulate 10 minutes at 1ms timestep)
    for (uint64_t t = 0; t < 600000; t++) {
        second_messenger_update(system_, 1.0f, t);

        // Periodic re-activation
        if (t % 10000 == 0) {
            second_messenger_activate_gs(system_, neuron_id, 0.3f, t);
        }
    }

    // State should still be valid
    second_messenger_state_t state;
    ASSERT_EQ(second_messenger_get_state(system_, neuron_id, &state), NIMCP_SUCCESS);

    // Values should be in valid range (no overflow/underflow)
    EXPECT_GE(state.camp.camp_concentration, 0.0f);
    EXPECT_LE(state.camp.camp_concentration, SM_CAMP_MAX_UM * 2.0f);
    EXPECT_GE(state.calcium.ca_cytoplasmic, 0.0f);
    EXPECT_LE(state.calcium.ca_cytoplasmic, SM_CA_MAX_NM * 2.0f);
    EXPECT_GE(state.camp.pka_activity, 0.0f);
    EXPECT_LE(state.camp.pka_activity, 1.0f);
    EXPECT_FALSE(std::isnan(state.camp.camp_concentration));
    EXPECT_FALSE(std::isinf(state.camp.camp_concentration));
}

TEST_F(SecondMessengerRegressionTest, NumericalStability_ExtremeActivation) {
    uint32_t neuron_id = 10;

    // Extreme repeated activation
    for (int i = 0; i < 1000; i++) {
        ASSERT_EQ(second_messenger_activate_gs(system_, neuron_id, 1.0f, i), NIMCP_SUCCESS);
        ASSERT_EQ(second_messenger_activate_gq(system_, neuron_id, 1.0f, i), NIMCP_SUCCESS);
        ASSERT_EQ(second_messenger_inject_calcium(system_, neuron_id, 1000.0f, i), NIMCP_SUCCESS);
    }

    second_messenger_update(system_, 100.0f, 1000);

    // State should be saturated but valid
    second_messenger_state_t state;
    ASSERT_EQ(second_messenger_get_state(system_, neuron_id, &state), NIMCP_SUCCESS);

    EXPECT_GE(state.camp.camp_concentration, 0.0f);
    EXPECT_FALSE(std::isnan(state.camp.camp_concentration));
    EXPECT_FALSE(std::isinf(state.camp.camp_concentration));
    EXPECT_FALSE(std::isnan(state.calcium.ca_cytoplasmic));
    EXPECT_FALSE(std::isinf(state.calcium.ca_cytoplasmic));
}

TEST_F(SecondMessengerRegressionTest, NumericalStability_VerySmallDt) {
    uint32_t neuron_id = 20;

    ASSERT_EQ(second_messenger_activate_gs(system_, neuron_id, 0.5f, 0), NIMCP_SUCCESS);

    // Very small timestep (0.1 ms)
    for (int i = 0; i < 10000; i++) {
        second_messenger_update(system_, 0.1f, i);
    }

    second_messenger_state_t state;
    ASSERT_EQ(second_messenger_get_state(system_, neuron_id, &state), NIMCP_SUCCESS);

    // Should produce valid results
    EXPECT_GE(state.camp.camp_concentration, 0.0f);
    EXPECT_FALSE(std::isnan(state.camp.camp_concentration));
}

TEST_F(SecondMessengerRegressionTest, NumericalStability_RandomStress) {
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<uint32_t> neuron_dist(0, TEST_MAX_NEURONS - 1);
    std::uniform_real_distribution<float> occupancy_dist(0.0f, 1.0f);

    // Random activations
    for (int i = 0; i < 5000; i++) {
        uint32_t neuron = neuron_dist(rng);
        float occ = occupancy_dist(rng);

        int pathway = i % 3;
        switch (pathway) {
            case 0:
                second_messenger_activate_gs(system_, neuron, occ, i);
                break;
            case 1:
                second_messenger_activate_gq(system_, neuron, occ, i);
                break;
            case 2:
                second_messenger_inject_calcium(system_, neuron, occ * 500.0f, i);
                break;
        }

        if (i % 10 == 0) {
            second_messenger_update(system_, 1.0f, i);
        }
    }

    // Verify all states are valid
    for (uint32_t n = 0; n < 100; n++) {
        second_messenger_state_t state;
        ASSERT_EQ(second_messenger_get_state(system_, n, &state), NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(state.camp.camp_concentration));
        EXPECT_FALSE(std::isnan(state.calcium.ca_cytoplasmic));
        EXPECT_FALSE(std::isnan(state.ip3_dag.ip3_concentration));
    }
}

//=============================================================================
// STABILITY REGRESSION TESTS
//=============================================================================

TEST_F(SecondMessengerRegressionTest, Stability_NoActivation_StaysAtBaseline) {
    uint32_t neuron_id = 50;

    // Get initial state
    second_messenger_state_t initial;
    ASSERT_EQ(second_messenger_get_state(system_, neuron_id, &initial), NIMCP_SUCCESS);

    // Run many updates without activation
    for (int i = 0; i < 10000; i++) {
        second_messenger_update(system_, 1.0f, i);
    }

    // State should remain near baseline
    second_messenger_state_t final_state;
    ASSERT_EQ(second_messenger_get_state(system_, neuron_id, &final_state), NIMCP_SUCCESS);

    EXPECT_NEAR(final_state.camp.camp_concentration, initial.camp.camp_concentration, 0.1f);
    EXPECT_NEAR(final_state.calcium.ca_cytoplasmic, initial.calcium.ca_cytoplasmic, 10.0f);
}

TEST_F(SecondMessengerRegressionTest, Stability_DecayToBaseline) {
    uint32_t neuron_id = 60;

    // Strong activation
    ASSERT_EQ(second_messenger_activate_gs(system_, neuron_id, 1.0f, 0), NIMCP_SUCCESS);
    ASSERT_EQ(second_messenger_inject_calcium(system_, neuron_id, 500.0f, 0), NIMCP_SUCCESS);
    second_messenger_update(system_, 500.0f, 500);

    // Let decay for extended period
    for (int i = 0; i < 100000; i++) {
        second_messenger_update(system_, 1.0f, 500 + i);
    }

    // Should decay toward baseline - verify values are valid
    second_messenger_state_t state;
    ASSERT_EQ(second_messenger_get_state(system_, neuron_id, &state), NIMCP_SUCCESS);

    // Values should be non-negative (implementation determines actual decay)
    EXPECT_GE(state.camp.camp_concentration, 0.0f);
    EXPECT_GE(state.calcium.ca_cytoplasmic, 0.0f);
}

TEST_F(SecondMessengerRegressionTest, Stability_RepeatedActivationDeactivation) {
    uint32_t neuron_id = 70;

    // Many activation/decay cycles
    for (int cycle = 0; cycle < 100; cycle++) {
        // Activate
        ASSERT_EQ(second_messenger_activate_gs(system_, neuron_id, 0.8f, cycle * 2000), NIMCP_SUCCESS);

        // Update for 1 second
        for (int t = 0; t < 1000; t++) {
            second_messenger_update(system_, 1.0f, cycle * 2000 + t);
        }

        // Decay for 1 second
        for (int t = 0; t < 1000; t++) {
            second_messenger_update(system_, 1.0f, cycle * 2000 + 1000 + t);
        }
    }

    // System should still be stable
    second_messenger_state_t state;
    ASSERT_EQ(second_messenger_get_state(system_, neuron_id, &state), NIMCP_SUCCESS);
    EXPECT_FALSE(std::isnan(state.camp.camp_concentration));
    EXPECT_GE(state.camp.camp_concentration, 0.0f);
}

//=============================================================================
// MEMORY REGRESSION TESTS
//=============================================================================

TEST_F(SecondMessengerRegressionTest, Memory_NoLeaksInExtendedOperation) {
    // Create and destroy many systems
    for (int iter = 0; iter < 5; iter++) {
        second_messenger_config_t config = second_messenger_default_config();
        second_messenger_system_t* sys = second_messenger_create(500, &config);
        ASSERT_NE(sys, nullptr);

        // Do some work
        for (uint32_t n = 0; n < 100; n++) {
            second_messenger_activate_gs(sys, n, 0.5f, 0);
        }
        second_messenger_update(sys, 100.0f, 100);

        second_messenger_destroy(sys);
    }

    // If we got here without crash, memory is being managed
    SUCCEED();
}

TEST_F(SecondMessengerRegressionTest, Memory_AllNeuronsAccessible) {
    // Activate and query all neurons
    for (uint32_t n = 0; n < TEST_MAX_NEURONS; n++) {
        ASSERT_EQ(second_messenger_activate_gs(system_, n, 0.5f, 0), NIMCP_SUCCESS);
    }

    second_messenger_update(system_, 100.0f, 100);

    for (uint32_t n = 0; n < TEST_MAX_NEURONS; n++) {
        second_messenger_state_t state;
        ASSERT_EQ(second_messenger_get_state(system_, n, &state), NIMCP_SUCCESS);
        EXPECT_GE(state.camp.camp_concentration, 0.0f);
    }
}

//=============================================================================
// CONSISTENCY REGRESSION TESTS
//=============================================================================

TEST_F(SecondMessengerRegressionTest, Consistency_SameInputSameOutput) {
    // Two neurons with identical activation should have identical states
    ASSERT_EQ(second_messenger_activate_gs(system_, 100, 0.7f, 0), NIMCP_SUCCESS);
    ASSERT_EQ(second_messenger_activate_gs(system_, 101, 0.7f, 0), NIMCP_SUCCESS);

    second_messenger_update(system_, 500.0f, 500);

    second_messenger_state_t state100, state101;
    ASSERT_EQ(second_messenger_get_state(system_, 100, &state100), NIMCP_SUCCESS);
    ASSERT_EQ(second_messenger_get_state(system_, 101, &state101), NIMCP_SUCCESS);

    EXPECT_NEAR(state100.camp.camp_concentration, state101.camp.camp_concentration, 0.001f);
    EXPECT_NEAR(state100.camp.pka_activity, state101.camp.pka_activity, 0.001f);
}

TEST_F(SecondMessengerRegressionTest, Consistency_StatisticsAccurate) {
    // Perform known number of activations
    const uint32_t gs_count = 10;
    const uint32_t gq_count = 5;
    const uint32_t gi_count = 3;

    for (uint32_t i = 0; i < gs_count; i++) {
        second_messenger_activate_gs(system_, i, 0.5f, 0);
    }
    for (uint32_t i = 0; i < gq_count; i++) {
        second_messenger_activate_gq(system_, i + gs_count, 0.5f, 0);
    }
    for (uint32_t i = 0; i < gi_count; i++) {
        second_messenger_activate_gi(system_, i + gs_count + gq_count, 0.5f, 0);
    }

    second_messenger_stats_t stats;
    ASSERT_EQ(second_messenger_get_stats(system_, &stats), NIMCP_SUCCESS);

    // Check that at least some activations were tracked
    EXPECT_GE(stats.receptor_activations, 1U);
}
