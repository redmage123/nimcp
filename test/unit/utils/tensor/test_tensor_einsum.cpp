//=============================================================================
// test_tensor_einsum.cpp - Unit Tests for Einstein Summation
//=============================================================================
/**
 * @file test_tensor_einsum.cpp
 * @brief Unit tests for nimcp_tensor_einsum()
 *
 * WHAT: Tests all einsum equation patterns and error cases
 * WHY:  Einsum is a general tensor contraction interface used throughout
 *       neural network operations; correctness is critical
 * HOW:  Construct tensors with known values, run einsum, verify results
 *
 * TEST COVERAGE:
 * 1. Matrix multiply: "ij,jk->ik"
 * 2. Batch matrix multiply: "bij,bjk->bik"
 * 3. Trace: "ii->"
 * 4. Transpose: "ij->ji"
 * 5. Outer product: "i,j->ij"
 * 6. Diagonal extraction: "ii->i"
 * 7. Hadamard product: "ij,ij->ij"
 * 8. Full summation: "ij->"
 * 9. Row sum: "ij->i"
 * 10. Error cases: NULL, invalid equation, dimension mismatch
 * 11. Implicit output (no -> in equation)
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

class TensorEinsumTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-5f;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_tensor_init();
    }

    void TearDown() override {
        nimcp_tensor_shutdown();
    }

    /**
     * @brief Create a 2x2 matrix with values [[a,b],[c,d]]
     */
    nimcp_tensor_t* Create2x2(float a, float b, float c, float d) {
        uint32_t dims[] = {2, 2};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
        if (!t) return nullptr;
        nimcp_tensor_set_flat(t, 0, a);
        nimcp_tensor_set_flat(t, 1, b);
        nimcp_tensor_set_flat(t, 2, c);
        nimcp_tensor_set_flat(t, 3, d);
        return t;
    }

    /**
     * @brief Create a 2x3 matrix with values [[1,2,3],[4,5,6]]
     */
    nimcp_tensor_t* Create2x3() {
        float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
        uint32_t dims[] = {2, 3};
        return nimcp_tensor_from_data(data, dims, 2, NIMCP_DTYPE_F32, true);
    }

    /**
     * @brief Create a 3x2 matrix with values [[1,2],[3,4],[5,6]]
     */
    nimcp_tensor_t* Create3x2() {
        float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
        uint32_t dims[] = {3, 2};
        return nimcp_tensor_from_data(data, dims, 2, NIMCP_DTYPE_F32, true);
    }
};

//=============================================================================
// Unit Tests: Matrix Multiply via Einsum
//=============================================================================

TEST_F(TensorEinsumTest, MatrixMultiply_Known2x2) {
    // WHAT: "ij,jk->ik" is standard matrix multiply
    // WHY:  Most common einsum pattern; verify against hand-computed result
    //
    // A = [[1,2],[3,4]], B = [[5,6],[7,8]]
    // C = A @ B = [[1*5+2*7, 1*6+2*8],[3*5+4*7, 3*6+4*8]]
    //           = [[19,22],[43,50]]

    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    nimcp_tensor_t* b = Create2x2(5, 6, 7, 8);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    nimcp_tensor_t* tensors[] = {a, b};
    nimcp_tensor_t* c = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
    ASSERT_NE(c, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(c);
    ASSERT_NE(shape, nullptr);
    EXPECT_EQ(shape->rank, 2u);
    EXPECT_EQ(shape->dims[0], 2u);
    EXPECT_EQ(shape->dims[1], 2u);

    EXPECT_NEAR(nimcp_tensor_get_flat(c, 0), 19.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 1), 22.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 2), 43.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 3), 50.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

TEST_F(TensorEinsumTest, MatrixMultiply_Rectangular) {
    // WHAT: "ij,jk->ik" with non-square matrices (2x3) @ (3x2) -> (2x2)
    //
    // A = [[1,2,3],[4,5,6]], B = [[1,2],[3,4],[5,6]]
    // C[0,0] = 1*1+2*3+3*5 = 22
    // C[0,1] = 1*2+2*4+3*6 = 28
    // C[1,0] = 4*1+5*3+6*5 = 49
    // C[1,1] = 4*2+5*4+6*6 = 64

    nimcp_tensor_t* a = Create2x3();
    nimcp_tensor_t* b = Create3x2();
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    nimcp_tensor_t* tensors[] = {a, b};
    nimcp_tensor_t* c = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
    ASSERT_NE(c, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(c);
    EXPECT_EQ(shape->dims[0], 2u);
    EXPECT_EQ(shape->dims[1], 2u);

    EXPECT_NEAR(nimcp_tensor_get_flat(c, 0), 22.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 1), 28.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 2), 49.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 3), 64.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

