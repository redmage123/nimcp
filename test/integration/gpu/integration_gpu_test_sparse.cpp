/**
 * @file integration_gpu_test_sparse.cpp
 * @brief Integration tests for GPU Sparse Tensor Operations
 *
 * WHAT: Integration tests verifying sparse tensor operations with other GPU systems
 * WHY:  Ensure sparse tensors work correctly with neural network layers and memory pools
 * HOW:  Test sparse linear layers, sparse attention, and GPU pool integration
 *
 * TEST COVERAGE:
 * - Sparse linear layer forward/backward passes
 * - Sparse attention with mask operations
 * - Sparse tensor with GPU memory pool allocation
 * - Sparse operations in batched contexts
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
#include <chrono>

// GPU headers
#include "gpu/sparse/nimcp_sparse_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/memory/nimcp_gpu_pool.h"

// Headers already have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr size_t BATCH_SIZE = 32;
    constexpr size_t INPUT_DIM = 256;
    constexpr size_t OUTPUT_DIM = 128;
    constexpr size_t HIDDEN_DIM = 512;
    constexpr size_t SEQ_LEN = 64;
    constexpr size_t NUM_HEADS = 8;
    constexpr float TOLERANCE = 1e-4f;
    constexpr float SPARSITY_TOLERANCE = 0.05f;

//=============================================================================
// Test Fixture
//=============================================================================

class SparseGPUIntegrationTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    nimcp_sparse_ctx_t* sparse_ctx = nullptr;
    nimcp_gpu_pool_t* memory_pool = nullptr;
    std::mt19937 rng{54321};
    bool gpu_available = false;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        gpu_ctx = nimcp_gpu_context_create_auto();
        gpu_available = (gpu_ctx != nullptr && nimcp_gpu_context_is_valid(gpu_ctx));

        if (gpu_available) {
            sparse_ctx = nimcp_sparse_ctx_create(gpu_ctx);

            // Create GPU memory pool
            nimcp_gpu_pool_config_t pool_config = {
                .initial_size = 64 * 1024 * 1024,  // 64 MB
                .growth_factor = 2.0f,
                .max_size = 256 * 1024 * 1024,     // 256 MB
                .alignment = 256,
                .enable_fragmentation_tracking = true
            };
            memory_pool = nimcp_gpu_pool_create(gpu_ctx, &pool_config);
        }
    }

    void TearDown() override {
        if (memory_pool) {
            nimcp_gpu_pool_destroy(memory_pool);
            memory_pool = nullptr;
        }

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
        EXPECT_LT(stats.current_allocated, 8192)
            << "Potential memory leak: " << stats.current_allocated << " bytes";
    }

    void RequireGPU() {
        if (!gpu_available || !sparse_ctx) {
            GTEST_SKIP() << "GPU or sparse context not available";
        }
    }

    // Helper: Generate random matrix
    std::vector<float> generateMatrix(size_t rows, size_t cols, float sparsity = 0.0f) {
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

    // Helper: Create GPU tensor from host data
    nimcp_gpu_tensor_t* createTensor(const std::vector<float>& data,
                                      const std::vector<size_t>& dims) {
        if (!gpu_ctx) return nullptr;
        return nimcp_gpu_tensor_from_host(gpu_ctx, data.data(), dims.data(),
                                          dims.size(), NIMCP_GPU_PRECISION_FP32);
    }

    // Helper: Create GPU tensor with pool memory
    nimcp_gpu_tensor_t* createPoolTensor(const std::vector<size_t>& dims) {
        if (!gpu_ctx || !memory_pool) return nullptr;

        size_t numel = 1;
        for (size_t d : dims) numel *= d;
        size_t size_bytes = numel * sizeof(float);

        void* ptr = nimcp_gpu_pool_alloc(memory_pool, size_bytes);
        if (!ptr) return nullptr;

        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create_with_memory(
            gpu_ctx, dims.data(), dims.size(), NIMCP_GPU_PRECISION_FP32, ptr);

        return tensor;
    }

    // Helper: Copy GPU tensor to host
    std::vector<float> copyToHost(const nimcp_gpu_tensor_t* tensor) {
        if (!tensor) return {};
        std::vector<float> result(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, result.data());
        return result;
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

    // Helper: CPU dense matrix multiply
    void cpuMatmul(const float* A, size_t A_rows, size_t A_cols,
                   const float* B, size_t B_cols,
                   float* C) {
        for (size_t i = 0; i < A_rows; i++) {
            for (size_t j = 0; j < B_cols; j++) {
                float sum = 0.0f;
                for (size_t k = 0; k < A_cols; k++) {
                    sum += A[i * A_cols + k] * B[k * B_cols + j];
                }
                C[i * B_cols + j] = sum;
            }
        }
    }

    // Helper: Softmax on CPU
    void cpuSoftmax(float* data, size_t rows, size_t cols) {
        for (size_t i = 0; i < rows; i++) {
            float max_val = data[i * cols];
            for (size_t j = 1; j < cols; j++) {
                max_val = std::max(max_val, data[i * cols + j]);
            }

            float sum = 0.0f;
            for (size_t j = 0; j < cols; j++) {
                data[i * cols + j] = std::exp(data[i * cols + j] - max_val);
                sum += data[i * cols + j];
            }

            for (size_t j = 0; j < cols; j++) {
                data[i * cols + j] /= sum;
            }
        }
    }
};

//=============================================================================
// Sparse Linear Layer Forward Tests
//=============================================================================

TEST_F(SparseGPUIntegrationTest, SparseLinear_Forward_MatchesDenseLinear) {
    RequireGPU();

    // Create sparse weight matrix (70% sparsity)
    auto weight_data = generateMatrix(OUTPUT_DIM, INPUT_DIM, 0.7f);
    auto input_data = generateMatrix(BATCH_SIZE, INPUT_DIM, 0.0f);
    auto bias_data = generateMatrix(OUTPUT_DIM, 1, 0.0f);

    std::vector<size_t> weight_dims = {OUTPUT_DIM, INPUT_DIM};
    std::vector<size_t> input_dims = {BATCH_SIZE, INPUT_DIM};
    std::vector<size_t> bias_dims = {OUTPUT_DIM};
    std::vector<size_t> output_dims = {BATCH_SIZE, OUTPUT_DIM};

    // Create tensors
    nimcp_gpu_tensor_t* weight_dense = createTensor(weight_data, weight_dims);
    nimcp_gpu_tensor_t* input = createTensor(input_data, input_dims);
    nimcp_gpu_tensor_t* bias = createTensor(bias_data, bias_dims);
    ASSERT_NE(weight_dense, nullptr);
    ASSERT_NE(input, nullptr);

    // Create sparse weight
    nimcp_sparse_tensor_t* weight_sparse = nimcp_sparse_from_dense(
        sparse_ctx, weight_dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(weight_sparse, nullptr);

    // Sparse forward pass
    nimcp_gpu_tensor_t* output_sparse = nimcp_sparse_linear_forward(
        sparse_ctx, input, weight_sparse, bias);
    ASSERT_NE(output_sparse, nullptr);

    // Dense reference forward pass: output = input @ weight^T + bias
    nimcp_gpu_tensor_t* output_dense = nimcp_gpu_tensor_create(
        gpu_ctx, output_dims.data(), output_dims.size(), NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_linear_forward(gpu_ctx, input, weight_dense, bias, output_dense);

    // Compare results
    auto sparse_result = copyToHost(output_sparse);
    auto dense_result = copyToHost(output_dense);

    float max_err = maxAbsError(sparse_result, dense_result);
    EXPECT_LT(max_err, TOLERANCE)
        << "Sparse linear forward differs from dense: " << max_err;

    // Cleanup
    nimcp_sparse_tensor_destroy(weight_sparse);
    nimcp_gpu_tensor_destroy(weight_dense);
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(bias);
    nimcp_gpu_tensor_destroy(output_sparse);
    nimcp_gpu_tensor_destroy(output_dense);
}

TEST_F(SparseGPUIntegrationTest, SparseLinear_Forward_WithoutBias) {
    RequireGPU();

    auto weight_data = generateMatrix(OUTPUT_DIM, INPUT_DIM, 0.8f);
    auto input_data = generateMatrix(BATCH_SIZE, INPUT_DIM, 0.0f);

    std::vector<size_t> weight_dims = {OUTPUT_DIM, INPUT_DIM};
    std::vector<size_t> input_dims = {BATCH_SIZE, INPUT_DIM};

    nimcp_gpu_tensor_t* weight_dense = createTensor(weight_data, weight_dims);
    nimcp_gpu_tensor_t* input = createTensor(input_data, input_dims);

    nimcp_sparse_tensor_t* weight_sparse = nimcp_sparse_from_dense(
        sparse_ctx, weight_dense, SPARSE_FORMAT_CSR, 0.0f);

    // Forward without bias
    nimcp_gpu_tensor_t* output = nimcp_sparse_linear_forward(
        sparse_ctx, input, weight_sparse, nullptr);
    ASSERT_NE(output, nullptr);

    EXPECT_EQ(output->dims[0], BATCH_SIZE);
    EXPECT_EQ(output->dims[1], OUTPUT_DIM);

    nimcp_sparse_tensor_destroy(weight_sparse);
    nimcp_gpu_tensor_destroy(weight_dense);
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
}

//=============================================================================
// Sparse Linear Layer Backward Tests
//=============================================================================

TEST_F(SparseGPUIntegrationTest, SparseLinear_Backward_ComputesGradients) {
    RequireGPU();

    auto weight_data = generateMatrix(OUTPUT_DIM, INPUT_DIM, 0.6f);
    auto input_data = generateMatrix(BATCH_SIZE, INPUT_DIM, 0.0f);
    auto grad_output_data = generateMatrix(BATCH_SIZE, OUTPUT_DIM, 0.0f);

    std::vector<size_t> weight_dims = {OUTPUT_DIM, INPUT_DIM};
    std::vector<size_t> input_dims = {BATCH_SIZE, INPUT_DIM};
    std::vector<size_t> grad_output_dims = {BATCH_SIZE, OUTPUT_DIM};

    nimcp_gpu_tensor_t* weight_dense = createTensor(weight_data, weight_dims);
    nimcp_gpu_tensor_t* input = createTensor(input_data, input_dims);
    nimcp_gpu_tensor_t* grad_output = createTensor(grad_output_data, grad_output_dims);

    nimcp_sparse_tensor_t* weight_sparse = nimcp_sparse_from_dense(
        sparse_ctx, weight_dense, SPARSE_FORMAT_CSR, 0.0f);

    // Backward pass
    nimcp_gpu_tensor_t* grad_input = nullptr;
    nimcp_sparse_tensor_t* grad_weight = nullptr;
    nimcp_gpu_tensor_t* grad_bias = nullptr;

    bool result = nimcp_sparse_linear_backward(
        sparse_ctx, input, weight_sparse, grad_output,
        &grad_input, &grad_weight, &grad_bias, true, true, false);

    EXPECT_TRUE(result);
    ASSERT_NE(grad_input, nullptr);
    ASSERT_NE(grad_weight, nullptr);

    // Verify dimensions
    EXPECT_EQ(grad_input->dims[0], BATCH_SIZE);
    EXPECT_EQ(grad_input->dims[1], INPUT_DIM);
    EXPECT_EQ(nimcp_sparse_rows(grad_weight), static_cast<int>(OUTPUT_DIM));
    EXPECT_EQ(nimcp_sparse_cols(grad_weight), static_cast<int>(INPUT_DIM));

    // Cleanup
    nimcp_sparse_tensor_destroy(weight_sparse);
    nimcp_sparse_tensor_destroy(grad_weight);
    nimcp_gpu_tensor_destroy(weight_dense);
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(grad_output);
    nimcp_gpu_tensor_destroy(grad_input);
    if (grad_bias) nimcp_gpu_tensor_destroy(grad_bias);
}

TEST_F(SparseGPUIntegrationTest, SparseLinear_Backward_PreservesSparsityPattern) {
    RequireGPU();

    const float sparsity = 0.75f;
    auto weight_data = generateMatrix(OUTPUT_DIM, INPUT_DIM, sparsity);
    auto input_data = generateMatrix(BATCH_SIZE, INPUT_DIM, 0.0f);
    auto grad_output_data = generateMatrix(BATCH_SIZE, OUTPUT_DIM, 0.0f);

    std::vector<size_t> weight_dims = {OUTPUT_DIM, INPUT_DIM};
    std::vector<size_t> input_dims = {BATCH_SIZE, INPUT_DIM};
    std::vector<size_t> grad_output_dims = {BATCH_SIZE, OUTPUT_DIM};

    nimcp_gpu_tensor_t* weight_dense = createTensor(weight_data, weight_dims);
    nimcp_gpu_tensor_t* input = createTensor(input_data, input_dims);
    nimcp_gpu_tensor_t* grad_output = createTensor(grad_output_data, grad_output_dims);

    nimcp_sparse_tensor_t* weight_sparse = nimcp_sparse_from_dense(
        sparse_ctx, weight_dense, SPARSE_FORMAT_CSR, 0.0f);

    int original_nnz = nimcp_sparse_nnz(weight_sparse);

    // Backward with sparsity-preserving gradients
    nimcp_gpu_tensor_t* grad_input = nullptr;
    nimcp_sparse_tensor_t* grad_weight = nullptr;

    bool result = nimcp_sparse_linear_backward(
        sparse_ctx, input, weight_sparse, grad_output,
        &grad_input, &grad_weight, nullptr, true, true, false);

    EXPECT_TRUE(result);

    // Gradient should have same sparsity pattern (same nnz)
    int grad_nnz = nimcp_sparse_nnz(grad_weight);
    EXPECT_EQ(grad_nnz, original_nnz);

    nimcp_sparse_tensor_destroy(weight_sparse);
    nimcp_sparse_tensor_destroy(grad_weight);
    nimcp_gpu_tensor_destroy(weight_dense);
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(grad_output);
    nimcp_gpu_tensor_destroy(grad_input);
}

//=============================================================================
// Sparse Attention Tests
//=============================================================================

TEST_F(SparseGPUIntegrationTest, SparseAttention_WithMask_ComputesCorrectly) {
    RequireGPU();

    const size_t batch = 4;
    const size_t seq = 32;
    const size_t dim = 64;

    // Create Q, K, V tensors
    auto Q_data = generateMatrix(batch * seq, dim, 0.0f);
    auto K_data = generateMatrix(batch * seq, dim, 0.0f);
    auto V_data = generateMatrix(batch * seq, dim, 0.0f);

    // Create sparse attention mask (causal mask with some random sparsity)
    std::vector<float> mask_data(seq * seq, 0.0f);
    for (size_t i = 0; i < seq; i++) {
        for (size_t j = 0; j <= i; j++) {
            mask_data[i * seq + j] = 1.0f;  // Causal mask
        }
    }

    std::vector<size_t> qkv_dims = {batch * seq, dim};
    std::vector<size_t> mask_dims = {seq, seq};

    nimcp_gpu_tensor_t* Q = createTensor(Q_data, qkv_dims);
    nimcp_gpu_tensor_t* K = createTensor(K_data, qkv_dims);
    nimcp_gpu_tensor_t* V = createTensor(V_data, qkv_dims);
    nimcp_gpu_tensor_t* mask_dense = createTensor(mask_data, mask_dims);

    ASSERT_NE(Q, nullptr);
    ASSERT_NE(K, nullptr);
    ASSERT_NE(V, nullptr);

    // Create sparse mask (50% zeros in causal mask)
    nimcp_sparse_tensor_t* mask_sparse = nimcp_sparse_from_dense(
        sparse_ctx, mask_dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(mask_sparse, nullptr);

    // Compute sparse attention
    nimcp_gpu_tensor_t* output = nimcp_sparse_attention(
        sparse_ctx, Q, K, V, mask_sparse, 1.0f / std::sqrt(static_cast<float>(dim)));
    ASSERT_NE(output, nullptr);

    // Verify output dimensions
    EXPECT_EQ(output->dims[0], batch * seq);
    EXPECT_EQ(output->dims[1], dim);

    // Verify output is not all zeros
    auto output_data = copyToHost(output);
    float sum = 0.0f;
    for (float v : output_data) sum += std::fabs(v);
    EXPECT_GT(sum, 0.0f);

    nimcp_sparse_tensor_destroy(mask_sparse);
    nimcp_gpu_tensor_destroy(Q);
    nimcp_gpu_tensor_destroy(K);
    nimcp_gpu_tensor_destroy(V);
    nimcp_gpu_tensor_destroy(mask_dense);
    nimcp_gpu_tensor_destroy(output);
}

TEST_F(SparseGPUIntegrationTest, SparseAttention_ApplyMask_MasksCorrectly) {
    RequireGPU();

    const size_t seq = 16;

    // Create attention scores
    auto scores_data = generateMatrix(seq, seq, 0.0f);

    // Create causal mask
    std::vector<float> mask_data(seq * seq, 0.0f);
    for (size_t i = 0; i < seq; i++) {
        for (size_t j = 0; j <= i; j++) {
            mask_data[i * seq + j] = 1.0f;
        }
    }

    std::vector<size_t> dims = {seq, seq};

    nimcp_gpu_tensor_t* scores = createTensor(scores_data, dims);
    nimcp_gpu_tensor_t* mask_dense = createTensor(mask_data, dims);

    nimcp_sparse_tensor_t* mask_sparse = nimcp_sparse_from_dense(
        sparse_ctx, mask_dense, SPARSE_FORMAT_CSR, 0.0f);

    // Apply mask
    nimcp_gpu_tensor_t* masked_scores = nimcp_sparse_apply_mask(
        sparse_ctx, scores, mask_sparse, -1e9f);
    ASSERT_NE(masked_scores, nullptr);

    auto result = copyToHost(masked_scores);

    // Verify mask was applied: upper triangle should be very negative
    for (size_t i = 0; i < seq; i++) {
        for (size_t j = i + 1; j < seq; j++) {
            EXPECT_LT(result[i * seq + j], -1e8f)
                << "Position (" << i << "," << j << ") should be masked";
        }
    }

    nimcp_sparse_tensor_destroy(mask_sparse);
    nimcp_gpu_tensor_destroy(scores);
    nimcp_gpu_tensor_destroy(mask_dense);
    nimcp_gpu_tensor_destroy(masked_scores);
}

//=============================================================================
// GPU Memory Pool Integration Tests
//=============================================================================

TEST_F(SparseGPUIntegrationTest, SparseWithPool_AllocatesFromPool) {
    RequireGPU();
    ASSERT_NE(memory_pool, nullptr) << "GPU pool not available";

    const size_t rows = 256;
    const size_t cols = 256;
    const float sparsity = 0.9f;

    // Get initial pool stats
    nimcp_gpu_pool_stats_t initial_stats;
    nimcp_gpu_pool_get_stats(memory_pool, &initial_stats);
    size_t initial_allocated = initial_stats.allocated_bytes;

    // Create dense tensor from pool
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* dense = createPoolTensor(dims);
    ASSERT_NE(dense, nullptr);

    // Fill with sparse data
    auto data = generateMatrix(rows, cols, sparsity);
    nimcp_gpu_memcpy(gpu_ctx, dense->data, data.data(),
                     data.size() * sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);

    // Convert to sparse
    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(sparse, nullptr);

    // Verify sparse tensor was created
    EXPECT_GT(nimcp_sparse_nnz(sparse), 0);
    EXPECT_LT(nimcp_sparse_sparsity(sparse), 1.0f);

    // Verify pool was used
    nimcp_gpu_pool_stats_t final_stats;
    nimcp_gpu_pool_get_stats(memory_pool, &final_stats);
    EXPECT_GT(final_stats.allocated_bytes, initial_allocated);

    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_pool_free(memory_pool, dense->data);
    // Note: tensor struct cleanup handled separately
}

TEST_F(SparseGPUIntegrationTest, SparseOperations_WithPoolMemory_WorkCorrectly) {
    RequireGPU();
    ASSERT_NE(memory_pool, nullptr) << "GPU pool not available";

    const size_t M = 64, K = 128, N = 32;

    // Allocate tensors from pool
    std::vector<size_t> A_dims = {M, K};
    std::vector<size_t> B_dims = {K, N};

    auto A_data = generateMatrix(M, K, 0.7f);
    auto B_data = generateMatrix(K, N, 0.0f);

    nimcp_gpu_tensor_t* A_dense = createTensor(A_data, A_dims);
    nimcp_gpu_tensor_t* B = createTensor(B_data, B_dims);

    nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
        sparse_ctx, A_dense, SPARSE_FORMAT_CSR, 0.0f);

    // SpMM operation
    nimcp_gpu_tensor_t* C = nimcp_sparse_mm(
        sparse_ctx, A_sparse, B, 1.0f, 0.0f, nullptr);
    ASSERT_NE(C, nullptr);

    EXPECT_EQ(C->dims[0], M);
    EXPECT_EQ(C->dims[1], N);

    // Verify computation
    auto C_data = copyToHost(C);
    float sum = 0.0f;
    for (float v : C_data) sum += std::fabs(v);
    EXPECT_GT(sum, 0.0f);

    nimcp_sparse_tensor_destroy(A_sparse);
    nimcp_gpu_tensor_destroy(A_dense);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C);
}

//=============================================================================
// Batched Sparse Operations Tests
//=============================================================================

TEST_F(SparseGPUIntegrationTest, BatchedSpMM_ProcessesBatchesCorrectly) {
    RequireGPU();

    const size_t batch = 8;
    const size_t M = 32, K = 64, N = 16;
    const float sparsity = 0.7f;

    // Create batch of sparse matrices
    std::vector<nimcp_sparse_tensor_t*> A_batch(batch);
    std::vector<nimcp_gpu_tensor_t*> B_batch(batch);
    std::vector<nimcp_gpu_tensor_t*> A_dense_batch(batch);

    std::vector<size_t> A_dims = {M, K};
    std::vector<size_t> B_dims = {K, N};

    for (size_t b = 0; b < batch; b++) {
        auto A_data = generateMatrix(M, K, sparsity);
        auto B_data = generateMatrix(K, N, 0.0f);

        A_dense_batch[b] = createTensor(A_data, A_dims);
        B_batch[b] = createTensor(B_data, B_dims);

        A_batch[b] = nimcp_sparse_from_dense(
            sparse_ctx, A_dense_batch[b], SPARSE_FORMAT_CSR, 0.0f);
    }

    // Batched SpMM
    std::vector<nimcp_gpu_tensor_t*> C_batch(batch);
    bool result = nimcp_sparse_mm_batched(
        sparse_ctx, A_batch.data(), B_batch.data(), C_batch.data(),
        batch, 1.0f, 0.0f);
    EXPECT_TRUE(result);

    // Verify all outputs
    for (size_t b = 0; b < batch; b++) {
        ASSERT_NE(C_batch[b], nullptr) << "Batch " << b << " output is null";
        EXPECT_EQ(C_batch[b]->dims[0], M);
        EXPECT_EQ(C_batch[b]->dims[1], N);
    }

    // Cleanup
    for (size_t b = 0; b < batch; b++) {
        nimcp_sparse_tensor_destroy(A_batch[b]);
        nimcp_gpu_tensor_destroy(A_dense_batch[b]);
        nimcp_gpu_tensor_destroy(B_batch[b]);
        nimcp_gpu_tensor_destroy(C_batch[b]);
    }
}

//=============================================================================
// Sparse Synapse Forward Tests (Neural Network Integration)
//=============================================================================

TEST_F(SparseGPUIntegrationTest, SparseSynapse_Forward_SimulatesNeuralConnections) {
    RequireGPU();

    const size_t n_pre = 1000;   // Pre-synaptic neurons
    const size_t n_post = 500;   // Post-synaptic neurons
    const float connectivity = 0.1f;  // 10% connectivity (90% sparse)

    // Create sparse connectivity matrix
    auto weight_data = generateMatrix(n_post, n_pre, 1.0f - connectivity);

    std::vector<size_t> weight_dims = {n_post, n_pre};
    std::vector<size_t> input_dims = {n_pre};

    nimcp_gpu_tensor_t* weight_dense = createTensor(weight_data, weight_dims);
    nimcp_sparse_tensor_t* weights = nimcp_sparse_from_dense(
        sparse_ctx, weight_dense, SPARSE_FORMAT_CSR, 0.0f);

    // Create pre-synaptic activity (some neurons firing)
    std::vector<float> pre_activity(n_pre, 0.0f);
    std::uniform_int_distribution<size_t> idx_dist(0, n_pre - 1);
    for (int i = 0; i < 100; i++) {  // 100 active neurons
        pre_activity[idx_dist(rng)] = 1.0f;
    }
    nimcp_gpu_tensor_t* input = createTensor(pre_activity, input_dims);

    // Synapse forward
    nimcp_gpu_tensor_t* post_activity = nimcp_sparse_synapse_forward(
        sparse_ctx, input, weights);
    ASSERT_NE(post_activity, nullptr);

    // Verify output
    EXPECT_EQ(post_activity->dims[0], n_post);

    auto output = copyToHost(post_activity);
    int active_post = 0;
    for (float v : output) {
        if (std::fabs(v) > 0.01f) active_post++;
    }
    EXPECT_GT(active_post, 0) << "Some post-synaptic neurons should be active";

    nimcp_sparse_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(weight_dense);
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(post_activity);
}

//=============================================================================
// Multi-Layer Sparse Network Tests
//=============================================================================

TEST_F(SparseGPUIntegrationTest, MultiLayerSparseNetwork_ForwardPass) {
    RequireGPU();

    // Define network architecture
    const std::vector<size_t> layer_sizes = {INPUT_DIM, HIDDEN_DIM, HIDDEN_DIM / 2, OUTPUT_DIM};
    const float sparsity = 0.7f;
    const size_t num_layers = layer_sizes.size() - 1;

    // Create sparse weights for each layer
    std::vector<nimcp_sparse_tensor_t*> weights(num_layers);
    std::vector<nimcp_gpu_tensor_t*> weight_dense_list(num_layers);

    for (size_t l = 0; l < num_layers; l++) {
        size_t in_dim = layer_sizes[l];
        size_t out_dim = layer_sizes[l + 1];

        auto w_data = generateMatrix(out_dim, in_dim, sparsity);
        std::vector<size_t> w_dims = {out_dim, in_dim};

        weight_dense_list[l] = createTensor(w_data, w_dims);
        weights[l] = nimcp_sparse_from_dense(
            sparse_ctx, weight_dense_list[l], SPARSE_FORMAT_CSR, 0.0f);
    }

    // Create input
    auto input_data = generateMatrix(BATCH_SIZE, INPUT_DIM, 0.0f);
    std::vector<size_t> input_dims = {BATCH_SIZE, INPUT_DIM};
    nimcp_gpu_tensor_t* activation = createTensor(input_data, input_dims);

    // Forward pass through all layers
    for (size_t l = 0; l < num_layers; l++) {
        nimcp_gpu_tensor_t* next_activation = nimcp_sparse_linear_forward(
            sparse_ctx, activation, weights[l], nullptr);
        ASSERT_NE(next_activation, nullptr) << "Layer " << l << " forward failed";

        // Apply ReLU (except last layer)
        if (l < num_layers - 1) {
            nimcp_gpu_relu(gpu_ctx, next_activation, next_activation);
        }

        // Swap activations
        nimcp_gpu_tensor_destroy(activation);
        activation = next_activation;
    }

    // Verify final output
    EXPECT_EQ(activation->dims[0], BATCH_SIZE);
    EXPECT_EQ(activation->dims[1], OUTPUT_DIM);

    auto output = copyToHost(activation);
    float sum = 0.0f;
    for (float v : output) sum += std::fabs(v);
    EXPECT_GT(sum, 0.0f);

    // Cleanup
    for (size_t l = 0; l < num_layers; l++) {
        nimcp_sparse_tensor_destroy(weights[l]);
        nimcp_gpu_tensor_destroy(weight_dense_list[l]);
    }
    nimcp_gpu_tensor_destroy(activation);
}

//=============================================================================
// Sparse Gradient Accumulation Integration Tests
//=============================================================================

TEST_F(SparseGPUIntegrationTest, GradientAccumulation_MultipleSteps) {
    RequireGPU();

    const size_t rows = 128;
    const size_t cols = 128;
    const int num_accumulation_steps = 4;

    // Create dense gradient accumulator
    std::vector<float> accum_data(rows * cols, 0.0f);
    std::vector<size_t> dims = {rows, cols};
    nimcp_gpu_tensor_t* grad_accumulator = createTensor(accum_data, dims);

    std::vector<float> expected_accum(rows * cols, 0.0f);

    // Accumulate sparse gradients
    for (int step = 0; step < num_accumulation_steps; step++) {
        auto sparse_grad_data = generateMatrix(rows, cols, 0.9f);

        nimcp_gpu_tensor_t* sparse_dense = createTensor(sparse_grad_data, dims);
        nimcp_sparse_tensor_t* sparse_grad = nimcp_sparse_from_dense(
            sparse_ctx, sparse_dense, SPARSE_FORMAT_CSR, 0.0f);

        // Accumulate
        bool result = nimcp_sparse_grad_accumulate(
            sparse_ctx, sparse_grad, grad_accumulator);
        EXPECT_TRUE(result);

        // Update expected
        for (size_t i = 0; i < expected_accum.size(); i++) {
            expected_accum[i] += sparse_grad_data[i];
        }

        nimcp_sparse_tensor_destroy(sparse_grad);
        nimcp_gpu_tensor_destroy(sparse_dense);
    }

    // Verify accumulation
    auto actual_accum = copyToHost(grad_accumulator);
    float max_err = maxAbsError(expected_accum, actual_accum);
    EXPECT_LT(max_err, TOLERANCE * num_accumulation_steps);

    nimcp_gpu_tensor_destroy(grad_accumulator);
}

//=============================================================================
// Performance Integration Tests
//=============================================================================

TEST_F(SparseGPUIntegrationTest, Performance_SparseVsDense_LinearForward) {
    RequireGPU();

    const size_t M = 512, K = 1024, N = 256;
    const float sparsity = 0.9f;  // 90% sparse
    const int warmup_runs = 3;
    const int timed_runs = 10;

    auto weight_data = generateMatrix(N, K, sparsity);
    auto input_data = generateMatrix(M, K, 0.0f);

    std::vector<size_t> weight_dims = {N, K};
    std::vector<size_t> input_dims = {M, K};

    nimcp_gpu_tensor_t* weight_dense = createTensor(weight_data, weight_dims);
    nimcp_gpu_tensor_t* input = createTensor(input_data, input_dims);

    nimcp_sparse_tensor_t* weight_sparse = nimcp_sparse_from_dense(
        sparse_ctx, weight_dense, SPARSE_FORMAT_CSR, 0.0f);

    // Warmup
    for (int i = 0; i < warmup_runs; i++) {
        nimcp_gpu_tensor_t* out = nimcp_sparse_linear_forward(
            sparse_ctx, input, weight_sparse, nullptr);
        nimcp_gpu_tensor_destroy(out);
    }
    nimcp_gpu_context_synchronize(gpu_ctx);

    // Time sparse
    auto sparse_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < timed_runs; i++) {
        nimcp_gpu_tensor_t* out = nimcp_sparse_linear_forward(
            sparse_ctx, input, weight_sparse, nullptr);
        nimcp_gpu_tensor_destroy(out);
    }
    nimcp_gpu_context_synchronize(gpu_ctx);
    auto sparse_end = std::chrono::high_resolution_clock::now();

    double sparse_ms = std::chrono::duration<double, std::milli>(
        sparse_end - sparse_start).count() / timed_runs;

    std::cout << "\n=== Sparse vs Dense Performance ===" << std::endl;
    std::cout << "Matrix size: " << M << "x" << K << " @ " << K << "x" << N << std::endl;
    std::cout << "Sparsity: " << (sparsity * 100) << "%" << std::endl;
    std::cout << "Sparse linear forward: " << sparse_ms << " ms" << std::endl;

    // Verify sparse has non-trivial performance
    EXPECT_GT(sparse_ms, 0.0);
    EXPECT_LT(sparse_ms, 1000.0);  // Should complete in reasonable time

    nimcp_sparse_tensor_destroy(weight_sparse);
    nimcp_gpu_tensor_destroy(weight_dense);
    nimcp_gpu_tensor_destroy(input);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
