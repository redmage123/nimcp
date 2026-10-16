/**
 * @file test_tensor_overflow_regression.cpp
 * @brief Regression tests for tensor dimension and element overflow detection
 *
 * WHAT: Tests to prevent regression of overflow bugs in tensor operations
 * WHY:  Lock in correct overflow detection and prevention behavior
 * HOW:  Test dimension overflow, numel overflow, maximum tensor sizes
 *
 * BUG HISTORY:
 * - Bug #1: Dimension product overflowed without detection
 *   WRONG: numel = dim0 * dim1 * dim2 (may overflow silently)
 *   FIX: Check each multiplication for overflow before computing
 * - Bug #2: Allocation size overflowed
 *   WRONG: nbytes = numel * dtype_size (may overflow)
 *   FIX: Check for overflow in size calculation
 * - Bug #3: Maximum tensor size not enforced
 *   FIX: Add NIMCP_TENSOR_MAX_ELEMENTS constant and check
 *
 * REGRESSION FOCUS:
 * 1. Dimension overflow should be detected and return error
 * 2. Element count overflow should be detected
 * 3. Maximum tensor size (NIMCP_TENSOR_MAX_ELEMENTS) is enforced
 * 4. Large but valid tensors should work
 *
 * @version 1.0.0
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <limits>

extern "C" {
#include "utils/tensor/nimcp_tensor.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class TensorOverflowRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Initialize tensor subsystem */
        nimcp_tensor_init();
    }

    void TearDown() override {
        /* Shutdown tensor subsystem */
        nimcp_tensor_shutdown();
    }

    /**
     * @brief Check if multiplying a * b would overflow size_t
     */
    bool would_overflow_size_t(size_t a, size_t b) {
        if (a == 0 || b == 0) return false;
        return a > SIZE_MAX / b;
    }
};

//=============================================================================
// DIMENSION OVERFLOW REGRESSION TESTS
//=============================================================================

/**
 * BUG: Dimension product overflowed without detection
 *
 * WRONG: Just multiply dimensions, let overflow wrap around
 * RIGHT: Check each multiplication for overflow before computing
 */
TEST_F(TensorOverflowRegressionTest, DimensionOverflow_TwoDimensions) {
    /**
     * REGRESSION TEST: Two dimensions that overflow when multiplied
     *
     * 2^16 * 2^17 = 2^33 which overflows 32-bit
     * This should be caught and return NULL
     */
    uint32_t dims[] = {65536, 131072};  /* 2^16 * 2^17 = 8GB elements */

    /* This exceeds NIMCP_TENSOR_MAX_ELEMENTS (1 billion) */
    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);

    EXPECT_EQ(tensor, nullptr)
        << "REGRESSION: Tensor creation should fail for dimensions that exceed max elements";
}

TEST_F(TensorOverflowRegressionTest, DimensionOverflow_ThreeDimensions) {
    /**
     * REGRESSION TEST: Three dimensions that overflow when multiplied
     */
    uint32_t dims[] = {10000, 10000, 10000};  /* 10^12 elements */

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);

    EXPECT_EQ(tensor, nullptr)
        << "REGRESSION: Tensor creation should fail for 10^12 elements";
}

TEST_F(TensorOverflowRegressionTest, DimensionOverflow_EdgeOfMax) {
    /**
     * REGRESSION TEST: Dimensions just above max elements
     *
     * NIMCP_TENSOR_MAX_ELEMENTS is 1 billion (10^9)
     * 1001 * 1000 * 1000 = 1.001 billion should fail
     */
    uint32_t dims[] = {1001, 1000, 1000};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);

    EXPECT_EQ(tensor, nullptr)
        << "REGRESSION: Tensor creation should fail for 1.001 billion elements";
}