//=============================================================================
// Unit Tests: Batch Matrix Multiply
//=============================================================================

TEST_F(TensorEinsumTest, BatchMatMul) {
    // WHAT: "bij,bjk->bik" is batched matrix multiply
    // WHY:  Critical for batched neural network operations
    //
    // Batch of 2 matrices, each 2x2:
    // Batch 0: A=[[1,0],[0,1]] (identity), B=[[5,6],[7,8]]
    //   -> C[0] = [[5,6],[7,8]]
    // Batch 1: A=[[2,0],[0,2]] (2*identity), B=[[1,2],[3,4]]
    //   -> C[1] = [[2,4],[6,8]]

    uint32_t a_dims[] = {2, 2, 2};
    nimcp_tensor_t* a = nimcp_tensor_zeros(a_dims, 3, NIMCP_DTYPE_F32);
    ASSERT_NE(a, nullptr);

    // Batch 0: identity
    uint32_t idx000[] = {0, 0, 0};
    uint32_t idx011[] = {0, 1, 1};
    nimcp_tensor_set(a, idx000, 1.0);
    nimcp_tensor_set(a, idx011, 1.0);
    // Batch 1: 2*identity
    uint32_t idx100[] = {1, 0, 0};
    uint32_t idx111[] = {1, 1, 1};
    nimcp_tensor_set(a, idx100, 2.0);
    nimcp_tensor_set(a, idx111, 2.0);

    uint32_t b_dims[] = {2, 2, 2};
    nimcp_tensor_t* b = nimcp_tensor_create(b_dims, 3, NIMCP_DTYPE_F32);
    ASSERT_NE(b, nullptr);

    // Batch 0: [[5,6],[7,8]]
    uint32_t bidx000[] = {0, 0, 0};
    uint32_t bidx001[] = {0, 0, 1};
    uint32_t bidx010[] = {0, 1, 0};
    uint32_t bidx011[] = {0, 1, 1};
    nimcp_tensor_set(b, bidx000, 5.0);
    nimcp_tensor_set(b, bidx001, 6.0);
    nimcp_tensor_set(b, bidx010, 7.0);
    nimcp_tensor_set(b, bidx011, 8.0);
    // Batch 1: [[1,2],[3,4]]
    uint32_t bidx100[] = {1, 0, 0};
    uint32_t bidx101[] = {1, 0, 1};
    uint32_t bidx110[] = {1, 1, 0};
    uint32_t bidx111[] = {1, 1, 1};
    nimcp_tensor_set(b, bidx100, 1.0);
    nimcp_tensor_set(b, bidx101, 2.0);
    nimcp_tensor_set(b, bidx110, 3.0);
    nimcp_tensor_set(b, bidx111, 4.0);

    nimcp_tensor_t* tensors[] = {a, b};
    nimcp_tensor_t* c = nimcp_tensor_einsum("bij,bjk->bik", tensors, 2);
    ASSERT_NE(c, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(c);
    EXPECT_EQ(shape->rank, 3u);
    EXPECT_EQ(shape->dims[0], 2u);
    EXPECT_EQ(shape->dims[1], 2u);
    EXPECT_EQ(shape->dims[2], 2u);

    // Batch 0: identity @ [[5,6],[7,8]] = [[5,6],[7,8]]
    uint32_t cidx000[] = {0, 0, 0};
    uint32_t cidx001[] = {0, 0, 1};
    uint32_t cidx010[] = {0, 1, 0};
    uint32_t cidx011[] = {0, 1, 1};
    EXPECT_NEAR(nimcp_tensor_get(c, cidx000), 5.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get(c, cidx001), 6.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get(c, cidx010), 7.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get(c, cidx011), 8.0f, EPSILON);

    // Batch 1: 2I @ [[1,2],[3,4]] = [[2,4],[6,8]]
    uint32_t cidx100[] = {1, 0, 0};
    uint32_t cidx101[] = {1, 0, 1};
    uint32_t cidx110[] = {1, 1, 0};
    uint32_t cidx111[] = {1, 1, 1};
    EXPECT_NEAR(nimcp_tensor_get(c, cidx100), 2.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get(c, cidx101), 4.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get(c, cidx110), 6.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get(c, cidx111), 8.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

//=============================================================================
// Unit Tests: Trace
//=============================================================================

TEST_F(TensorEinsumTest, Trace_SumOfDiagonal) {
    // WHAT: "ii->" gives sum of diagonal elements (trace)
    // WHY:  Common tensor operation; result is scalar
    //
    // A = [[1,2],[3,4]] -> trace = 1 + 4 = 5

    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ii->", tensors, 1);
    ASSERT_NE(result, nullptr);

    // Scalar output: rank 0, numel 1
    EXPECT_EQ(nimcp_tensor_numel(result), 1u);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 5.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(result);
}

TEST_F(TensorEinsumTest, Trace_3x3Identity) {
    // WHAT: Trace of 3x3 identity matrix should be 3
    nimcp_tensor_t* eye = nimcp_tensor_eye(3, NIMCP_DTYPE_F32);
    ASSERT_NE(eye, nullptr);

    nimcp_tensor_t* tensors[] = {eye};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ii->", tensors, 1);
    ASSERT_NE(result, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 3.0f, EPSILON);

    nimcp_tensor_destroy(eye);
    nimcp_tensor_destroy(result);
}

//=============================================================================
// Unit Tests: Transpose
//=============================================================================

TEST_F(TensorEinsumTest, Transpose_2x3) {
    // WHAT: "ij->ji" transposes matrix
    // WHY:  Basic shape operation via einsum
    //
    // A = [[1,2,3],[4,5,6]] -> A^T = [[1,4],[2,5],[3,6]]

    nimcp_tensor_t* a = Create2x3();
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij->ji", tensors, 1);
    ASSERT_NE(result, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(result);
    EXPECT_EQ(shape->dims[0], 3u);
    EXPECT_EQ(shape->dims[1], 2u);

    // Verify transposed elements
    uint32_t idx00[] = {0, 0};
    uint32_t idx01[] = {0, 1};
    uint32_t idx10[] = {1, 0};
    uint32_t idx20[] = {2, 0};
    uint32_t idx21[] = {2, 1};
    EXPECT_NEAR(nimcp_tensor_get(result, idx00), 1.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get(result, idx01), 4.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get(result, idx10), 2.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get(result, idx20), 3.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get(result, idx21), 6.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(result);
}

//=============================================================================
// Unit Tests: Outer Product
//=============================================================================

TEST_F(TensorEinsumTest, OuterProduct) {
    // WHAT: "i,j->ij" computes outer product
    // WHY:  Rank-1 tensors produce rank-2 result
    //
    // u = [1,2,3], v = [4,5]
    // u (x) v = [[4,5],[8,10],[12,15]]

    float u_data[] = {1.0f, 2.0f, 3.0f};
    uint32_t u_dims[] = {3};
    nimcp_tensor_t* u = nimcp_tensor_from_data(u_data, u_dims, 1, NIMCP_DTYPE_F32, true);

    float v_data[] = {4.0f, 5.0f};
    uint32_t v_dims[] = {2};
    nimcp_tensor_t* v = nimcp_tensor_from_data(v_data, v_dims, 1, NIMCP_DTYPE_F32, true);

    ASSERT_NE(u, nullptr);
    ASSERT_NE(v, nullptr);

    nimcp_tensor_t* tensors[] = {u, v};
    nimcp_tensor_t* result = nimcp_tensor_einsum("i,j->ij", tensors, 2);
    ASSERT_NE(result, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(result);
    EXPECT_EQ(shape->dims[0], 3u);
    EXPECT_EQ(shape->dims[1], 2u);

    // Row 0: [1*4, 1*5] = [4, 5]
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 4.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 1), 5.0f, EPSILON);
    // Row 1: [2*4, 2*5] = [8, 10]
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 2), 8.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 3), 10.0f, EPSILON);
    // Row 2: [3*4, 3*5] = [12, 15]
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 4), 12.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 5), 15.0f, EPSILON);

    nimcp_tensor_destroy(u);
    nimcp_tensor_destroy(v);
    nimcp_tensor_destroy(result);
}

