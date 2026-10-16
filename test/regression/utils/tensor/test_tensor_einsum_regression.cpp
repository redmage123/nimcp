//=============================================================================
// test_tensor_einsum_regression.cpp - Regression Tests for Einsum
//=============================================================================
/**
 * @file test_tensor_einsum_regression.cpp
 * @brief Regression tests for einsum correctness and stability
 *
 * WHAT: Regression suite verifying einsum against known mathematical results
 * WHY:  Detect regressions in einsum computation, memory handling, and consistency
 * HOW:  Fixed test cases with pre-computed expected values
 *
 * TEST COVERAGE:
 * 1. Identity matrix multiply produces same matrix
 * 2. Matrix multiply with zeros produces zeros
 * 3. Large tensor einsum does not crash
 * 4. Repeated einsum calls produce consistent results
 * 5. Einsum trace matches nimcp_tensor_trace
 * 6. Einsum matmul matches nimcp_tensor_matmul
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>

// Headers have their own extern "C" guards
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TensorEinsumRegressionTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_tensor_init();
    }

    void TearDown() override {
        nimcp_tensor_shutdown();
    }
};

//=============================================================================
// Regression Tests: Known Mathematical Results
//=============================================================================

TEST_F(TensorEinsumRegressionTest, IdentityMatrixMultiply_ProducesSameMatrix) {
    // WHAT: A @ I = A (multiplying by identity gives back original)
    // WHY:  Fundamental property; any failure indicates broken contraction
    //
    // A = [[1,2,3],[4,5,6]], I = eye(3)
    // A @ I = A

    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    uint32_t a_dims[] = {2, 3};
    nimcp_tensor_t* a = nimcp_tensor_from_data(a_data, a_dims, 2, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* identity = nimcp_tensor_eye(3, NIMCP_DTYPE_F32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(identity, nullptr);

    nimcp_tensor_t* tensors[] = {a, identity};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
    ASSERT_NE(result, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(result);
    EXPECT_EQ(shape->dims[0], 2u);
    EXPECT_EQ(shape->dims[1], 3u);

    // Result should equal A
    for (size_t i = 0; i < nimcp_tensor_numel(a); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(result, i),
                    nimcp_tensor_get_flat(a, i), EPSILON)
            << "Mismatch at flat index " << i;
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(identity);
    nimcp_tensor_destroy(result);
}

TEST_F(TensorEinsumRegressionTest, ZeroMatrixMultiply_ProducesZeros) {
    // WHAT: A @ 0 = 0 (multiplying by zero matrix gives zeros)
    // WHY:  Boundary condition; ensures accumulation starts at zero

    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t dims[] = {2, 2};
    nimcp_tensor_t* a = nimcp_tensor_from_data(a_data, dims, 2, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* zero = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(zero, nullptr);

    nimcp_tensor_t* tensors[] = {a, zero};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
    ASSERT_NE(result, nullptr);

    for (size_t i = 0; i < nimcp_tensor_numel(result); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(result, i), 0.0f, EPSILON)
            << "Non-zero at flat index " << i;
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(zero);
    nimcp_tensor_destroy(result);
}

TEST_F(TensorEinsumRegressionTest, EinsumMatMul_MatchesDirectMatMul) {
    // WHAT: Einsum "ij,jk->ik" must match nimcp_tensor_matmul exactly
    // WHY:  Two code paths for the same operation must agree

    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    uint32_t a_dims[] = {2, 3};
    nimcp_tensor_t* a = nimcp_tensor_from_data(a_data, a_dims, 2, NIMCP_DTYPE_F32, true);

    float b_data[] = {7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f};
    uint32_t b_dims[] = {3, 2};
    nimcp_tensor_t* b = nimcp_tensor_from_data(b_data, b_dims, 2, NIMCP_DTYPE_F32, true);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Compute via einsum
    nimcp_tensor_t* tensors[] = {a, b};
    nimcp_tensor_t* einsum_result = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
    ASSERT_NE(einsum_result, nullptr);

    // Compute via matmul
    nimcp_tensor_t* matmul_result = nimcp_tensor_matmul(a, b);
    ASSERT_NE(matmul_result, nullptr);

    // Both must have same shape
    EXPECT_EQ(nimcp_tensor_numel(einsum_result), nimcp_tensor_numel(matmul_result));

    // Both must have same values
    for (size_t i = 0; i < nimcp_tensor_numel(einsum_result); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(einsum_result, i),
                    nimcp_tensor_get_flat(matmul_result, i), EPSILON)
            << "Einsum and matmul disagree at index " << i;
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(einsum_result);
    nimcp_tensor_destroy(matmul_result);
}

TEST_F(TensorEinsumRegressionTest, EinsumTrace_MatchesDirectTrace) {
    // WHAT: Einsum "ii->" must match nimcp_tensor_trace
    // WHY:  Two code paths for trace must agree

    float data[] = {2.0f, 3.0f, 5.0f, 7.0f, 11.0f, 13.0f, 17.0f, 19.0f, 23.0f};
    uint32_t dims[] = {3, 3};
    nimcp_tensor_t* a = nimcp_tensor_from_data(data, dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(a, nullptr);

    // Direct trace
    double direct_trace = nimcp_tensor_trace(a);

    // Einsum trace
    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* einsum_trace = nimcp_tensor_einsum("ii->", tensors, 1);
    ASSERT_NE(einsum_trace, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(einsum_trace, 0), direct_trace, EPSILON)
        << "Einsum trace and direct trace disagree";

    // Verify the actual value: 2 + 11 + 23 = 36
    EXPECT_NEAR(direct_trace, 36.0, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(einsum_trace);
}

//=============================================================================
// Regression Tests: Stability
//=============================================================================

TEST_F(TensorEinsumRegressionTest, LargeTensor_NoMemoryCorruption) {
    // WHAT: Einsum on moderately large tensors does not crash or corrupt memory
    // WHY:  Buffer overflows or off-by-one errors may only manifest at larger sizes

    uint32_t a_dims[] = {32, 64};
    uint32_t b_dims[] = {64, 32};
    nimcp_tensor_t* a = nimcp_tensor_randn(a_dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* b = nimcp_tensor_randn(b_dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    nimcp_tensor_t* tensors[] = {a, b};
    nimcp_tensor_t* c = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
    ASSERT_NE(c, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(c);
    EXPECT_EQ(shape->dims[0], 32u);
    EXPECT_EQ(shape->dims[1], 32u);
    EXPECT_EQ(shape->numel, 1024u);

    // Verify we can read all elements without segfault
    for (size_t i = 0; i < nimcp_tensor_numel(c); i++) {
        double val = nimcp_tensor_get_flat(c, i);
        EXPECT_FALSE(std::isnan(val)) << "NaN at index " << i;
        EXPECT_FALSE(std::isinf(val)) << "Inf at index " << i;
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

TEST_F(TensorEinsumRegressionTest, RepeatedCalls_ConsistentResults) {
    // WHAT: Calling einsum multiple times with same inputs gives same result
    // WHY:  Detect state corruption between calls (stale counters, etc.)

    nimcp_tensor_t* a = nimcp_tensor_eye(3, NIMCP_DTYPE_F32);
    nimcp_tensor_t* b = nimcp_tensor_eye(3, NIMCP_DTYPE_F32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Set b to known non-identity values
    nimcp_tensor_set_flat(b, 1, 2.0);
    nimcp_tensor_set_flat(b, 3, 3.0);

    nimcp_tensor_t* tensors[] = {a, b};

    // Run einsum 5 times
    nimcp_tensor_t* results[5];
    for (int trial = 0; trial < 5; trial++) {
        results[trial] = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
        ASSERT_NE(results[trial], nullptr) << "Trial " << trial << " returned NULL";
    }

    // All results must match the first
    for (int trial = 1; trial < 5; trial++) {
        EXPECT_EQ(nimcp_tensor_numel(results[trial]), nimcp_tensor_numel(results[0]));
        for (size_t i = 0; i < nimcp_tensor_numel(results[0]); i++) {
            EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(results[trial], i),
                            nimcp_tensor_get_flat(results[0], i))
                << "Trial " << trial << " differs at index " << i;
        }
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    for (int trial = 0; trial < 5; trial++) {
        nimcp_tensor_destroy(results[trial]);
    }
}

TEST_F(TensorEinsumRegressionTest, TransposeOfTranspose_IsOriginal) {
    // WHAT: Transposing twice via einsum returns original matrix
    // WHY:  (A^T)^T = A is a mathematical invariant

    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    uint32_t dims[] = {2, 3};
    nimcp_tensor_t* a = nimcp_tensor_from_data(data, dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(a, nullptr);

    // First transpose: "ij->ji" (2x3 -> 3x2)
    nimcp_tensor_t* tensors1[] = {a};
    nimcp_tensor_t* at = nimcp_tensor_einsum("ij->ji", tensors1, 1);
    ASSERT_NE(at, nullptr);

    // Second transpose: "ij->ji" (3x2 -> 2x3)
    nimcp_tensor_t* tensors2[] = {at};
    nimcp_tensor_t* att = nimcp_tensor_einsum("ij->ji", tensors2, 1);
    ASSERT_NE(att, nullptr);

    // att should equal a
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(att);
    EXPECT_EQ(shape->dims[0], 2u);
    EXPECT_EQ(shape->dims[1], 3u);

    for (size_t i = 0; i < nimcp_tensor_numel(a); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(att, i),
                    nimcp_tensor_get_flat(a, i), EPSILON)
            << "Double transpose differs at index " << i;
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(at);
    nimcp_tensor_destroy(att);
}

TEST_F(TensorEinsumRegressionTest, Associativity_ABC) {
    // WHAT: (A@B)@C == A@(B@C) via einsum
    // WHY:  Matrix multiplication is associative; verify einsum preserves this
    //
    // A: 2x3, B: 3x4, C: 4x2

    uint32_t a_dims[] = {2, 3};
    uint32_t b_dims[] = {3, 4};
    uint32_t c_dims[] = {4, 2};

    nimcp_tensor_t* a = nimcp_tensor_full(a_dims, 2, NIMCP_DTYPE_F32, 1.0);
    nimcp_tensor_t* b = nimcp_tensor_full(b_dims, 2, NIMCP_DTYPE_F32, 2.0);
    nimcp_tensor_t* c = nimcp_tensor_full(c_dims, 2, NIMCP_DTYPE_F32, 3.0);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    // (A@B)@C: first A@B, then result@C
    nimcp_tensor_t* ab_tensors[] = {a, b};
    nimcp_tensor_t* ab = nimcp_tensor_einsum("ij,jk->ik", ab_tensors, 2);
    ASSERT_NE(ab, nullptr);

    nimcp_tensor_t* abc_left_tensors[] = {ab, c};
    nimcp_tensor_t* abc_left = nimcp_tensor_einsum("ij,jk->ik", abc_left_tensors, 2);
    ASSERT_NE(abc_left, nullptr);

    // A@(B@C): first B@C, then A@result
    nimcp_tensor_t* bc_tensors[] = {b, c};
    nimcp_tensor_t* bc = nimcp_tensor_einsum("ij,jk->ik", bc_tensors, 2);
    ASSERT_NE(bc, nullptr);

    nimcp_tensor_t* abc_right_tensors[] = {a, bc};
    nimcp_tensor_t* abc_right = nimcp_tensor_einsum("ij,jk->ik", abc_right_tensors, 2);
    ASSERT_NE(abc_right, nullptr);

    // Both paths should give the same result
    EXPECT_EQ(nimcp_tensor_numel(abc_left), nimcp_tensor_numel(abc_right));
    for (size_t i = 0; i < nimcp_tensor_numel(abc_left); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(abc_left, i),
                    nimcp_tensor_get_flat(abc_right, i), EPSILON)
            << "Associativity violated at index " << i;
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
    nimcp_tensor_destroy(ab);
    nimcp_tensor_destroy(bc);
    nimcp_tensor_destroy(abc_left);
    nimcp_tensor_destroy(abc_right);
}

TEST_F(TensorEinsumRegressionTest, TraceOfOnesMatrix) {
    // WHAT: Trace of NxN ones matrix = N
    // WHY:  Simple invariant; each diagonal element is 1

    for (uint32_t n = 1; n <= 5; n++) {
        uint32_t dims[] = {n, n};
        nimcp_tensor_t* ones = nimcp_tensor_ones(dims, 2, NIMCP_DTYPE_F32);
        ASSERT_NE(ones, nullptr) << "Failed to create " << n << "x" << n << " ones matrix";

        nimcp_tensor_t* tensors[] = {ones};
        nimcp_tensor_t* trace = nimcp_tensor_einsum("ii->", tensors, 1);
        ASSERT_NE(trace, nullptr) << "Einsum trace failed for " << n << "x" << n;

        EXPECT_NEAR(nimcp_tensor_get_flat(trace, 0), (float)n, EPSILON)
            << "Trace of " << n << "x" << n << " ones matrix should be " << n;

        nimcp_tensor_destroy(ones);
        nimcp_tensor_destroy(trace);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
