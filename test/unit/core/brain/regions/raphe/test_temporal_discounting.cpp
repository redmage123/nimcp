/**
 * @file test_temporal_discounting.cpp
 * @brief Unit tests for temporal discounting system
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "core/brain/regions/raphe/nimcp_temporal_discounting.h"
}

class TemporalDiscountingTest : public ::testing::Test {
protected:
    nimcp_temporal_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
    }

    void TearDown() override {
        if (system.initialized) {
            nimcp_temporal_shutdown(&system);
        }
    }

    /* Helper to compute expected hyperbolic discount */
    float hyperbolic(float amount, float delay, float k) {
        if (delay <= 0.0f) return amount;
        return amount / (1.0f + k * delay);
    }
};

/* ==========================================================================
 * Lifecycle Tests
 * ========================================================================== */

TEST_F(TemporalDiscountingTest, DefaultConfigHasValidValues) {
    nimcp_temporal_config_t config = nimcp_temporal_default_config();

    EXPECT_FLOAT_EQ(config.baseline_k, TEMPORAL_DEFAULT_K);
    EXPECT_FLOAT_EQ(config.baseline_orientation, TEMPORAL_DEFAULT_ORIENTATION);
    EXPECT_GT(config.ht_discount_gain, 0.0f);
    EXPECT_GT(config.min_k, 0.0f);
    EXPECT_GT(config.max_k, config.min_k);
}

TEST_F(TemporalDiscountingTest, InitWithNullReturnsError) {
    EXPECT_EQ(nimcp_temporal_init(nullptr, nullptr), -1);
}

TEST_F(TemporalDiscountingTest, InitWithDefaultConfigSucceeds) {
    EXPECT_EQ(nimcp_temporal_init(&system, nullptr), 0);
    EXPECT_TRUE(system.initialized);
}

TEST_F(TemporalDiscountingTest, InitWithCustomConfigSucceeds) {
    nimcp_temporal_config_t config = nimcp_temporal_default_config();
    config.baseline_k = 0.2f;

    EXPECT_EQ(nimcp_temporal_init(&system, &config), 0);
    EXPECT_FLOAT_EQ(system.config.baseline_k, 0.2f);
}

TEST_F(TemporalDiscountingTest, InitSetsCorrectInitialState) {
    nimcp_temporal_init(&system, nullptr);

    EXPECT_FLOAT_EQ(system.discount_rate, TEMPORAL_DEFAULT_K);
    EXPECT_FLOAT_EQ(system.future_orientation, TEMPORAL_DEFAULT_ORIENTATION);
}

TEST_F(TemporalDiscountingTest, ShutdownSucceeds) {
    nimcp_temporal_init(&system, nullptr);
    EXPECT_EQ(nimcp_temporal_shutdown(&system), 0);
    EXPECT_FALSE(system.initialized);
}

TEST_F(TemporalDiscountingTest, ResetRestoresInitialState) {
    nimcp_temporal_init(&system, nullptr);

    system.discount_rate = 0.5f;
    system.future_orientation = 0.9f;

    EXPECT_EQ(nimcp_temporal_reset(&system), 0);
    EXPECT_FLOAT_EQ(system.discount_rate, TEMPORAL_DEFAULT_K);
}

/* ==========================================================================
 * Update Tests
 * ========================================================================== */

TEST_F(TemporalDiscountingTest, UpdateWithNullReturnsError) {
    EXPECT_EQ(nimcp_temporal_update(nullptr, 20.0f, 10.0f), -1);
}

TEST_F(TemporalDiscountingTest, UpdateWithoutInitReturnsError) {
    EXPECT_EQ(nimcp_temporal_update(&system, 20.0f, 10.0f), -1);
}

TEST_F(TemporalDiscountingTest, UpdateWithBaseline5HTMaintainsBaselineK) {
    nimcp_temporal_init(&system, nullptr);

    for (int i = 0; i < 100; i++) {
        nimcp_temporal_update(&system, 20.0f, 100.0f);  /* Baseline 5-HT */
    }

    EXPECT_NEAR(system.discount_rate, TEMPORAL_DEFAULT_K, 0.02f);
}

TEST_F(TemporalDiscountingTest, UpdateWithHigh5HTDecreasesK) {
    nimcp_temporal_init(&system, nullptr);

    /* High 5-HT = more patient = lower k */
    for (int i = 0; i < 100; i++) {
        nimcp_temporal_update(&system, 40.0f, 100.0f);
    }

    EXPECT_LT(system.discount_rate, TEMPORAL_DEFAULT_K);
}