//=============================================================================
// Unit Tests: Diagonal Extraction
//=============================================================================

TEST_F(TensorEinsumTest, DiagonalExtraction) {
    // WHAT: "ii->i" extracts the diagonal of a matrix
    // WHY:  Common linear algebra operation
    //
    // A = [[1,2],[3,4]] -> diag = [1, 4]

    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ii->i", tensors, 1);
    ASSERT_NE(result, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(result);
    EXPECT_EQ(shape->rank, 1u);
    EXPECT_EQ(shape->dims[0], 2u);

    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 1.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 1), 4.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(result);
}

TEST_F(TensorEinsumTest, DiagonalExtraction_3x3) {
    // WHAT: "ii->i" on a 3x3 matrix
    // A = [[10,20,30],[40,50,60],[70,80,90]] -> diag = [10, 50, 90]

    uint32_t dims[] = {3, 3};
    nimcp_tensor_t* a = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(a, nullptr);

    float vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
    for (size_t i = 0; i < 9; i++) {
        nimcp_tensor_set_flat(a, i, vals[i]);
    }

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ii->i", tensors, 1);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(nimcp_tensor_numel(result), 3u);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 10.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 1), 50.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 2), 90.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(result);
}

//=============================================================================
// Unit Tests: Hadamard Product (Element-wise Multiply)
//=============================================================================

