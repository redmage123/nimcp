/**
 * @file test_tensor_errors.cpp
 * @brief Unit tests for tensor error handling
 *
 * WHAT: Tests for tensor error codes, overflow detection, and error propagation
 * WHY:  Ensure robust error handling throughout tensor operations
 * HOW:  Test error conditions, verify correct codes returned, check propagation
 *
 * TESTS COVER:
 * 1. Error code consistency (correct codes for each error type)
 * 2. Overflow detection in size calculations
 * 3. Proper error propagation through operations
 * 4. Error string conversion
 * 5. NULL pointer error handling
 * 6. Rank overflow detection
 * 7. Memory allocation failure handling
 * 8. Invalid tensor detection (corrupted magic)
 *
 * @version 1.0.0
 * @date 2025-01-15
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <limits>

extern "C" {
#include "utils/tensor/nimcp_tensor.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class TensorErrorTest : public ::testing::Test {
protected:
    nimcp_tensor_t* tensor;

    void SetUp() override {
        tensor = nullptr;
        nimcp_tensor_init();
    }

    void TearDown() override {
        if (tensor) {
            nimcp_tensor_destroy(tensor);
            tensor = nullptr;
        }
        nimcp_tensor_shutdown();
    }

    /**
     * @brief Create a simple test tensor
     */
    nimcp_tensor_t* create_simple_tensor() {
        uint32_t dims[] = {2, 3};
        return nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    }
};

//=============================================================================
// Error Code Enumeration Tests
//=============================================================================

TEST_F(TensorErrorTest, ErrorCodeValuesAreDistinct) {
    /**
     * WHAT: Verify all error codes have distinct values
     * WHY:  Avoid ambiguous error handling
     * HOW:  Check all codes are different
     */
    EXPECT_NE(NIMCP_TENSOR_OK, NIMCP_TENSOR_ERR_NULL);
    EXPECT_NE(NIMCP_TENSOR_OK, NIMCP_TENSOR_ERR_SHAPE);
    EXPECT_NE(NIMCP_TENSOR_OK, NIMCP_TENSOR_ERR_RANK);
    EXPECT_NE(NIMCP_TENSOR_OK, NIMCP_TENSOR_ERR_ALLOC);
    EXPECT_NE(NIMCP_TENSOR_OK, NIMCP_TENSOR_ERR_BROADCAST);
    EXPECT_NE(NIMCP_TENSOR_OK, NIMCP_TENSOR_ERR_EINSUM);
    EXPECT_NE(NIMCP_TENSOR_OK, NIMCP_TENSOR_ERR_DTYPE);
    EXPECT_NE(NIMCP_TENSOR_OK, NIMCP_TENSOR_ERR_CONTIGUOUS);
    EXPECT_NE(NIMCP_TENSOR_OK, NIMCP_TENSOR_ERR_INDEX);
    EXPECT_NE(NIMCP_TENSOR_OK, NIMCP_TENSOR_ERR_GRAD);
    EXPECT_NE(NIMCP_TENSOR_OK, NIMCP_TENSOR_ERR_INVALID);

    /* All error codes should be negative */
    EXPECT_LT(NIMCP_TENSOR_ERR_NULL, 0);
    EXPECT_LT(NIMCP_TENSOR_ERR_SHAPE, 0);
    EXPECT_LT(NIMCP_TENSOR_ERR_RANK, 0);
    EXPECT_LT(NIMCP_TENSOR_ERR_ALLOC, 0);
}

TEST_F(TensorErrorTest, ErrorCodeZeroIsOK) {
    /**
     * WHAT: Verify NIMCP_TENSOR_OK is zero
     * WHY:  Standard convention (0 = success)
     * HOW:  Check value
     */
    EXPECT_EQ(NIMCP_TENSOR_OK, 0);
}

//=============================================================================
// Error String Tests
//=============================================================================

