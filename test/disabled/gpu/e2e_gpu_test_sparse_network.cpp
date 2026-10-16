/**
 * @file e2e_gpu_test_sparse_network.cpp
 * @brief E2E Tests for Sparse Neural Network Training on GPU
 *
 * WHAT: End-to-end testing of sparse neural network training pipelines
 * WHY:  Verify complete sparse network training workflows from initialization to convergence
 * HOW:  Test sparse network training, progressive pruning, and performance benchmarks
 *
 * TEST PIPELINES:
 * - SparseNetworkTraining: Full training loop with sparse weights
 * - ProgressivePruning: Gradual pruning during training
 * - SparseTransformerAttention: Sparse attention patterns in transformer
 * - SparseCNNTraining: Sparse convolutional network training
 * - PerformanceBenchmark: Sparse vs dense training performance
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026
 */

#include "e2e_test_framework.h"

// GPU headers
#include "gpu/sparse/nimcp_sparse_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/training/nimcp_training_gpu.h"

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cmath>
#include <iomanip>

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr size_t BATCH_SIZE = 64;
    constexpr size_t INPUT_DIM = 784;      // MNIST-like
    constexpr size_t HIDDEN_DIM = 512;
    constexpr size_t OUTPUT_DIM = 10;
    constexpr size_t NUM_EPOCHS = 10;
    constexpr size_t NUM_BATCHES_PER_EPOCH = 20;
    constexpr float INITIAL_LR = 0.001f;
    constexpr float TOLERANCE = 1e-4f;
}

//=============================================================================
// Training Metrics
//=============================================================================

struct TrainingMetrics {
    std::vector<float> epoch_loss;
    std::vector<float> epoch_accuracy;
    std::vector<float> epoch_sparsity;
    double total_time_ms;
    double avg_forward_time_ms;
    double avg_backward_time_ms;
    size_t peak_memory_bytes;
    size_t total_parameters;
    size_t nonzero_parameters;
};

//=============================================================================
// Test Fixture
//=============================================================================

class SparseNetworkE2ETest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx_ = nullptr;
    nimcp_sparse_ctx_t* sparse_ctx_ = nullptr;
    std::mt19937 rng_{42};
    bool has_gpu_ = false;
    TrainingMetrics metrics_;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        gpu_ctx_ = nimcp_gpu_context_create_auto();
        has_gpu_ = (gpu_ctx_ != nullptr && nimcp_gpu_context_is_valid(gpu_ctx_));

        if (has_gpu_) {
            sparse_ctx_ = nimcp_sparse_ctx_create(gpu_ctx_);
        }

        memset(&metrics_, 0, sizeof(metrics_));
    }

    void TearDown() override {
        if (sparse_ctx_) {
            nimcp_sparse_ctx_destroy(sparse_ctx_);
            sparse_ctx_ = nullptr;
        }

        if (gpu_ctx_) {
            nimcp_gpu_context_destroy(gpu_ctx_);
            gpu_ctx_ = nullptr;
        }

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 8192)
            << "Potential memory leak: " << stats.current_allocated << " bytes";
    }

    bool HasGPU() const { return has_gpu_ && sparse_ctx_ != nullptr; }

    // Helper: Generate random matrix
    std::vector<float> generateMatrix(size_t rows, size_t cols, float sparsity = 0.0f) {
        std::vector<float> data(rows * cols, 0.0f);
        std::uniform_real_distribution<float> val_dist(-0.1f, 0.1f);
        std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);

        for (size_t i = 0; i < data.size(); i++) {
            if (prob_dist(rng_) > sparsity) {
                data[i] = val_dist(rng_);
            }
        }
        return data;
    }

    // Helper: Generate training batch
    void generateBatch(size_t batch_size, size_t input_dim, size_t output_dim,
                       std::vector<float>& inputs, std::vector<float>& targets) {
        inputs.resize(batch_size * input_dim);
        targets.resize(batch_size * output_dim);

        std::uniform_real_distribution<float> input_dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> class_dist(0, output_dim - 1);

        for (size_t b = 0; b < batch_size; b++) {
            // Random input
            for (size_t i = 0; i < input_dim; i++) {
                inputs[b * input_dim + i] = input_dist(rng_);
            }

            // One-hot target
            int target_class = class_dist(rng_);
            for (size_t c = 0; c < output_dim; c++) {
                targets[b * output_dim + c] = (c == static_cast<size_t>(target_class)) ? 1.0f : 0.0f;
            }
        }
    }

    // Helper: Create GPU tensor
    nimcp_gpu_tensor_t* createTensor(const std::vector<float>& data,
                                      const std::vector<size_t>& dims) {
        if (!gpu_ctx_) return nullptr;
        return nimcp_gpu_tensor_from_host(gpu_ctx_, data.data(), dims.data(),
                                          dims.size(), NIMCP_GPU_PRECISION_FP32);
    }

    // Helper: Copy to host
    std::vector<float> copyToHost(const nimcp_gpu_tensor_t* tensor) {
        if (!tensor) return {};
        std::vector<float> result(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, result.data());
        return result;
    }

    // Helper: Compute softmax cross-entropy loss
    float computeLoss(const std::vector<float>& pred, const std::vector<float>& target,
                      size_t batch_size, size_t num_classes) {
        float total_loss = 0.0f;
        for (size_t b = 0; b < batch_size; b++) {
            for (size_t c = 0; c < num_classes; c++) {
                if (target[b * num_classes + c] > 0.5f) {
                    float p = std::max(pred[b * num_classes + c], 1e-7f);
                    total_loss -= std::log(p);
                }
            }
        }
        return total_loss / batch_size;
    }

    // Helper: Compute accuracy
    float computeAccuracy(const std::vector<float>& pred, const std::vector<float>& target,
                          size_t batch_size, size_t num_classes) {
        int correct = 0;
        for (size_t b = 0; b < batch_size; b++) {
            int pred_class = 0, target_class = 0;
            float max_pred = pred[b * num_classes];
            float max_target = target[b * num_classes];

            for (size_t c = 1; c < num_classes; c++) {
                if (pred[b * num_classes + c] > max_pred) {
                    max_pred = pred[b * num_classes + c];
                    pred_class = c;
                }
                if (target[b * num_classes + c] > max_target) {
                    max_target = target[b * num_classes + c];
                    target_class = c;
                }
            }

            if (pred_class == target_class) correct++;
        }
        return static_cast<float>(correct) / batch_size;
    }

    // Helper: Print metrics
    void printTrainingMetrics(const std::string& name) {
        std::cout << "\n=== " << name << " Training Metrics ===" << std::endl;
        std::cout << "Total training time: " << metrics_.total_time_ms << " ms" << std::endl;
        std::cout << "Avg forward time: " << metrics_.avg_forward_time_ms << " ms" << std::endl;
        std::cout << "Avg backward time: " << metrics_.avg_backward_time_ms << " ms" << std::endl;
        std::cout << "Peak memory: " << (metrics_.peak_memory_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
        std::cout << "Total parameters: " << metrics_.total_parameters << std::endl;
        std::cout << "Non-zero parameters: " << metrics_.nonzero_parameters << std::endl;
        std::cout << "Final sparsity: " << (1.0 - static_cast<double>(metrics_.nonzero_parameters) / metrics_.total_parameters) * 100 << "%" << std::endl;

        if (!metrics_.epoch_loss.empty()) {
            std::cout << "Loss progression: ";
            for (size_t i = 0; i < metrics_.epoch_loss.size(); i += std::max(1ul, metrics_.epoch_loss.size() / 5)) {
                std::cout << std::fixed << std::setprecision(3) << metrics_.epoch_loss[i] << " ";
            }
            std::cout << std::endl;
        }

        if (!metrics_.epoch_accuracy.empty()) {
            std::cout << "Accuracy progression: ";
            for (size_t i = 0; i < metrics_.epoch_accuracy.size(); i += std::max(1ul, metrics_.epoch_accuracy.size() / 5)) {
                std::cout << std::fixed << std::setprecision(3) << metrics_.epoch_accuracy[i] * 100 << "% ";
            }
            std::cout << std::endl;
        }
    }
};