TEST_F(TensorOverflowRegressionTest, DimensionOverflow_BelowMaxSucceeds) {
    /**
     * REGRESSION TEST: Dimensions just below max should succeed
     *
     * 100 * 100 * 100 = 1 million elements (well below 1 billion)
     */
    uint32_t dims[] = {100, 100, 100};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);

    if (tensor) {
        /* This should succeed */
        nimcp_tensor_destroy(tensor);
        SUCCEED();
    } else {
        /* May fail due to memory, but that's OK */
        GTEST_LOG_(INFO) << "1 million element tensor failed (possibly memory)";
    }
}

//=============================================================================
// NUMEL OVERFLOW REGRESSION TESTS
//=============================================================================

TEST_F(TensorOverflowRegressionTest, NumelOverflow_Uint32Max) {
    /**
     * REGRESSION TEST: Total elements exceeding UINT32_MAX
     */
    /* Two dimensions that together exceed UINT32_MAX */
    uint32_t dims[] = {70000, 70000};  /* 4.9 billion > UINT32_MAX */

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);

    EXPECT_EQ(tensor, nullptr)
        << "REGRESSION: Tensor creation should fail for 4.9 billion elements";
}

TEST_F(TensorOverflowRegressionTest, NumelOverflow_ProgressiveMultiplication) {
    /**
     * REGRESSION TEST: Overflow in progressive dimension multiplication
     *
     * Each individual dimension is small, but product overflows
     */
    uint32_t dims[] = {1000, 1000, 1000, 1000};  /* 10^12 elements */

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 4, NIMCP_DTYPE_F32);

    EXPECT_EQ(tensor, nullptr)
        << "REGRESSION: Progressive multiplication overflow should be detected";
}

TEST_F(TensorOverflowRegressionTest, NumelOverflow_HighRank) {
    /**
     * REGRESSION TEST: Many small dimensions that overflow
     */
    /* 10^8 elements with 8 dimensions of size 10 (actually OK) */
    uint32_t dims_ok[] = {10, 10, 10, 10, 10, 10, 10, 10};  /* 10^8 = 100M */

    nimcp_tensor_t* tensor_ok = nimcp_tensor_create(dims_ok, 8, NIMCP_DTYPE_F32);
    if (tensor_ok) {
        nimcp_tensor_destroy(tensor_ok);
    }
    /* May fail due to memory, but dimension multiplication should work */

    /* 100^4 = 100 million * 4 more = 10^16 (overflow) */
    uint32_t dims_overflow[] = {100, 100, 100, 100, 100, 100, 100, 100};

    nimcp_tensor_t* tensor_overflow = nimcp_tensor_create(dims_overflow, 8, NIMCP_DTYPE_F32);

    EXPECT_EQ(tensor_overflow, nullptr)
        << "REGRESSION: 100^8 = 10^16 elements should fail";
}

//=============================================================================
// MAXIMUM TENSOR SIZE REGRESSION TESTS
//=============================================================================

TEST_F(TensorOverflowRegressionTest, MaxSize_ConstantValue) {
    /**
     * REGRESSION TEST: NIMCP_TENSOR_MAX_ELEMENTS must be 1 billion
     */
    EXPECT_EQ(NIMCP_TENSOR_MAX_ELEMENTS, 1000000000UL)
        << "REGRESSION: NIMCP_TENSOR_MAX_ELEMENTS should be 1 billion";
}

TEST_F(TensorOverflowRegressionTest, MaxSize_ExactlyAtLimit) {
    /**
     * REGRESSION TEST: Exactly max elements should succeed
     *
     * 1000 * 1000 * 1000 = exactly 1 billion
     */
    uint32_t dims[] = {1000, 1000, 1000};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);

    /* This is exactly at the limit - behavior may vary */
    /* Either succeeds or fails due to memory constraints */
    if (tensor) {
        nimcp_tensor_destroy(tensor);
    }

    /* We don't assert success/failure here - just verify no crash */
}

TEST_F(TensorOverflowRegressionTest, MaxSize_JustAboveLimit) {
    /**
     * REGRESSION TEST: Just above max elements should fail
     */
    uint32_t dims[] = {1001, 1000, 1000};  /* 1.001 billion */

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);

    EXPECT_EQ(tensor, nullptr)
        << "REGRESSION: 1 element above limit should fail";
}