TEST_F(TensorEinsumTest, HadamardProduct) {
    // WHAT: "ij,ij->ij" is element-wise multiplication
    // WHY:  All indices appear in both inputs and output
    //
    // A = [[1,2],[3,4]], B = [[5,6],[7,8]]
    // C = A * B = [[5,12],[21,32]]

    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    nimcp_tensor_t* b = Create2x2(5, 6, 7, 8);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    nimcp_tensor_t* tensors[] = {a, b};
    nimcp_tensor_t* c = nimcp_tensor_einsum("ij,ij->ij", tensors, 2);
    ASSERT_NE(c, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(c, 0), 5.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 1), 12.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 2), 21.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 3), 32.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

//=============================================================================
// Unit Tests: Full Summation
//=============================================================================

TEST_F(TensorEinsumTest, SumAllElements) {
    // WHAT: "ij->" sums all elements to a scalar
    // WHY:  No output indices means full contraction
    //
    // A = [[1,2],[3,4]] -> sum = 10

    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij->", tensors, 1);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(nimcp_tensor_numel(result), 1u);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 10.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(result);
}

TEST_F(TensorEinsumTest, SumAllElements_Larger) {
    // WHAT: "ij->" on a 2x3 matrix
    // [[1,2,3],[4,5,6]] -> sum = 21

    nimcp_tensor_t* a = Create2x3();
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij->", tensors, 1);
    ASSERT_NE(result, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 21.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(result);
}

//=============================================================================
// Unit Tests: Row Sum
//=============================================================================

TEST_F(TensorEinsumTest, RowSum) {
    // WHAT: "ij->i" sums along columns, keeping rows
    // WHY:  Partial contraction over j index
    //
    // A = [[1,2,3],[4,5,6]] -> row_sums = [6, 15]

    nimcp_tensor_t* a = Create2x3();
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij->i", tensors, 1);
    ASSERT_NE(result, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(result);
    EXPECT_EQ(shape->rank, 1u);
    EXPECT_EQ(shape->dims[0], 2u);

    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 6.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 1), 15.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(result);
}

TEST_F(TensorEinsumTest, ColumnSum) {
    // WHAT: "ij->j" sums along rows, keeping columns
    //
    // A = [[1,2,3],[4,5,6]] -> col_sums = [5, 7, 9]

    nimcp_tensor_t* a = Create2x3();
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij->j", tensors, 1);
    ASSERT_NE(result, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(result);
    EXPECT_EQ(shape->rank, 1u);
    EXPECT_EQ(shape->dims[0], 3u);

    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 5.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 1), 7.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 2), 9.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(result);
}

//=============================================================================
// Unit Tests: Implicit Output (no -> in equation)
//=============================================================================

TEST_F(TensorEinsumTest, ImplicitOutput_MatMul) {
    // WHAT: "ij,jk" without -> should auto-derive output labels
    // WHY:  Labels appearing exactly once (i, k) form the output in alphabetical order
    //       So "ij,jk" is equivalent to "ij,jk->ik"
    //
    // A = [[1,2],[3,4]], B = [[5,6],[7,8]]
    // C = [[19,22],[43,50]]

    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    nimcp_tensor_t* b = Create2x2(5, 6, 7, 8);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    nimcp_tensor_t* tensors[] = {a, b};
    nimcp_tensor_t* c = nimcp_tensor_einsum("ij,jk", tensors, 2);
    ASSERT_NE(c, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(c);
    EXPECT_EQ(shape->rank, 2u);
    EXPECT_EQ(shape->dims[0], 2u);
    EXPECT_EQ(shape->dims[1], 2u);

    EXPECT_NEAR(nimcp_tensor_get_flat(c, 0), 19.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 1), 22.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 2), 43.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(c, 3), 50.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

TEST_F(TensorEinsumTest, ImplicitOutput_SingleOperand) {
    // WHAT: "ij" without -> on single operand
    // WHY:  Both i and j appear once, so output is "ij" (identity operation)
    //
    // A = [[1,2],[3,4]] -> result should equal A

    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij", tensors, 1);
    ASSERT_NE(result, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 1.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 1), 2.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 2), 3.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 3), 4.0f, EPSILON);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(result);
}