TEST_F(TemporalDiscountingTest, UpdateWithLow5HTIncreasesK) {
    nimcp_temporal_init(&system, nullptr);

    /* Low 5-HT = more impulsive = higher k */
    for (int i = 0; i < 100; i++) {
        nimcp_temporal_update(&system, 10.0f, 100.0f);
    }

    EXPECT_GT(system.discount_rate, TEMPORAL_DEFAULT_K);
}

TEST_F(TemporalDiscountingTest, UpdateClampsKToValidRange) {
    nimcp_temporal_init(&system, nullptr);

    /* Extreme 5-HT values */
    for (int i = 0; i < 200; i++) {
        nimcp_temporal_update(&system, 100.0f, 100.0f);  /* Very high */
    }

    EXPECT_GE(system.discount_rate, system.config.min_k);

    for (int i = 0; i < 200; i++) {
        nimcp_temporal_update(&system, 1.0f, 100.0f);  /* Very low */
    }

    EXPECT_LE(system.discount_rate, system.config.max_k);
}

/* ==========================================================================
 * Discounting API Tests
 * ========================================================================== */

TEST_F(TemporalDiscountingTest, DiscountValueWithNullReturnsError) {
    nimcp_temporal_init(&system, nullptr);

    float discounted;
    EXPECT_EQ(nimcp_temporal_discount_value(nullptr, 100.0f, 1000.0f, &discounted), -1);
    EXPECT_EQ(nimcp_temporal_discount_value(&system, 100.0f, 1000.0f, nullptr), -1);
}

TEST_F(TemporalDiscountingTest, DiscountValueWithNegativeValueReturnsError) {
    nimcp_temporal_init(&system, nullptr);

    float discounted;
    EXPECT_EQ(nimcp_temporal_discount_value(&system, -100.0f, 1000.0f, &discounted), -1);
}

TEST_F(TemporalDiscountingTest, DiscountValueWithZeroDelayEqualsAmount) {
    nimcp_temporal_init(&system, nullptr);

    float discounted;
    nimcp_temporal_discount_value(&system, 100.0f, 0.0f, &discounted);

    EXPECT_FLOAT_EQ(discounted, 100.0f);
}

TEST_F(TemporalDiscountingTest, DiscountValueDecreasesWithDelay) {
    nimcp_temporal_init(&system, nullptr);

    float value1, value2, value3;
    nimcp_temporal_discount_value(&system, 100.0f, 100.0f, &value1);
    nimcp_temporal_discount_value(&system, 100.0f, 500.0f, &value2);
    nimcp_temporal_discount_value(&system, 100.0f, 1000.0f, &value3);

    EXPECT_GT(value1, value2);
    EXPECT_GT(value2, value3);
}

TEST_F(TemporalDiscountingTest, DiscountValueMatchesHyperbolicFormula) {
    nimcp_temporal_init(&system, nullptr);

    float k = system.discount_rate;
    float amount = 100.0f;
    float delay = 1000.0f;

    float expected = hyperbolic(amount, delay, k);

    float actual;
    nimcp_temporal_discount_value(&system, amount, delay, &actual);

    EXPECT_NEAR(actual, expected, 0.01f);
}

TEST_F(TemporalDiscountingTest, GetCurrentKSucceeds) {
    nimcp_temporal_init(&system, nullptr);

    float k;
    EXPECT_EQ(nimcp_temporal_get_current_k(&system, &k), 0);
    EXPECT_FLOAT_EQ(k, TEMPORAL_DEFAULT_K);
}

/* ==========================================================================
 * Choice Evaluation Tests
 * ========================================================================== */

TEST_F(TemporalDiscountingTest, EvaluateChoiceWithNullReturnsError) {
    nimcp_temporal_init(&system, nullptr);

    nimcp_temporal_choice_t result;
    EXPECT_EQ(nimcp_temporal_evaluate_choice(nullptr, 50.0f, 100.0f, 1000.0f, &result), -1);
    EXPECT_EQ(nimcp_temporal_evaluate_choice(&system, 50.0f, 100.0f, 1000.0f, nullptr), -1);
}

