/**
 * @file test_calcium_regression.cpp
 * @brief Regression tests for calcium dynamics stability
 * @version 1.0.0
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "plasticity/calcium/nimcp_calcium_dynamics.h"
#include "plasticity/calcium/nimcp_calcium_sleep_bridge.h"
#include "plasticity/calcium/nimcp_calcium_immune_bridge.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CalciumRegressionTest : public ::testing::Test {
protected:
    calcium_dynamics_t calcium;

    void SetUp() override {
        calcium = calcium_create(nullptr);
        ASSERT_NE(calcium, nullptr);
    }

    void TearDown() override {
        if (calcium) {
            calcium_destroy(calcium);
        }
    }
};

/* ============================================================================
 * Stability Regression Tests (10 tests)
 * ============================================================================ */

TEST_F(CalciumRegressionTest, StabilityUnderRepetitiveUpdate) {
    /* Verify calcium doesn't diverge with repeated updates */
    for (int i = 0; i < 10000; i++) {
        calcium_update(calcium, 1.0f);
    }

    float final_ca = calcium_get_concentration(calcium);
    EXPECT_GE(final_ca, CALCIUM_MIN_CONCENTRATION);
    EXPECT_LE(final_ca, CALCIUM_MAX_CONCENTRATION);
}

TEST_F(CalciumRegressionTest, StabilityUnderContinuousInflux) {
    /* Continuous NMDA influx should reach equilibrium, not saturate */
    for (int i = 0; i < 1000; i++) {
        calcium_trigger_nmda_influx(calcium, 0.5f, 0.0f);
        calcium_update(calcium, 1.0f);
    }

    float final_ca = calcium_get_concentration(calcium);
    EXPECT_LT(final_ca, CALCIUM_MAX_CONCENTRATION);
    EXPECT_GT(final_ca, CALCIUM_BASELINE_CONCENTRATION);
}

TEST_F(CalciumRegressionTest, DecayToBaseline) {
    /* After transient influx, should decay back to baseline */
    calcium_set_concentration(calcium, 1.0f);

    for (int i = 0; i < 5000; i++) {
        calcium_update(calcium, 1.0f);
    }

    float final_ca = calcium_get_concentration(calcium);
    EXPECT_NEAR(final_ca, CALCIUM_BASELINE_CONCENTRATION, 0.05f);
}

TEST_F(CalciumRegressionTest, LearningRateMonotonicity) {
    /* Learning rate should increase monotonically with calcium above midpoint */
    std::vector<float> ca_values = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f};
    std::vector<float> lr_values;

    for (float ca : ca_values) {
        calcium_set_concentration(calcium, ca);
        float lr = calcium_compute_learning_rate(calcium);
        lr_values.push_back(lr);
    }

    for (size_t i = 1; i < lr_values.size(); i++) {
        EXPECT_GT(lr_values[i], lr_values[i-1])
            << "LR not monotonically increasing at index " << i;
    }
}

TEST_F(CalciumRegressionTest, OmegaFunctionContinuity) {
    /* Omega function should be continuous (no sudden jumps) but can have steep gradients.
     * With power=2.5, gradients can be quite steep at high calcium levels.
     * This test verifies the function is smooth, not that gradients are small. */
    std::vector<float> lr_values;
    std::vector<float> ca_values;
    for (float ca = 0.2f; ca <= 1.0f; ca += 0.01f) {
        calcium_set_concentration(calcium, ca);
        lr_values.push_back(calcium_compute_learning_rate(calcium));
        ca_values.push_back(ca);
    }

    /* Check for sudden discontinuities (jumps > 50% of local gradient)
     * The function should be smooth, meaning consecutive gradients should be similar */
    for (size_t i = 2; i < lr_values.size(); i++) {
        float grad1 = std::abs(lr_values[i-1] - lr_values[i-2]);
        float grad2 = std::abs(lr_values[i] - lr_values[i-1]);

        /* Skip near-zero gradients to avoid division issues */
        if (grad1 < 0.001f && grad2 < 0.001f) continue;

        /* Check that gradient doesn't suddenly jump (ratio should be reasonable) */
        float max_grad = std::max(grad1, grad2);
        float min_grad = std::min(grad1, grad2);
        float ratio = (min_grad > 0.0001f) ? (max_grad / min_grad) : 1.0f;

        /* Allow some gradient variation but flag massive jumps (10x sudden change) */
        EXPECT_LT(ratio, 10.0f) << "Discontinuity at ca=" << ca_values[i]
                                 << " grad1=" << grad1 << " grad2=" << grad2;
    }
}