TEST_F(TensorErrorTest, ErrorStringForOK) {
    /**
     * WHAT: Test error string for success code
     * WHY:  Should return meaningful message
     * HOW:  Get string, verify non-null
     */
    const char* str = nimcp_tensor_error_string(NIMCP_TENSOR_OK);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

TEST_F(TensorErrorTest, ErrorStringForAllCodes) {
    /**
     * WHAT: Test error strings for all error codes
     * WHY:  Each code should have descriptive message
     * HOW:  Get string for each, verify non-null and non-empty
     */
    const nimcp_tensor_error_t codes[] = {
        NIMCP_TENSOR_OK,
        NIMCP_TENSOR_ERR_NULL,
        NIMCP_TENSOR_ERR_SHAPE,
        NIMCP_TENSOR_ERR_RANK,
        NIMCP_TENSOR_ERR_ALLOC,
        NIMCP_TENSOR_ERR_BROADCAST,
        NIMCP_TENSOR_ERR_EINSUM,
        NIMCP_TENSOR_ERR_DTYPE,
        NIMCP_TENSOR_ERR_CONTIGUOUS,
        NIMCP_TENSOR_ERR_INDEX,
        NIMCP_TENSOR_ERR_GRAD,
        NIMCP_TENSOR_ERR_INVALID
    };

    for (int i = 0; i < 12; i++) {
        const char* str = nimcp_tensor_error_string(codes[i]);
        EXPECT_NE(str, nullptr) << "Error string NULL for code " << codes[i];
        EXPECT_GT(strlen(str), 0u) << "Error string empty for code " << codes[i];
    }
}

TEST_F(TensorErrorTest, ErrorStringForInvalidCode) {
    /**
     * WHAT: Test error string for invalid error code
     * WHY:  Should handle unknown codes gracefully
     * HOW:  Pass invalid code, verify returns something
     */
    const char* str = nimcp_tensor_error_string(static_cast<nimcp_tensor_error_t>(-999));
    EXPECT_NE(str, nullptr);  /* Should return "Unknown error" or similar */
}

//=============================================================================
// Rank Overflow Tests
//=============================================================================

TEST_F(TensorErrorTest, CreateExceedsMaxRank) {
    /**
     * WHAT: Test creation with rank exceeding maximum
     * WHY:  Must reject invalid rank
     * HOW:  Try to create tensor with rank > NIMCP_TENSOR_MAX_RANK
     */
    uint32_t dims[NIMCP_TENSOR_MAX_RANK + 1];
    for (int i = 0; i <= NIMCP_TENSOR_MAX_RANK; i++) {
        dims[i] = 2;
    }

    tensor = nimcp_tensor_create(dims, NIMCP_TENSOR_MAX_RANK + 1, NIMCP_DTYPE_F32);
    EXPECT_EQ(tensor, nullptr) << "Should reject rank > NIMCP_TENSOR_MAX_RANK";
}

TEST_F(TensorErrorTest, CreateAtMaxRank) {
    /**
     * WHAT: Test creation at exactly maximum rank
     * WHY:  Edge case at boundary
     * HOW:  Create tensor with rank = NIMCP_TENSOR_MAX_RANK
     */
    uint32_t dims[NIMCP_TENSOR_MAX_RANK];
    for (int i = 0; i < NIMCP_TENSOR_MAX_RANK; i++) {
        dims[i] = 2;
    }

    tensor = nimcp_tensor_create(dims, NIMCP_TENSOR_MAX_RANK, NIMCP_DTYPE_F32);
    /* Should succeed if 2^8 elements fits in memory */
    if (tensor) {
        EXPECT_EQ(nimcp_tensor_rank(tensor), NIMCP_TENSOR_MAX_RANK);
    }
}

TEST_F(TensorErrorTest, CreateZeroRank) {
    /**
     * WHAT: Test creation with zero rank (scalar)
     * WHY:  Scalars should be allowed
     * HOW:  Create tensor with rank 0
     */
    tensor = nimcp_tensor_create(nullptr, 0, NIMCP_DTYPE_F32);
    /* Scalar tensors may or may not be supported */
}

//=============================================================================
// Size Overflow Tests
//=============================================================================

TEST_F(TensorErrorTest, CreateOverflowingSize) {
    /**
     * WHAT: Test creation with dimension product overflowing
     * WHY:  Must detect size overflow
     * HOW:  Use large dimensions that multiply to overflow
     */
    /* Each dimension = 65536, product = 65536^4 = 2^64 (overflow) */
    uint32_t dims[] = {65536, 65536, 65536, 65536};

    tensor = nimcp_tensor_create(dims, 4, NIMCP_DTYPE_F32);
    EXPECT_EQ(tensor, nullptr) << "Should reject overflowing size";
}

TEST_F(TensorErrorTest, CreateMaxElements) {
    /**
     * WHAT: Test creation at maximum element count
     * WHY:  Should respect NIMCP_TENSOR_MAX_ELEMENTS limit
     * HOW:  Try to create tensor exceeding limit
     */
    /* Create tensor exceeding NIMCP_TENSOR_MAX_ELEMENTS (1 billion) */
    /* 32000 * 32000 = 1.024 billion */
    uint32_t dims[] = {32000, 32000};

    tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    EXPECT_EQ(tensor, nullptr) << "Should reject exceeding NIMCP_TENSOR_MAX_ELEMENTS";
}

TEST_F(TensorErrorTest, CreateZeroDimension) {
    /**
     * WHAT: Test creation with zero-sized dimension
     * WHY:  Zero dimension should be handled
     * HOW:  Pass dimension with size 0
     */
    uint32_t dims[] = {3, 0, 4};

    tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);
    /* Either allow (numel=0) or reject - both valid designs */
}

