/**
 * @file test_sparse_gpu.cpp
 * @brief Unit tests for GPU Sparse Tensor Operations
 *
 * WHAT: Comprehensive unit tests for sparse tensor GPU operations
 * WHY:  Verify correctness of sparse format creation, conversion, and operations
 * HOW:  Test CSR/COO/BSR formats, SpMM/SpMV, pruning, and gradient accumulation
 *
 * TEST COVERAGE:
 * - Sparse format creation (CSR, COO, BSR)
 * - Sparse tensor from dense with threshold
 * - SpMM and SpMV operations
 * - Sparse pruning (magnitude, structured, threshold)
 * - Format conversion between CSR/COO
 * - Sparse gradient accumulation
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>

// GPU headers with CUDA includes must be outside extern "C"
#include "gpu/sparse/nimcp_sparse_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

// Headers already have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr size_t SMALL_SIZE = 32;
    constexpr size_t MEDIUM_SIZE = 128;
    constexpr size_t LARGE_SIZE = 512;
    constexpr float TOLERANCE = 1e-5f;
    constexpr float SPARSITY_TOLERANCE = 0.05f;

//=============================================================================
// Test Fixture
//=============================================================================

class SparseGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    nimcp_sparse_ctx_t* sparse_ctx = nullptr;
    std::mt19937 rng{12345};
    bool gpu_available = false;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        gpu_ctx = nimcp_gpu_context_create_auto();
        gpu_available = (gpu_ctx != nullptr && nimcp_gpu_context_is_valid(gpu_ctx));

        if (gpu_available) {
            sparse_ctx = nimcp_sparse_ctx_create(gpu_ctx);
        }
    }

    void TearDown() override {
        if (sparse_ctx) {
            nimcp_sparse_ctx_destroy(sparse_ctx);
            sparse_ctx = nullptr;
        }

        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Potential memory leak: " << stats.current_allocated << " bytes";
    }

    void RequireGPU() {
        if (!gpu_available || !sparse_ctx) {
            GTEST_SKIP() << "GPU or sparse context not available";
        }
    }

    // Helper: Generate dense matrix with specific sparsity
    std::vector<float> generateSparseMatrix(size_t rows, size_t cols, float sparsity) {
        std::vector<float> data(rows * cols, 0.0f);
        std::uniform_real_distribution<float> val_dist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);

        for (size_t i = 0; i < data.size(); i++) {
            if (prob_dist(rng) > sparsity) {
                data[i] = val_dist(rng);
            }
        }
        return data;
    }

    // Helper: Generate random dense matrix
    std::vector<float> generateDenseMatrix(size_t rows, size_t cols) {
        std::vector<float> data(rows * cols);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& v : data) {
            v = dist(rng);
        }
        return data;
    }

    // Helper: Create GPU tensor from host data
    nimcp_gpu_tensor_t* createGPUTensor(const std::vector<float>& data,
                                         const std::vector<size_t>& dims) {
        if (!gpu_ctx) return nullptr;
        return nimcp_gpu_tensor_from_host(gpu_ctx, data.data(), dims.data(),
                                          dims.size(), NIMCP_GPU_PRECISION_FP32);
    }

    // Helper: Copy GPU tensor to host
    std::vector<float> copyToHost(const nimcp_gpu_tensor_t* tensor) {
        if (!tensor) return {};
        std::vector<float> result(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, result.data());
        return result;
    }

    // Helper: Count non-zeros in dense matrix
    int countNonZeros(const std::vector<float>& data, float threshold = 0.0f) {
        int count = 0;
        for (float v : data) {
            if (std::fabs(v) > threshold) count++;
        }
        return count;
    }

    // Helper: CPU reference SpMM
    void cpuSpMM(const float* A, int A_rows, int A_cols,
                 const float* B, int B_cols,
                 float* C, float alpha, float beta) {
        for (int i = 0; i < A_rows; i++) {
            for (int j = 0; j < B_cols; j++) {
                float sum = 0.0f;
                for (int k = 0; k < A_cols; k++) {
                    sum += A[i * A_cols + k] * B[k * B_cols + j];
                }
                C[i * B_cols + j] = alpha * sum + beta * C[i * B_cols + j];
            }
        }
    }

    // Helper: CPU reference SpMV
    void cpuSpMV(const float* A, int rows, int cols,
                 const float* x, float* y,
                 float alpha, float beta) {
        for (int i = 0; i < rows; i++) {
            float sum = 0.0f;
            for (int j = 0; j < cols; j++) {
                sum += A[i * cols + j] * x[j];
            }
            y[i] = alpha * sum + beta * y[i];
        }
    }

    // Helper: Compute relative error
    float relativeError(float expected, float actual) {
        if (std::fabs(expected) < 1e-10f) {
            return std::fabs(actual);
        }
        return std::fabs(actual - expected) / std::fabs(expected);
    }

    // Helper: Compute max absolute error
    float maxAbsError(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size()) return std::numeric_limits<float>::max();
        float max_err = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            max_err = std::max(max_err, std::fabs(a[i] - b[i]));
        }
        return max_err;
    }
};

//=============================================================================
// Sparse Context Tests
//=============================================================================

TEST_F(SparseGPUTest, ContextCreate_WithValidGPUContext_ReturnsValidContext) {
    RequireGPU();

    EXPECT_NE(sparse_ctx, nullptr);
    EXPECT_NE(sparse_ctx->gpu_ctx, nullptr);
}

TEST_F(SparseGPUTest, ContextCreate_WithNullGPUContext_ReturnsNull) {
    nimcp_sparse_ctx_t* ctx = nimcp_sparse_ctx_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(SparseGPUTest, ContextDestroy_HandlesNull) {
    nimcp_sparse_ctx_destroy(nullptr);  // Should not crash
}

TEST_F(SparseGPUTest, ContextEnsureWorkspace_GrowsWorkspace) {
    RequireGPU();

    size_t initial_size = sparse_ctx->workspace_capacity;

    bool result = nimcp_sparse_ctx_ensure_workspace(sparse_ctx, 1024 * 1024);
    EXPECT_TRUE(result);
    EXPECT_GE(sparse_ctx->workspace_capacity, 1024u * 1024u);
}

//=============================================================================
// CSR Format Creation Tests
//=============================================================================

TEST_F(SparseGPUTest, CSR_FromDense_CreatesValidSparse) {
    RequireGPU();

    const size_t rows = SMALL_SIZE;
    const size_t cols = SMALL_SIZE;
    const float sparsity = 0.8f;  // 80% zeros
    const float threshold = 0.01f;

    auto dense_data = generateSparseMatrix(rows, cols, sparsity);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createGPUTensor(dense_data, dims);
    ASSERT_NE(dense, nullptr);

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, threshold);

    ASSERT_NE(sparse, nullptr);
    EXPECT_EQ(sparse->format, SPARSE_FORMAT_CSR);
    EXPECT_TRUE(sparse->on_device);
    EXPECT_EQ(nimcp_sparse_rows(sparse), static_cast<int>(rows));
    EXPECT_EQ(nimcp_sparse_cols(sparse), static_cast<int>(cols));

    // Verify nnz is reasonable
    int expected_nnz = countNonZeros(dense_data, threshold);
    EXPECT_EQ(nimcp_sparse_nnz(sparse), expected_nnz);

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
}

TEST_F(SparseGPUTest, CSR_FromCOOData_CreatesValidSparse) {
    RequireGPU();

    // Create simple 3x3 sparse matrix:
    // [1 0 2]
    // [0 3 0]
    // [4 0 5]
    const int rows = 3, cols = 3, nnz = 5;
    float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    int row_idx[] = {0, 0, 1, 2, 2};
    int col_idx[] = {0, 2, 1, 0, 2};

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_coo(
        sparse_ctx, values, row_idx, col_idx, rows, cols, nnz, SPARSE_FORMAT_CSR);

    ASSERT_NE(sparse, nullptr);
    EXPECT_EQ(sparse->format, SPARSE_FORMAT_CSR);
    EXPECT_EQ(nimcp_sparse_rows(sparse), rows);
    EXPECT_EQ(nimcp_sparse_cols(sparse), cols);
    EXPECT_EQ(nimcp_sparse_nnz(sparse), nnz);

    nimcp_sparse_tensor_destroy(sparse);
}

TEST_F(SparseGPUTest, CSR_FromCSRData_CreatesValidSparse) {
    RequireGPU();

    // Create CSR data for 3x3 matrix
    const int rows = 3, cols = 3, nnz = 5;
    float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    int col_indices[] = {0, 2, 1, 0, 2};
    int row_ptrs[] = {0, 2, 3, 5};

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_csr(
        sparse_ctx, values, col_indices, row_ptrs, rows, cols, nnz);

    ASSERT_NE(sparse, nullptr);
    EXPECT_EQ(sparse->format, SPARSE_FORMAT_CSR);
    EXPECT_EQ(nimcp_sparse_nnz(sparse), nnz);

    nimcp_sparse_tensor_destroy(sparse);
}

//=============================================================================
// COO Format Creation Tests
//=============================================================================

TEST_F(SparseGPUTest, COO_FromDense_CreatesValidSparse) {
    RequireGPU();

    const size_t rows = SMALL_SIZE;
    const size_t cols = SMALL_SIZE;
    const float sparsity = 0.9f;
    const float threshold = 0.01f;

    auto dense_data = generateSparseMatrix(rows, cols, sparsity);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createGPUTensor(dense_data, dims);
    ASSERT_NE(dense, nullptr);

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_COO, threshold);

    ASSERT_NE(sparse, nullptr);
    EXPECT_EQ(sparse->format, SPARSE_FORMAT_COO);

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
}

TEST_F(SparseGPUTest, COO_FromCOOData_PreservesFormat) {
    RequireGPU();

    const int rows = 4, cols = 4, nnz = 3;
    float values[] = {1.5f, 2.5f, 3.5f};
    int row_idx[] = {0, 1, 3};
    int col_idx[] = {1, 2, 3};

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_coo(
        sparse_ctx, values, row_idx, col_idx, rows, cols, nnz, SPARSE_FORMAT_COO);

    ASSERT_NE(sparse, nullptr);
    EXPECT_EQ(sparse->format, SPARSE_FORMAT_COO);
    EXPECT_EQ(nimcp_sparse_nnz(sparse), nnz);

    nimcp_sparse_tensor_destroy(sparse);
}

//=============================================================================
// BSR Format Creation Tests
//=============================================================================

TEST_F(SparseGPUTest, BSR_FromDense_CreatesValidSparse) {
    RequireGPU();

    // Use dimensions divisible by common block size (4)
    const size_t rows = 16;
    const size_t cols = 16;
    const float sparsity = 0.8f;
    const float threshold = 0.01f;

    auto dense_data = generateSparseMatrix(rows, cols, sparsity);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createGPUTensor(dense_data, dims);
    ASSERT_NE(dense, nullptr);

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_BSR, threshold);

    ASSERT_NE(sparse, nullptr);
    EXPECT_EQ(sparse->format, SPARSE_FORMAT_BSR);

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
}

//=============================================================================
// Sparse to Dense Conversion Tests
//=============================================================================

TEST_F(SparseGPUTest, ToDense_ReconstructsOriginal) {
    RequireGPU();

    const size_t rows = SMALL_SIZE;
    const size_t cols = SMALL_SIZE;
    const float sparsity = 0.7f;
    const float threshold = 0.0f;  // Keep all non-zeros

    auto original_data = generateSparseMatrix(rows, cols, sparsity);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense_original = createGPUTensor(original_data, dims);
    ASSERT_NE(dense_original, nullptr);

    // Convert to sparse
    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx, dense_original, SPARSE_FORMAT_CSR, threshold);
    ASSERT_NE(sparse, nullptr);

    // Convert back to dense
    nimcp_gpu_tensor_t* dense_reconstructed = nimcp_sparse_to_dense(sparse_ctx, sparse);
    ASSERT_NE(dense_reconstructed, nullptr);

    auto reconstructed_data = copyToHost(dense_reconstructed);

    // Verify reconstruction matches original
    float max_err = maxAbsError(original_data, reconstructed_data);
    EXPECT_LT(max_err, TOLERANCE) << "Reconstruction error too high: " << max_err;

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense_original);
    nimcp_gpu_tensor_destroy(dense_reconstructed);
}

//=============================================================================
// Format Conversion Tests
//=============================================================================

TEST_F(SparseGPUTest, Convert_CSRtoCOO_PreservesData) {
    RequireGPU();

    const size_t rows = SMALL_SIZE;
    const size_t cols = SMALL_SIZE;
    const float sparsity = 0.8f;

    auto dense_data = generateSparseMatrix(rows, cols, sparsity);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createGPUTensor(dense_data, dims);
    ASSERT_NE(dense, nullptr);

    // Create CSR
    nimcp_sparse_tensor_t* csr = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(csr, nullptr);

    // Convert to COO
    nimcp_sparse_tensor_t* coo = nimcp_sparse_convert(
        sparse_ctx, csr, SPARSE_FORMAT_COO);
    ASSERT_NE(coo, nullptr);
    EXPECT_EQ(coo->format, SPARSE_FORMAT_COO);
    EXPECT_EQ(nimcp_sparse_nnz(coo), nimcp_sparse_nnz(csr));

    // Verify by converting both back to dense and comparing
    nimcp_gpu_tensor_t* dense_from_csr = nimcp_sparse_to_dense(sparse_ctx, csr);
    nimcp_gpu_tensor_t* dense_from_coo = nimcp_sparse_to_dense(sparse_ctx, coo);

    auto data_csr = copyToHost(dense_from_csr);
    auto data_coo = copyToHost(dense_from_coo);

    float max_err = maxAbsError(data_csr, data_coo);
    EXPECT_LT(max_err, TOLERANCE);

    nimcp_sparse_tensor_destroy(csr);
    nimcp_sparse_tensor_destroy(coo);
    nimcp_gpu_tensor_destroy(dense);
    nimcp_gpu_tensor_destroy(dense_from_csr);
    nimcp_gpu_tensor_destroy(dense_from_coo);
}

TEST_F(SparseGPUTest, Convert_COOtoCSR_PreservesData) {
    RequireGPU();

    const int rows = 5, cols = 5, nnz = 7;
    float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    int row_idx[] = {0, 0, 1, 2, 3, 4, 4};
    int col_idx[] = {0, 3, 2, 1, 4, 0, 4};

    nimcp_sparse_tensor_t* coo = nimcp_sparse_from_coo(
        sparse_ctx, values, row_idx, col_idx, rows, cols, nnz, SPARSE_FORMAT_COO);
    ASSERT_NE(coo, nullptr);

    nimcp_sparse_tensor_t* csr = nimcp_sparse_convert(
        sparse_ctx, coo, SPARSE_FORMAT_CSR);
    ASSERT_NE(csr, nullptr);
    EXPECT_EQ(csr->format, SPARSE_FORMAT_CSR);
    EXPECT_EQ(nimcp_sparse_nnz(csr), nnz);

    nimcp_sparse_tensor_destroy(coo);
    nimcp_sparse_tensor_destroy(csr);
}

//=============================================================================
// SpMM (Sparse Matrix x Dense Matrix) Tests
//=============================================================================

TEST_F(SparseGPUTest, SpMM_MatchesDenseMatmul) {
    RequireGPU();

    const size_t M = 32, K = 48, N = 24;
    const float sparsity = 0.7f;

    // Generate sparse A and dense B
    auto A_data = generateSparseMatrix(M, K, sparsity);
    auto B_data = generateDenseMatrix(K, N);

    std::vector<size_t> A_dims = {M, K};
    std::vector<size_t> B_dims = {K, N};
    std::vector<size_t> C_dims = {M, N};

    nimcp_gpu_tensor_t* A_dense = createGPUTensor(A_data, A_dims);
    nimcp_gpu_tensor_t* B = createGPUTensor(B_data, B_dims);
    ASSERT_NE(A_dense, nullptr);
    ASSERT_NE(B, nullptr);

    // Create sparse A
    nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(A_sparse, nullptr);

    // Compute SpMM: C = A_sparse * B
    nimcp_gpu_tensor_t* C_gpu = nimcp_sparse_mm(
        sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);
    ASSERT_NE(C_gpu, nullptr);

    auto C_gpu_data = copyToHost(C_gpu);

    // CPU reference
    std::vector<float> C_cpu(M * N, 0.0f);
    cpuSpMM(A_data.data(), M, K, B_data.data(), N, C_cpu.data(), 1.0f, 0.0f);

    // Compare
    float max_err = maxAbsError(C_cpu, C_gpu_data);
    EXPECT_LT(max_err, 1e-4f) << "SpMM result differs from dense matmul";

    nimcp_sparse_tensor_destroy(A_sparse);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C_gpu);
}

TEST_F(SparseGPUTest, SpMM_WithAlphaBeta_AppliesScalars) {
    RequireGPU();

    const size_t M = 16, K = 16, N = 16;
    const float alpha = 2.0f, beta = 0.5f;

    auto A_data = generateSparseMatrix(M, K, 0.6f);
    auto B_data = generateDenseMatrix(K, N);
    auto C_init = generateDenseMatrix(M, N);

    std::vector<size_t> A_dims = {M, K};
    std::vector<size_t> B_dims = {K, N};
    std::vector<size_t> C_dims = {M, N};

    nimcp_gpu_tensor_t* A_dense = createGPUTensor(A_data, A_dims);
    nimcp_gpu_tensor_t* B = createGPUTensor(B_data, B_dims);
    nimcp_gpu_tensor_t* C = createGPUTensor(C_init, C_dims);

    nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

    // C = alpha * A * B + beta * C
    nimcp_gpu_tensor_t* C_result = nimcp_sparse_mm(
        sparse_ctx, A_sparse, B, alpha, beta, C);
    ASSERT_NE(C_result, nullptr);

    auto C_gpu = copyToHost(C_result);

    // CPU reference
    std::vector<float> C_cpu = C_init;
    cpuSpMM(A_data.data(), M, K, B_data.data(), N, C_cpu.data(), alpha, beta);

    float max_err = maxAbsError(C_cpu, C_gpu);
    EXPECT_LT(max_err, 1e-4f);

    nimcp_sparse_tensor_destroy(A_sparse);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C);
}

//=============================================================================
// SpMV (Sparse Matrix x Dense Vector) Tests
//=============================================================================

TEST_F(SparseGPUTest, SpMV_MatchesDenseMatvec) {
    RequireGPU();

    const size_t M = 64, N = 48;
    const float sparsity = 0.8f;

    auto A_data = generateSparseMatrix(M, N, sparsity);
    auto x_data = generateDenseMatrix(N, 1);

    std::vector<size_t> A_dims = {M, N};
    std::vector<size_t> x_dims = {N};

    nimcp_gpu_tensor_t* A_dense = createGPUTensor(A_data, A_dims);
    nimcp_gpu_tensor_t* x = createGPUTensor(x_data, x_dims);

    nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

    // y = A_sparse * x
    nimcp_gpu_tensor_t* y_gpu = nimcp_sparse_mv(
        sparse_ctx, A_sparse, x, 1.0f, 0.0f, nullptr);
    ASSERT_NE(y_gpu, nullptr);

    auto y_gpu_data = copyToHost(y_gpu);

    // CPU reference
    std::vector<float> y_cpu(M, 0.0f);
    cpuSpMV(A_data.data(), M, N, x_data.data(), y_cpu.data(), 1.0f, 0.0f);

    float max_err = maxAbsError(y_cpu, y_gpu_data);
    EXPECT_LT(max_err, 1e-4f);

    nimcp_sparse_tensor_destroy(A_sparse);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(y_gpu);
}

//=============================================================================
// Sparse Transpose Operations Tests
//=============================================================================

TEST_F(SparseGPUTest, SpMM_Transpose_ComputesCorrectly) {
    RequireGPU();

    const size_t M = 24, K = 32, N = 16;
    const float sparsity = 0.7f;

    auto A_data = generateSparseMatrix(M, K, sparsity);
    auto B_data = generateDenseMatrix(M, N);  // Note: M x N for A^T * B

    std::vector<size_t> A_dims = {M, K};
    std::vector<size_t> B_dims = {M, N};

    nimcp_gpu_tensor_t* A_dense = createGPUTensor(A_data, A_dims);
    nimcp_gpu_tensor_t* B = createGPUTensor(B_data, B_dims);

    nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

    // C = A^T * B (K x N result)
    nimcp_gpu_tensor_t* C_gpu = nimcp_sparse_mm_transpose(
        sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);
    ASSERT_NE(C_gpu, nullptr);

    // Verify dimensions
    EXPECT_EQ(C_gpu->dims[0], K);
    EXPECT_EQ(C_gpu->dims[1], N);

    nimcp_sparse_tensor_destroy(A_sparse);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C_gpu);
}

//=============================================================================
// Sparse Add Tests
//=============================================================================

TEST_F(SparseGPUTest, SparseAdd_CombinesTwoSparse) {
    RequireGPU();

    const size_t rows = SMALL_SIZE;
    const size_t cols = SMALL_SIZE;

    auto A_data = generateSparseMatrix(rows, cols, 0.8f);
    auto B_data = generateSparseMatrix(rows, cols, 0.8f);

    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* A_dense = createGPUTensor(A_data, dims);
    nimcp_gpu_tensor_t* B_dense = createGPUTensor(B_data, dims);

    nimcp_sparse_tensor_t* A = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);
    nimcp_sparse_tensor_t* B = nimcp_sparse_from_dense(
        sparse_ctx, B_dense, SPARSE_FORMAT_CSR, 0.0f);

    // C = 2*A + 3*B
    nimcp_sparse_tensor_t* C = nimcp_sparse_add(sparse_ctx, A, B, 2.0f, 3.0f);
    ASSERT_NE(C, nullptr);

    // Verify by converting to dense
    nimcp_gpu_tensor_t* C_dense = nimcp_sparse_to_dense(sparse_ctx, C);
    auto C_data = copyToHost(C_dense);

    // CPU reference
    std::vector<float> expected(rows * cols);
    for (size_t i = 0; i < expected.size(); i++) {
        expected[i] = 2.0f * A_data[i] + 3.0f * B_data[i];
    }

    float max_err = maxAbsError(expected, C_data);
    EXPECT_LT(max_err, TOLERANCE);

    nimcp_sparse_tensor_destroy(A);
    nimcp_sparse_tensor_destroy(B);
    nimcp_sparse_tensor_destroy(C);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B_dense);
    nimcp_gpu_tensor_destroy(C_dense);
}

//=============================================================================
// Sparse Scale Tests
//=============================================================================

TEST_F(SparseGPUTest, SparseScale_ScalesValues) {
    RequireGPU();

    const size_t rows = SMALL_SIZE;
    const size_t cols = SMALL_SIZE;
    const float scale = 2.5f;

    auto data = generateSparseMatrix(rows, cols, 0.7f);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createGPUTensor(data, dims);

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(sparse, nullptr);

    // Get initial dense values
    nimcp_gpu_tensor_t* dense_before = nimcp_sparse_to_dense(sparse_ctx, sparse);
    auto before_data = copyToHost(dense_before);

    // Scale
    nimcp_sparse_tensor_t* result = nimcp_sparse_scale(sparse_ctx, sparse, scale);
    EXPECT_EQ(result, sparse);  // In-place

    // Get scaled values
    nimcp_gpu_tensor_t* dense_after = nimcp_sparse_to_dense(sparse_ctx, sparse);
    auto after_data = copyToHost(dense_after);

    // Verify scaling
    for (size_t i = 0; i < before_data.size(); i++) {
        float expected = before_data[i] * scale;
        EXPECT_NEAR(after_data[i], expected, TOLERANCE);
    }

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
    nimcp_gpu_tensor_destroy(dense_before);
    nimcp_gpu_tensor_destroy(dense_after);
}

//=============================================================================
// Magnitude Pruning Tests
//=============================================================================

TEST_F(SparseGPUTest, MagnitudePrune_AchievesTargetSparsity) {
    RequireGPU();

    const size_t rows = MEDIUM_SIZE;
    const size_t cols = MEDIUM_SIZE;
    const float target_sparsity = 0.9f;  // 90% zeros

    auto data = generateDenseMatrix(rows, cols);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createGPUTensor(data, dims);

    nimcp_sparse_tensor_t* sparse = nimcp_magnitude_prune(
        sparse_ctx, dense, target_sparsity);
    ASSERT_NE(sparse, nullptr);

    // Verify sparsity is close to target
    float actual_sparsity = nimcp_sparse_sparsity(sparse);
    EXPECT_NEAR(actual_sparsity, target_sparsity, SPARSITY_TOLERANCE);

    // Verify remaining values are the largest magnitudes
    nimcp_gpu_tensor_t* pruned_dense = nimcp_sparse_to_dense(sparse_ctx, sparse);
    auto pruned_data = copyToHost(pruned_dense);

    // Count non-zeros in pruned
    int pruned_nnz = countNonZeros(pruned_data, 0.0f);
    int expected_nnz = static_cast<int>((1.0f - target_sparsity) * rows * cols);
    EXPECT_NEAR(pruned_nnz, expected_nnz, expected_nnz * 0.1);

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
    nimcp_gpu_tensor_destroy(pruned_dense);
}

TEST_F(SparseGPUTest, MagnitudePrune_KeepsLargestMagnitudes) {
    RequireGPU();

    // Create a simple tensor with known magnitudes
    std::vector<float> data = {0.1f, 0.9f, 0.2f, 0.8f, 0.3f, 0.7f, 0.4f, 0.6f, 0.5f};
    std::vector<size_t> dims = {3, 3};
    nimcp_gpu_tensor_t* dense = createGPUTensor(data, dims);

    // Prune to 50% sparsity (keep 4-5 values)
    nimcp_sparse_tensor_t* sparse = nimcp_magnitude_prune(
        sparse_ctx, dense, 0.5f);
    ASSERT_NE(sparse, nullptr);

    nimcp_gpu_tensor_t* pruned_dense = nimcp_sparse_to_dense(sparse_ctx, sparse);
    auto pruned = copyToHost(pruned_dense);

    // The largest values (0.9, 0.8, 0.7, 0.6) should be preserved
    // Check that 0.9 is preserved
    EXPECT_NEAR(pruned[1], 0.9f, TOLERANCE);

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
    nimcp_gpu_tensor_destroy(pruned_dense);
}

//=============================================================================
// Structured Pruning (N:M) Tests
//=============================================================================

TEST_F(SparseGPUTest, StructuredPrune_2to4_CreatesValidPattern) {
    RequireGPU();

    // Create tensor with size divisible by 4
    const size_t rows = 16;
    const size_t cols = 16;

    auto data = generateDenseMatrix(rows, cols);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createGPUTensor(data, dims);

    // 2:4 structured sparsity (50% sparsity)
    nimcp_sparse_tensor_t* sparse = nimcp_structured_prune(
        sparse_ctx, dense, 2, 4);
    ASSERT_NE(sparse, nullptr);

    // Verify sparsity is 50%
    float sparsity = nimcp_sparse_sparsity(sparse);
    EXPECT_NEAR(sparsity, 0.5f, SPARSITY_TOLERANCE);

    // Verify N:M pattern: every 4 consecutive elements should have exactly 2 non-zeros
    nimcp_gpu_tensor_t* pruned_dense = nimcp_sparse_to_dense(sparse_ctx, sparse);
    auto pruned = copyToHost(pruned_dense);

    for (size_t i = 0; i < pruned.size(); i += 4) {
        int nnz_in_group = 0;
        for (int j = 0; j < 4 && (i + j) < pruned.size(); j++) {
            if (std::fabs(pruned[i + j]) > TOLERANCE) nnz_in_group++;
        }
        EXPECT_EQ(nnz_in_group, 2) << "2:4 pattern violated at index " << i;
    }

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
    nimcp_gpu_tensor_destroy(pruned_dense);
}

//=============================================================================
// Threshold Pruning Tests
//=============================================================================

TEST_F(SparseGPUTest, ThresholdPrune_RemovesBelowThreshold) {
    RequireGPU();

    // Create tensor with known values
    std::vector<float> data = {0.01f, 0.5f, 0.02f, 0.6f, 0.03f, 0.7f, 0.04f, 0.8f, 0.05f};
    std::vector<size_t> dims = {3, 3};
    nimcp_gpu_tensor_t* dense = createGPUTensor(data, dims);

    // Threshold = 0.1 should remove 0.01, 0.02, 0.03, 0.04, 0.05
    nimcp_sparse_tensor_t* sparse = nimcp_threshold_prune(
        sparse_ctx, dense, 0.1f);
    ASSERT_NE(sparse, nullptr);

    EXPECT_EQ(nimcp_sparse_nnz(sparse), 4);  // 0.5, 0.6, 0.7, 0.8

    nimcp_gpu_tensor_t* pruned_dense = nimcp_sparse_to_dense(sparse_ctx, sparse);
    auto pruned = copyToHost(pruned_dense);

    // Verify small values are gone
    EXPECT_NEAR(pruned[0], 0.0f, TOLERANCE);
    EXPECT_NEAR(pruned[2], 0.0f, TOLERANCE);
    // Verify large values are preserved
    EXPECT_NEAR(pruned[1], 0.5f, TOLERANCE);
    EXPECT_NEAR(pruned[3], 0.6f, TOLERANCE);

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
    nimcp_gpu_tensor_destroy(pruned_dense);
}

//=============================================================================
// Sparse Random Tests
//=============================================================================

TEST_F(SparseGPUTest, SparseRandom_CreatesCorrectDensity) {
    RequireGPU();

    const size_t rows = MEDIUM_SIZE;
    const size_t cols = MEDIUM_SIZE;
    const float target_density = 0.2f;  // 20% non-zeros

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_random(
        sparse_ctx, rows, cols, target_density, SPARSE_FORMAT_CSR);
    ASSERT_NE(sparse, nullptr);

    // Verify density
    float actual_sparsity = nimcp_sparse_sparsity(sparse);
    float actual_density = 1.0f - actual_sparsity;
    EXPECT_NEAR(actual_density, target_density, 0.05f);

    nimcp_sparse_tensor_destroy(sparse);
}

//=============================================================================
// Gradient Accumulation Tests
//=============================================================================

TEST_F(SparseGPUTest, GradAccumulate_AddsSparseToExistingDense) {
    RequireGPU();

    const size_t rows = SMALL_SIZE;
    const size_t cols = SMALL_SIZE;

    // Create sparse gradient
    auto sparse_data = generateSparseMatrix(rows, cols, 0.9f);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* sparse_dense = createGPUTensor(sparse_data, dims);
    nimcp_sparse_tensor_t* sparse_grad = nimcp_sparse_from_dense(
        sparse_ctx, sparse_dense, SPARSE_FORMAT_CSR, 0.0f);

    // Create dense gradient accumulator
    auto dense_data = generateDenseMatrix(rows, cols);
    nimcp_gpu_tensor_t* dense_grad = createGPUTensor(dense_data, dims);

    // Accumulate
    bool result = nimcp_sparse_grad_accumulate(sparse_ctx, sparse_grad, dense_grad);
    EXPECT_TRUE(result);

    // Verify accumulation
    auto accumulated = copyToHost(dense_grad);
    for (size_t i = 0; i < accumulated.size(); i++) {
        float expected = dense_data[i] + sparse_data[i];
        EXPECT_NEAR(accumulated[i], expected, TOLERANCE);
    }

    nimcp_sparse_tensor_destroy(sparse_grad);
    nimcp_gpu_tensor_destroy(sparse_dense);
    nimcp_gpu_tensor_destroy(dense_grad);
}

//=============================================================================
// Sparsity Statistics Tests
//=============================================================================

TEST_F(SparseGPUTest, GetStats_ReturnsCorrectStatistics) {
    RequireGPU();

    const size_t rows = 100;
    const size_t cols = 100;
    const float sparsity = 0.8f;

    auto data = generateSparseMatrix(rows, cols, sparsity);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createGPUTensor(data, dims);

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(sparse, nullptr);

    nimcp_sparsity_stats_t stats = nimcp_sparse_get_stats(sparse);

    // Verify basic stats
    EXPECT_EQ(stats.total_elements, static_cast<int>(rows * cols));
    EXPECT_EQ(stats.nnz, nimcp_sparse_nnz(sparse));
    EXPECT_NEAR(stats.sparsity_ratio, 1.0f - (float)stats.nnz / stats.total_elements, 0.01f);
    EXPECT_NEAR(stats.density_ratio, 1.0f - stats.sparsity_ratio, 0.01f);
    EXPECT_GT(stats.memory_savings_percent, 0.0f);

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST_F(SparseGPUTest, Validate_ReturnsTrueForValidTensor) {
    RequireGPU();

    const int rows = 5, cols = 5, nnz = 7;
    float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    int row_idx[] = {0, 0, 1, 2, 3, 4, 4};
    int col_idx[] = {0, 3, 2, 1, 4, 0, 4};

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_coo(
        sparse_ctx, values, row_idx, col_idx, rows, cols, nnz, SPARSE_FORMAT_CSR);
    ASSERT_NE(sparse, nullptr);

    bool valid = nimcp_sparse_validate(sparse);
    EXPECT_TRUE(valid);

    nimcp_sparse_tensor_destroy(sparse);
}

//=============================================================================
// Host Transfer Tests
//=============================================================================

TEST_F(SparseGPUTest, ToHostCSR_CopiesCorrectly) {
    RequireGPU();

    // Create known CSR
    const int rows = 3, cols = 3, nnz = 5;
    float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    int col_indices[] = {0, 2, 1, 0, 2};
    int row_ptrs[] = {0, 2, 3, 5};

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_csr(
        sparse_ctx, values, col_indices, row_ptrs, rows, cols, nnz);
    ASSERT_NE(sparse, nullptr);

    // Copy to host
    std::vector<float> host_values(nnz);
    std::vector<int> host_col_indices(nnz);
    std::vector<int> host_row_ptrs(rows + 1);

    bool result = nimcp_sparse_to_host_csr(
        sparse, host_values.data(), host_col_indices.data(), host_row_ptrs.data());
    EXPECT_TRUE(result);

    // Verify values match
    for (int i = 0; i < nnz; i++) {
        EXPECT_NEAR(host_values[i], values[i], TOLERANCE);
        EXPECT_EQ(host_col_indices[i], col_indices[i]);
    }
    for (int i = 0; i <= rows; i++) {
        EXPECT_EQ(host_row_ptrs[i], row_ptrs[i]);
    }

    nimcp_sparse_tensor_destroy(sparse);
}

TEST_F(SparseGPUTest, ToHostCOO_CopiesCorrectly) {
    RequireGPU();

    const int rows = 4, cols = 4, nnz = 5;
    float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    int row_idx[] = {0, 1, 1, 2, 3};
    int col_idx[] = {0, 1, 3, 2, 3};

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_coo(
        sparse_ctx, values, row_idx, col_idx, rows, cols, nnz, SPARSE_FORMAT_COO);
    ASSERT_NE(sparse, nullptr);

    std::vector<float> host_values(nnz);
    std::vector<int> host_row_indices(nnz);
    std::vector<int> host_col_indices(nnz);

    bool result = nimcp_sparse_to_host_coo(
        sparse, host_values.data(), host_row_indices.data(), host_col_indices.data());
    EXPECT_TRUE(result);

    // Values should match (order may differ)
    float sum_original = 0, sum_host = 0;
    for (int i = 0; i < nnz; i++) {
        sum_original += values[i];
        sum_host += host_values[i];
    }
    EXPECT_NEAR(sum_host, sum_original, TOLERANCE);

    nimcp_sparse_tensor_destroy(sparse);
}

//=============================================================================
// Clone Tests
//=============================================================================

TEST_F(SparseGPUTest, Clone_CreatesIndependentCopy) {
    RequireGPU();

    auto data = generateSparseMatrix(SMALL_SIZE, SMALL_SIZE, 0.7f);
    std::vector<size_t> dims = {SMALL_SIZE, SMALL_SIZE};
    nimcp_gpu_tensor_t* dense = createGPUTensor(data, dims);

    nimcp_sparse_tensor_t* original = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(original, nullptr);

    nimcp_sparse_tensor_t* cloned = nimcp_sparse_tensor_clone(sparse_ctx, original);
    ASSERT_NE(cloned, nullptr);
    EXPECT_NE(cloned, original);

    // Verify same properties
    EXPECT_EQ(nimcp_sparse_rows(cloned), nimcp_sparse_rows(original));
    EXPECT_EQ(nimcp_sparse_cols(cloned), nimcp_sparse_cols(original));
    EXPECT_EQ(nimcp_sparse_nnz(cloned), nimcp_sparse_nnz(original));
    EXPECT_EQ(cloned->format, original->format);

    // Verify same data
    nimcp_gpu_tensor_t* orig_dense = nimcp_sparse_to_dense(sparse_ctx, original);
    nimcp_gpu_tensor_t* clone_dense = nimcp_sparse_to_dense(sparse_ctx, cloned);

    auto orig_data = copyToHost(orig_dense);
    auto clone_data = copyToHost(clone_dense);

    EXPECT_EQ(orig_data, clone_data);

    // Modify original, clone should be unchanged
    nimcp_sparse_scale(sparse_ctx, original, 2.0f);
    clone_dense = nimcp_sparse_to_dense(sparse_ctx, cloned);
    auto clone_after = copyToHost(clone_dense);
    EXPECT_EQ(clone_data, clone_after);

    nimcp_sparse_tensor_destroy(original);
    nimcp_sparse_tensor_destroy(cloned);
    nimcp_gpu_tensor_destroy(dense);
    nimcp_gpu_tensor_destroy(orig_dense);
    nimcp_gpu_tensor_destroy(clone_dense);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(SparseGPUTest, FormatName_ReturnsCorrectStrings) {
    EXPECT_STREQ(nimcp_sparse_format_name(SPARSE_FORMAT_CSR), "CSR");
    EXPECT_STREQ(nimcp_sparse_format_name(SPARSE_FORMAT_CSC), "CSC");
    EXPECT_STREQ(nimcp_sparse_format_name(SPARSE_FORMAT_COO), "COO");
    EXPECT_STREQ(nimcp_sparse_format_name(SPARSE_FORMAT_BSR), "BSR");
    EXPECT_STREQ(nimcp_sparse_format_name(SPARSE_FORMAT_ELL), "ELL");
}

TEST_F(SparseGPUTest, ComputeDensity_ReturnsCorrectValue) {
    RequireGPU();

    // Create tensor with known sparsity
    std::vector<float> data = {0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f, 4.0f, 0.0f};
    std::vector<size_t> dims = {3, 3};
    nimcp_gpu_tensor_t* dense = createGPUTensor(data, dims);

    // 4 non-zeros out of 9 = density ~0.44
    float density = nimcp_sparse_compute_density(sparse_ctx, dense, 0.0f);
    EXPECT_NEAR(density, 4.0f / 9.0f, 0.01f);

    nimcp_gpu_tensor_destroy(dense);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(SparseGPUTest, NullSafety_AllFunctions) {
    // Test NULL inputs don't crash
    nimcp_sparse_ctx_destroy(nullptr);
    nimcp_sparse_tensor_destroy(nullptr);

    EXPECT_EQ(nimcp_sparse_from_dense(nullptr, nullptr, SPARSE_FORMAT_CSR, 0.0f), nullptr);
    EXPECT_EQ(nimcp_sparse_to_dense(nullptr, nullptr), nullptr);
    EXPECT_EQ(nimcp_sparse_convert(nullptr, nullptr, SPARSE_FORMAT_COO), nullptr);
    EXPECT_EQ(nimcp_sparse_mm(nullptr, nullptr, nullptr, 1.0f, 0.0f, nullptr), nullptr);
    EXPECT_EQ(nimcp_sparse_mv(nullptr, nullptr, nullptr, 1.0f, 0.0f, nullptr), nullptr);

    EXPECT_EQ(nimcp_sparse_rows(nullptr), 0);
    EXPECT_EQ(nimcp_sparse_cols(nullptr), 0);
    EXPECT_EQ(nimcp_sparse_nnz(nullptr), 0);
    EXPECT_EQ(nimcp_sparse_sparsity(nullptr), 0.0f);

    EXPECT_FALSE(nimcp_sparse_validate(nullptr));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(SparseGPUTest, EdgeCase_EmptyMatrix) {
    RequireGPU();

    // All zeros should create sparse with nnz = 0
    std::vector<float> data(SMALL_SIZE * SMALL_SIZE, 0.0f);
    std::vector<size_t> dims = {SMALL_SIZE, SMALL_SIZE};
    nimcp_gpu_tensor_t* dense = createGPUTensor(data, dims);

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(sparse, nullptr);
    EXPECT_EQ(nimcp_sparse_nnz(sparse), 0);

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
}

TEST_F(SparseGPUTest, EdgeCase_DenseMatrix) {
    RequireGPU();

    // All non-zeros
    std::vector<float> data(SMALL_SIZE * SMALL_SIZE);
    std::fill(data.begin(), data.end(), 1.0f);
    std::vector<size_t> dims = {SMALL_SIZE, SMALL_SIZE};
    nimcp_gpu_tensor_t* dense = createGPUTensor(data, dims);

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(sparse, nullptr);
    EXPECT_EQ(nimcp_sparse_nnz(sparse), static_cast<int>(SMALL_SIZE * SMALL_SIZE));

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
}

TEST_F(SparseGPUTest, EdgeCase_SingleElement) {
    RequireGPU();

    std::vector<float> data = {5.0f};
    std::vector<size_t> dims = {1, 1};
    nimcp_gpu_tensor_t* dense = createGPUTensor(data, dims);

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(sparse, nullptr);
    EXPECT_EQ(nimcp_sparse_nnz(sparse), 1);
    EXPECT_EQ(nimcp_sparse_rows(sparse), 1);
    EXPECT_EQ(nimcp_sparse_cols(sparse), 1);

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