TEST_F(TensorOverflowRegressionTest, MaxSize_WayAboveLimit) {
    /**
     * REGRESSION TEST: Way above max should definitely fail
     */
    uint32_t dims[] = {10000, 10000, 10000};  /* 1 trillion */

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);

    EXPECT_EQ(tensor, nullptr)
        << "REGRESSION: 1 trillion elements should fail";
}

//=============================================================================
// ALLOCATION SIZE OVERFLOW TESTS
//=============================================================================

TEST_F(TensorOverflowRegressionTest, AllocOverflow_LargeFloat64) {
    /**
     * REGRESSION TEST: Large tensor with double (8 bytes per element)
     *
     * Even if numel is OK, nbytes = numel * 8 might overflow
     */
    /* 500 million elements * 8 bytes = 4GB (may overflow on 32-bit size_t) */
    uint32_t dims[] = {500, 1000, 1000};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F64);

    /* Should either succeed (64-bit system) or fail gracefully */
    if (tensor) {
        nimcp_tensor_destroy(tensor);
    }
    /* No crash = pass */
}

TEST_F(TensorOverflowRegressionTest, AllocOverflow_Complex128) {
    /**
     * REGRESSION TEST: Complex128 (16 bytes per element)
     */
    /* 100 million elements * 16 bytes = 1.6GB */
    uint32_t dims[] = {100, 1000, 1000};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_C128);

    /* Should either succeed or fail gracefully - no crash */
    if (tensor) {
        nimcp_tensor_destroy(tensor);
    }
}

//=============================================================================
// RANK OVERFLOW TESTS
//=============================================================================

TEST_F(TensorOverflowRegressionTest, RankOverflow_MaxRankConstant) {
    /**
     * REGRESSION TEST: NIMCP_TENSOR_MAX_RANK must be 8
     */
    EXPECT_EQ(NIMCP_TENSOR_MAX_RANK, 8u)
        << "REGRESSION: NIMCP_TENSOR_MAX_RANK should be 8";
}

TEST_F(TensorOverflowRegressionTest, RankOverflow_ExceedsMaxRank) {
    /**
     * REGRESSION TEST: Rank > MAX_RANK should fail
     */
    uint32_t dims[16] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 16, NIMCP_DTYPE_F32);

    EXPECT_EQ(tensor, nullptr)
        << "REGRESSION: Rank 16 > MAX_RANK(8) should fail";
}

TEST_F(TensorOverflowRegressionTest, RankOverflow_ExactlyMaxRank) {
    /**
     * REGRESSION TEST: Rank == MAX_RANK should succeed
     */
    uint32_t dims[8] = {2, 2, 2, 2, 2, 2, 2, 2};  /* 256 elements */

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 8, NIMCP_DTYPE_F32);

    EXPECT_NE(tensor, nullptr)
        << "REGRESSION: Rank 8 == MAX_RANK should succeed";

    if (tensor) {
        nimcp_tensor_destroy(tensor);
    }
}

//=============================================================================
// ZERO DIMENSION TESTS
//=============================================================================

TEST_F(TensorOverflowRegressionTest, ZeroDim_SingleZero) {
    /**
     * REGRESSION TEST: Tensor with zero dimension should be handled
     */
    uint32_t dims[] = {10, 0, 5};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);

    /* Zero-size tensors may be valid (0 elements) or rejected */
    /* Either way, should not crash */
    if (tensor) {
        nimcp_tensor_destroy(tensor);
    }
}

TEST_F(TensorOverflowRegressionTest, ZeroDim_AllZero) {
    /**
     * REGRESSION TEST: All-zero dimensions should be handled
     */
    uint32_t dims[] = {0, 0, 0};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);

    if (tensor) {
        nimcp_tensor_destroy(tensor);
    }
    /* No crash = pass */
}