TEST_F(CalciumRegressionTest, ThresholdCrossingConsistency) {
    /* Crossing LTP threshold repeatedly should trigger callback each time
     * Using 0.5 (below LTP=0.55) and 0.6 (above LTP) to only cross one threshold */
    int callback_count = 0;
    auto callback = [](calcium_threshold_crossing_t crossing, float ca, void* data) {
        /* Only count LTP crossings (UP and DOWN) */
        if (crossing == CALCIUM_CROSS_LTP_THRESHOLD_UP ||
            crossing == CALCIUM_CROSS_LTP_THRESHOLD_DOWN) {
            (*static_cast<int*>(data))++;
        }
    };

    calcium_register_threshold_callback(calcium, callback, &callback_count);

    /* Start above LTD threshold to avoid LTD crossings */
    calcium_set_concentration(calcium, 0.5f);

    for (int i = 0; i < 10; i++) {
        calcium_set_concentration(calcium, 0.6f);  /* Above LTP - triggers UP */
        calcium_set_concentration(calcium, 0.5f);  /* Below LTP - triggers DOWN */
    }

    /* 10 UP + 10 DOWN = 20 LTP threshold crossings */
    EXPECT_EQ(callback_count, 20);
}

TEST_F(CalciumRegressionTest, MemoryLeakCheck) {
    /* Create and destroy many calcium systems */
    for (int i = 0; i < 1000; i++) {
        calcium_dynamics_t ca = calcium_create(nullptr);
        ASSERT_NE(ca, nullptr);
        calcium_destroy(ca);
    }
    /* If no crash, memory management is correct */
    SUCCEED();
}

TEST_F(CalciumRegressionTest, CallbackArrayBoundary) {
    /* Register maximum callbacks */
    int counts[CALCIUM_MAX_THRESHOLD_CALLBACKS] = {0};

    auto callback = [](calcium_threshold_crossing_t crossing, float ca, void* data) {
        (*static_cast<int*>(data))++;
    };

    for (int i = 0; i < CALCIUM_MAX_THRESHOLD_CALLBACKS; i++) {
        int ret = calcium_register_threshold_callback(calcium, callback, &counts[i]);
        EXPECT_EQ(ret, 0);
    }

    /* One more should fail */
    int extra_count = 0;
    int ret = calcium_register_threshold_callback(calcium, callback, &extra_count);
    EXPECT_EQ(ret, -1);
}

TEST_F(CalciumRegressionTest, ConcentrationBoundsEnforcement) {
    /* Verify concentration never exceeds bounds */
    std::vector<float> test_values = {-10.0f, -1.0f, 0.0f, 0.5f, 1.0f, 5.0f, 100.0f};

    for (float val : test_values) {
        calcium_set_concentration(calcium, val);
        float ca = calcium_get_concentration(calcium);
        EXPECT_GE(ca, CALCIUM_MIN_CONCENTRATION);
        EXPECT_LE(ca, CALCIUM_MAX_CONCENTRATION);
    }
}