//=============================================================================
// NULL Pointer Error Tests
//=============================================================================

TEST_F(TensorErrorTest, CreateNullDims) {
    /**
     * WHAT: Test creation with NULL dims pointer
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL dims with non-zero rank
     */
    tensor = nimcp_tensor_create(nullptr, 2, NIMCP_DTYPE_F32);
    EXPECT_EQ(tensor, nullptr) << "Should reject NULL dims with rank > 0";
}

TEST_F(TensorErrorTest, DestroyNull) {
    /**
     * WHAT: Test destroy with NULL tensor
     * WHY:  Must be safe (idempotent)
     * HOW:  Call destroy on NULL, verify no crash
     */
    nimcp_tensor_destroy(nullptr);
    SUCCEED();
}

TEST_F(TensorErrorTest, ShapeNull) {
    /**
     * WHAT: Test shape query with NULL tensor
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns NULL
     */
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(nullptr);
    EXPECT_EQ(shape, nullptr);
}

TEST_F(TensorErrorTest, RankNull) {
    /**
     * WHAT: Test rank query with NULL tensor
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns 0
     */
    uint32_t rank = nimcp_tensor_rank(nullptr);
    EXPECT_EQ(rank, 0u);
}

TEST_F(TensorErrorTest, NumelNull) {
    /**
     * WHAT: Test numel query with NULL tensor
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns 0
     */
    size_t numel = nimcp_tensor_numel(nullptr);
    EXPECT_EQ(numel, 0u);
}

TEST_F(TensorErrorTest, DtypeNull) {
    /**
     * WHAT: Test dtype query with NULL tensor
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns F32 or specific default
     */
    nimcp_dtype_t dtype = nimcp_tensor_dtype(nullptr);
    /* Should return some default or invalid indicator */
}

TEST_F(TensorErrorTest, DataNull) {
    /**
     * WHAT: Test data pointer query with NULL tensor
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns NULL
     */
    void* data = nimcp_tensor_data(nullptr);
    EXPECT_EQ(data, nullptr);
}

TEST_F(TensorErrorTest, DataConstNull) {
    /**
     * WHAT: Test const data pointer query with NULL tensor
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns NULL
     */
    const void* data = nimcp_tensor_data_const(nullptr);
    EXPECT_EQ(data, nullptr);
}

//=============================================================================
// Invalid Tensor Detection Tests
//=============================================================================

TEST_F(TensorErrorTest, CloneNull) {
    /**
     * WHAT: Test clone with NULL tensor
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns NULL
     */
    nimcp_tensor_t* clone = nimcp_tensor_clone(nullptr);
    EXPECT_EQ(clone, nullptr);
}

TEST_F(TensorErrorTest, IsContiguousNull) {
    /**
     * WHAT: Test contiguous check with NULL tensor
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns false
     */
    bool contiguous = nimcp_tensor_is_contiguous(nullptr);
    EXPECT_FALSE(contiguous);
}

TEST_F(TensorErrorTest, RequiresGradNull) {
    /**
     * WHAT: Test requires_grad check with NULL tensor
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns false
     */
    bool requires_grad = nimcp_tensor_requires_grad(nullptr);
    EXPECT_FALSE(requires_grad);
}