TEST_F(TemporalDiscountingTest, EvaluateChoicePrefersImmediateWhenDiscountedLower) {
    nimcp_temporal_init(&system, nullptr);

    /* High k = strong discounting */
    system.discount_rate = 0.5f;

    nimcp_temporal_choice_t result;
    nimcp_temporal_evaluate_choice(&system, 50.0f, 60.0f, 1000.0f, &result);

    /* 60 / (1 + 0.5 * 1000) = 60/501 ≈ 0.12 < 50 */
    EXPECT_FALSE(result.prefer_delayed);
}

TEST_F(TemporalDiscountingTest, EvaluateChoicePrefersDelayedWhenValueHigher) {
    nimcp_temporal_init(&system, nullptr);

    /* Low k = little discounting */
    system.discount_rate = 0.001f;

    nimcp_temporal_choice_t result;
    nimcp_temporal_evaluate_choice(&system, 50.0f, 100.0f, 100.0f, &result);

    /* 100 / (1 + 0.001 * 100) = 100/1.1 ≈ 90.9 > 50 */
    EXPECT_TRUE(result.prefer_delayed);
}

TEST_F(TemporalDiscountingTest, EvaluateChoiceReturnsDiscountedValue) {
    nimcp_temporal_init(&system, nullptr);

    nimcp_temporal_choice_t result;
    nimcp_temporal_evaluate_choice(&system, 50.0f, 100.0f, 1000.0f, &result);

    float expected = hyperbolic(100.0f, 1000.0f, system.discount_rate);
    EXPECT_NEAR(result.discounted_value, expected, 0.01f);
}

TEST_F(TemporalDiscountingTest, EvaluateChoiceReturnsEffectiveK) {
    nimcp_temporal_init(&system, nullptr);

    nimcp_temporal_choice_t result;
    nimcp_temporal_evaluate_choice(&system, 50.0f, 100.0f, 1000.0f, &result);

    EXPECT_FLOAT_EQ(result.effective_k, system.discount_rate);
}

TEST_F(TemporalDiscountingTest, EvaluateChoiceUpdatesStatistics) {
    nimcp_temporal_init(&system, nullptr);

    nimcp_temporal_choice_t result;
    nimcp_temporal_evaluate_choice(&system, 50.0f, 100.0f, 1000.0f, &result);

    EXPECT_EQ(system.choices_made, 1u);
    EXPECT_TRUE(system.delayed_chosen == 1u || system.immediate_chosen == 1u);
}

/* ==========================================================================
 * Indifference Tests
 * ========================================================================== */

TEST_F(TemporalDiscountingTest, FindIndifferenceWithNullReturnsError) {
    nimcp_temporal_init(&system, nullptr);

    float delay;
    EXPECT_EQ(nimcp_temporal_find_indifference(nullptr, 50.0f, 100.0f, &delay), -1);
    EXPECT_EQ(nimcp_temporal_find_indifference(&system, 50.0f, 100.0f, nullptr), -1);
}

TEST_F(TemporalDiscountingTest, FindIndifferenceWithZeroImmediateReturnsError) {
    nimcp_temporal_init(&system, nullptr);

    float delay;
    EXPECT_EQ(nimcp_temporal_find_indifference(&system, 0.0f, 100.0f, &delay), -1);
}

TEST_F(TemporalDiscountingTest, FindIndifferenceWhenDelayedLessOrEqual) {
    nimcp_temporal_init(&system, nullptr);

    float delay;
    nimcp_temporal_find_indifference(&system, 100.0f, 50.0f, &delay);

    /* Delayed is worth less, no positive delay helps */
    EXPECT_FLOAT_EQ(delay, 0.0f);
}

TEST_F(TemporalDiscountingTest, FindIndifferenceComputesCorrectDelay) {
    nimcp_temporal_init(&system, nullptr);

    /* At indifference: immediate = delayed / (1 + k*D)
     * D = (delayed/immediate - 1) / k
     */
    float immediate = 50.0f;
    float delayed = 100.0f;
    float k = system.discount_rate;

    float expected_delay = (delayed / immediate - 1.0f) / k;

    float actual_delay;
    nimcp_temporal_find_indifference(&system, immediate, delayed, &actual_delay);

    EXPECT_NEAR(actual_delay, expected_delay, 0.1f);
}

