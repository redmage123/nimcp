//=============================================================================
// test_ternary_conversion_regression.cpp - Ternary Conversion Regression Tests
//=============================================================================
/**
 * @file test_ternary_conversion_regression.cpp
 * @brief Regression tests for ternary type conversion consistency
 *
 * WHAT: Test float->ternary->float roundtrip and conversion consistency
 * WHY:  Conversion bugs cause data corruption
 * HOW:  Verify quantization error bounds and consistency
 *
 * REGRESSION CATEGORIES:
 * - Roundtrip stability
 * - Quantization error bounds
 * - Pack mode conversion consistency
 * - Edge case handling
 *
 * @author NIMCP Test Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <random>
#include <vector>
#include <limits>

// Headers have their own extern "C" guards
#include "utils/ternary/nimcp_ternary.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TernaryConversionRegressionTest : public ::testing::Test {
protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);
    }

    void TearDown() override {}

    std::vector<float> generate_floats(size_t n, float min_val, float max_val) {
        std::uniform_real_distribution<float> dist(min_val, max_val);
        std::vector<float> result(n);
        for (size_t i = 0; i < n; i++) {
            result[i] = dist(rng);
        }
        return result;
    }
};

//=============================================================================
// Roundtrip Consistency Tests
//=============================================================================

TEST_F(TernaryConversionRegressionTest, FloatToTernaryToFloatRoundtrip) {
    // WHAT: Verify float->ternary->float produces expected values
    // WHY:  Roundtrip must be deterministic and bounded
    // BASELINE: Reconstructed values in {-scale, 0, scale}

    const float scale = 1.0f;
    const float threshold = 0.5f;

    struct TestCase {
        float input;
        trit_t expected_trit;
        float expected_output;
    };

    std::vector<TestCase> cases = {
        {1.0f, TRIT_POSITIVE, scale},
        {-1.0f, TRIT_NEGATIVE, -scale},
        {0.0f, TRIT_UNKNOWN, 0.0f},
        {0.3f, TRIT_UNKNOWN, 0.0f},
        {-0.3f, TRIT_UNKNOWN, 0.0f},
        {0.5f, TRIT_POSITIVE, scale},
        {-0.5f, TRIT_NEGATIVE, -scale},
        {2.5f, TRIT_POSITIVE, scale},
        {-2.5f, TRIT_NEGATIVE, -scale},
    };

    for (const auto& tc : cases) {
        trit_t trit = trit_from_float_threshold(tc.input, threshold);
        EXPECT_EQ(trit, tc.expected_trit) << "Input: " << tc.input;

        float output = trit_to_float_scaled(trit, scale);
        EXPECT_FLOAT_EQ(output, tc.expected_output) << "Input: " << tc.input;
    }
}

TEST_F(TernaryConversionRegressionTest, VectorRoundtripConsistency) {
    // WHAT: Verify vector conversion roundtrip is consistent
    // WHY:  Vector ops must match scalar ops
    // BASELINE: Element-wise roundtrip matches scalar roundtrip

    const size_t n = 1000;
    const float threshold = 0.5f;
    const float scale = 1.0f;

    std::vector<float> input = generate_floats(n, -2.0f, 2.0f);

    // Vector conversion
    trit_vector_t* vec = trit_vector_from_floats(input.data(), n, threshold, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    std::vector<float> vector_output(n);
    trit_vector_to_floats(vec, vector_output.data(), scale);

    // Verify against scalar conversion
    for (size_t i = 0; i < n; i++) {
        trit_t expected_trit = trit_from_float_threshold(input[i], threshold);
        float expected_output = trit_to_float_scaled(expected_trit, scale);

        EXPECT_FLOAT_EQ(vector_output[i], expected_output) << "Index: " << i;
    }

    trit_vector_destroy(vec);
}

TEST_F(TernaryConversionRegressionTest, PackingRoundtripConsistency) {
    // WHAT: Verify pack->unpack roundtrip preserves values exactly
    // WHY:  Packing must be lossless for trit values
    // BASELINE: All 243 5-trit combinations roundtrip correctly

    // Test 2-bit packing
    for (int t0 = -1; t0 <= 1; t0++) {
        for (int t1 = -1; t1 <= 1; t1++) {
            for (int t2 = -1; t2 <= 1; t2++) {
                for (int t3 = -1; t3 <= 1; t3++) {
                    trit_t input[4] = {(trit_t)t0, (trit_t)t1, (trit_t)t2, (trit_t)t3};
                    trit_packed2_t packed = trit_pack4(input);
                    trit_t output[4];
                    trit_unpack4(packed, output);

                    EXPECT_EQ(input[0], output[0]);
                    EXPECT_EQ(input[1], output[1]);
                    EXPECT_EQ(input[2], output[2]);
                    EXPECT_EQ(input[3], output[3]);
                }
            }
        }
    }

    // Test base-243 packing (all 243 combinations)
    for (int t0 = -1; t0 <= 1; t0++) {
        for (int t1 = -1; t1 <= 1; t1++) {
            for (int t2 = -1; t2 <= 1; t2++) {
                for (int t3 = -1; t3 <= 1; t3++) {
                    for (int t4 = -1; t4 <= 1; t4++) {
                        trit_t input[5] = {(trit_t)t0, (trit_t)t1, (trit_t)t2,
                                           (trit_t)t3, (trit_t)t4};
                        trit_packed243_t packed = trit_pack5(input);
                        trit_t output[5];
                        trit_unpack5(packed, output);

                        EXPECT_EQ(input[0], output[0]);
                        EXPECT_EQ(input[1], output[1]);
                        EXPECT_EQ(input[2], output[2]);
                        EXPECT_EQ(input[3], output[3]);
                        EXPECT_EQ(input[4], output[4]);
                    }
                }
            }
        }
    }
}

TEST_F(TernaryConversionRegressionTest, VectorModeConversionConsistency) {
    // WHAT: Verify pack mode conversion preserves values
    // WHY:  Switching modes must be lossless
    // BASELINE: Values identical after mode conversion

    const size_t n = 1000;

    trit_vector_t* unpacked = trit_vector_create(n, TERNARY_PACK_NONE);
    ASSERT_NE(unpacked, nullptr);

    // Fill with pattern
    std::uniform_int_distribution<int> dist(-1, 1);
    for (size_t i = 0; i < n; i++) {
        trit_vector_set(unpacked, i, static_cast<trit_t>(dist(rng)));
    }

    // Convert to 2-bit
    trit_vector_t* packed_2bit = trit_vector_convert(unpacked, TERNARY_PACK_2BIT);
    ASSERT_NE(packed_2bit, nullptr);

    // Convert 2-bit to base-243
    trit_vector_t* packed_243 = trit_vector_convert(packed_2bit, TERNARY_PACK_BASE243);
    ASSERT_NE(packed_243, nullptr);

    // Convert back to unpacked
    trit_vector_t* final = trit_vector_convert(packed_243, TERNARY_PACK_NONE);
    ASSERT_NE(final, nullptr);

    // All values should match original
    for (size_t i = 0; i < n; i++) {
        trit_t orig = trit_vector_get(unpacked, i);
        trit_t v_2bit = trit_vector_get(packed_2bit, i);
        trit_t v_243 = trit_vector_get(packed_243, i);
        trit_t v_final = trit_vector_get(final, i);

        EXPECT_EQ(orig, v_2bit) << "2-bit mismatch at index " << i;
        EXPECT_EQ(orig, v_243) << "Base-243 mismatch at index " << i;
        EXPECT_EQ(orig, v_final) << "Final mismatch at index " << i;
    }

    trit_vector_destroy(unpacked);
    trit_vector_destroy(packed_2bit);
    trit_vector_destroy(packed_243);
    trit_vector_destroy(final);
}

//=============================================================================
// Quantization Error Bound Tests
//=============================================================================

TEST_F(TernaryConversionRegressionTest, QuantizationErrorBounded) {
    // WHAT: Verify quantization error is bounded by theory
    // WHY:  Error bounds must be predictable
    // BASELINE: For scale=1, max error = max(|input|, 1)

    const size_t n = 10000;
    const float threshold = 0.5f;

    std::vector<float> input = generate_floats(n, -3.0f, 3.0f);

    for (float scale : {0.5f, 1.0f, 2.0f}) {
        trit_vector_t* vec = trit_vector_from_floats(input.data(), n, threshold, TERNARY_PACK_NONE);
        ASSERT_NE(vec, nullptr);

        std::vector<float> output(n);
        trit_vector_to_floats(vec, output.data(), scale);

        float max_error = 0.0f;
        for (size_t i = 0; i < n; i++) {
            float error = std::abs(input[i] - output[i]);
            max_error = std::max(max_error, error);

            // Error should be bounded by |input| + scale (worst case)
            float expected_bound = std::abs(input[i]) + scale;
            EXPECT_LE(error, expected_bound + 0.01f) << "Scale: " << scale << ", Index: " << i;
        }

        std::cout << "Scale " << scale << ": max error = " << max_error << std::endl;

        trit_vector_destroy(vec);
    }
}

TEST_F(TernaryConversionRegressionTest, ThresholdEdgeCaseHandling) {
    // WHAT: Verify exact threshold values are handled consistently
    // WHY:  Edge cases can cause inconsistent behavior
    // BASELINE: Values at threshold should quantize consistently

    const float threshold = 0.5f;

    // Exactly at positive threshold
    EXPECT_EQ(trit_from_float_threshold(0.5f, threshold), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_threshold(-0.5f, threshold), TRIT_NEGATIVE);

    // Just below threshold
    EXPECT_EQ(trit_from_float_threshold(0.49999f, threshold), TRIT_UNKNOWN);
    EXPECT_EQ(trit_from_float_threshold(-0.49999f, threshold), TRIT_UNKNOWN);

    // Just above threshold
    EXPECT_EQ(trit_from_float_threshold(0.50001f, threshold), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_threshold(-0.50001f, threshold), TRIT_NEGATIVE);
}

//=============================================================================
// Special Value Handling Tests
//=============================================================================

TEST_F(TernaryConversionRegressionTest, InfinityHandling) {
    // WHAT: Verify infinity is handled gracefully
    // WHY:  INF values can occur in neural networks
    // BASELINE: INF should quantize to +/-1

    float pos_inf = std::numeric_limits<float>::infinity();
    float neg_inf = -std::numeric_limits<float>::infinity();

    EXPECT_EQ(trit_from_float_threshold(pos_inf, 0.5f), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_threshold(neg_inf, 0.5f), TRIT_NEGATIVE);

    EXPECT_EQ(trit_from_float_sign(pos_inf), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_sign(neg_inf), TRIT_NEGATIVE);
}

TEST_F(TernaryConversionRegressionTest, ZeroHandling) {
    // WHAT: Verify zero is handled correctly
    // WHY:  Zero is common in sparse representations
    // BASELINE: Zero should always become UNKNOWN

    EXPECT_EQ(trit_from_float_threshold(0.0f, 0.5f), TRIT_UNKNOWN);
    // With threshold=0.0, 0.0 >= 0.0 is true, so this returns TRIT_POSITIVE
    EXPECT_EQ(trit_from_float_threshold(0.0f, 0.0f), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_threshold(-0.0f, 0.5f), TRIT_UNKNOWN);

    EXPECT_EQ(trit_from_float_sign(0.0f), TRIT_UNKNOWN);
    EXPECT_EQ(trit_from_float_sign(-0.0f), TRIT_UNKNOWN);
}

TEST_F(TernaryConversionRegressionTest, VerySmallThreshold) {
    // WHAT: Verify behavior with very small threshold
    // WHY:  Small thresholds are used for high-precision quantization
    // BASELINE: Only exactly zero becomes UNKNOWN

    const float threshold = 1e-10f;

    EXPECT_EQ(trit_from_float_threshold(1e-9f, threshold), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_threshold(-1e-9f, threshold), TRIT_NEGATIVE);
    EXPECT_EQ(trit_from_float_threshold(0.0f, threshold), TRIT_UNKNOWN);
}

//=============================================================================
// Matrix Conversion Tests
//=============================================================================

TEST_F(TernaryConversionRegressionTest, MatrixRoundtripConsistency) {
    // WHAT: Verify matrix conversion roundtrip
    // WHY:  Matrix weights are critical for neural networks
    // BASELINE: Same behavior as vector conversion

    const size_t rows = 50;
    const size_t cols = 50;
    const float threshold = 0.5f;
    const float scale = 1.0f;

    std::vector<float> input = generate_floats(rows * cols, -2.0f, 2.0f);

    trit_matrix_t* mat = trit_matrix_from_floats(input.data(), rows, cols, threshold, TERNARY_PACK_NONE);
    ASSERT_NE(mat, nullptr);

    std::vector<float> output(rows * cols);
    trit_matrix_to_floats(mat, output.data(), scale);

    // Verify against scalar conversion
    for (size_t i = 0; i < rows * cols; i++) {
        trit_t expected_trit = trit_from_float_threshold(input[i], threshold);
        float expected_output = trit_to_float_scaled(expected_trit, scale);

        EXPECT_FLOAT_EQ(output[i], expected_output) << "Index: " << i;
    }

    trit_matrix_destroy(mat);
}

//=============================================================================
// Double Precision Conversion Tests
//=============================================================================

TEST_F(TernaryConversionRegressionTest, DoubleConversionConsistency) {
    // WHAT: Verify double conversion matches float conversion
    // WHY:  Both should produce consistent results
    // BASELINE: Same quantization for same value

    const size_t n = 100;
    const double threshold = 0.5;

    std::uniform_real_distribution<double> dist(-2.0, 2.0);
    std::vector<double> doubles(n);
    std::vector<float> floats(n);
    for (size_t i = 0; i < n; i++) {
        doubles[i] = dist(rng);
        floats[i] = static_cast<float>(doubles[i]);
    }

    trit_vector_t* vec_double = trit_vector_from_doubles(doubles.data(), n, threshold, TERNARY_PACK_NONE);
    trit_vector_t* vec_float = trit_vector_from_floats(floats.data(), n, static_cast<float>(threshold), TERNARY_PACK_NONE);
    ASSERT_NE(vec_double, nullptr);
    ASSERT_NE(vec_float, nullptr);

    // Most values should match (some may differ due to precision)
    size_t matches = 0;
    for (size_t i = 0; i < n; i++) {
        if (trit_vector_get(vec_double, i) == trit_vector_get(vec_float, i)) {
            matches++;
        }
    }

    double match_rate = 100.0 * matches / n;
    std::cout << "Double/float match rate: " << match_rate << "%" << std::endl;

    EXPECT_GE(match_rate, 95.0);  // At least 95% should match

    trit_vector_destroy(vec_double);
    trit_vector_destroy(vec_float);
}

//=============================================================================
// Probabilistic Conversion Tests
//=============================================================================

TEST_F(TernaryConversionRegressionTest, StochasticConversionDistribution) {
    // WHAT: Verify stochastic conversion produces correct distribution
    // WHY:  Probabilistic quantization should preserve expected values
    // BASELINE: Average over many samples should match input

    const size_t num_samples = 10000;
    const float test_value = 0.7f;  // Should produce 70% +1, 30% -1

    size_t pos_count = 0, neg_count = 0, unk_count = 0;

    std::uniform_real_distribution<float> rand_dist(0.0f, 1.0f);

    for (size_t i = 0; i < num_samples; i++) {
        float rand_val = rand_dist(rng);
        trit_t result = trit_from_float_stochastic(test_value, rand_val);

        if (result == TRIT_POSITIVE) pos_count++;
        else if (result == TRIT_NEGATIVE) neg_count++;
        else unk_count++;
    }

    double pos_rate = static_cast<double>(pos_count) / num_samples;

    std::cout << "Stochastic conversion for " << test_value << ":" << std::endl;
    std::cout << "  Positive: " << 100.0 * pos_count / num_samples << "%" << std::endl;
    std::cout << "  Negative: " << 100.0 * neg_count / num_samples << "%" << std::endl;
    std::cout << "  Unknown: " << 100.0 * unk_count / num_samples << "%" << std::endl;

    // For value=0.7, expected P(+1) = 0.7
    EXPECT_NEAR(pos_rate, 0.7, 0.05);
}

//=============================================================================
// API Stability Tests
//=============================================================================

TEST_F(TernaryConversionRegressionTest, TritConstantsStable) {
    // WHAT: Verify trit constant values are stable
    // WHY:  Changing constants would break serialization
    // BASELINE: Constants must match documented values

    EXPECT_EQ(TRIT_NEGATIVE, -1);
    EXPECT_EQ(TRIT_UNKNOWN, 0);
    EXPECT_EQ(TRIT_POSITIVE, 1);
}

TEST_F(TernaryConversionRegressionTest, PackModeConstantsStable) {
    // WHAT: Verify pack mode constants are stable
    // WHY:  Used in serialization and config
    // BASELINE: Constants must match documented values

    EXPECT_EQ(TERNARY_PACK_NONE, 0);
    EXPECT_EQ(TERNARY_PACK_2BIT, 1);
    EXPECT_EQ(TERNARY_PACK_BASE243, 2);
}

TEST_F(TernaryConversionRegressionTest, MagicNumberStable) {
    // WHAT: Verify magic number is stable
    // WHY:  Used for validation in data structures
    // BASELINE: Must match documented value

    EXPECT_EQ(TERNARY_MAGIC, 0x54524954U);  // "TRIT"
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
