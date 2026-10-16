/**
 * @file regression_gpu_test_sparse.cpp
 * @brief Regression tests for GPU Sparse Tensor Operations
 *
 * WHAT: Regression tests to verify numerical accuracy and stability over time
 * WHY:  Prevent reintroduction of bugs and numerical drift in sparse operations
 * HOW:  Test SpMM vs dense matmul accuracy, sparsity preservation, memory usage
 *
 * TEST COVERAGE:
 * - Numerical accuracy of SpMM vs dense matrix multiplication
 * - Sparsity preservation after various operations
 * - Memory usage scaling with different sparsity levels
 * - Numerical stability under repeated operations
 * - Format conversion accuracy preservation
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
#include <iomanip>

// GPU headers
#include "gpu/sparse/nimcp_sparse_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

// Headers already have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr size_t SMALL_SIZE = 64;
    constexpr size_t MEDIUM_SIZE = 256;
    constexpr size_t LARGE_SIZE = 1024;
    constexpr float STRICT_TOLERANCE = 1e-5f;
    constexpr float RELAXED_TOLERANCE = 1e-4f;
    constexpr float SPARSITY_TOLERANCE = 0.02f;
    constexpr int STABILITY_ITERATIONS = 100;

//=============================================================================
// Test Fixture
//=============================================================================

class SparseGPURegressionTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    nimcp_sparse_ctx_t* sparse_ctx = nullptr;
    std::mt19937 rng{98765};
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

    // Helper: Generate matrix with specific sparsity
    std::vector<float> generateMatrix(size_t rows, size_t cols, float sparsity) {
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

    // Helper: Create GPU tensor
    nimcp_gpu_tensor_t* createTensor(const std::vector<float>& data,
                                      const std::vector<size_t>& dims) {
        if (!gpu_ctx) return nullptr;
        return nimcp_gpu_tensor_from_host(gpu_ctx, data.data(), dims.data(),
                                          dims.size(), NIMCP_GPU_PRECISION_FP32);
    }

    // Helper: Copy to host
    std::vector<float> copyToHost(const nimcp_gpu_tensor_t* tensor) {
        if (!tensor) return {};
        std::vector<float> result(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, result.data());
        return result;
    }

    // Helper: CPU reference matmul
    void cpuMatmul(const float* A, size_t M, size_t K,
                   const float* B, size_t N,
                   float* C) {
        for (size_t i = 0; i < M; i++) {
            for (size_t j = 0; j < N; j++) {
                double sum = 0.0;  // Use double for reference accuracy
                for (size_t k = 0; k < K; k++) {
                    sum += static_cast<double>(A[i * K + k]) * B[k * N + j];
                }
                C[i * N + j] = static_cast<float>(sum);
            }
        }
    }

    // Helper: Compute statistics
    struct ErrorStats {
        float max_abs;
        float mean_abs;
        float rmse;
        float max_rel;
    };

    ErrorStats computeErrorStats(const std::vector<float>& ref,
                                  const std::vector<float>& test) {
        ErrorStats stats = {0.0f, 0.0f, 0.0f, 0.0f};
        if (ref.size() != test.size()) return stats;

        double sum_abs = 0.0, sum_sq = 0.0;
        for (size_t i = 0; i < ref.size(); i++) {
            float abs_err = std::fabs(ref[i] - test[i]);
            float rel_err = (std::fabs(ref[i]) > 1e-10f) ?
                abs_err / std::fabs(ref[i]) : abs_err;

            stats.max_abs = std::max(stats.max_abs, abs_err);
            stats.max_rel = std::max(stats.max_rel, rel_err);
            sum_abs += abs_err;
            sum_sq += abs_err * abs_err;
        }

        stats.mean_abs = static_cast<float>(sum_abs / ref.size());
        stats.rmse = static_cast<float>(std::sqrt(sum_sq / ref.size()));
        return stats;
    }

    // Helper: Count non-zeros
    int countNonZeros(const std::vector<float>& data, float threshold = 0.0f) {
        int count = 0;
        for (float v : data) {
            if (std::fabs(v) > threshold) count++;
        }
        return count;
    }

    // Helper: Print error stats
    void printErrorStats(const std::string& name, const ErrorStats& stats) {
        std::cout << std::fixed << std::setprecision(8);
        std::cout << "  " << name << ":" << std::endl;
        std::cout << "    Max Abs Error: " << stats.max_abs << std::endl;
        std::cout << "    Mean Abs Error: " << stats.mean_abs << std::endl;
        std::cout << "    RMSE: " << stats.rmse << std::endl;
        std::cout << "    Max Rel Error: " << stats.max_rel << std::endl;
    }
};

//=============================================================================
// Numerical Accuracy Tests - SpMM vs Dense
//=============================================================================

TEST_F(SparseGPURegressionTest, SpMM_Accuracy_SmallMatrix) {
    RequireGPU();

    const size_t M = SMALL_SIZE, K = SMALL_SIZE, N = SMALL_SIZE;
    const float sparsity = 0.7f;

    auto A_data = generateMatrix(M, K, sparsity);
    auto B_data = generateMatrix(K, N, 0.0f);

    std::vector<size_t> A_dims = {M, K};
    std::vector<size_t> B_dims = {K, N};

    nimcp_gpu_tensor_t* A_dense = createTensor(A_data, A_dims);
    nimcp_gpu_tensor_t* B = createTensor(B_data, B_dims);

    nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

    // Sparse multiplication
    nimcp_gpu_tensor_t* C_sparse = nimcp_sparse_mm(
        sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);
    ASSERT_NE(C_sparse, nullptr);

    auto sparse_result = copyToHost(C_sparse);

    // CPU reference
    std::vector<float> cpu_result(M * N);
    cpuMatmul(A_data.data(), M, K, B_data.data(), N, cpu_result.data());

    ErrorStats stats = computeErrorStats(cpu_result, sparse_result);

    std::cout << "\n=== SpMM Accuracy (Small " << M << "x" << K << " @ " << K << "x" << N << ") ===" << std::endl;
    printErrorStats("SpMM vs CPU", stats);

    EXPECT_LT(stats.max_abs, STRICT_TOLERANCE)
        << "Max absolute error exceeds tolerance";
    EXPECT_LT(stats.rmse, STRICT_TOLERANCE)
        << "RMSE exceeds tolerance";

    nimcp_sparse_tensor_destroy(A_sparse);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C_sparse);
}

TEST_F(SparseGPURegressionTest, SpMM_Accuracy_MediumMatrix) {
    RequireGPU();

    const size_t M = MEDIUM_SIZE, K = MEDIUM_SIZE, N = MEDIUM_SIZE / 2;
    const float sparsity = 0.8f;

    auto A_data = generateMatrix(M, K, sparsity);
    auto B_data = generateMatrix(K, N, 0.0f);

    std::vector<size_t> A_dims = {M, K};
    std::vector<size_t> B_dims = {K, N};

    nimcp_gpu_tensor_t* A_dense = createTensor(A_data, A_dims);
    nimcp_gpu_tensor_t* B = createTensor(B_data, B_dims);

    nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

    nimcp_gpu_tensor_t* C_sparse = nimcp_sparse_mm(
        sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);

    auto sparse_result = copyToHost(C_sparse);

    std::vector<float> cpu_result(M * N);
    cpuMatmul(A_data.data(), M, K, B_data.data(), N, cpu_result.data());

    ErrorStats stats = computeErrorStats(cpu_result, sparse_result);

    std::cout << "\n=== SpMM Accuracy (Medium " << M << "x" << K << " @ " << K << "x" << N << ") ===" << std::endl;
    printErrorStats("SpMM vs CPU", stats);

    EXPECT_LT(stats.max_abs, RELAXED_TOLERANCE);
    EXPECT_LT(stats.rmse, STRICT_TOLERANCE);

    nimcp_sparse_tensor_destroy(A_sparse);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C_sparse);
}

TEST_F(SparseGPURegressionTest, SpMM_Accuracy_HighSparsity) {
    RequireGPU();

    const size_t M = MEDIUM_SIZE, K = MEDIUM_SIZE, N = SMALL_SIZE;
    const float sparsity = 0.95f;  // 95% zeros

    auto A_data = generateMatrix(M, K, sparsity);
    auto B_data = generateMatrix(K, N, 0.0f);

    std::vector<size_t> A_dims = {M, K};
    std::vector<size_t> B_dims = {K, N};

    nimcp_gpu_tensor_t* A_dense = createTensor(A_data, A_dims);
    nimcp_gpu_tensor_t* B = createTensor(B_data, B_dims);

    nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

    float actual_sparsity = nimcp_sparse_sparsity(A_sparse);
    EXPECT_NEAR(actual_sparsity, sparsity, SPARSITY_TOLERANCE);

    nimcp_gpu_tensor_t* C_sparse = nimcp_sparse_mm(
        sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);

    auto sparse_result = copyToHost(C_sparse);

    std::vector<float> cpu_result(M * N);
    cpuMatmul(A_data.data(), M, K, B_data.data(), N, cpu_result.data());

    ErrorStats stats = computeErrorStats(cpu_result, sparse_result);

    std::cout << "\n=== SpMM Accuracy (High Sparsity " << (sparsity*100) << "%) ===" << std::endl;
    printErrorStats("SpMM vs CPU", stats);

    EXPECT_LT(stats.max_abs, RELAXED_TOLERANCE);

    nimcp_sparse_tensor_destroy(A_sparse);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C_sparse);
}

TEST_F(SparseGPURegressionTest, SpMM_Accuracy_VaryingSparsityLevels) {
    RequireGPU();

    const size_t M = 128, K = 128, N = 64;
    std::vector<float> sparsity_levels = {0.0f, 0.5f, 0.7f, 0.9f, 0.95f, 0.99f};

    std::cout << "\n=== SpMM Accuracy Across Sparsity Levels ===" << std::endl;
    std::cout << "| Sparsity | Max Abs Err | Mean Abs Err | RMSE |" << std::endl;
    std::cout << "|----------|-------------|--------------|------|" << std::endl;

    for (float sparsity : sparsity_levels) {
        auto A_data = generateMatrix(M, K, sparsity);
        auto B_data = generateMatrix(K, N, 0.0f);

        std::vector<size_t> A_dims = {M, K};
        std::vector<size_t> B_dims = {K, N};

        nimcp_gpu_tensor_t* A_dense = createTensor(A_data, A_dims);
        nimcp_gpu_tensor_t* B = createTensor(B_data, B_dims);

        nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
            sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

        nimcp_gpu_tensor_t* C_sparse = nimcp_sparse_mm(
            sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);

        auto sparse_result = copyToHost(C_sparse);

        std::vector<float> cpu_result(M * N);
        cpuMatmul(A_data.data(), M, K, B_data.data(), N, cpu_result.data());

        ErrorStats stats = computeErrorStats(cpu_result, sparse_result);

        std::cout << "| " << std::fixed << std::setprecision(2) << (sparsity * 100) << "% "
                  << "| " << std::scientific << std::setprecision(2) << stats.max_abs
                  << " | " << stats.mean_abs
                  << " | " << stats.rmse << " |" << std::endl;

        EXPECT_LT(stats.max_abs, RELAXED_TOLERANCE)
            << "Accuracy failed at sparsity " << sparsity;

        nimcp_sparse_tensor_destroy(A_sparse);
        nimcp_gpu_tensor_destroy(A_dense);
        nimcp_gpu_tensor_destroy(B);
        nimcp_gpu_tensor_destroy(C_sparse);
    }
}

//=============================================================================
// Sparsity Preservation Tests
//=============================================================================

TEST_F(SparseGPURegressionTest, SparsityPreservation_AfterConversion) {
    RequireGPU();

    const size_t rows = MEDIUM_SIZE;
    const size_t cols = MEDIUM_SIZE;
    const float target_sparsity = 0.85f;

    auto data = generateMatrix(rows, cols, target_sparsity);
    int original_nnz = countNonZeros(data, 0.0f);

    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createTensor(data, dims);

    // Convert dense -> CSR -> COO -> CSR -> dense
    nimcp_sparse_tensor_t* csr1 = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(csr1, nullptr);

    nimcp_sparse_tensor_t* coo = nimcp_sparse_convert(
        sparse_ctx, csr1, SPARSE_FORMAT_COO);
    ASSERT_NE(coo, nullptr);

    nimcp_sparse_tensor_t* csr2 = nimcp_sparse_convert(
        sparse_ctx, coo, SPARSE_FORMAT_CSR);
    ASSERT_NE(csr2, nullptr);

    nimcp_gpu_tensor_t* reconstructed = nimcp_sparse_to_dense(sparse_ctx, csr2);
    ASSERT_NE(reconstructed, nullptr);

    auto reconstructed_data = copyToHost(reconstructed);
    int final_nnz = countNonZeros(reconstructed_data, 0.0f);

    std::cout << "\n=== Sparsity Preservation After Conversions ===" << std::endl;
    std::cout << "Original nnz: " << original_nnz << std::endl;
    std::cout << "After CSR->COO->CSR nnz: " << final_nnz << std::endl;
    std::cout << "CSR1 nnz: " << nimcp_sparse_nnz(csr1) << std::endl;
    std::cout << "COO nnz: " << nimcp_sparse_nnz(coo) << std::endl;
    std::cout << "CSR2 nnz: " << nimcp_sparse_nnz(csr2) << std::endl;

    EXPECT_EQ(final_nnz, original_nnz);
    EXPECT_EQ(nimcp_sparse_nnz(csr1), original_nnz);
    EXPECT_EQ(nimcp_sparse_nnz(coo), original_nnz);
    EXPECT_EQ(nimcp_sparse_nnz(csr2), original_nnz);

    // Verify data integrity
    ErrorStats stats = computeErrorStats(data, reconstructed_data);
    EXPECT_LT(stats.max_abs, STRICT_TOLERANCE);

    nimcp_sparse_tensor_destroy(csr1);
    nimcp_sparse_tensor_destroy(coo);
    nimcp_sparse_tensor_destroy(csr2);
    nimcp_gpu_tensor_destroy(dense);
    nimcp_gpu_tensor_destroy(reconstructed);
}

TEST_F(SparseGPURegressionTest, SparsityPreservation_AfterScaling) {
    RequireGPU();

    const size_t rows = 100;
    const size_t cols = 100;
    const float sparsity = 0.8f;

    auto data = generateMatrix(rows, cols, sparsity);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createTensor(data, dims);

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    int original_nnz = nimcp_sparse_nnz(sparse);

    // Scale by various factors
    std::vector<float> scales = {2.0f, 0.5f, -1.0f, 0.0f, 100.0f};

    for (float scale : scales) {
        // Clone and scale
        nimcp_sparse_tensor_t* scaled = nimcp_sparse_tensor_clone(sparse_ctx, sparse);
        nimcp_sparse_scale(sparse_ctx, scaled, scale);

        int scaled_nnz = nimcp_sparse_nnz(scaled);

        if (scale != 0.0f) {
            EXPECT_EQ(scaled_nnz, original_nnz)
                << "NNZ changed after scaling by " << scale;
        }

        nimcp_sparse_tensor_destroy(scaled);
    }

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);
}

TEST_F(SparseGPURegressionTest, SparsityPreservation_AfterAddition) {
    RequireGPU();

    const size_t rows = 64;
    const size_t cols = 64;

    // Create two sparse matrices with different sparsity patterns
    auto A_data = generateMatrix(rows, cols, 0.9f);
    auto B_data = generateMatrix(rows, cols, 0.9f);

    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* A_dense = createTensor(A_data, dims);
    nimcp_gpu_tensor_t* B_dense = createTensor(B_data, dims);

    nimcp_sparse_tensor_t* A = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);
    nimcp_sparse_tensor_t* B = nimcp_sparse_from_dense(
        sparse_ctx, B_dense, SPARSE_FORMAT_CSR, 0.0f);

    // C = A + B
    nimcp_sparse_tensor_t* C = nimcp_sparse_add(sparse_ctx, A, B, 1.0f, 1.0f);
    ASSERT_NE(C, nullptr);

    // Expected: union of non-zero patterns
    std::vector<float> expected(rows * cols);
    int expected_nnz = 0;
    for (size_t i = 0; i < expected.size(); i++) {
        expected[i] = A_data[i] + B_data[i];
        if (std::fabs(expected[i]) > 0.0f) expected_nnz++;
    }

    nimcp_gpu_tensor_t* C_dense = nimcp_sparse_to_dense(sparse_ctx, C);
    auto C_data = copyToHost(C_dense);

    int actual_nnz = countNonZeros(C_data, 0.0f);

    std::cout << "\n=== Sparse Addition NNZ ===" << std::endl;
    std::cout << "A nnz: " << nimcp_sparse_nnz(A) << std::endl;
    std::cout << "B nnz: " << nimcp_sparse_nnz(B) << std::endl;
    std::cout << "C nnz (sparse): " << nimcp_sparse_nnz(C) << std::endl;
    std::cout << "Expected nnz: " << expected_nnz << std::endl;
    std::cout << "Actual dense nnz: " << actual_nnz << std::endl;

    // Verify data
    ErrorStats stats = computeErrorStats(expected, C_data);
    EXPECT_LT(stats.max_abs, STRICT_TOLERANCE);

    nimcp_sparse_tensor_destroy(A);
    nimcp_sparse_tensor_destroy(B);
    nimcp_sparse_tensor_destroy(C);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B_dense);
    nimcp_gpu_tensor_destroy(C_dense);
}

//=============================================================================
// Memory Usage Tests
//=============================================================================

TEST_F(SparseGPURegressionTest, MemoryUsage_ScalesWithSparsity) {
    RequireGPU();

    const size_t rows = 512;
    const size_t cols = 512;
    size_t dense_size = rows * cols * sizeof(float);

    std::vector<float> sparsity_levels = {0.5f, 0.7f, 0.9f, 0.95f, 0.99f};

    std::cout << "\n=== Memory Usage vs Sparsity ===" << std::endl;
    std::cout << "Dense size: " << (dense_size / 1024.0) << " KB" << std::endl;
    std::cout << "| Sparsity | NNZ | Sparse Size (KB) | Savings % |" << std::endl;
    std::cout << "|----------|-----|------------------|-----------|" << std::endl;

    for (float sparsity : sparsity_levels) {
        auto data = generateMatrix(rows, cols, sparsity);
        std::vector<size_t> dims = {rows, cols};
        nimcp_gpu_tensor_t* dense = createTensor(data, dims);

        nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
            sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);

        nimcp_sparsity_stats_t stats = nimcp_sparse_get_stats(sparse);

        // Compute expected sparse memory (CSR format)
        // values: nnz * sizeof(float)
        // col_indices: nnz * sizeof(int)
        // row_ptrs: (rows + 1) * sizeof(int)
        size_t expected_sparse_size = stats.nnz * sizeof(float) +
                                       stats.nnz * sizeof(int) +
                                       (rows + 1) * sizeof(int);

        float savings = 100.0f * (1.0f - static_cast<float>(expected_sparse_size) / dense_size);

        std::cout << "| " << std::fixed << std::setprecision(0) << (sparsity * 100) << "% "
                  << "| " << stats.nnz
                  << " | " << std::setprecision(1) << (expected_sparse_size / 1024.0)
                  << " | " << std::setprecision(1) << savings << "% |" << std::endl;

        // For high sparsity, sparse format should save memory
        if (sparsity > 0.7f) {
            EXPECT_GT(stats.memory_savings_percent, 0.0f)
                << "Expected memory savings at sparsity " << sparsity;
        }

        nimcp_sparse_tensor_destroy(sparse);
        nimcp_gpu_tensor_destroy(dense);
    }
}

TEST_F(SparseGPURegressionTest, MemoryUsage_NoLeaksUnderRepeatedOperations) {
    RequireGPU();

    const size_t rows = 128;
    const size_t cols = 128;
    const int num_iterations = 50;

    nimcp_memory_stats_t initial_stats;
    nimcp_memory_get_stats(&initial_stats);

    for (int iter = 0; iter < num_iterations; iter++) {
        auto A_data = generateMatrix(rows, cols, 0.8f);
        auto B_data = generateMatrix(cols, rows / 2, 0.0f);

        std::vector<size_t> A_dims = {rows, cols};
        std::vector<size_t> B_dims = {cols, rows / 2};

        nimcp_gpu_tensor_t* A_dense = createTensor(A_data, A_dims);
        nimcp_gpu_tensor_t* B = createTensor(B_data, B_dims);

        nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
            sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

        // SpMM
        nimcp_gpu_tensor_t* C = nimcp_sparse_mm(
            sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);

        // Convert to COO and back
        nimcp_sparse_tensor_t* coo = nimcp_sparse_convert(
            sparse_ctx, A_sparse, SPARSE_FORMAT_COO);
        nimcp_sparse_tensor_t* csr = nimcp_sparse_convert(
            sparse_ctx, coo, SPARSE_FORMAT_CSR);

        // Cleanup
        nimcp_sparse_tensor_destroy(A_sparse);
        nimcp_sparse_tensor_destroy(coo);
        nimcp_sparse_tensor_destroy(csr);
        nimcp_gpu_tensor_destroy(A_dense);
        nimcp_gpu_tensor_destroy(B);
        nimcp_gpu_tensor_destroy(C);
    }

    nimcp_memory_stats_t final_stats;
    nimcp_memory_get_stats(&final_stats);

    size_t leaked = final_stats.current_allocated - initial_stats.current_allocated;

    std::cout << "\n=== Memory Leak Check ===" << std::endl;
    std::cout << "Iterations: " << num_iterations << std::endl;
    std::cout << "Initial allocated: " << initial_stats.current_allocated << " bytes" << std::endl;
    std::cout << "Final allocated: " << final_stats.current_allocated << " bytes" << std::endl;
    std::cout << "Potential leak: " << leaked << " bytes" << std::endl;

    // Allow some overhead for caches, but not much
    EXPECT_LT(leaked, 8192) << "Possible memory leak detected";
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(SparseGPURegressionTest, NumericalStability_RepeatedSpMM) {
    RequireGPU();

    const size_t M = 64, K = 64, N = 64;
    const float sparsity = 0.7f;

    auto A_data = generateMatrix(M, K, sparsity);
    auto B_data = generateMatrix(K, N, 0.0f);

    std::vector<size_t> A_dims = {M, K};
    std::vector<size_t> B_dims = {K, N};

    nimcp_gpu_tensor_t* A_dense = createTensor(A_data, A_dims);
    nimcp_gpu_tensor_t* B = createTensor(B_data, B_dims);

    nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

    // Get reference result
    nimcp_gpu_tensor_t* C_ref = nimcp_sparse_mm(
        sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);
    auto ref_result = copyToHost(C_ref);

    // Perform repeated operations and check stability
    float max_drift = 0.0f;
    for (int iter = 0; iter < STABILITY_ITERATIONS; iter++) {
        nimcp_gpu_tensor_t* C = nimcp_sparse_mm(
            sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);

        auto result = copyToHost(C);

        for (size_t i = 0; i < result.size(); i++) {
            float drift = std::fabs(result[i] - ref_result[i]);
            max_drift = std::max(max_drift, drift);
        }

        nimcp_gpu_tensor_destroy(C);
    }

    std::cout << "\n=== Numerical Stability (Repeated SpMM) ===" << std::endl;
    std::cout << "Iterations: " << STABILITY_ITERATIONS << std::endl;
    std::cout << "Max drift from reference: " << max_drift << std::endl;

    EXPECT_LT(max_drift, STRICT_TOLERANCE)
        << "Results drifted over repeated operations";

    nimcp_sparse_tensor_destroy(A_sparse);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C_ref);
}

TEST_F(SparseGPURegressionTest, NumericalStability_SmallValues) {
    RequireGPU();

    const size_t M = 64, K = 64, N = 64;
    const float sparsity = 0.8f;

    // Create matrix with small values
    auto A_data = generateMatrix(M, K, sparsity);
    for (auto& v : A_data) v *= 1e-5f;  // Scale down

    auto B_data = generateMatrix(K, N, 0.0f);
    for (auto& v : B_data) v *= 1e-5f;

    std::vector<size_t> A_dims = {M, K};
    std::vector<size_t> B_dims = {K, N};

    nimcp_gpu_tensor_t* A_dense = createTensor(A_data, A_dims);
    nimcp_gpu_tensor_t* B = createTensor(B_data, B_dims);

    nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

    nimcp_gpu_tensor_t* C = nimcp_sparse_mm(
        sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);

    auto sparse_result = copyToHost(C);

    std::vector<float> cpu_result(M * N);
    cpuMatmul(A_data.data(), M, K, B_data.data(), N, cpu_result.data());

    ErrorStats stats = computeErrorStats(cpu_result, sparse_result);

    std::cout << "\n=== Small Values Stability ===" << std::endl;
    printErrorStats("SpMM with small values", stats);

    // Results will be very small, use relative tolerance
    EXPECT_LT(stats.max_rel, 1e-3f)
        << "Large relative error with small values";

    nimcp_sparse_tensor_destroy(A_sparse);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C);
}

TEST_F(SparseGPURegressionTest, NumericalStability_LargeValues) {
    RequireGPU();

    const size_t M = 64, K = 64, N = 64;
    const float sparsity = 0.8f;

    // Create matrix with large values
    auto A_data = generateMatrix(M, K, sparsity);
    for (auto& v : A_data) v *= 1e5f;  // Scale up

    auto B_data = generateMatrix(K, N, 0.0f);
    // Keep B normal to avoid overflow

    std::vector<size_t> A_dims = {M, K};
    std::vector<size_t> B_dims = {K, N};

    nimcp_gpu_tensor_t* A_dense = createTensor(A_data, A_dims);
    nimcp_gpu_tensor_t* B = createTensor(B_data, B_dims);

    nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

    nimcp_gpu_tensor_t* C = nimcp_sparse_mm(
        sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);

    auto sparse_result = copyToHost(C);

    std::vector<float> cpu_result(M * N);
    cpuMatmul(A_data.data(), M, K, B_data.data(), N, cpu_result.data());

    ErrorStats stats = computeErrorStats(cpu_result, sparse_result);

    std::cout << "\n=== Large Values Stability ===" << std::endl;
    printErrorStats("SpMM with large values", stats);

    // For large values, use relative tolerance
    EXPECT_LT(stats.max_rel, 1e-4f)
        << "Large relative error with large values";

    nimcp_sparse_tensor_destroy(A_sparse);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C);
}

//=============================================================================
// Format-Specific Regression Tests
//=============================================================================

TEST_F(SparseGPURegressionTest, FormatRegression_CSR_Consistency) {
    RequireGPU();

    const size_t rows = 128;
    const size_t cols = 128;

    auto data = generateMatrix(rows, cols, 0.75f);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createTensor(data, dims);

    // Create 10 CSR tensors and verify they're all identical
    std::vector<nimcp_sparse_tensor_t*> csr_tensors(10);
    for (int i = 0; i < 10; i++) {
        csr_tensors[i] = nimcp_sparse_from_dense(
            sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
        ASSERT_NE(csr_tensors[i], nullptr);
    }

    // Compare all against first
    int ref_nnz = nimcp_sparse_nnz(csr_tensors[0]);
    nimcp_gpu_tensor_t* ref_dense = nimcp_sparse_to_dense(sparse_ctx, csr_tensors[0]);
    auto ref_data = copyToHost(ref_dense);

    for (int i = 1; i < 10; i++) {
        EXPECT_EQ(nimcp_sparse_nnz(csr_tensors[i]), ref_nnz)
            << "CSR[" << i << "] has different nnz";

        nimcp_gpu_tensor_t* test_dense = nimcp_sparse_to_dense(sparse_ctx, csr_tensors[i]);
        auto test_data = copyToHost(test_dense);

        ErrorStats stats = computeErrorStats(ref_data, test_data);
        EXPECT_EQ(stats.max_abs, 0.0f)
            << "CSR[" << i << "] differs from CSR[0]";

        nimcp_gpu_tensor_destroy(test_dense);
    }

    // Cleanup
    nimcp_gpu_tensor_destroy(ref_dense);
    nimcp_gpu_tensor_destroy(dense);
    for (auto csr : csr_tensors) {
        nimcp_sparse_tensor_destroy(csr);
    }
}

TEST_F(SparseGPURegressionTest, FormatRegression_COO_vs_CSR_Equivalence) {
    RequireGPU();

    const size_t rows = 100;
    const size_t cols = 100;
    const float sparsity = 0.85f;

    auto data = generateMatrix(rows, cols, sparsity);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createTensor(data, dims);

    // Create in both formats
    nimcp_sparse_tensor_t* csr = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    nimcp_sparse_tensor_t* coo = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_COO, 0.0f);

    ASSERT_NE(csr, nullptr);
    ASSERT_NE(coo, nullptr);

    // Convert both back to dense
    nimcp_gpu_tensor_t* from_csr = nimcp_sparse_to_dense(sparse_ctx, csr);
    nimcp_gpu_tensor_t* from_coo = nimcp_sparse_to_dense(sparse_ctx, coo);

    auto csr_data = copyToHost(from_csr);
    auto coo_data = copyToHost(from_coo);

    ErrorStats stats = computeErrorStats(csr_data, coo_data);

    std::cout << "\n=== CSR vs COO Equivalence ===" << std::endl;
    std::cout << "CSR nnz: " << nimcp_sparse_nnz(csr) << std::endl;
    std::cout << "COO nnz: " << nimcp_sparse_nnz(coo) << std::endl;
    printErrorStats("CSR vs COO", stats);

    EXPECT_EQ(nimcp_sparse_nnz(csr), nimcp_sparse_nnz(coo));
    EXPECT_EQ(stats.max_abs, 0.0f);

    nimcp_sparse_tensor_destroy(csr);
    nimcp_sparse_tensor_destroy(coo);
    nimcp_gpu_tensor_destroy(dense);
    nimcp_gpu_tensor_destroy(from_csr);
    nimcp_gpu_tensor_destroy(from_coo);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(SparseGPURegressionTest, BackwardCompat_APIStability) {
    RequireGPU();

    // Test that all documented API functions exist and have expected signatures

    // Context functions
    nimcp_sparse_ctx_t* ctx = nimcp_sparse_ctx_create(gpu_ctx);
    EXPECT_NE(ctx, nullptr);

    bool workspace_ok = nimcp_sparse_ctx_ensure_workspace(ctx, 1024);
    EXPECT_TRUE(workspace_ok);

    nimcp_sparse_ctx_destroy(ctx);
    ctx = sparse_ctx;  // Use fixture context for remaining tests

    // Creation functions
    std::vector<float> data = {1.0f, 0.0f, 2.0f, 0.0f};
    std::vector<size_t> dims = {2, 2};
    nimcp_gpu_tensor_t* dense = createTensor(data, dims);

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    EXPECT_NE(sparse, nullptr);

    // Accessor functions
    int rows = nimcp_sparse_rows(sparse);
    int cols = nimcp_sparse_cols(sparse);
    int nnz = nimcp_sparse_nnz(sparse);
    float sparsity = nimcp_sparse_sparsity(sparse);

    EXPECT_EQ(rows, 2);
    EXPECT_EQ(cols, 2);
    EXPECT_EQ(nnz, 2);
    EXPECT_GT(sparsity, 0.0f);

    // Utility functions
    const char* format_name = nimcp_sparse_format_name(SPARSE_FORMAT_CSR);
    EXPECT_STREQ(format_name, "CSR");

    bool valid = nimcp_sparse_validate(sparse);
    EXPECT_TRUE(valid);

    nimcp_sparsity_stats_t stats = nimcp_sparse_get_stats(sparse);
    EXPECT_EQ(stats.nnz, nnz);

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