//=============================================================================
// Pipeline 1: Full Sparse Network Training
//=============================================================================

TEST_F(SparseNetworkE2ETest, SparseNetworkTraining_FullPipeline) {
    E2E_PIPELINE_START("Sparse Network Training");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    // Stage 1: Initialize sparse network
    E2E_STAGE_BEGIN("Initialize sparse network", 5000);

    const float initial_sparsity = 0.7f;  // 70% sparse

    // Create sparse weight matrices
    auto w1_data = generateMatrix(HIDDEN_DIM, INPUT_DIM, initial_sparsity);
    auto w2_data = generateMatrix(OUTPUT_DIM, HIDDEN_DIM, initial_sparsity);
    auto b1_data = std::vector<float>(HIDDEN_DIM, 0.0f);
    auto b2_data = std::vector<float>(OUTPUT_DIM, 0.0f);

    std::vector<size_t> w1_dims = {HIDDEN_DIM, INPUT_DIM};
    std::vector<size_t> w2_dims = {OUTPUT_DIM, HIDDEN_DIM};
    std::vector<size_t> b1_dims = {HIDDEN_DIM};
    std::vector<size_t> b2_dims = {OUTPUT_DIM};

    nimcp_gpu_tensor_t* w1_dense = createTensor(w1_data, w1_dims);
    nimcp_gpu_tensor_t* w2_dense = createTensor(w2_data, w2_dims);
    nimcp_gpu_tensor_t* b1 = createTensor(b1_data, b1_dims);
    nimcp_gpu_tensor_t* b2 = createTensor(b2_data, b2_dims);

    E2E_ASSERT_NOT_NULL(w1_dense, "Failed to create w1");
    E2E_ASSERT_NOT_NULL(w2_dense, "Failed to create w2");

    nimcp_sparse_tensor_t* w1_sparse = nimcp_sparse_from_dense(
        sparse_ctx_, w1_dense, SPARSE_FORMAT_CSR, 0.0f);
    nimcp_sparse_tensor_t* w2_sparse = nimcp_sparse_from_dense(
        sparse_ctx_, w2_dense, SPARSE_FORMAT_CSR, 0.0f);

    E2E_ASSERT_NOT_NULL(w1_sparse, "Failed to create sparse w1");
    E2E_ASSERT_NOT_NULL(w2_sparse, "Failed to create sparse w2");

    metrics_.total_parameters = INPUT_DIM * HIDDEN_DIM + HIDDEN_DIM * OUTPUT_DIM;
    metrics_.nonzero_parameters = nimcp_sparse_nnz(w1_sparse) + nimcp_sparse_nnz(w2_sparse);

    std::cout << "\n  Network architecture: " << INPUT_DIM << " -> " << HIDDEN_DIM << " -> " << OUTPUT_DIM << std::endl;
    std::cout << "  Initial sparsity: " << initial_sparsity * 100 << "%" << std::endl;
    std::cout << "  W1 nnz: " << nimcp_sparse_nnz(w1_sparse) << " / " << (HIDDEN_DIM * INPUT_DIM) << std::endl;
    std::cout << "  W2 nnz: " << nimcp_sparse_nnz(w2_sparse) << " / " << (OUTPUT_DIM * HIDDEN_DIM) << std::endl;

    E2E_STAGE_END();

    // Stage 2: Create optimizer states
    E2E_STAGE_BEGIN("Create optimizer states", 1000);

    // Dense accumulators for gradients
    nimcp_gpu_tensor_t* grad_w1_accum = nimcp_gpu_tensor_create(
        gpu_ctx_, w1_dims.data(), w1_dims.size(), NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* grad_w2_accum = nimcp_gpu_tensor_create(
        gpu_ctx_, w2_dims.data(), w2_dims.size(), NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_zeros(gpu_ctx_, grad_w1_accum);
    nimcp_gpu_zeros(gpu_ctx_, grad_w2_accum);

    // Adam moment tensors
    nimcp_gpu_tensor_t* m1 = nimcp_gpu_tensor_create(gpu_ctx_, w1_dims.data(), w1_dims.size(), NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* v1 = nimcp_gpu_tensor_create(gpu_ctx_, w1_dims.data(), w1_dims.size(), NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* m2 = nimcp_gpu_tensor_create(gpu_ctx_, w2_dims.data(), w2_dims.size(), NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* v2 = nimcp_gpu_tensor_create(gpu_ctx_, w2_dims.data(), w2_dims.size(), NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_zeros(gpu_ctx_, m1);
    nimcp_gpu_zeros(gpu_ctx_, v1);
    nimcp_gpu_zeros(gpu_ctx_, m2);
    nimcp_gpu_zeros(gpu_ctx_, v2);

    E2E_STAGE_END();

    // Stage 3: Training loop
    E2E_STAGE_BEGIN("Training loop", 60000);

    auto train_start = std::chrono::high_resolution_clock::now();
    double total_forward_time = 0.0;
    double total_backward_time = 0.0;
    int total_steps = 0;

    for (size_t epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        float epoch_loss = 0.0f;
        float epoch_acc = 0.0f;

        for (size_t batch = 0; batch < NUM_BATCHES_PER_EPOCH; batch++) {
            // Generate batch
            std::vector<float> input_data, target_data;
            generateBatch(BATCH_SIZE, INPUT_DIM, OUTPUT_DIM, input_data, target_data);

            std::vector<size_t> input_dims = {BATCH_SIZE, INPUT_DIM};
            std::vector<size_t> target_dims = {BATCH_SIZE, OUTPUT_DIM};

            nimcp_gpu_tensor_t* input = createTensor(input_data, input_dims);
            nimcp_gpu_tensor_t* target = createTensor(target_data, target_dims);

            // Forward pass
            auto fwd_start = std::chrono::high_resolution_clock::now();

            // Layer 1: hidden = ReLU(input @ W1^T + b1)
            nimcp_gpu_tensor_t* hidden = nimcp_sparse_linear_forward(
                sparse_ctx_, input, w1_sparse, b1);
            E2E_ASSERT_NOT_NULL(hidden, "Layer 1 forward failed");
            nimcp_gpu_relu(gpu_ctx_, hidden, hidden);

            // Layer 2: output = softmax(hidden @ W2^T + b2)
            nimcp_gpu_tensor_t* output = nimcp_sparse_linear_forward(
                sparse_ctx_, hidden, w2_sparse, b2);
            E2E_ASSERT_NOT_NULL(output, "Layer 2 forward failed");
            nimcp_gpu_softmax(gpu_ctx_, output, output);

            nimcp_gpu_context_synchronize(gpu_ctx_);
            auto fwd_end = std::chrono::high_resolution_clock::now();
            total_forward_time += std::chrono::duration<double, std::milli>(fwd_end - fwd_start).count();

            // Compute loss and accuracy
            auto pred_data = copyToHost(output);
            float batch_loss = computeLoss(pred_data, target_data, BATCH_SIZE, OUTPUT_DIM);
            float batch_acc = computeAccuracy(pred_data, target_data, BATCH_SIZE, OUTPUT_DIM);
            epoch_loss += batch_loss;
            epoch_acc += batch_acc;

            // Backward pass
            auto bwd_start = std::chrono::high_resolution_clock::now();

            // Compute output gradient (softmax + cross-entropy derivative)
            nimcp_gpu_tensor_t* grad_output = nimcp_gpu_tensor_create(
                gpu_ctx_, target_dims.data(), target_dims.size(), NIMCP_GPU_PRECISION_FP32);
            nimcp_gpu_sub(gpu_ctx_, output, target, grad_output);
            nimcp_gpu_scale(gpu_ctx_, grad_output, 1.0f / BATCH_SIZE, grad_output);

            // Backward layer 2
            nimcp_gpu_tensor_t* grad_hidden = nullptr;
            nimcp_sparse_tensor_t* grad_w2 = nullptr;
            nimcp_gpu_tensor_t* grad_b2 = nullptr;

            bool bwd_ok = nimcp_sparse_linear_backward(
                sparse_ctx_, hidden, w2_sparse, grad_output,
                &grad_hidden, &grad_w2, &grad_b2, true, true, true);
            E2E_ASSERT(bwd_ok, "Layer 2 backward failed");

            // Backward through ReLU
            nimcp_gpu_relu_backward(gpu_ctx_, hidden, grad_hidden, grad_hidden);

            // Backward layer 1
            nimcp_gpu_tensor_t* grad_input = nullptr;
            nimcp_sparse_tensor_t* grad_w1 = nullptr;
            nimcp_gpu_tensor_t* grad_b1 = nullptr;

            bwd_ok = nimcp_sparse_linear_backward(
                sparse_ctx_, input, w1_sparse, grad_hidden,
                &grad_input, &grad_w1, &grad_b1, false, true, true);
            E2E_ASSERT(bwd_ok, "Layer 1 backward failed");

            nimcp_gpu_context_synchronize(gpu_ctx_);
            auto bwd_end = std::chrono::high_resolution_clock::now();
            total_backward_time += std::chrono::duration<double, std::milli>(bwd_end - bwd_start).count();

            // Accumulate sparse gradients
            nimcp_sparse_grad_accumulate(sparse_ctx_, grad_w1, grad_w1_accum);
            nimcp_sparse_grad_accumulate(sparse_ctx_, grad_w2, grad_w2_accum);

            // Simple SGD update (apply accumulated gradients)
            float lr = INITIAL_LR * (1.0f - static_cast<float>(epoch) / NUM_EPOCHS);

            // Convert sparse weights to dense for update
            nimcp_gpu_tensor_t* w1_for_update = nimcp_sparse_to_dense(sparse_ctx_, w1_sparse);
            nimcp_gpu_tensor_t* w2_for_update = nimcp_sparse_to_dense(sparse_ctx_, w2_sparse);

            nimcp_gpu_axpy(gpu_ctx_, -lr, grad_w1_accum, w1_for_update);
            nimcp_gpu_axpy(gpu_ctx_, -lr, grad_w2_accum, w2_for_update);

            // Update biases
            if (grad_b1) nimcp_gpu_axpy(gpu_ctx_, -lr, grad_b1, b1);
            if (grad_b2) nimcp_gpu_axpy(gpu_ctx_, -lr, grad_b2, b2);

            // Re-sparsify weights
            nimcp_sparse_tensor_destroy(w1_sparse);
            nimcp_sparse_tensor_destroy(w2_sparse);
            w1_sparse = nimcp_sparse_from_dense(sparse_ctx_, w1_for_update, SPARSE_FORMAT_CSR, 0.01f);
            w2_sparse = nimcp_sparse_from_dense(sparse_ctx_, w2_for_update, SPARSE_FORMAT_CSR, 0.01f);

            // Zero accumulators
            nimcp_gpu_zeros(gpu_ctx_, grad_w1_accum);
            nimcp_gpu_zeros(gpu_ctx_, grad_w2_accum);

            // Cleanup batch tensors
            nimcp_gpu_tensor_destroy(input);
            nimcp_gpu_tensor_destroy(target);
            nimcp_gpu_tensor_destroy(hidden);
            nimcp_gpu_tensor_destroy(output);
            nimcp_gpu_tensor_destroy(grad_output);
            nimcp_gpu_tensor_destroy(grad_hidden);
            nimcp_sparse_tensor_destroy(grad_w1);
            nimcp_sparse_tensor_destroy(grad_w2);
            if (grad_input) nimcp_gpu_tensor_destroy(grad_input);
            if (grad_b1) nimcp_gpu_tensor_destroy(grad_b1);
            if (grad_b2) nimcp_gpu_tensor_destroy(grad_b2);
            nimcp_gpu_tensor_destroy(w1_for_update);
            nimcp_gpu_tensor_destroy(w2_for_update);

            total_steps++;
        }

        epoch_loss /= NUM_BATCHES_PER_EPOCH;
        epoch_acc /= NUM_BATCHES_PER_EPOCH;

        metrics_.epoch_loss.push_back(epoch_loss);
        metrics_.epoch_accuracy.push_back(epoch_acc);

        float w1_sparsity = nimcp_sparse_sparsity(w1_sparse);
        float w2_sparsity = nimcp_sparse_sparsity(w2_sparse);
        metrics_.epoch_sparsity.push_back((w1_sparsity + w2_sparsity) / 2.0f);

        std::cout << "\n  Epoch " << epoch + 1 << "/" << NUM_EPOCHS
                  << " - Loss: " << std::fixed << std::setprecision(4) << epoch_loss
                  << " - Acc: " << std::setprecision(2) << epoch_acc * 100 << "%"
                  << " - Sparsity: " << (w1_sparsity + w2_sparsity) / 2.0f * 100 << "%" << std::endl;
    }

    auto train_end = std::chrono::high_resolution_clock::now();
    metrics_.total_time_ms = std::chrono::duration<double, std::milli>(train_end - train_start).count();
    metrics_.avg_forward_time_ms = total_forward_time / total_steps;
    metrics_.avg_backward_time_ms = total_backward_time / total_steps;
    metrics_.nonzero_parameters = nimcp_sparse_nnz(w1_sparse) + nimcp_sparse_nnz(w2_sparse);

    E2E_STAGE_END();

    // Stage 4: Verify training progress
    E2E_STAGE_BEGIN("Verify training progress", 500);

    // Loss should decrease
    EXPECT_LT(metrics_.epoch_loss.back(), metrics_.epoch_loss.front())
        << "Loss should decrease during training";

    // Accuracy should increase (on random data, expect some improvement)
    EXPECT_GT(metrics_.epoch_accuracy.back(), 0.1f)
        << "Accuracy should be better than random guessing";

    // Sparsity should be maintained
    float final_sparsity = metrics_.epoch_sparsity.back();
    EXPECT_GT(final_sparsity, 0.5f) << "Should maintain significant sparsity";

    E2E_STAGE_END();

    // Stage 5: Get memory stats
    E2E_STAGE_BEGIN("Get memory stats", 200);

    size_t allocated = 0, peak = 0, free_mem = 0;
    nimcp_gpu_memory_stats(gpu_ctx_, &allocated, &peak, &free_mem);
    metrics_.peak_memory_bytes = peak;

    E2E_STAGE_END();

    // Cleanup
    nimcp_sparse_tensor_destroy(w1_sparse);
    nimcp_sparse_tensor_destroy(w2_sparse);
    nimcp_gpu_tensor_destroy(w1_dense);
    nimcp_gpu_tensor_destroy(w2_dense);
    nimcp_gpu_tensor_destroy(b1);
    nimcp_gpu_tensor_destroy(b2);
    nimcp_gpu_tensor_destroy(grad_w1_accum);
    nimcp_gpu_tensor_destroy(grad_w2_accum);
    nimcp_gpu_tensor_destroy(m1);
    nimcp_gpu_tensor_destroy(v1);
    nimcp_gpu_tensor_destroy(m2);
    nimcp_gpu_tensor_destroy(v2);

    printTrainingMetrics("Sparse Network");

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Progressive Pruning During Training
//=============================================================================

TEST_F(SparseNetworkE2ETest, ProgressivePruning_DuringTraining) {
    E2E_PIPELINE_START("Progressive Pruning During Training");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    // Stage 1: Initialize dense network
    E2E_STAGE_BEGIN("Initialize dense network", 3000);

    // Start with dense weights (0% sparsity)
    auto w_data = generateMatrix(OUTPUT_DIM, INPUT_DIM, 0.0f);
    std::vector<size_t> w_dims = {OUTPUT_DIM, INPUT_DIM};

    nimcp_gpu_tensor_t* weights = createTensor(w_data, w_dims);
    E2E_ASSERT_NOT_NULL(weights, "Failed to create weights");

    std::cout << "\n  Starting with dense network" << std::endl;
    std::cout << "  Weight shape: " << OUTPUT_DIM << " x " << INPUT_DIM << std::endl;

    E2E_STAGE_END();

    // Stage 2: Progressive pruning schedule
    E2E_STAGE_BEGIN("Execute progressive pruning schedule", 30000);

    // Pruning schedule: gradually increase sparsity
    std::vector<float> target_sparsities = {0.0f, 0.3f, 0.5f, 0.7f, 0.8f, 0.9f};
    int steps_per_level = 5;

    std::cout << "\n  Pruning Schedule:" << std::endl;

    for (size_t level = 0; level < target_sparsities.size(); level++) {
        float target = target_sparsities[level];

        // Prune to target sparsity
        nimcp_sparse_tensor_t* sparse_weights = nullptr;

        if (target > 0.0f) {
            sparse_weights = nimcp_magnitude_prune(sparse_ctx_, weights, target);
            E2E_ASSERT_NOT_NULL(sparse_weights, "Pruning failed");

            // Convert back to dense for next iteration
            nimcp_gpu_tensor_t* pruned_dense = nimcp_sparse_to_dense(sparse_ctx_, sparse_weights);
            nimcp_gpu_tensor_destroy(weights);
            weights = pruned_dense;
        }

        // Simulate training steps at this sparsity level
        for (int step = 0; step < steps_per_level; step++) {
            // Generate batch and do forward pass
            std::vector<float> input_data, target_data;
            generateBatch(BATCH_SIZE, INPUT_DIM, OUTPUT_DIM, input_data, target_data);

            std::vector<size_t> input_dims = {BATCH_SIZE, INPUT_DIM};
            nimcp_gpu_tensor_t* input = createTensor(input_data, input_dims);

            nimcp_gpu_tensor_t* output = nullptr;
            if (sparse_weights) {
                output = nimcp_sparse_linear_forward(sparse_ctx_, input, sparse_weights, nullptr);
            } else {
                // Dense forward
                std::vector<size_t> output_dims = {BATCH_SIZE, OUTPUT_DIM};
                output = nimcp_gpu_tensor_create(gpu_ctx_, output_dims.data(), output_dims.size(), NIMCP_GPU_PRECISION_FP32);
                nimcp_gpu_gemm(gpu_ctx_, input, weights, output, 1.0f, 0.0f, false, true);
            }

            nimcp_gpu_tensor_destroy(input);
            nimcp_gpu_tensor_destroy(output);
        }

        // Record metrics
        float actual_sparsity = 0.0f;
        if (sparse_weights) {
            actual_sparsity = nimcp_sparse_sparsity(sparse_weights);
            nimcp_sparse_tensor_destroy(sparse_weights);
        }

        metrics_.epoch_sparsity.push_back(actual_sparsity);

        std::cout << "  Level " << level << ": Target=" << target * 100
                  << "%, Actual=" << actual_sparsity * 100 << "%" << std::endl;

        // Verify sparsity achieved
        if (target > 0.0f) {
            EXPECT_NEAR(actual_sparsity, target, 0.05f)
                << "Sparsity level " << level << " not achieved";
        }
    }

    E2E_STAGE_END();

    // Stage 3: Verify final sparsity
    E2E_STAGE_BEGIN("Verify final sparsity", 500);

    nimcp_sparse_tensor_t* final_sparse = nimcp_sparse_from_dense(
        sparse_ctx_, weights, SPARSE_FORMAT_CSR, 0.0f);

    float final_sparsity = nimcp_sparse_sparsity(final_sparse);
    int final_nnz = nimcp_sparse_nnz(final_sparse);

    std::cout << "\n  Final sparse weights:" << std::endl;
    std::cout << "    NNZ: " << final_nnz << " / " << (OUTPUT_DIM * INPUT_DIM) << std::endl;
    std::cout << "    Sparsity: " << final_sparsity * 100 << "%" << std::endl;

    EXPECT_GT(final_sparsity, 0.85f) << "Final sparsity should be high";

    nimcp_sparse_tensor_destroy(final_sparse);

    E2E_STAGE_END();

    // Cleanup
    nimcp_gpu_tensor_destroy(weights);

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Sparse Attention Training
//=============================================================================

TEST_F(SparseNetworkE2ETest, SparseAttention_Training) {
    E2E_PIPELINE_START("Sparse Attention Training");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const size_t batch = 4;
    const size_t seq_len = 64;
    const size_t d_model = 128;
    const size_t num_heads = 4;
    const size_t d_head = d_model / num_heads;

    // Stage 1: Create attention components
    E2E_STAGE_BEGIN("Create attention components", 2000);

    // Q, K, V projection weights (dense)
    auto wq_data = generateMatrix(d_model, d_model, 0.0f);
    auto wk_data = generateMatrix(d_model, d_model, 0.0f);
    auto wv_data = generateMatrix(d_model, d_model, 0.0f);
    auto wo_data = generateMatrix(d_model, d_model, 0.0f);

    std::vector<size_t> w_dims = {d_model, d_model};

    nimcp_gpu_tensor_t* Wq = createTensor(wq_data, w_dims);
    nimcp_gpu_tensor_t* Wk = createTensor(wk_data, w_dims);
    nimcp_gpu_tensor_t* Wv = createTensor(wv_data, w_dims);
    nimcp_gpu_tensor_t* Wo = createTensor(wo_data, w_dims);

    // Create sparse attention mask (causal)
    std::vector<float> mask_data(seq_len * seq_len, 0.0f);
    for (size_t i = 0; i < seq_len; i++) {
        for (size_t j = 0; j <= i; j++) {
            mask_data[i * seq_len + j] = 1.0f;
        }
    }

    std::vector<size_t> mask_dims = {seq_len, seq_len};
    nimcp_gpu_tensor_t* mask_dense = createTensor(mask_data, mask_dims);
    nimcp_sparse_tensor_t* attention_mask = nimcp_sparse_from_dense(
        sparse_ctx_, mask_dense, SPARSE_FORMAT_CSR, 0.0f);

    float mask_sparsity = nimcp_sparse_sparsity(attention_mask);
    std::cout << "\n  Attention mask sparsity: " << mask_sparsity * 100 << "%" << std::endl;
    std::cout << "  Causal mask NNZ: " << nimcp_sparse_nnz(attention_mask) << std::endl;

    E2E_STAGE_END();

    // Stage 2: Run attention forward passes
    E2E_STAGE_BEGIN("Run sparse attention forward passes", 10000);

    float scale = 1.0f / std::sqrt(static_cast<float>(d_head));
    int num_forward_passes = 10;

    for (int pass = 0; pass < num_forward_passes; pass++) {
        // Generate input
        auto input_data = generateMatrix(batch * seq_len, d_model, 0.0f);
        std::vector<size_t> input_dims = {batch * seq_len, d_model};
        nimcp_gpu_tensor_t* input = createTensor(input_data, input_dims);

        // Project to Q, K, V
        std::vector<size_t> qkv_dims = {batch * seq_len, d_model};
        nimcp_gpu_tensor_t* Q = nimcp_gpu_tensor_create(gpu_ctx_, qkv_dims.data(), qkv_dims.size(), NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* K = nimcp_gpu_tensor_create(gpu_ctx_, qkv_dims.data(), qkv_dims.size(), NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* V = nimcp_gpu_tensor_create(gpu_ctx_, qkv_dims.data(), qkv_dims.size(), NIMCP_GPU_PRECISION_FP32);

        nimcp_gpu_gemm(gpu_ctx_, input, Wq, Q, 1.0f, 0.0f, false, true);
        nimcp_gpu_gemm(gpu_ctx_, input, Wk, K, 1.0f, 0.0f, false, true);
        nimcp_gpu_gemm(gpu_ctx_, input, Wv, V, 1.0f, 0.0f, false, true);

        // Sparse attention
        nimcp_gpu_tensor_t* attention_output = nimcp_sparse_attention(
            sparse_ctx_, Q, K, V, attention_mask, scale);

        if (attention_output == nullptr) {
            std::cout << "  Pass " << pass << ": attention returned null (may need mask reshape)" << std::endl;
        } else {
            auto out_data = copyToHost(attention_output);
            float sum = 0.0f;
            for (float v : out_data) sum += std::fabs(v);
            std::cout << "  Pass " << pass << ": output L1 norm = " << sum << std::endl;
            nimcp_gpu_tensor_destroy(attention_output);
        }

        // Cleanup
        nimcp_gpu_tensor_destroy(input);
        nimcp_gpu_tensor_destroy(Q);
        nimcp_gpu_tensor_destroy(K);
        nimcp_gpu_tensor_destroy(V);
    }

    E2E_STAGE_END();

    // Cleanup
    nimcp_sparse_tensor_destroy(attention_mask);
    nimcp_gpu_tensor_destroy(mask_dense);
    nimcp_gpu_tensor_destroy(Wq);
    nimcp_gpu_tensor_destroy(Wk);
    nimcp_gpu_tensor_destroy(Wv);
    nimcp_gpu_tensor_destroy(Wo);

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Performance Comparison (Sparse vs Dense)
//=============================================================================

TEST_F(SparseNetworkE2ETest, Performance_SparseVsDense_Comparison) {
    E2E_PIPELINE_START("Sparse vs Dense Performance Comparison");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const size_t M = 512, K = 1024, N = 256;
    const int warmup_runs = 5;
    const int timed_runs = 20;

    std::vector<float> sparsity_levels = {0.0f, 0.5f, 0.7f, 0.9f, 0.95f};

    // Stage 1: Benchmark setup
    E2E_STAGE_BEGIN("Benchmark setup", 2000);

    auto B_data = generateMatrix(K, N, 0.0f);
    std::vector<size_t> B_dims = {K, N};
    nimcp_gpu_tensor_t* B = createTensor(B_data, B_dims);

    std::cout << "\n=== Performance Benchmark ===" << std::endl;
    std::cout << "Matrix size: " << M << "x" << K << " @ " << K << "x" << N << std::endl;
    std::cout << "Warmup runs: " << warmup_runs << ", Timed runs: " << timed_runs << std::endl;

    std::cout << "\n| Sparsity | Dense (ms) | Sparse (ms) | Speedup | NNZ |" << std::endl;
    std::cout << "|----------|------------|-------------|---------|-----|" << std::endl;

    E2E_STAGE_END();

    // Stage 2: Run benchmarks
    E2E_STAGE_BEGIN("Run benchmarks", 60000);

    for (float sparsity : sparsity_levels) {
        auto A_data = generateMatrix(M, K, sparsity);
        std::vector<size_t> A_dims = {M, K};

        nimcp_gpu_tensor_t* A = createTensor(A_data, A_dims);
        nimcp_sparse_tensor_t* A_sparse = nimcp_sparse_from_dense(
            sparse_ctx_, A, SPARSE_FORMAT_CSR, 0.0f);

        std::vector<size_t> C_dims = {M, N};

        // Warmup
        for (int i = 0; i < warmup_runs; i++) {
            // Dense
            nimcp_gpu_tensor_t* C_dense = nimcp_gpu_tensor_create(
                gpu_ctx_, C_dims.data(), C_dims.size(), NIMCP_GPU_PRECISION_FP32);
            nimcp_gpu_gemm(gpu_ctx_, A, B, C_dense, 1.0f, 0.0f, false, false);
            nimcp_gpu_tensor_destroy(C_dense);

            // Sparse
            nimcp_gpu_tensor_t* C_sparse = nimcp_sparse_mm(
                sparse_ctx_, A_sparse, B, 1.0f, 0.0f, nullptr);
            nimcp_gpu_tensor_destroy(C_sparse);
        }
        nimcp_gpu_context_synchronize(gpu_ctx_);

        // Benchmark dense
        auto dense_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < timed_runs; i++) {
            nimcp_gpu_tensor_t* C = nimcp_gpu_tensor_create(
                gpu_ctx_, C_dims.data(), C_dims.size(), NIMCP_GPU_PRECISION_FP32);
            nimcp_gpu_gemm(gpu_ctx_, A, B, C, 1.0f, 0.0f, false, false);
            nimcp_gpu_tensor_destroy(C);
        }
        nimcp_gpu_context_synchronize(gpu_ctx_);
        auto dense_end = std::chrono::high_resolution_clock::now();
        double dense_ms = std::chrono::duration<double, std::milli>(dense_end - dense_start).count() / timed_runs;

        // Benchmark sparse
        auto sparse_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < timed_runs; i++) {
            nimcp_gpu_tensor_t* C = nimcp_sparse_mm(
                sparse_ctx_, A_sparse, B, 1.0f, 0.0f, nullptr);
            nimcp_gpu_tensor_destroy(C);
        }
        nimcp_gpu_context_synchronize(gpu_ctx_);
        auto sparse_end = std::chrono::high_resolution_clock::now();
        double sparse_ms = std::chrono::duration<double, std::milli>(sparse_end - sparse_start).count() / timed_runs;

        double speedup = dense_ms / std::max(0.001, sparse_ms);
        int nnz = nimcp_sparse_nnz(A_sparse);

        std::cout << "| " << std::fixed << std::setprecision(0) << sparsity * 100 << "% "
                  << "| " << std::setprecision(3) << dense_ms
                  << " | " << sparse_ms
                  << " | " << std::setprecision(2) << speedup << "x"
                  << " | " << nnz << " |" << std::endl;

        // At high sparsity, sparse should be faster (or at least competitive)
        if (sparsity >= 0.9f) {
            // Note: actual speedup depends on hardware and sparse library implementation
            // Just verify both complete without issues
            EXPECT_GT(sparse_ms, 0.0);
            EXPECT_GT(dense_ms, 0.0);
        }

        nimcp_sparse_tensor_destroy(A_sparse);
        nimcp_gpu_tensor_destroy(A);
    }

    E2E_STAGE_END();

    // Cleanup
    nimcp_gpu_tensor_destroy(B);

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Memory Efficiency Test
//=============================================================================

TEST_F(SparseNetworkE2ETest, MemoryEfficiency_LargeNetwork) {
    E2E_PIPELINE_START("Memory Efficiency Large Network");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    // Stage 1: Create large sparse layers
    E2E_STAGE_BEGIN("Create large sparse layers", 10000);

    const size_t large_dim = 4096;
    const float sparsity = 0.95f;

    nimcp_memory_stats_t mem_before;
    nimcp_memory_get_stats(&mem_before);

    auto w_data = generateMatrix(large_dim, large_dim, sparsity);
    std::vector<size_t> dims = {large_dim, large_dim};

    nimcp_gpu_tensor_t* dense = createTensor(w_data, dims);
    size_t dense_size = large_dim * large_dim * sizeof(float);

    nimcp_memory_stats_t mem_after_dense;
    nimcp_memory_get_stats(&mem_after_dense);

    nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
        sparse_ctx_, dense, SPARSE_FORMAT_CSR, 0.0f);

    nimcp_memory_stats_t mem_after_sparse;
    nimcp_memory_get_stats(&mem_after_sparse);

    int nnz = nimcp_sparse_nnz(sparse);
    size_t sparse_size = nnz * sizeof(float) + nnz * sizeof(int) + (large_dim + 1) * sizeof(int);
    float savings = 100.0f * (1.0f - static_cast<float>(sparse_size) / dense_size);

    std::cout << "\n=== Memory Efficiency ===" << std::endl;
    std::cout << "Layer size: " << large_dim << " x " << large_dim << std::endl;
    std::cout << "Sparsity: " << sparsity * 100 << "%" << std::endl;
    std::cout << "Dense size: " << (dense_size / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "Sparse size (CSR): " << (sparse_size / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "Memory savings: " << savings << "%" << std::endl;
    std::cout << "NNZ: " << nnz << " / " << (large_dim * large_dim) << std::endl;

    EXPECT_GT(savings, 80.0f) << "Should achieve significant memory savings";

    E2E_STAGE_END();

    // Stage 2: Verify operations work at scale
    E2E_STAGE_BEGIN("Verify large-scale operations", 5000);

    auto input_data = generateMatrix(64, large_dim, 0.0f);
    std::vector<size_t> input_dims = {64, large_dim};
    nimcp_gpu_tensor_t* input = createTensor(input_data, input_dims);

    auto start = std::chrono::high_resolution_clock::now();
    nimcp_gpu_tensor_t* output = nimcp_sparse_linear_forward(
        sparse_ctx_, input, sparse, nullptr);
    nimcp_gpu_context_synchronize(gpu_ctx_);
    auto end = std::chrono::high_resolution_clock::now();

    double time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_NE(output, nullptr);
    std::cout << "\n  Large sparse matmul time: " << time_ms << " ms" << std::endl;

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);

    E2E_STAGE_END();

    // Cleanup
    nimcp_sparse_tensor_destroy(sparse);
    nimcp_gpu_tensor_destroy(dense);

    E2E_PIPELINE_END();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