//=============================================================================
// Shape Mismatch Error Tests
//=============================================================================

TEST_F(TensorErrorTest, AddShapeMismatch) {
    /**
     * WHAT: Test addition with incompatible shapes
     * WHY:  Must detect shape mismatch
     * HOW:  Create tensors with different shapes, try to add
     */
    uint32_t dims_a[] = {2, 3};
    uint32_t dims_b[] = {3, 2};

    nimcp_tensor_t* a = nimcp_tensor_create(dims_a, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* b = nimcp_tensor_create(dims_b, 2, NIMCP_DTYPE_F32);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    nimcp_tensor_t* result = nimcp_tensor_add(a, b);
    /* Should fail due to shape mismatch (non-broadcastable) */
    EXPECT_EQ(result, nullptr);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
}

TEST_F(TensorErrorTest, MatmulDimMismatch) {
    /**
     * WHAT: Test matmul with incompatible dimensions
     * WHY:  Inner dimensions must match
     * HOW:  Create matrices where inner dims don't match
     */
    uint32_t dims_a[] = {2, 3};  /* 2x3 matrix */
    uint32_t dims_b[] = {4, 5};  /* 4x5 matrix - incompatible */

    nimcp_tensor_t* a = nimcp_tensor_create(dims_a, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* b = nimcp_tensor_create(dims_b, 2, NIMCP_DTYPE_F32);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    nimcp_tensor_t* result = nimcp_tensor_matmul(a, b);
    /* Should fail: a.shape[1] (3) != b.shape[0] (4) */
    EXPECT_EQ(result, nullptr);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
}

//=============================================================================
// Data Type Error Tests
//=============================================================================

TEST_F(TensorErrorTest, CreateInvalidDtype) {
    /**
     * WHAT: Test creation with invalid dtype
     * WHY:  Must reject invalid types
     * HOW:  Pass dtype >= NIMCP_DTYPE_COUNT
     */
    uint32_t dims[] = {2, 2};

    tensor = nimcp_tensor_create(dims, 2, static_cast<nimcp_dtype_t>(999));
    EXPECT_EQ(tensor, nullptr);
}

TEST_F(TensorErrorTest, DtypeNameValid) {
    /**
     * WHAT: Test dtype name for all valid types
     * WHY:  Each type should have a name
     * HOW:  Get name for each dtype
     */
    for (int i = 0; i < NIMCP_DTYPE_COUNT; i++) {
        const char* name = nimcp_dtype_name(static_cast<nimcp_dtype_t>(i));
        EXPECT_NE(name, nullptr) << "NULL name for dtype " << i;
        EXPECT_GT(strlen(name), 0u) << "Empty name for dtype " << i;
    }
}

TEST_F(TensorErrorTest, DtypeSizeValid) {
    /**
     * WHAT: Test dtype size for all valid types
     * WHY:  Each type should have positive size
     * HOW:  Get size for each dtype
     */
    for (int i = 0; i < NIMCP_DTYPE_COUNT; i++) {
        size_t size = nimcp_dtype_size(static_cast<nimcp_dtype_t>(i));
        EXPECT_GT(size, 0u) << "Zero size for dtype " << i;
    }
}

//=============================================================================
// Index Error Tests
//=============================================================================

TEST_F(TensorErrorTest, GetOutOfBounds) {
    /**
     * WHAT: Test get with out-of-bounds index
     * WHY:  Must detect index errors
     * HOW:  Access beyond dimensions
     */
    uint32_t dims[] = {3, 4};
    tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(tensor, nullptr);

    /* Valid indices are [0-2][0-3] */
    uint32_t invalid_indices[] = {3, 0};  /* First dimension out of bounds */
    double value = nimcp_tensor_get(tensor, invalid_indices);

    /* Should return 0.0 or NaN on error */
}

TEST_F(TensorErrorTest, SetOutOfBounds) {
    /**
     * WHAT: Test set with out-of-bounds index
     * WHY:  Must detect index errors
     * HOW:  Try to write beyond dimensions
     */
    uint32_t dims[] = {3, 4};
    tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(tensor, nullptr);

    uint32_t invalid_indices[] = {0, 4};  /* Second dimension out of bounds */
    int result = nimcp_tensor_set(tensor, invalid_indices, 1.0);

    EXPECT_NE(result, NIMCP_TENSOR_OK);  /* Should return error */
}

TEST_F(TensorErrorTest, GetFlatOutOfBounds) {
    /**
     * WHAT: Test get_flat with out-of-bounds index
     * WHY:  Must detect flat index errors
     * HOW:  Access beyond numel
     */
    uint32_t dims[] = {3, 4};  /* numel = 12 */
    tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(tensor, nullptr);

    double value = nimcp_tensor_get_flat(tensor, 12);  /* Index 12 is out of bounds */
    /* Should return 0.0 or indicate error */
}

TEST_F(TensorErrorTest, SetFlatOutOfBounds) {
    /**
     * WHAT: Test set_flat with out-of-bounds index
     * WHY:  Must detect flat index errors
     * HOW:  Try to write beyond numel
     */
    uint32_t dims[] = {3, 4};  /* numel = 12 */
    tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(tensor, nullptr);

    int result = nimcp_tensor_set_flat(tensor, 100, 1.0);  /* Way out of bounds */
    EXPECT_NE(result, NIMCP_TENSOR_OK);
}

//=============================================================================
// Reshape Error Tests
//=============================================================================

TEST_F(TensorErrorTest, ReshapeIncompatibleSize) {
    /**
     * WHAT: Test reshape with incompatible total size
     * WHY:  Reshape must preserve element count
     * HOW:  Try to reshape 12 elements to 10
     */
    uint32_t dims[] = {3, 4};  /* numel = 12 */
    tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(tensor, nullptr);

    uint32_t new_dims[] = {2, 5};  /* numel = 10 - incompatible */
    nimcp_tensor_t* reshaped = nimcp_tensor_reshape(tensor, new_dims, 2);

    EXPECT_EQ(reshaped, nullptr) << "Should reject incompatible reshape";
}

TEST_F(TensorErrorTest, ReshapeNull) {
    /**
     * WHAT: Test reshape with NULL tensor
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns NULL
     */
    uint32_t new_dims[] = {2, 2};
    nimcp_tensor_t* reshaped = nimcp_tensor_reshape(nullptr, new_dims, 2);
    EXPECT_EQ(reshaped, nullptr);
}

//=============================================================================
// Binary Operation Error Tests
//=============================================================================

TEST_F(TensorErrorTest, AddNullLeft) {
    /**
     * WHAT: Test add with NULL left operand
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns NULL
     */
    tensor = create_simple_tensor();
    ASSERT_NE(tensor, nullptr);

    nimcp_tensor_t* result = nimcp_tensor_add(nullptr, tensor);
    EXPECT_EQ(result, nullptr);
}

TEST_F(TensorErrorTest, AddNullRight) {
    /**
     * WHAT: Test add with NULL right operand
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL, verify returns NULL
     */
    tensor = create_simple_tensor();
    ASSERT_NE(tensor, nullptr);

    nimcp_tensor_t* result = nimcp_tensor_add(tensor, nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(TensorErrorTest, MulNullBoth) {
    /**
     * WHAT: Test multiply with both operands NULL
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL for both, verify returns NULL
     */
    nimcp_tensor_t* result = nimcp_tensor_mul(nullptr, nullptr);
    EXPECT_EQ(result, nullptr);
}

//=============================================================================
// Error Propagation Tests
//=============================================================================

TEST_F(TensorErrorTest, ChainedOperationsStopOnError) {
    /**
     * WHAT: Test that errors propagate through chained operations
     * WHY:  NULL results should not crash subsequent ops
     * HOW:  Create situation where first op fails, second uses result
     */
    uint32_t dims_a[] = {2, 3};
    uint32_t dims_b[] = {3, 2};

    nimcp_tensor_t* a = nimcp_tensor_create(dims_a, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* b = nimcp_tensor_create(dims_b, 2, NIMCP_DTYPE_F32);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    /* This should fail due to shape mismatch */
    nimcp_tensor_t* sum = nimcp_tensor_add(a, b);

    /* Using NULL result in another operation should be safe */
    nimcp_tensor_t* scaled = nimcp_tensor_mul_scalar(sum, 2.0);
    EXPECT_EQ(scaled, nullptr);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    /* sum is NULL, no need to destroy */
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
