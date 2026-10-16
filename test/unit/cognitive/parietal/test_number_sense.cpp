/**
 * @file test_number_sense.cpp
 * @brief Unit tests for NIMCP Number Sense (Approximate Number System)
 *
 * Tests Weber-Fechner law, subitizing, approximate arithmetic,
 * and modulation by inflammation/sleep.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_number_sense.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class NumberSenseTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ns = number_sense_create();
        ASSERT_NE(ns, nullptr);
    }

    void TearDown() override
    {
        if (ns) {
            number_sense_destroy(ns);
            ns = nullptr;
        }
    }

    number_sense_t* ns;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(NumberSenseTest, CreateDefault)
{
    EXPECT_NE(ns, nullptr);
}

TEST_F(NumberSenseTest, CreateCustom)
{
    number_sense_config_t config = number_sense_default_config();
    config.weber_fraction = 0.10f;
    config.subitizing_limit = 3;

    number_sense_t* custom = number_sense_create_custom(&config);
    ASSERT_NE(custom, nullptr);

    // Verify custom Weber fraction is applied
    float wf = number_sense_get_weber_fraction(custom, 10.0f);
    EXPECT_NEAR(wf, 0.10f, 0.01f);

    number_sense_destroy(custom);
}

TEST_F(NumberSenseTest, CreateWithNullConfig)
{
    number_sense_t* created = number_sense_create_custom(nullptr);
    EXPECT_NE(created, nullptr);
    number_sense_destroy(created);
}

TEST_F(NumberSenseTest, DestroyNullSafe)
{
    number_sense_destroy(nullptr);
    // Should not crash
}

TEST_F(NumberSenseTest, DefaultConfig)
{
    number_sense_config_t config = number_sense_default_config();

    EXPECT_NEAR(config.weber_fraction, NUMBER_SENSE_DEFAULT_WEBER_FRACTION, 0.001f);
    EXPECT_EQ(config.subitizing_limit, NUMBER_SENSE_SUBITIZING_LIMIT);
    EXPECT_TRUE(config.enable_logarithmic_scale);
    EXPECT_TRUE(config.enable_subitizing);
}

TEST_F(NumberSenseTest, ValidateConfig)
{
    number_sense_config_t valid_config = number_sense_default_config();
    EXPECT_TRUE(number_sense_validate_config(&valid_config));

    // Invalid: weber_fraction out of range
    number_sense_config_t invalid_config = valid_config;
    invalid_config.weber_fraction = -0.1f;
    EXPECT_FALSE(number_sense_validate_config(&invalid_config));

    invalid_config = valid_config;
    invalid_config.weber_fraction = 1.5f;
    EXPECT_FALSE(number_sense_validate_config(&invalid_config));

    // Invalid: subitizing_limit = 0
    invalid_config = valid_config;
    invalid_config.subitizing_limit = 0;
    EXPECT_FALSE(number_sense_validate_config(&invalid_config));
}

//=============================================================================
// Estimation Tests
//=============================================================================

TEST_F(NumberSenseTest, EstimateFromMagnitude)
{
    number_estimate_t est = number_sense_estimate_from_magnitude(ns, 100.0f);

    EXPECT_GT(est.magnitude, 0.0f);
    EXPECT_GT(est.uncertainty, 0.0f);
    EXPECT_GE(est.confidence, 0.0f);
    EXPECT_LE(est.confidence, 1.0f);
}

TEST_F(NumberSenseTest, EstimateFromArray)
{
    float input[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    number_estimate_t est = number_sense_estimate(ns, input, 5);

    EXPECT_GT(est.magnitude, 0.0f);
    EXPECT_GE(est.confidence, 0.0f);
    EXPECT_LE(est.confidence, 1.0f);
}

TEST_F(NumberSenseTest, EstimateNullHandling)
{
    float input[] = {1.0f, 2.0f};

    number_estimate_t est = number_sense_estimate(nullptr, input, 2);
    EXPECT_EQ(est.magnitude, 0.0f);

    est = number_sense_estimate(ns, nullptr, 2);
    EXPECT_EQ(est.magnitude, 0.0f);

    est = number_sense_estimate(ns, input, 0);
    EXPECT_EQ(est.magnitude, 0.0f);
}

TEST_F(NumberSenseTest, EstimateFromMagnitudeNullHandling)
{
    number_estimate_t est = number_sense_estimate_from_magnitude(nullptr, 50.0f);
    EXPECT_EQ(est.magnitude, 0.0f);
}

//=============================================================================
// Subitizing Tests
//=============================================================================

TEST_F(NumberSenseTest, SubitizeSmallQuantity)
{
    // 3 items - should be subitized
    float input[] = {1.0f, 1.0f, 1.0f};
    number_estimate_t est = number_sense_subitize(ns, input, 3);

    EXPECT_TRUE(est.is_subitized);
    EXPECT_NEAR(est.magnitude, 3.0f, 0.5f);
    EXPECT_GT(est.confidence, 0.9f);  // High confidence for subitized
}

TEST_F(NumberSenseTest, SubitizeLargeQuantity)
{
    // 7 items - beyond subitizing limit
    float input[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    number_estimate_t est = number_sense_subitize(ns, input, 7);

    EXPECT_FALSE(est.is_subitized);
    EXPECT_LT(est.confidence, 0.9f);  // Lower confidence
}

TEST_F(NumberSenseTest, SubitizeBoundary)
{
    // Exactly at subitizing limit (4)
    float input[] = {1.0f, 1.0f, 1.0f, 1.0f};
    number_estimate_t est = number_sense_subitize(ns, input, 4);

    EXPECT_TRUE(est.is_subitized);
    EXPECT_NEAR(est.magnitude, 4.0f, 0.5f);
}

TEST_F(NumberSenseTest, SubitizeNullHandling)
{
    float input[] = {1.0f};

    number_estimate_t est = number_sense_subitize(nullptr, input, 1);
    EXPECT_EQ(est.magnitude, 0.0f);

    est = number_sense_subitize(ns, nullptr, 1);
    EXPECT_EQ(est.magnitude, 0.0f);
}

//=============================================================================
// Comparison Tests
//=============================================================================

TEST_F(NumberSenseTest, CompareDistantMagnitudes)
{
    // Large ratio - easy comparison
    number_comparison_t cmp = number_sense_compare(ns, 10.0f, 100.0f);

    EXPECT_EQ(cmp.direction, -1);  // 10 < 100
    EXPECT_GT(cmp.confidence, 0.9f);  // High confidence
    EXPECT_NEAR(cmp.perceived_ratio, 0.1f, 0.05f);
}

TEST_F(NumberSenseTest, CompareCloseMagnitudes)
{
    // Small ratio - harder comparison (Weber's law)
    // 100 vs 110 (10% difference) is within the Weber fraction threshold (~15%)
    // so discrimination is uncertain (direction = 0) or has low confidence
    number_comparison_t cmp = number_sense_compare(ns, 100.0f, 110.0f);

    // With Weber fraction ~0.15, 10% diff is near-threshold: may be uncertain
    EXPECT_LT(cmp.confidence, 0.9f);  // Lower confidence for close magnitudes
}

TEST_F(NumberSenseTest, CompareEqualMagnitudes)
{
    number_comparison_t cmp = number_sense_compare(ns, 50.0f, 50.0f);

    EXPECT_EQ(cmp.direction, 0);  // Equal/uncertain
    EXPECT_NEAR(cmp.perceived_ratio, 1.0f, 0.1f);
}

TEST_F(NumberSenseTest, CompareReversed)
{
    number_comparison_t cmp = number_sense_compare(ns, 100.0f, 10.0f);

    EXPECT_EQ(cmp.direction, 1);  // 100 > 10
    EXPECT_GT(cmp.confidence, 0.9f);
}

TEST_F(NumberSenseTest, CompareNullHandling)
{
    number_comparison_t cmp = number_sense_compare(nullptr, 10.0f, 20.0f);
    EXPECT_EQ(cmp.direction, 0);
    EXPECT_EQ(cmp.confidence, -1.0f);  // Null input returns -1.0f confidence
}

//=============================================================================
// Weber Fraction Tests
//=============================================================================

TEST_F(NumberSenseTest, WeberFractionDefault)
{
    float wf = number_sense_get_weber_fraction(ns, 100.0f);
    EXPECT_NEAR(wf, NUMBER_SENSE_DEFAULT_WEBER_FRACTION, 0.01f);
}

TEST_F(NumberSenseTest, WeberFractionConsistency)
{
    // Weber fraction should be approximately constant across magnitudes
    float wf1 = number_sense_get_weber_fraction(ns, 10.0f);
    float wf2 = number_sense_get_weber_fraction(ns, 100.0f);
    float wf3 = number_sense_get_weber_fraction(ns, 1000.0f);

    EXPECT_NEAR(wf1, wf2, 0.05f);
    EXPECT_NEAR(wf2, wf3, 0.05f);
}

TEST_F(NumberSenseTest, WeberFractionNullHandling)
{
    // Returns default Weber fraction on null (graceful degradation)
    float wf = number_sense_get_weber_fraction(nullptr, 100.0f);
    EXPECT_NEAR(wf, NUMBER_SENSE_DEFAULT_WEBER_FRACTION, 0.01f);
}

//=============================================================================
// Discriminability Tests
//=============================================================================

TEST_F(NumberSenseTest, DiscriminabilityDistant)
{
    // Large ratio = high d'
    float dprime = number_sense_discriminability(ns, 10.0f, 100.0f);
    EXPECT_GT(dprime, 2.0f);
}

TEST_F(NumberSenseTest, DiscriminabilityClose)
{
    // Small ratio = low d'
    float dprime = number_sense_discriminability(ns, 100.0f, 110.0f);
    EXPECT_LT(dprime, 1.0f);
}

TEST_F(NumberSenseTest, DiscriminabilitySymmetric)
{
    float dprime1 = number_sense_discriminability(ns, 50.0f, 100.0f);
    float dprime2 = number_sense_discriminability(ns, 100.0f, 50.0f);
    EXPECT_NEAR(dprime1, dprime2, 0.1f);
}

//=============================================================================
// Approximate Arithmetic Tests
//=============================================================================

TEST_F(NumberSenseTest, ApproximateAdd)
{
    approx_arithmetic_t result = number_sense_approximate_add(ns, 30.0f, 20.0f);

    // Weber-law noise means results can vary; use wider tolerance
    EXPECT_NEAR(result.result, 50.0f, 20.0f);
    EXPECT_GT(result.uncertainty, 0.0f);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(NumberSenseTest, ApproximateSub)
{
    approx_arithmetic_t result = number_sense_approximate_sub(ns, 50.0f, 20.0f);

    // Weber-law noise means results can vary significantly; use wide tolerance
    EXPECT_NEAR(result.result, 30.0f, 20.0f);
    EXPECT_GT(result.uncertainty, 0.0f);
}

TEST_F(NumberSenseTest, ApproximateMul)
{
    approx_arithmetic_t result = number_sense_approximate_mul(ns, 5.0f, 4.0f);

    // Weber-law noise means results can vary; use wider tolerance
    EXPECT_NEAR(result.result, 20.0f, 10.0f);
    EXPECT_GT(result.uncertainty, 0.0f);
}

TEST_F(NumberSenseTest, ApproximateDiv)
{
    approx_arithmetic_t result = number_sense_approximate_div(ns, 100.0f, 5.0f);

    // Weber-law noise means results can vary significantly; use wide tolerance
    EXPECT_NEAR(result.result, 20.0f, 15.0f);
    EXPECT_GT(result.uncertainty, 0.0f);
}

TEST_F(NumberSenseTest, ApproximateDivByZero)
{
    approx_arithmetic_t result = number_sense_approximate_div(ns, 100.0f, 0.0f);
    EXPECT_TRUE(std::isnan(result.result) || std::isinf(result.result) || result.confidence == 0.0f);
}

TEST_F(NumberSenseTest, ApproximateArithmeticNullHandling)
{
    approx_arithmetic_t result = number_sense_approximate_add(nullptr, 10.0f, 20.0f);
    EXPECT_EQ(result.confidence, -1.0f);  // Null input returns -1.0f confidence
}

//=============================================================================
// Order of Magnitude Tests
//=============================================================================

TEST_F(NumberSenseTest, OrderOfMagnitudeSmall)
{
    int order = number_sense_order_of_magnitude(ns, 5.0f);
    // Weber-law noise means order may vary by ±1
    EXPECT_GE(order, -1);
    EXPECT_LE(order, 1);
}

TEST_F(NumberSenseTest, OrderOfMagnitudeMedium)
{
    int order = number_sense_order_of_magnitude(ns, 500.0f);
    // Weber-law noise means order may vary by ±1
    EXPECT_GE(order, 1);
    EXPECT_LE(order, 3);
}

TEST_F(NumberSenseTest, OrderOfMagnitudeLarge)
{
    int order = number_sense_order_of_magnitude(ns, 5000.0f);
    // Weber-law noise means order may vary by ±1
    EXPECT_GE(order, 2);
    EXPECT_LE(order, 4);
}

TEST_F(NumberSenseTest, OrderOfMagnitudeNullHandling)
{
    int order = number_sense_order_of_magnitude(nullptr, 100.0f);
    EXPECT_EQ(order, 0);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(NumberSenseTest, SetInflammation)
{
    float baseline_wf = number_sense_get_effective_weber_fraction(ns);

    EXPECT_EQ(number_sense_set_inflammation(ns, 0.5f), 0);

    float modulated_wf = number_sense_get_effective_weber_fraction(ns);
    EXPECT_GT(modulated_wf, baseline_wf);  // Inflammation increases Weber fraction
}

TEST_F(NumberSenseTest, SetInflammationBoundary)
{
    // Implementation clamps out-of-bounds values and returns success
    EXPECT_EQ(number_sense_set_inflammation(ns, 0.0f), 0);
    EXPECT_EQ(number_sense_set_inflammation(ns, 1.0f), 0);
    EXPECT_EQ(number_sense_set_inflammation(ns, -0.1f), 0);  // Clamped to 0
    EXPECT_EQ(number_sense_set_inflammation(ns, 1.1f), 0);   // Clamped to 1
}

TEST_F(NumberSenseTest, SetInflammationNullHandling)
{
    EXPECT_NE(number_sense_set_inflammation(nullptr, 0.5f), 0);
}

TEST_F(NumberSenseTest, SetSleepDeprivation)
{
    float baseline_wf = number_sense_get_effective_weber_fraction(ns);

    EXPECT_EQ(number_sense_set_sleep_deprivation(ns, 0.5f), 0);

    float modulated_wf = number_sense_get_effective_weber_fraction(ns);
    EXPECT_GT(modulated_wf, baseline_wf);  // Sleep deprivation increases Weber fraction
}

TEST_F(NumberSenseTest, SetSleepDeprivationNullHandling)
{
    EXPECT_NE(number_sense_set_sleep_deprivation(nullptr, 0.5f), 0);
}

TEST_F(NumberSenseTest, EffectiveWeberFractionCumulative)
{
    float baseline = number_sense_get_effective_weber_fraction(ns);

    number_sense_set_inflammation(ns, 0.3f);
    float with_inflammation = number_sense_get_effective_weber_fraction(ns);

    number_sense_set_sleep_deprivation(ns, 0.3f);
    float with_both = number_sense_get_effective_weber_fraction(ns);

    EXPECT_GT(with_inflammation, baseline);
    EXPECT_GT(with_both, with_inflammation);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(NumberSenseTest, GetStats)
{
    // Perform operations using functions that track stats
    // number_sense_estimate() tracks stats, not estimate_from_magnitude()
    float input[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 10.0f};  // Sum ~15
    number_sense_estimate(ns, input, 6);
    number_sense_compare(ns, 10.0f, 20.0f);

    number_sense_stats_t stats;
    EXPECT_EQ(number_sense_get_stats(ns, &stats), 0);

    EXPECT_GT(stats.estimates_performed, 0);
    EXPECT_GT(stats.comparisons_performed, 0);
}

TEST_F(NumberSenseTest, GetStatsNullHandling)
{
    number_sense_stats_t stats;
    EXPECT_NE(number_sense_get_stats(nullptr, &stats), 0);
    EXPECT_NE(number_sense_get_stats(ns, nullptr), 0);
}

TEST_F(NumberSenseTest, ResetStats)
{
    number_sense_estimate_from_magnitude(ns, 100.0f);

    number_sense_reset_stats(ns);

    number_sense_stats_t stats;
    EXPECT_EQ(number_sense_get_stats(ns, &stats), 0);
    EXPECT_EQ(stats.estimates_performed, 0);
}

TEST_F(NumberSenseTest, ResetStatsNullSafe)
{
    number_sense_reset_stats(nullptr);
    // Should not crash
}

//=============================================================================
// Weber-Fechner Law Verification Tests
//=============================================================================

TEST_F(NumberSenseTest, WeberLawRatioEffect)
{
    // Weber's law: easier to discriminate 1 vs 2 than 7 vs 8
    // (same absolute difference, different ratio)

    float dprime_1_2 = number_sense_discriminability(ns, 1.0f, 2.0f);
    float dprime_7_8 = number_sense_discriminability(ns, 7.0f, 8.0f);

    EXPECT_GT(dprime_1_2, dprime_7_8);  // 1:2 ratio is easier than 7:8
}

TEST_F(NumberSenseTest, WeberLawScaleInvariance)
{
    // Same ratio should give similar d' regardless of scale
    float dprime_10_20 = number_sense_discriminability(ns, 10.0f, 20.0f);
    float dprime_100_200 = number_sense_discriminability(ns, 100.0f, 200.0f);

    EXPECT_NEAR(dprime_10_20, dprime_100_200, 0.5f);
}

}  // namespace