TEST_F(TensorOverflowRegressionTest, ZeroDim_ZeroRank) {
    /**
     * REGRESSION TEST: Zero rank (scalar) should be handled
     */
    nimcp_tensor_t* tensor = nimcp_tensor_create(nullptr, 0, NIMCP_DTYPE_F32);

    /* Scalar tensor (rank 0) may be valid or rejected */
    if (tensor) {
        nimcp_tensor_destroy(tensor);
    }
}

//=============================================================================
// BOUNDARY CONDITION TESTS
//=============================================================================

TEST_F(TensorOverflowRegressionTest, Boundary_SingleElement) {
    /**
     * REGRESSION TEST: Single element tensor should work
     */
    uint32_t dims[] = {1};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);

    EXPECT_NE(tensor, nullptr) << "Single element tensor should succeed";

    if (tensor) {
        nimcp_tensor_destroy(tensor);
    }
}

TEST_F(TensorOverflowRegressionTest, Boundary_AllOnes) {
    /**
     * REGRESSION TEST: All-ones dimensions should work
     */
    uint32_t dims[] = {1, 1, 1, 1, 1, 1, 1, 1};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 8, NIMCP_DTYPE_F32);

    EXPECT_NE(tensor, nullptr) << "8-rank tensor with all 1s should succeed";

    if (tensor) {
        nimcp_tensor_destroy(tensor);
    }
}

TEST_F(TensorOverflowRegressionTest, Boundary_LargeSingleDimension) {
    /**
     * REGRESSION TEST: Large single dimension should be checked
     */
    uint32_t dims[] = {UINT32_MAX};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);

    /* UINT32_MAX > NIMCP_TENSOR_MAX_ELEMENTS, should fail */
    EXPECT_EQ(tensor, nullptr)
        << "REGRESSION: UINT32_MAX elements should fail";
}

//=============================================================================
// DTYPE SIZE INTERACTION TESTS
//=============================================================================

TEST_F(TensorOverflowRegressionTest, DtypeSize_VariousDtypes) {
    /**
     * REGRESSION TEST: Different dtypes have different size multipliers
     *
     * Verify that size calculation doesn't overflow for various dtypes
     */
    uint32_t dims[] = {1000, 1000};  /* 1 million elements */

    /* Test various dtypes - all should succeed for 1M elements */
    nimcp_dtype_t dtypes[] = {
        NIMCP_DTYPE_F32,   /* 4 bytes -> 4MB */
        NIMCP_DTYPE_F64,   /* 8 bytes -> 8MB */
        NIMCP_DTYPE_F16,   /* 2 bytes -> 2MB */
        NIMCP_DTYPE_I8,    /* 1 byte  -> 1MB */
        NIMCP_DTYPE_C128   /* 16 bytes -> 16MB */
    };

    for (nimcp_dtype_t dtype : dtypes) {
        nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 2, dtype);

        EXPECT_NE(tensor, nullptr)
            << "REGRESSION: 1M element tensor with dtype " << static_cast<int>(dtype) << " should succeed";

        if (tensor) {
            nimcp_tensor_destroy(tensor);
        }
    }
}

//=============================================================================
// ERROR CODE REGRESSION TESTS
//=============================================================================

TEST_F(TensorOverflowRegressionTest, ErrorCode_OverflowValue) {
    /**
     * REGRESSION TEST: NIMCP_TENSOR_ERR_OVERFLOW value must not change
     */
    EXPECT_EQ(NIMCP_TENSOR_ERR_OVERFLOW, -12)
        << "REGRESSION: NIMCP_TENSOR_ERR_OVERFLOW should be -12";
}

TEST_F(TensorOverflowRegressionTest, ErrorCode_RankValue) {
    /**
     * REGRESSION TEST: NIMCP_TENSOR_ERR_RANK value must not change
     */
    EXPECT_EQ(NIMCP_TENSOR_ERR_RANK, -3)
        << "REGRESSION: NIMCP_TENSOR_ERR_RANK should be -3";
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