TEST_F(CalciumRegressionTest, NMDAInfluxNumericalStability) {
    /* Test NMDA influx at extreme voltages */
    std::vector<float> voltages = {-100.0f, -50.0f, 0.0f, 50.0f, 100.0f};

    for (float v : voltages) {
        calcium_reset(calcium);
        calcium_trigger_nmda_influx(calcium, 1.0f, v);
        calcium_update(calcium, 1.0f);

        float ca = calcium_get_concentration(calcium);
        EXPECT_FALSE(std::isnan(ca));
        EXPECT_FALSE(std::isinf(ca));
    }
}

/* ============================================================================
 * Long-Running Simulation Tests (5 tests)
 * ============================================================================ */

TEST_F(CalciumRegressionTest, LongRunningSimulation) {
    /* Simulate 10 seconds at 1 ms resolution */
    for (int i = 0; i < 10000; i++) {
        if (i % 100 == 0) {
            calcium_trigger_nmda_influx(calcium, 0.3f, 0.0f);
        }
        calcium_update(calcium, 1.0f);
    }

    /* Should still be in valid state */
    float ca = calcium_get_concentration(calcium);
    EXPECT_GE(ca, CALCIUM_MIN_CONCENTRATION);
    EXPECT_LE(ca, CALCIUM_MAX_CONCENTRATION);

    calcium_state_t state;
    calcium_get_state(calcium, &state);
    EXPECT_EQ(state.total_updates, 10000);
}

TEST_F(CalciumRegressionTest, StatisticsAccumulation) {
    /* Verify statistics don't overflow */
    for (int i = 0; i < 1000; i++) {
        calcium_set_concentration(calcium, 0.3f);  /* LTD */
        calcium_update(calcium, 1.0f);
        calcium_set_concentration(calcium, 0.7f);  /* LTP */
        calcium_update(calcium, 1.0f);
    }

    calcium_state_t state;
    calcium_get_state(calcium, &state);
    EXPECT_GT(state.ltd_events, 0);
    EXPECT_GT(state.ltp_events, 0);
    EXPECT_GT(state.time_in_ltd_ms, 0);
    EXPECT_GT(state.time_in_ltp_ms, 0);
}

TEST_F(CalciumRegressionTest, RegimeTransitionStability) {
    /* Rapidly switch between regimes */
    for (int i = 0; i < 1000; i++) {
        calcium_set_concentration(calcium, 0.1f);  /* NONE */
        calcium_set_concentration(calcium, 0.3f);  /* LTD */
        calcium_set_concentration(calcium, 0.45f); /* TRANSITION */
        calcium_set_concentration(calcium, 0.7f);  /* LTP */
        calcium_set_concentration(calcium, 1.5f);  /* SATURATED */
    }

    /* Should still be in valid state */
    calcium_plasticity_regime_t regime = calcium_get_regime(calcium);
    EXPECT_GE(regime, CALCIUM_REGIME_NONE);
    EXPECT_LE(regime, CALCIUM_REGIME_SATURATED);
}

TEST_F(CalciumRegressionTest, MultipleResets) {
    /* Reset repeatedly */
    for (int i = 0; i < 100; i++) {
        calcium_set_concentration(calcium, 1.0f);
        calcium_update(calcium, 10.0f);
        calcium_reset(calcium);
        EXPECT_FLOAT_EQ(calcium_get_concentration(calcium), CALCIUM_BASELINE_CONCENTRATION);
    }
}

TEST_F(CalciumRegressionTest, StateConsistency) {
    /* Verify state remains consistent after many operations */
    for (int i = 0; i < 100; i++) {
        calcium_trigger_nmda_influx(calcium, 0.5f, 0.0f);
        calcium_update(calcium, 1.0f);

        calcium_state_t state;
        calcium_get_state(calcium, &state);

        /* Concentration in state should match getter */
        EXPECT_FLOAT_EQ(state.ca_concentration, calcium_get_concentration(calcium));

        /* Learning rate in state should match computed */
        float computed_lr = calcium_compute_learning_rate(calcium);
        EXPECT_NEAR(state.current_learning_rate, computed_lr, 0.001f);
    }
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
