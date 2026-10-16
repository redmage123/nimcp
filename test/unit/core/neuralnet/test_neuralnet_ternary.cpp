//=============================================================================
// test_neuralnet_ternary.cpp - Unit Tests for Ternary Neural Network Integration
//=============================================================================
/**
 * @file test_neuralnet_ternary.cpp
 * @brief Comprehensive unit tests for ternary weight integration in neural networks
 *
 * WHAT: Tests ternary weight creation, conversion, operations, and memory savings
 * WHY:  Validate ternary neural network features for memory-efficient inference
 * HOW:  GTest-based unit tests with edge cases and memory verification
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/ternary/nimcp_ternary_convert.h"

//=============================================================================
// Ternary Weight Creation Tests
//=============================================================================

class TernaryWeightCreationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TernaryWeightCreationTest, CreateTernaryWeightVector) {
    // Test creating a ternary weight vector
    const size_t weight_count = 100;
    trit_vector_t* weights = trit_vector_create(weight_count, TERNARY_PACK_NONE);
    ASSERT_NE(weights, nullptr);
    EXPECT_EQ(weights->length, weight_count);

    // Initialize with pattern
    for (size_t i = 0; i < weight_count; i++) {
        trit_t val = (trit_t)((i % 3) - 1);  // -1, 0, +1 pattern
        trit_vector_set(weights, i, val);
    }

    // Verify pattern
    for (size_t i = 0; i < weight_count; i++) {
        trit_t expected = (trit_t)((i % 3) - 1);
        EXPECT_EQ(trit_vector_get(weights, i), expected);
    }

    trit_vector_destroy(weights);
}

TEST_F(TernaryWeightCreationTest, CreateTernaryWeightMatrix) {
    // Test creating a ternary weight matrix (layer weights)
    const size_t rows = 64;
    const size_t cols = 128;

    trit_matrix_t* weight_matrix = trit_matrix_create(rows, cols, TERNARY_PACK_NONE);
    ASSERT_NE(weight_matrix, nullptr);
    EXPECT_EQ(weight_matrix->rows, rows);
    EXPECT_EQ(weight_matrix->cols, cols);
    EXPECT_EQ(weight_matrix->numel, rows * cols);

    // Initialize with Hebbian-like pattern (excitatory/inhibitory)
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            trit_t val;
            if ((r + c) % 5 == 0) val = TRIT_POSITIVE;      // Excitatory
            else if ((r + c) % 5 == 1) val = TRIT_NEGATIVE; // Inhibitory
            else val = TRIT_UNKNOWN;                         // No connection
            trit_matrix_set(weight_matrix, r, c, val);
        }
    }

    // Verify matrix
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            trit_t expected;
            if ((r + c) % 5 == 0) expected = TRIT_POSITIVE;
            else if ((r + c) % 5 == 1) expected = TRIT_NEGATIVE;
            else expected = TRIT_UNKNOWN;
            EXPECT_EQ(trit_matrix_get(weight_matrix, r, c), expected);
        }
    }

    trit_matrix_destroy(weight_matrix);
}

TEST_F(TernaryWeightCreationTest, CreatePackedTernaryWeights) {
    // Test creating packed ternary weights for memory efficiency
    const size_t weight_count = 1000;

    // Create with 2-bit packing
    trit_vector_t* packed_2bit = trit_vector_create(weight_count, TERNARY_PACK_2BIT);
    ASSERT_NE(packed_2bit, nullptr);

    // Create with base-243 packing (5 trits per byte)
    trit_vector_t* packed_243 = trit_vector_create(weight_count, TERNARY_PACK_BASE243);
    ASSERT_NE(packed_243, nullptr);

    // Initialize both with same pattern
    for (size_t i = 0; i < weight_count; i++) {
        trit_t val = (trit_t)((i % 3) - 1);
        trit_vector_set(packed_2bit, i, val);
        trit_vector_set(packed_243, i, val);
    }

    // Verify both contain same data
    for (size_t i = 0; i < weight_count; i++) {
        EXPECT_EQ(trit_vector_get(packed_2bit, i), trit_vector_get(packed_243, i));
    }

    // Verify memory savings
    size_t unpacked_bytes = weight_count * sizeof(trit_t);
    size_t packed_2bit_bytes = trit_packed_bytes(weight_count, TERNARY_PACK_2BIT);
    size_t packed_243_bytes = trit_packed_bytes(weight_count, TERNARY_PACK_BASE243);

    EXPECT_LT(packed_2bit_bytes, unpacked_bytes);
    EXPECT_LT(packed_243_bytes, packed_2bit_bytes);  // Base-243 is more efficient

    trit_vector_destroy(packed_2bit);
    trit_vector_destroy(packed_243);
}

TEST_F(TernaryWeightCreationTest, NullPointerHandling) {
    // Test null pointer handling for ternary vectors
    EXPECT_EQ(trit_vector_get(nullptr, 0), TRIT_UNKNOWN);

    // Clone null should return null
    trit_vector_t* clone = trit_vector_clone(nullptr);
    EXPECT_EQ(clone, nullptr);

    // Destroy null should not crash
    trit_vector_destroy(nullptr);
    trit_matrix_destroy(nullptr);
}

//=============================================================================
// Float-Ternary Conversion Tests
//=============================================================================

class FloatTernaryConversionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FloatTernaryConversionTest, FloatToTernarySign) {
    // Test sign-based conversion
    EXPECT_EQ(trit_from_float_sign(1.5f), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_sign(-1.5f), TRIT_NEGATIVE);
    EXPECT_EQ(trit_from_float_sign(0.0f), TRIT_UNKNOWN);
    EXPECT_EQ(trit_from_float_sign(0.001f), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_sign(-0.001f), TRIT_NEGATIVE);

    // Edge cases
    EXPECT_EQ(trit_from_float_sign(INFINITY), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_sign(-INFINITY), TRIT_NEGATIVE);
}

TEST_F(FloatTernaryConversionTest, FloatToTernaryThreshold) {
    const float threshold = 0.5f;

    // Values above threshold -> POSITIVE
    EXPECT_EQ(trit_from_float_threshold(1.0f, threshold), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_threshold(0.5f, threshold), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_threshold(0.7f, threshold), TRIT_POSITIVE);

    // Values below negative threshold -> NEGATIVE
    EXPECT_EQ(trit_from_float_threshold(-1.0f, threshold), TRIT_NEGATIVE);
    EXPECT_EQ(trit_from_float_threshold(-0.5f, threshold), TRIT_NEGATIVE);
    EXPECT_EQ(trit_from_float_threshold(-0.7f, threshold), TRIT_NEGATIVE);

    // Values in dead zone -> UNKNOWN
    EXPECT_EQ(trit_from_float_threshold(0.3f, threshold), TRIT_UNKNOWN);
    EXPECT_EQ(trit_from_float_threshold(-0.3f, threshold), TRIT_UNKNOWN);
    EXPECT_EQ(trit_from_float_threshold(0.0f, threshold), TRIT_UNKNOWN);
    EXPECT_EQ(trit_from_float_threshold(0.49f, threshold), TRIT_UNKNOWN);
    EXPECT_EQ(trit_from_float_threshold(-0.49f, threshold), TRIT_UNKNOWN);
}

TEST_F(FloatTernaryConversionTest, TernaryToFloat) {
    EXPECT_FLOAT_EQ(trit_to_float(TRIT_NEGATIVE), -1.0f);
    EXPECT_FLOAT_EQ(trit_to_float(TRIT_UNKNOWN), 0.0f);
    EXPECT_FLOAT_EQ(trit_to_float(TRIT_POSITIVE), 1.0f);
}

TEST_F(FloatTernaryConversionTest, TernaryToFloatScaled) {
    const float scale = 0.25f;

    EXPECT_FLOAT_EQ(trit_to_float_scaled(TRIT_NEGATIVE, scale), -0.25f);
    EXPECT_FLOAT_EQ(trit_to_float_scaled(TRIT_UNKNOWN, scale), 0.0f);
    EXPECT_FLOAT_EQ(trit_to_float_scaled(TRIT_POSITIVE, scale), 0.25f);

    // Zero scale
    EXPECT_FLOAT_EQ(trit_to_float_scaled(TRIT_POSITIVE, 0.0f), 0.0f);

    // Negative scale (inverts sign)
    EXPECT_FLOAT_EQ(trit_to_float_scaled(TRIT_POSITIVE, -1.0f), -1.0f);
}

TEST_F(FloatTernaryConversionTest, VectorFloatToTernary) {
    float weights[] = {0.8f, -0.2f, 0.0f, -0.9f, 0.6f, 0.1f, -0.6f, 0.4f};
    const size_t count = sizeof(weights) / sizeof(weights[0]);
    const float threshold = 0.5f;

    trit_vector_t* ternary = trit_vector_from_floats(weights, count, threshold, TERNARY_PACK_NONE);
    ASSERT_NE(ternary, nullptr);
    EXPECT_EQ(ternary->length, count);

    // Verify conversion: |w| >= 0.5 -> sign(w), else 0
    EXPECT_EQ(trit_vector_get(ternary, 0), TRIT_POSITIVE);   // 0.8
    EXPECT_EQ(trit_vector_get(ternary, 1), TRIT_UNKNOWN);    // -0.2
    EXPECT_EQ(trit_vector_get(ternary, 2), TRIT_UNKNOWN);    // 0.0
    EXPECT_EQ(trit_vector_get(ternary, 3), TRIT_NEGATIVE);   // -0.9
    EXPECT_EQ(trit_vector_get(ternary, 4), TRIT_POSITIVE);   // 0.6
    EXPECT_EQ(trit_vector_get(ternary, 5), TRIT_UNKNOWN);    // 0.1
    EXPECT_EQ(trit_vector_get(ternary, 6), TRIT_NEGATIVE);   // -0.6
    EXPECT_EQ(trit_vector_get(ternary, 7), TRIT_UNKNOWN);    // 0.4

    trit_vector_destroy(ternary);
}

TEST_F(FloatTernaryConversionTest, VectorTernaryToFloat) {
    trit_vector_t* ternary = trit_vector_create(5, TERNARY_PACK_NONE);
    ASSERT_NE(ternary, nullptr);

    trit_vector_set(ternary, 0, TRIT_POSITIVE);
    trit_vector_set(ternary, 1, TRIT_NEGATIVE);
    trit_vector_set(ternary, 2, TRIT_UNKNOWN);
    trit_vector_set(ternary, 3, TRIT_POSITIVE);
    trit_vector_set(ternary, 4, TRIT_NEGATIVE);

    float floats[5];
    ternary_error_t err = trit_vector_to_floats(ternary, floats, 1.0f);
    EXPECT_EQ(err, TERNARY_OK);

    EXPECT_FLOAT_EQ(floats[0], 1.0f);
    EXPECT_FLOAT_EQ(floats[1], -1.0f);
    EXPECT_FLOAT_EQ(floats[2], 0.0f);
    EXPECT_FLOAT_EQ(floats[3], 1.0f);
    EXPECT_FLOAT_EQ(floats[4], -1.0f);

    // Test with scale factor
    err = trit_vector_to_floats(ternary, floats, 0.5f);
    EXPECT_EQ(err, TERNARY_OK);

    EXPECT_FLOAT_EQ(floats[0], 0.5f);
    EXPECT_FLOAT_EQ(floats[1], -0.5f);
    EXPECT_FLOAT_EQ(floats[2], 0.0f);

    trit_vector_destroy(ternary);
}

TEST_F(FloatTernaryConversionTest, MatrixFloatToTernary) {
    const size_t rows = 4;
    const size_t cols = 4;
    float matrix[16] = {
        0.8f, -0.9f, 0.1f, -0.1f,
        0.6f, -0.7f, 0.0f,  0.5f,
       -0.8f,  0.9f, 0.2f, -0.2f,
        0.7f, -0.6f, 0.3f, -0.3f
    };

    const float threshold = 0.5f;
    trit_matrix_t* ternary = trit_matrix_from_floats(matrix, rows, cols, threshold, TERNARY_PACK_NONE);
    ASSERT_NE(ternary, nullptr);
    EXPECT_EQ(ternary->rows, rows);
    EXPECT_EQ(ternary->cols, cols);

    // Verify conversion
    EXPECT_EQ(trit_matrix_get(ternary, 0, 0), TRIT_POSITIVE);   // 0.8
    EXPECT_EQ(trit_matrix_get(ternary, 0, 1), TRIT_NEGATIVE);   // -0.9
    EXPECT_EQ(trit_matrix_get(ternary, 0, 2), TRIT_UNKNOWN);    // 0.1
    EXPECT_EQ(trit_matrix_get(ternary, 0, 3), TRIT_UNKNOWN);    // -0.1
    EXPECT_EQ(trit_matrix_get(ternary, 1, 3), TRIT_POSITIVE);   // 0.5 (edge)

    trit_matrix_destroy(ternary);
}

TEST_F(FloatTernaryConversionTest, RoundTripConversion) {
    // Test float -> ternary -> float preserves information (within quantization)
    const size_t count = 100;
    std::vector<float> original(count);

    // Generate random-like weights in [-1, 1]
    for (size_t i = 0; i < count; i++) {
        original[i] = 2.0f * (float)(i % 11) / 10.0f - 1.0f;  // Deterministic
    }

    const float threshold = 0.3f;
    trit_vector_t* ternary = trit_vector_from_floats(original.data(), count, threshold, TERNARY_PACK_NONE);
    ASSERT_NE(ternary, nullptr);

    std::vector<float> recovered(count);
    ternary_error_t err = trit_vector_to_floats(ternary, recovered.data(), 1.0f);
    EXPECT_EQ(err, TERNARY_OK);

    // Verify round-trip: recovered values should be {-1, 0, +1}
    for (size_t i = 0; i < count; i++) {
        float r = recovered[i];
        EXPECT_TRUE(r == -1.0f || r == 0.0f || r == 1.0f);

        // Sign should be preserved for values above threshold
        if (fabsf(original[i]) >= threshold) {
            if (original[i] > 0) EXPECT_FLOAT_EQ(r, 1.0f);
            else EXPECT_FLOAT_EQ(r, -1.0f);
        } else {
            EXPECT_FLOAT_EQ(r, 0.0f);
        }
    }

    trit_vector_destroy(ternary);
}

TEST_F(FloatTernaryConversionTest, ExtendedTritConversion) {
    // Test extended trit with confidence
    trit_extended_t ext;

    // Strong positive signal
    ext = trit_extended_from_float(1.0f, 0.5f);
    EXPECT_EQ(ext.value, TRIT_POSITIVE);
    EXPECT_GT(ext.confidence, 0.5f);
    EXPECT_LT(ext.uncertainty, 0.5f);

    // Strong negative signal
    ext = trit_extended_from_float(-1.0f, 0.5f);
    EXPECT_EQ(ext.value, TRIT_NEGATIVE);
    EXPECT_GT(ext.confidence, 0.5f);

    // Weak signal (in dead zone)
    ext = trit_extended_from_float(0.0f, 0.5f);
    EXPECT_EQ(ext.value, TRIT_UNKNOWN);
    EXPECT_FLOAT_EQ(ext.confidence, 1.0f);  // Very confident in being unknown

    // Edge of threshold
    ext = trit_extended_from_float(0.5f, 0.5f);
    EXPECT_EQ(ext.value, TRIT_POSITIVE);
}

//=============================================================================
// Ternary Operations Tests
//=============================================================================

class TernaryOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TernaryOperationsTest, VectorDotProduct) {
    const size_t n = 8;
    trit_vector_t* a = trit_vector_create(n, TERNARY_PACK_NONE);
    trit_vector_t* b = trit_vector_create(n, TERNARY_PACK_NONE);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // a = [+1, +1, -1, -1,  0,  0, +1, -1]
    // b = [+1, -1, +1, -1, +1, -1,  0,  0]
    // dot = 1*1 + 1*(-1) + (-1)*1 + (-1)*(-1) + 0*1 + 0*(-1) + 1*0 + (-1)*0
    //     = 1 - 1 - 1 + 1 + 0 + 0 + 0 + 0 = 0
    trit_t a_vals[] = {TRIT_POSITIVE, TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_NEGATIVE,
                       TRIT_UNKNOWN, TRIT_UNKNOWN, TRIT_POSITIVE, TRIT_NEGATIVE};
    trit_t b_vals[] = {TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_POSITIVE, TRIT_NEGATIVE,
                       TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_UNKNOWN, TRIT_UNKNOWN};

    for (size_t i = 0; i < n; i++) {
        trit_vector_set(a, i, a_vals[i]);
        trit_vector_set(b, i, b_vals[i]);
    }

    int dot = trit_vector_dot(a, b);
    EXPECT_EQ(dot, 0);

    trit_vector_destroy(a);
    trit_vector_destroy(b);
}

TEST_F(TernaryOperationsTest, MatrixVectorMultiply) {
    // 2x3 matrix times 3-element vector
    trit_matrix_t* mat = trit_matrix_create(2, 3, TERNARY_PACK_NONE);
    trit_vector_t* vec = trit_vector_create(3, TERNARY_PACK_NONE);
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(vec, nullptr);

    // Matrix:
    // [+1,  0, -1]
    // [+1, +1, +1]
    trit_matrix_set(mat, 0, 0, TRIT_POSITIVE);
    trit_matrix_set(mat, 0, 1, TRIT_UNKNOWN);
    trit_matrix_set(mat, 0, 2, TRIT_NEGATIVE);
    trit_matrix_set(mat, 1, 0, TRIT_POSITIVE);
    trit_matrix_set(mat, 1, 1, TRIT_POSITIVE);
    trit_matrix_set(mat, 1, 2, TRIT_POSITIVE);

    // Vector: [+1, +1, +1]
    trit_vector_set(vec, 0, TRIT_POSITIVE);
    trit_vector_set(vec, 1, TRIT_POSITIVE);
    trit_vector_set(vec, 2, TRIT_POSITIVE);

    trit_vector_t* result = trit_matrix_vector_mul(mat, vec);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->length, 2UL);

    // Row 0: 1*1 + 0*1 + (-1)*1 = 1 + 0 - 1 = 0 -> UNKNOWN
    // Row 1: 1*1 + 1*1 + 1*1 = 3 -> POSITIVE (clamped)
    EXPECT_EQ(trit_vector_get(result, 0), TRIT_UNKNOWN);
    EXPECT_EQ(trit_vector_get(result, 1), TRIT_POSITIVE);

    trit_matrix_destroy(mat);
    trit_vector_destroy(vec);
    trit_vector_destroy(result);
}

TEST_F(TernaryOperationsTest, VectorElementwiseAdd) {
    const size_t n = 4;
    trit_vector_t* a = trit_vector_create(n, TERNARY_PACK_NONE);
    trit_vector_t* b = trit_vector_create(n, TERNARY_PACK_NONE);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // a = [+1, -1,  0, +1]
    // b = [+1, +1, -1, -1]
    // a+b= [+1,  0, -1,  0] (clamped)
    trit_vector_set(a, 0, TRIT_POSITIVE);
    trit_vector_set(a, 1, TRIT_NEGATIVE);
    trit_vector_set(a, 2, TRIT_UNKNOWN);
    trit_vector_set(a, 3, TRIT_POSITIVE);

    trit_vector_set(b, 0, TRIT_POSITIVE);
    trit_vector_set(b, 1, TRIT_POSITIVE);
    trit_vector_set(b, 2, TRIT_NEGATIVE);
    trit_vector_set(b, 3, TRIT_NEGATIVE);

    trit_vector_t* result = trit_vector_add(a, b);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(trit_vector_get(result, 0), TRIT_POSITIVE);  // 1+1=2 -> clamped to +1
    EXPECT_EQ(trit_vector_get(result, 1), TRIT_UNKNOWN);   // -1+1=0
    EXPECT_EQ(trit_vector_get(result, 2), TRIT_NEGATIVE);  // 0+(-1)=-1
    EXPECT_EQ(trit_vector_get(result, 3), TRIT_UNKNOWN);   // 1+(-1)=0

    trit_vector_destroy(a);
    trit_vector_destroy(b);
    trit_vector_destroy(result);
}

TEST_F(TernaryOperationsTest, VectorMajorityVoting) {
    const size_t n = 7;
    trit_vector_t* votes = trit_vector_create(n, TERNARY_PACK_NONE);
    ASSERT_NE(votes, nullptr);

    // 4 positive, 2 negative, 1 unknown -> majority is POSITIVE
    trit_vector_set(votes, 0, TRIT_POSITIVE);
    trit_vector_set(votes, 1, TRIT_POSITIVE);
    trit_vector_set(votes, 2, TRIT_POSITIVE);
    trit_vector_set(votes, 3, TRIT_POSITIVE);
    trit_vector_set(votes, 4, TRIT_NEGATIVE);
    trit_vector_set(votes, 5, TRIT_NEGATIVE);
    trit_vector_set(votes, 6, TRIT_UNKNOWN);

    trit_t majority = trit_vector_majority(votes);
    EXPECT_EQ(majority, TRIT_POSITIVE);

    trit_vector_destroy(votes);
}

TEST_F(TernaryOperationsTest, VectorHammingDistance) {
    const size_t n = 8;
    trit_vector_t* a = trit_vector_create(n, TERNARY_PACK_NONE);
    trit_vector_t* b = trit_vector_create(n, TERNARY_PACK_NONE);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Set identical vectors
    for (size_t i = 0; i < n; i++) {
        trit_t val = (trit_t)((i % 3) - 1);
        trit_vector_set(a, i, val);
        trit_vector_set(b, i, val);
    }

    // Hamming distance should be 0 for identical vectors
    EXPECT_EQ(trit_vector_hamming(a, b), 0UL);

    // Change 3 elements
    trit_vector_set(b, 0, TRIT_POSITIVE);  // Was NEGATIVE
    trit_vector_set(b, 3, TRIT_POSITIVE);  // Was NEGATIVE
    trit_vector_set(b, 6, TRIT_POSITIVE);  // Was NEGATIVE

    EXPECT_EQ(trit_vector_hamming(a, b), 3UL);

    trit_vector_destroy(a);
    trit_vector_destroy(b);
}

//=============================================================================
// Memory Savings Verification Tests
//=============================================================================

class MemorySavingsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MemorySavingsTest, PackedVectorMemorySavings) {
    const size_t weight_count = 10000;

    // Calculate memory for different representations
    size_t float_bytes = weight_count * sizeof(float);              // 40,000 bytes
    size_t int8_bytes = weight_count * sizeof(int8_t);              // 10,000 bytes
    size_t unpacked_bytes = weight_count * sizeof(trit_t);          // 10,000 bytes
    size_t packed_2bit_bytes = trit_packed_bytes(weight_count, TERNARY_PACK_2BIT);    // 2,500 bytes
    size_t packed_243_bytes = trit_packed_bytes(weight_count, TERNARY_PACK_BASE243);  // 2,000 bytes

    // Verify memory savings ratios
    float savings_vs_float_2bit = 1.0f - (float)packed_2bit_bytes / float_bytes;
    float savings_vs_float_243 = 1.0f - (float)packed_243_bytes / float_bytes;
    float savings_vs_int8_2bit = 1.0f - (float)packed_2bit_bytes / int8_bytes;
    float savings_vs_int8_243 = 1.0f - (float)packed_243_bytes / int8_bytes;

    // 2-bit packing: 4 trits per byte -> 75% savings vs int8
    EXPECT_GT(savings_vs_float_2bit, 0.90f);  // >90% vs float
    EXPECT_GT(savings_vs_int8_2bit, 0.70f);   // >70% vs int8

    // Base-243 packing: 5 trits per byte -> 80% savings vs int8
    EXPECT_GT(savings_vs_float_243, 0.90f);   // >90% vs float
    EXPECT_GT(savings_vs_int8_243, 0.75f);    // >75% vs int8

    // Base-243 should be more efficient than 2-bit
    EXPECT_LT(packed_243_bytes, packed_2bit_bytes);
}

TEST_F(MemorySavingsTest, MatrixMemorySavings) {
    const size_t rows = 1000;
    const size_t cols = 1000;
    const size_t total = rows * cols;  // 1 million weights

    size_t float_matrix_bytes = total * sizeof(float);  // 4 MB
    size_t packed_matrix_bytes = trit_packed_bytes(total, TERNARY_PACK_BASE243);  // ~200 KB

    float savings = 1.0f - (float)packed_matrix_bytes / float_matrix_bytes;

    // Should achieve >95% savings for large matrices
    EXPECT_GT(savings, 0.95f);

    // Verify exact calculation
    // 1,000,000 trits / 5 trits per byte = 200,000 bytes
    EXPECT_EQ(packed_matrix_bytes, (total + 4) / 5);
}

TEST_F(MemorySavingsTest, NeuralLayerMemorySavings) {
    // Simulate a typical neural layer: 768 input x 768 output
    const size_t input_dim = 768;
    const size_t output_dim = 768;
    const size_t weights = input_dim * output_dim;  // 589,824 weights

    // Float32 representation
    size_t float32_bytes = weights * sizeof(float);  // 2.25 MB

    // Ternary packed representation
    size_t ternary_bytes = trit_packed_bytes(weights, TERNARY_PACK_BASE243);  // ~118 KB

    float compression_ratio = (float)float32_bytes / ternary_bytes;
    float memory_reduction = 1.0f - (float)ternary_bytes / float32_bytes;

    // Should achieve ~19x compression
    EXPECT_GT(compression_ratio, 18.0f);
    EXPECT_GT(memory_reduction, 0.94f);
}

TEST_F(MemorySavingsTest, ActualAllocationVerification) {
    const size_t count = 5000;

    // Allocate packed vector
    trit_vector_t* packed = trit_vector_create(count, TERNARY_PACK_BASE243);
    ASSERT_NE(packed, nullptr);

    // Initialize with data
    for (size_t i = 0; i < count; i++) {
        trit_vector_set(packed, i, (trit_t)((i % 3) - 1));
    }

    // Verify data integrity after packing
    for (size_t i = 0; i < count; i++) {
        trit_t expected = (trit_t)((i % 3) - 1);
        EXPECT_EQ(trit_vector_get(packed, i), expected);
    }

    // The packed storage bytes should be close to theoretical minimum
    size_t expected_bytes = trit_packed_bytes(count, TERNARY_PACK_BASE243);
    EXPECT_EQ(expected_bytes, (count + 4) / 5);  // 1000 bytes for 5000 trits

    trit_vector_destroy(packed);
}

//=============================================================================
// Edge Cases and Boundary Conditions
//=============================================================================

class TernaryEdgeCasesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TernaryEdgeCasesTest, EmptyVector) {
    // Creating empty vector should work
    trit_vector_t* empty = trit_vector_create(0, TERNARY_PACK_NONE);
    // Behavior depends on implementation - could be NULL or valid with length 0
    if (empty != nullptr) {
        EXPECT_EQ(empty->length, 0UL);
        trit_vector_destroy(empty);
    }
}

TEST_F(TernaryEdgeCasesTest, SingleElement) {
    trit_vector_t* single = trit_vector_create(1, TERNARY_PACK_NONE);
    ASSERT_NE(single, nullptr);
    EXPECT_EQ(single->length, 1UL);

    trit_vector_set(single, 0, TRIT_POSITIVE);
    EXPECT_EQ(trit_vector_get(single, 0), TRIT_POSITIVE);

    trit_vector_destroy(single);
}

TEST_F(TernaryEdgeCasesTest, PackingBoundaries) {
    // Test at packing boundaries
    // 2-bit: 4 trits per byte
    // Base-243: 5 trits per byte

    for (size_t n = 1; n <= 10; n++) {
        trit_vector_t* vec_2bit = trit_vector_create(n, TERNARY_PACK_2BIT);
        trit_vector_t* vec_243 = trit_vector_create(n, TERNARY_PACK_BASE243);

        if (vec_2bit != nullptr && vec_243 != nullptr) {
            // Initialize
            for (size_t i = 0; i < n; i++) {
                trit_t val = (trit_t)((i % 3) - 1);
                trit_vector_set(vec_2bit, i, val);
                trit_vector_set(vec_243, i, val);
            }

            // Verify
            for (size_t i = 0; i < n; i++) {
                trit_t expected = (trit_t)((i % 3) - 1);
                EXPECT_EQ(trit_vector_get(vec_2bit, i), expected);
                EXPECT_EQ(trit_vector_get(vec_243, i), expected);
            }
        }

        trit_vector_destroy(vec_2bit);
        trit_vector_destroy(vec_243);
    }
}

TEST_F(TernaryEdgeCasesTest, AllSameValue) {
    const size_t n = 100;

    for (trit_t val : {TRIT_NEGATIVE, TRIT_UNKNOWN, TRIT_POSITIVE}) {
        trit_vector_t* vec = trit_vector_create_filled(n, val, TERNARY_PACK_NONE);
        ASSERT_NE(vec, nullptr);

        for (size_t i = 0; i < n; i++) {
            EXPECT_EQ(trit_vector_get(vec, i), val);
        }

        trit_vector_destroy(vec);
    }
}

TEST_F(TernaryEdgeCasesTest, LargeVector) {
    const size_t n = 100000;

    trit_vector_t* large = trit_vector_create(n, TERNARY_PACK_BASE243);
    ASSERT_NE(large, nullptr);
    EXPECT_EQ(large->length, n);

    // Initialize with pattern
    for (size_t i = 0; i < n; i++) {
        trit_vector_set(large, i, (trit_t)((i % 3) - 1));
    }

    // Verify random samples
    for (size_t i = 0; i < n; i += 1000) {
        trit_t expected = (trit_t)((i % 3) - 1);
        EXPECT_EQ(trit_vector_get(large, i), expected);
    }

    trit_vector_destroy(large);
}

TEST_F(TernaryEdgeCasesTest, InvalidTritValue) {
    // Test handling of invalid trit values
    EXPECT_FALSE(trit_is_valid((trit_t)-2));
    EXPECT_FALSE(trit_is_valid((trit_t)2));
    EXPECT_FALSE(trit_is_valid((trit_t)100));

    // Valid values
    EXPECT_TRUE(trit_is_valid(TRIT_NEGATIVE));
    EXPECT_TRUE(trit_is_valid(TRIT_UNKNOWN));
    EXPECT_TRUE(trit_is_valid(TRIT_POSITIVE));
}

TEST_F(TernaryEdgeCasesTest, FloatSpecialValues) {
    // Test conversion with special float values
    EXPECT_EQ(trit_from_float_sign(INFINITY), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_sign(-INFINITY), TRIT_NEGATIVE);

    // NaN behavior (implementation-defined, but should not crash)
    trit_t nan_result = trit_from_float_sign(NAN);
    EXPECT_TRUE(trit_is_valid(nan_result) || nan_result == TRIT_UNKNOWN);
}

//=============================================================================
// Neural Weight Quantization Tests
//=============================================================================

class NeuralWeightQuantizationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(NeuralWeightQuantizationTest, StatisticalQuantization) {
    // Test quantization based on weight statistics
    const float mean = 0.0f;
    const float std_dev = 1.0f;
    const float threshold_scale = 0.5f;

    // Weights > 0.5*std are positive
    EXPECT_EQ(trit_quantize_weight(1.0f, threshold_scale, mean, std_dev), TRIT_POSITIVE);
    EXPECT_EQ(trit_quantize_weight(0.6f, threshold_scale, mean, std_dev), TRIT_POSITIVE);

    // Weights < -0.5*std are negative
    EXPECT_EQ(trit_quantize_weight(-1.0f, threshold_scale, mean, std_dev), TRIT_NEGATIVE);
    EXPECT_EQ(trit_quantize_weight(-0.6f, threshold_scale, mean, std_dev), TRIT_NEGATIVE);

    // Weights in [-0.5*std, 0.5*std] are unknown (zero)
    EXPECT_EQ(trit_quantize_weight(0.3f, threshold_scale, mean, std_dev), TRIT_UNKNOWN);
    EXPECT_EQ(trit_quantize_weight(-0.3f, threshold_scale, mean, std_dev), TRIT_UNKNOWN);
    EXPECT_EQ(trit_quantize_weight(0.0f, threshold_scale, mean, std_dev), TRIT_UNKNOWN);
}

TEST_F(NeuralWeightQuantizationTest, Dequantization) {
    // Test dequantization to float
    const float pos_scale = 0.8f;
    const float neg_scale = -0.6f;

    EXPECT_FLOAT_EQ(trit_dequantize_weight(TRIT_POSITIVE, pos_scale, neg_scale), pos_scale);
    EXPECT_FLOAT_EQ(trit_dequantize_weight(TRIT_NEGATIVE, pos_scale, neg_scale), neg_scale);
    EXPECT_FLOAT_EQ(trit_dequantize_weight(TRIT_UNKNOWN, pos_scale, neg_scale), 0.0f);
}

TEST_F(NeuralWeightQuantizationTest, AsymmetricDequantization) {
    // Asymmetric scales (common in trained networks)
    const float pos_scale = 0.7f;
    const float neg_scale = -0.4f;

    EXPECT_FLOAT_EQ(trit_dequantize_weight(TRIT_POSITIVE, pos_scale, neg_scale), 0.7f);
    EXPECT_FLOAT_EQ(trit_dequantize_weight(TRIT_NEGATIVE, pos_scale, neg_scale), -0.4f);
}

TEST_F(NeuralWeightQuantizationTest, ProbabilisticConversion) {
    // Test stochastic quantization
    float rand_val = 0.0f;

    // With x=0.5, p_pos=0.5, p_neg=0
    // rand_val < p_neg (0) -> false
    // rand_val < p_neg + p_pos (0.5) -> true for rand_val=0
    trit_t result = trit_from_float_stochastic(0.5f, rand_val);
    // rand_val=0 is < p_neg (0.5)? No. < p_neg+p_pos (0.5)? Yes -> POSITIVE
    EXPECT_EQ(result, TRIT_POSITIVE);

    // With x=-0.5, p_pos=0, p_neg=0.5
    result = trit_from_float_stochastic(-0.5f, rand_val);
    // rand_val=0 < p_neg (0.5)? Yes -> NEGATIVE
    EXPECT_EQ(result, TRIT_NEGATIVE);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