TEST_F(TemporalDiscountingTest, AtIndifferencePointChoiceIsNeutral) {
    nimcp_temporal_init(&system, nullptr);

    float immediate = 50.0f;
    float delayed = 100.0f;

    float indiff_delay;
    nimcp_temporal_find_indifference(&system, immediate, delayed, &indiff_delay);

    /* Verify by discounting at indifference point */
    float discounted;
    nimcp_temporal_discount_value(&system, delayed, indiff_delay, &discounted);

    EXPECT_NEAR(discounted, immediate, 0.1f);
}

/* ==========================================================================
 * Query API Tests
 * ========================================================================== */

TEST_F(TemporalDiscountingTest, GetDiscountRateSucceeds) {
    nimcp_temporal_init(&system, nullptr);

    float rate;
    EXPECT_EQ(nimcp_temporal_get_discount_rate(&system, &rate), 0);
    EXPECT_FLOAT_EQ(rate, TEMPORAL_DEFAULT_K);
}

TEST_F(TemporalDiscountingTest, GetFutureOrientationSucceeds) {
    nimcp_temporal_init(&system, nullptr);

    float orientation;
    EXPECT_EQ(nimcp_temporal_get_future_orientation(&system, &orientation), 0);
    EXPECT_FLOAT_EQ(orientation, TEMPORAL_DEFAULT_ORIENTATION);
}

TEST_F(TemporalDiscountingTest, GetDelayToleranceSucceeds) {
    nimcp_temporal_init(&system, nullptr);

    float tolerance;
    EXPECT_EQ(nimcp_temporal_get_delay_tolerance(&system, &tolerance), 0);
    EXPECT_GE(tolerance, 0.0f);
    EXPECT_LE(tolerance, 1.0f);
}

/* ==========================================================================
 * 5-HT Effect Tests
 * ========================================================================== */

TEST_F(TemporalDiscountingTest, High5HTIncreasesFutureOrientation) {
    nimcp_temporal_init(&system, nullptr);

    float initial = system.future_orientation;

    for (int i = 0; i < 100; i++) {
        nimcp_temporal_update(&system, 40.0f, 100.0f);
    }

    EXPECT_GT(system.future_orientation, initial);
}

TEST_F(TemporalDiscountingTest, Low5HTDecreasesFutureOrientation) {
    nimcp_temporal_init(&system, nullptr);

    float initial = system.future_orientation;

    for (int i = 0; i < 100; i++) {
        nimcp_temporal_update(&system, 10.0f, 100.0f);
    }

    EXPECT_LT(system.future_orientation, initial);
}

TEST_F(TemporalDiscountingTest, High5HTIncreasesDelayTolerance) {
    nimcp_temporal_init(&system, nullptr);

    float initial = system.delay_tolerance;

    for (int i = 0; i < 100; i++) {
        nimcp_temporal_update(&system, 40.0f, 100.0f);
    }

    EXPECT_GT(system.delay_tolerance, initial);
}

/* ==========================================================================
 * Running Average Tests
 * ========================================================================== */

TEST_F(TemporalDiscountingTest, AvgKTracksDiscountRate) {
    nimcp_temporal_init(&system, nullptr);

    for (int i = 0; i < 1000; i++) {
        nimcp_temporal_update(&system, 30.0f, 100.0f);
    }

    /* avg_k should approach discount_rate */
    EXPECT_NEAR(system.avg_k, system.discount_rate, 0.02f);
}

/* ==========================================================================
 * Boundary Tests
 * ========================================================================== */

TEST_F(TemporalDiscountingTest, FutureOrientationClamped) {
    nimcp_temporal_init(&system, nullptr);

    /* Very high 5-HT */
    for (int i = 0; i < 200; i++) {
        nimcp_temporal_update(&system, 100.0f, 100.0f);
    }

    EXPECT_LE(system.future_orientation, 0.9f);

    /* Very low 5-HT */
    for (int i = 0; i < 200; i++) {
        nimcp_temporal_update(&system, 1.0f, 100.0f);
    }

    EXPECT_GE(system.future_orientation, 0.1f);
}

TEST_F(TemporalDiscountingTest, DelayToleranceClamped) {
    nimcp_temporal_init(&system, nullptr);

    for (int i = 0; i < 200; i++) {
        nimcp_temporal_update(&system, 100.0f, 100.0f);
    }

    EXPECT_LE(system.delay_tolerance, 1.0f);

    for (int i = 0; i < 200; i++) {
        nimcp_temporal_update(&system, 1.0f, 100.0f);
    }

    EXPECT_GE(system.delay_tolerance, 0.0f);
}