//=============================================================================
// Unit Tests: Vector Operations via Einsum
//=============================================================================

TEST_F(TensorEinsumTest, DotProduct) {
    // WHAT: "i,i->" computes dot product (inner product)
    // [1,2,3] . [4,5,6] = 4+10+18 = 32

    float u_data[] = {1.0f, 2.0f, 3.0f};
    float v_data[] = {4.0f, 5.0f, 6.0f};
    uint32_t dims[] = {3};

    nimcp_tensor_t* u = nimcp_tensor_from_data(u_data, dims, 1, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* v = nimcp_tensor_from_data(v_data, dims, 1, NIMCP_DTYPE_F32, true);
    ASSERT_NE(u, nullptr);
    ASSERT_NE(v, nullptr);

    nimcp_tensor_t* tensors[] = {u, v};
    nimcp_tensor_t* result = nimcp_tensor_einsum("i,i->", tensors, 2);
    ASSERT_NE(result, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 32.0f, EPSILON);

    nimcp_tensor_destroy(u);
    nimcp_tensor_destroy(v);
    nimcp_tensor_destroy(result);
}

TEST_F(TensorEinsumTest, VectorSum) {
    // WHAT: "i->" sums all elements of a vector
    // [1,2,3] -> 6

    float data[] = {1.0f, 2.0f, 3.0f};
    uint32_t dims[] = {3};
    nimcp_tensor_t* v = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);
    ASSERT_NE(v, nullptr);

    nimcp_tensor_t* tensors[] = {v};
    nimcp_tensor_t* result = nimcp_tensor_einsum("i->", tensors, 1);
    ASSERT_NE(result, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 6.0f, EPSILON);

    nimcp_tensor_destroy(v);
    nimcp_tensor_destroy(result);
}

//=============================================================================
// Unit Tests: Error Cases
//=============================================================================

TEST_F(TensorEinsumTest, Error_NullEquation) {
    // WHAT: NULL equation string should return NULL
    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum(nullptr, tensors, 1);
    EXPECT_EQ(result, nullptr);

    nimcp_tensor_destroy(a);
}

TEST_F(TensorEinsumTest, Error_NullTensors) {
    // WHAT: NULL tensors array should return NULL
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij->ji", nullptr, 1);
    EXPECT_EQ(result, nullptr);
}

TEST_F(TensorEinsumTest, Error_ZeroTensors) {
    // WHAT: Zero num_tensors should return NULL
    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij->ji", tensors, 0);
    EXPECT_EQ(result, nullptr);

    nimcp_tensor_destroy(a);
}

TEST_F(TensorEinsumTest, Error_InvalidEquation_UpperCase) {
    // WHAT: Uppercase letters in equation should fail
    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("IJ->JI", tensors, 1);
    EXPECT_EQ(result, nullptr);

    nimcp_tensor_destroy(a);
}

TEST_F(TensorEinsumTest, Error_DimensionMismatch) {
    // WHAT: Mismatched dimensions for same label should fail
    // "ij,jk->ik" where j-dims don't match: A is 2x3, B is 4x2
    nimcp_tensor_t* a = Create2x3();
    ASSERT_NE(a, nullptr);

    uint32_t b_dims[] = {4, 2};
    nimcp_tensor_t* b = nimcp_tensor_ones(b_dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(b, nullptr);

    nimcp_tensor_t* tensors[] = {a, b};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
    EXPECT_EQ(result, nullptr);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
}

TEST_F(TensorEinsumTest, Error_RankMismatch) {
    // WHAT: Number of subscripts doesn't match tensor rank
    // A is 2x2 (rank 2), but equation says "ijk" (rank 3)
    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ijk->ij", tensors, 1);
    EXPECT_EQ(result, nullptr);

    nimcp_tensor_destroy(a);
}

TEST_F(TensorEinsumTest, Error_WrongOperandCount) {
    // WHAT: Equation has 2 operands but only 1 tensor provided
    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij,jk->ik", tensors, 1);
    EXPECT_EQ(result, nullptr);

    nimcp_tensor_destroy(a);
}

TEST_F(TensorEinsumTest, Error_EmptyEquation) {
    // WHAT: Empty equation string should fail
    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("", tensors, 1);
    EXPECT_EQ(result, nullptr);

    nimcp_tensor_destroy(a);
}

TEST_F(TensorEinsumTest, Error_OutputLabelNotInInput) {
    // WHAT: Output label 'k' doesn't exist in inputs
    nimcp_tensor_t* a = Create2x2(1, 2, 3, 4);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij->ik", tensors, 1);
    EXPECT_EQ(result, nullptr);

    nimcp_tensor_destroy(a);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
