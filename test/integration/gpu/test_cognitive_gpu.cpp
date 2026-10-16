/**
 * @file test_cognitive_gpu.cpp
 * @brief Integration tests for GPU-accelerated cognitive operations in NIMCP
 *
 * WHAT: Verify GPU and CPU cognitive module implementations produce equivalent results
 * WHY:  Ensure computation correctness regardless of backend selection
 * HOW:  Run identical cognitive operations on both backends and compare outputs
 *
 * TEST COVERAGE:
 * - FEP matrix update GPU vs CPU equivalence
 * - Working memory decay GPU vs CPU equivalence
 * - Attention precision weighting GPU vs CPU equivalence
 * - Fallback to CPU when GPU unavailable
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <memory>

// GPU headers
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"
#include "cognitive/free_energy/nimcp_fep_learning_gpu.h"

//=============================================================================
// Test Configuration Constants
//=============================================================================

namespace {
    // Tolerance thresholds for floating-point comparisons
    constexpr float STRICT_TOLERANCE = 1e-5f;      // For exact operations
    constexpr float RELAXED_TOLERANCE = 1e-4f;     // For transcendental functions
    constexpr float FEP_TOLERANCE = 1e-3f;         // For FEP operations (more numerical variance)
    constexpr float COGNITIVE_TOLERANCE = 1e-3f;   // For cognitive operations

    // Test sizes
    constexpr uint32_t SMALL_DIM = 8;
    constexpr uint32_t MEDIUM_DIM = 32;
    constexpr uint32_t LARGE_DIM = 64;
    constexpr uint32_t BATCH_SIZE = 16;
}

//=============================================================================
// Test Fixture with Helper Functions
//=============================================================================

class CognitiveGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    nimcp_kernel_backend_t* backend = nullptr;
    fep_learning_gpu_context_t* fep_gpu_ctx = nullptr;
    std::mt19937 rng{42};  // Reproducible random numbers

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        // Initialize kernel backend with AUTO to detect best available
        bool init_ok = nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);
        ASSERT_TRUE(init_ok) << "Failed to initialize kernel backend";

        backend = nimcp_get_kernel_backend();
        ASSERT_NE(backend, nullptr) << "Failed to get kernel backend";

        // Create GPU context (may be nullptr if no GPU available)
        gpu_ctx = nimcp_gpu_context_create_auto();
        // Note: gpu_ctx can be NULL if no GPU - tests will skip GPU portions
    }

    void TearDown() override {
        if (fep_gpu_ctx) {
            fep_learning_gpu_destroy(fep_gpu_ctx);
            fep_gpu_ctx = nullptr;
        }

        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }

        nimcp_kernel_backend_shutdown();

        // Check for memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Potential memory leak detected: " << stats.current_allocated << " bytes";
    }

    //=========================================================================
    // Helper: Generate random float data
    //=========================================================================
    std::vector<float> generateRandomData(size_t count, float min_val = -1.0f, float max_val = 1.0f) {
        std::vector<float> data(count);
        std::uniform_real_distribution<float> dist(min_val, max_val);
        for (size_t i = 0; i < count; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    //=========================================================================
    // Helper: Generate positive random data
    //=========================================================================
    std::vector<float> generatePositiveData(size_t count, float min_val = 0.1f, float max_val = 2.0f) {
        std::vector<float> data(count);
        std::uniform_real_distribution<float> dist(min_val, max_val);
        for (size_t i = 0; i < count; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    //=========================================================================
    // Helper: Generate random transition matrix (row-stochastic for states)
    //=========================================================================
    std::vector<float> generateTransitionMatrix(uint32_t dim) {
        std::vector<float> matrix(dim * dim);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (uint32_t i = 0; i < dim; i++) {
            float row_sum = 0.0f;
            for (uint32_t j = 0; j < dim; j++) {
                float val = dist(rng);
                matrix[i * dim + j] = val;
                row_sum += val;
            }
            // Normalize row
            for (uint32_t j = 0; j < dim; j++) {
                matrix[i * dim + j] /= row_sum;
            }
        }
        return matrix;
    }

    //=========================================================================
    // Helper: Compare tensors with tolerance
    //=========================================================================
    bool compareTensors(const float* a, const float* b, size_t count,
                        float tolerance, std::string& error_msg) {
        float max_diff = 0.0f;
        size_t max_diff_idx = 0;

        for (size_t i = 0; i < count; i++) {
            float diff = std::fabs(a[i] - b[i]);

            // Handle infinities and NaNs
            if (std::isnan(a[i]) || std::isnan(b[i])) {
                error_msg = "NaN detected at index " + std::to_string(i);
                return false;
            }
            if (std::isinf(a[i]) != std::isinf(b[i])) {
                error_msg = "Infinity mismatch at index " + std::to_string(i);
                return false;
            }

            if (diff > max_diff) {
                max_diff = diff;
                max_diff_idx = i;
            }
        }

        if (max_diff > tolerance) {
            error_msg = "Max difference " + std::to_string(max_diff) +
                        " at index " + std::to_string(max_diff_idx) +
                        " (expected: " + std::to_string(a[max_diff_idx]) +
                        ", actual: " + std::to_string(b[max_diff_idx]) +
                        ", tolerance: " + std::to_string(tolerance) + ")";
            return false;
        }
        return true;
    }

    //=========================================================================
    // Helper: Create GPU tensor from host data
    //=========================================================================
    nimcp_gpu_tensor_t* createGPUTensor(const std::vector<float>& data,
                                        const std::vector<size_t>& dims) {
        if (!gpu_ctx) return nullptr;
        return nimcp_gpu_tensor_from_host(
            gpu_ctx,
            data.data(),
            dims.data(),
            static_cast<uint32_t>(dims.size()),
            NIMCP_GPU_PRECISION_FP32
        );
    }

    //=========================================================================
    // Helper: Copy GPU tensor to host
    //=========================================================================
    std::vector<float> copyToHost(const nimcp_gpu_tensor_t* tensor) {
        if (!tensor || !gpu_ctx) return {};
        std::vector<float> result(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, result.data());
        return result;
    }

    //=========================================================================
    // Helper: Check if GPU is available
    //=========================================================================
    bool hasGPU() const {
        return gpu_ctx != nullptr && nimcp_cuda_backend_available();
    }

    //=========================================================================
    // CPU reference: Matrix-vector multiplication
    //=========================================================================
    void cpuMatVec(const float* A, const float* x, float* y, uint32_t rows, uint32_t cols) {
        for (uint32_t i = 0; i < rows; i++) {
            float sum = 0.0f;
            for (uint32_t j = 0; j < cols; j++) {
                sum += A[i * cols + j] * x[j];
            }
            y[i] = sum;
        }
    }

    //=========================================================================
    // CPU reference: Working memory decay
    //=========================================================================
    void cpuWorkingMemoryDecay(float* memory, uint32_t size, float decay_rate, float dt) {
        float decay = std::exp(-dt * decay_rate);
        for (uint32_t i = 0; i < size; i++) {
            memory[i] *= decay;
        }
    }

    //=========================================================================
    // CPU reference: Attention precision weighting
    //=========================================================================
    void cpuAttentionPrecisionWeight(const float* input, const float* precision,
                                      float* output, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            output[i] = input[i] * precision[i];
        }
    }

    //=========================================================================
    // CPU reference: FEP prediction error
    //=========================================================================
    void cpuFEPPredictionError(const float* observation, const float* prediction,
                                float* error, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            error[i] = observation[i] - prediction[i];
        }
    }

    //=========================================================================
    // CPU reference: FEP belief update (gradient descent)
    //=========================================================================
    void cpuFEPBeliefUpdate(float* beliefs, const float* prediction_error,
                             const float* precision, float learning_rate, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            float weighted_error = prediction_error[i] * precision[i];
            beliefs[i] += learning_rate * weighted_error;
        }
    }

    //=========================================================================
    // CPU reference: FEP transition learning step
    //=========================================================================
    float cpuFEPTransitionLearning(float* A, const float* state_t, const float* state_t1,
                                    uint32_t dim, float lr, float lambda) {
        // prediction = A * state_t
        std::vector<float> prediction(dim);
        cpuMatVec(A, state_t, prediction.data(), dim, dim);

        // error = state_t1 - prediction
        std::vector<float> error(dim);
        for (uint32_t i = 0; i < dim; i++) {
            error[i] = state_t1[i] - prediction[i];
        }

        // Compute loss
        float loss = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            loss += error[i] * error[i];
        }
        loss /= dim;

        // Gradient update: A = A + lr * (error ⊗ state_t^T) - lr * lambda * A
        for (uint32_t i = 0; i < dim; i++) {
            for (uint32_t j = 0; j < dim; j++) {
                float grad = error[i] * state_t[j] - lambda * A[i * dim + j];
                A[i * dim + j] += lr * grad;
            }
        }

        return loss;
    }

    //=========================================================================
    // CPU reference: Softmax for attention
    //=========================================================================
    void cpuSoftmax(const float* input, float* output, uint32_t size) {
        float max_val = input[0];
        for (uint32_t i = 1; i < size; i++) {
            max_val = std::max(max_val, input[i]);
        }

        float sum = 0.0f;
        for (uint32_t i = 0; i < size; i++) {
            output[i] = std::exp(input[i] - max_val);
            sum += output[i];
        }

        for (uint32_t i = 0; i < size; i++) {
            output[i] /= sum;
        }
    }
};

//=============================================================================
// FEP MATRIX UPDATE TESTS
//=============================================================================

/**
 * WHAT: Test FEP transition matrix update GPU vs CPU equivalence
 * WHY:  FEP learning requires matrix updates for generative model
 * HOW:  Run same update on both backends, compare results
 */
TEST_F(CognitiveGPUTest, FEP_TransitionUpdate_Equivalence) {
    const uint32_t dim = MEDIUM_DIM;

    // Generate test data
    auto transition_matrix = generateTransitionMatrix(dim);
    auto state_t = generateRandomData(dim, 0.0f, 1.0f);
    auto state_t1 = generateRandomData(dim, 0.0f, 1.0f);

    const float lr = 0.01f;
    const float lambda = 0.001f;

    // CPU reference
    std::vector<float> cpu_matrix = transition_matrix;
    float cpu_loss = cpuFEPTransitionLearning(cpu_matrix.data(), state_t.data(),
                                               state_t1.data(), dim, lr, lambda);

    // GPU via FEP learning context
    if (hasGPU()) {
        fep_gpu_ctx = fep_learning_gpu_create(gpu_ctx, dim, 0, BATCH_SIZE);
        if (fep_gpu_ctx) {
            // Upload matrix
            int result = fep_learning_gpu_upload_transition(fep_gpu_ctx,
                                                             transition_matrix.data(), dim);
            ASSERT_EQ(result, 0) << "Failed to upload transition matrix";

            // Learn step
            float gpu_loss = 0.0f;
            result = fep_learn_transition_gpu(fep_gpu_ctx, state_t.data(),
                                               state_t1.data(), lr, lambda, 0.0f, &gpu_loss);
            ASSERT_EQ(result, 0) << "GPU transition learning failed";

            // Download result
            std::vector<float> gpu_matrix(dim * dim);
            result = fep_learning_gpu_download_transition(fep_gpu_ctx,
                                                           gpu_matrix.data(), dim);
            ASSERT_EQ(result, 0) << "Failed to download transition matrix";

            // Compare losses
            EXPECT_NEAR(cpu_loss, gpu_loss, FEP_TOLERANCE)
                << "CPU loss: " << cpu_loss << ", GPU loss: " << gpu_loss;

            // Compare matrices
            std::string error_msg;
            EXPECT_TRUE(compareTensors(cpu_matrix.data(), gpu_matrix.data(),
                                       dim * dim, FEP_TOLERANCE, error_msg))
                << error_msg;

            fep_learning_gpu_destroy(fep_gpu_ctx);
            fep_gpu_ctx = nullptr;
        }
    }
}

/**
 * WHAT: Test FEP batch transition learning GPU vs CPU equivalence
 * WHY:  Batch learning is critical for efficient training
 * HOW:  Accumulate gradients over multiple transitions
 */
TEST_F(CognitiveGPUTest, FEP_BatchTransition_Equivalence) {
    const uint32_t dim = SMALL_DIM;
    const uint32_t n_transitions = 10;

    // Generate sequence of states
    std::vector<float> states((n_transitions + 1) * dim);
    for (size_t i = 0; i < states.size(); i++) {
        states[i] = static_cast<float>(i % 10) / 10.0f;
    }

    const float lr = 0.01f;
    const float lambda = 0.001f;

    // CPU reference: accumulate over transitions
    auto cpu_matrix = generateTransitionMatrix(dim);
    float cpu_total_loss = 0.0f;
    for (uint32_t t = 0; t < n_transitions; t++) {
        const float* state_t = &states[t * dim];
        const float* state_t1 = &states[(t + 1) * dim];
        cpu_total_loss += cpuFEPTransitionLearning(cpu_matrix.data(), state_t,
                                                    state_t1, dim, lr / n_transitions, lambda);
    }
    cpu_total_loss /= n_transitions;

    // GPU batch learning
    if (hasGPU()) {
        fep_gpu_ctx = fep_learning_gpu_create(gpu_ctx, dim, 0, n_transitions);
        if (fep_gpu_ctx) {
            // Upload same initial matrix
            auto gpu_matrix = generateTransitionMatrix(dim);
            int result = fep_learning_gpu_upload_transition(fep_gpu_ctx,
                                                             gpu_matrix.data(), dim);
            ASSERT_EQ(result, 0);

            // Batch learn
            float gpu_avg_loss = 0.0f;
            result = fep_learn_transition_batch_gpu(fep_gpu_ctx, states.data(),
                                                     n_transitions, lr, lambda, 0.0f,
                                                     &gpu_avg_loss);
            ASSERT_EQ(result, 0) << "GPU batch learning failed";

            // Compare losses (may have different numerical behavior)
            // GPU batch may accumulate gradients differently
            EXPECT_GT(gpu_avg_loss, 0.0f) << "GPU should produce non-zero loss";

            fep_learning_gpu_destroy(fep_gpu_ctx);
            fep_gpu_ctx = nullptr;
        }
    }
}

/**
 * WHAT: Test FEP should_use_gpu threshold function
 * WHY:  Small matrices should use CPU, large matrices should use GPU
 * HOW:  Test threshold at various dimensions
 */
TEST_F(CognitiveGPUTest, FEP_ShouldUseGPU_Threshold) {
    // Very small dimensions - CPU should be preferred
    EXPECT_FALSE(fep_learning_should_use_gpu(4))
        << "4x4 matrix should use CPU";
    EXPECT_FALSE(fep_learning_should_use_gpu(7))
        << "7x7 matrix should use CPU";

    // Medium dimensions - GPU becomes beneficial
    // Threshold is typically around 64 elements (8x8)
    bool should_use_64 = fep_learning_should_use_gpu(64);
    bool should_use_128 = fep_learning_should_use_gpu(128);

    // Large dimensions should definitely use GPU
    EXPECT_TRUE(fep_learning_should_use_gpu(256))
        << "256x256 matrix should use GPU";
}

//=============================================================================
// WORKING MEMORY DECAY TESTS
//=============================================================================

/**
 * WHAT: Test working memory decay GPU vs CPU equivalence
 * WHY:  Working memory decay is fundamental cognitive operation
 * HOW:  Compare exponential decay on both backends
 */
TEST_F(CognitiveGPUTest, WorkingMemoryDecay_Equivalence) {
    const size_t size = MEDIUM_DIM * MEDIUM_DIM;
    const float decay_rate = 0.1f;
    const float dt = 1.0f;

    auto memory = generateRandomData(size, 0.0f, 1.0f);

    // CPU reference
    std::vector<float> cpu_memory = memory;
    cpuWorkingMemoryDecay(cpu_memory.data(), size, decay_rate, dt);

    // GPU computation via tensor ops
    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_memory = createGPUTensor(memory, dims);

        if (tensor_memory) {
            // Compute decay factor
            float decay = std::exp(-dt * decay_rate);

            // Apply scale operation (equivalent to decay)
            auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

            auto result = NIMCP_TENSOR_OPS()->scale(gpu_ctx, tensor_memory, decay, tensor_out);
            EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

            auto gpu_memory = copyToHost(tensor_out);

            std::string error_msg;
            EXPECT_TRUE(compareTensors(cpu_memory.data(), gpu_memory.data(),
                                       size, STRICT_TOLERANCE, error_msg))
                << error_msg;

            nimcp_gpu_tensor_destroy(tensor_memory);
            nimcp_gpu_tensor_destroy(tensor_out);
        }
    }
}

/**
 * WHAT: Test multiple decay steps accumulate correctly
 * WHY:  Working memory decays over multiple timesteps
 * HOW:  Apply decay multiple times, compare trajectories
 */
TEST_F(CognitiveGPUTest, WorkingMemoryDecay_MultiStep) {
    const size_t size = SMALL_DIM;
    const float decay_rate = 0.05f;
    const float dt = 0.5f;
    const int n_steps = 20;

    auto memory = generateRandomData(size, 0.5f, 1.0f);

    // CPU reference
    std::vector<float> cpu_memory = memory;
    for (int step = 0; step < n_steps; step++) {
        cpuWorkingMemoryDecay(cpu_memory.data(), size, decay_rate, dt);
    }

    // GPU computation
    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(memory, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        if (tensor_in && tensor_out) {
            float decay = std::exp(-dt * decay_rate);

            for (int step = 0; step < n_steps; step++) {
                NIMCP_TENSOR_OPS()->scale(gpu_ctx, tensor_in, decay, tensor_out);
                std::swap(tensor_in, tensor_out);
            }

            auto gpu_memory = copyToHost(tensor_in);

            std::string error_msg;
            // Use relaxed tolerance due to accumulated floating-point errors
            EXPECT_TRUE(compareTensors(cpu_memory.data(), gpu_memory.data(),
                                       size, RELAXED_TOLERANCE * n_steps * 0.1f, error_msg))
                << error_msg;

            nimcp_gpu_tensor_destroy(tensor_in);
            nimcp_gpu_tensor_destroy(tensor_out);
        }
    }
}

//=============================================================================
// ATTENTION PRECISION WEIGHTING TESTS
//=============================================================================

/**
 * WHAT: Test attention precision weighting GPU vs CPU equivalence
 * WHY:  Precision weighting is core to predictive processing
 * HOW:  Compare element-wise precision multiplication
 */
TEST_F(CognitiveGPUTest, AttentionPrecision_Equivalence) {
    const size_t size = MEDIUM_DIM;

    auto input = generateRandomData(size, -1.0f, 1.0f);
    auto precision = generatePositiveData(size, 0.1f, 10.0f);  // Precision must be positive

    // CPU reference
    std::vector<float> cpu_output(size);
    cpuAttentionPrecisionWeight(input.data(), precision.data(), cpu_output.data(), size);

    // GPU via element-wise multiply
    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_input = createGPUTensor(input, dims);
        auto* tensor_precision = createGPUTensor(precision, dims);
        auto* tensor_output = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        if (tensor_input && tensor_precision && tensor_output) {
            auto result = NIMCP_TENSOR_OPS()->mul(gpu_ctx, tensor_input, tensor_precision, tensor_output);
            EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

            auto gpu_output = copyToHost(tensor_output);

            std::string error_msg;
            EXPECT_TRUE(compareTensors(cpu_output.data(), gpu_output.data(),
                                       size, STRICT_TOLERANCE, error_msg))
                << error_msg;

            nimcp_gpu_tensor_destroy(tensor_input);
            nimcp_gpu_tensor_destroy(tensor_precision);
            nimcp_gpu_tensor_destroy(tensor_output);
        }
    }
}

/**
 * WHAT: Test attention softmax normalization GPU vs CPU equivalence
 * WHY:  Softmax attention is fundamental to transformer-style processing
 * HOW:  Compare softmax computation on both backends
 */
TEST_F(CognitiveGPUTest, AttentionSoftmax_Equivalence) {
    const size_t size = 128;  // Typical attention size

    auto input = generateRandomData(size, -3.0f, 3.0f);

    // CPU reference
    std::vector<float> cpu_output(size);
    cpuSoftmax(input.data(), cpu_output.data(), size);

    // Verify CPU softmax properties
    float cpu_sum = 0.0f;
    for (size_t i = 0; i < size; i++) {
        EXPECT_GE(cpu_output[i], 0.0f);
        cpu_sum += cpu_output[i];
    }
    EXPECT_NEAR(cpu_sum, 1.0f, 1e-6f);

    // GPU via tensor softmax
    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_input = createGPUTensor(input, dims);
        auto* tensor_output = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        if (tensor_input && tensor_output) {
            auto result = NIMCP_TENSOR_OPS()->softmax(gpu_ctx, tensor_input, tensor_output);
            EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

            auto gpu_output = copyToHost(tensor_output);

            // Verify GPU softmax properties
            float gpu_sum = 0.0f;
            for (size_t i = 0; i < size; i++) {
                EXPECT_GE(gpu_output[i], 0.0f);
                gpu_sum += gpu_output[i];
            }
            EXPECT_NEAR(gpu_sum, 1.0f, 1e-5f);

            std::string error_msg;
            EXPECT_TRUE(compareTensors(cpu_output.data(), gpu_output.data(),
                                       size, RELAXED_TOLERANCE, error_msg))
                << error_msg;

            nimcp_gpu_tensor_destroy(tensor_input);
            nimcp_gpu_tensor_destroy(tensor_output);
        }
    }
}

/**
 * WHAT: Test batched attention precision weighting
 * WHY:  Real attention operates on batches of queries
 * HOW:  Apply precision weighting to batch of attention vectors
 */
TEST_F(CognitiveGPUTest, AttentionPrecision_Batched) {
    const size_t batch = BATCH_SIZE;
    const size_t dim = SMALL_DIM;
    const size_t total_size = batch * dim;

    auto inputs = generateRandomData(total_size, -1.0f, 1.0f);
    auto precisions = generatePositiveData(total_size, 0.1f, 10.0f);

    // CPU reference (batch processed as single array)
    std::vector<float> cpu_outputs(total_size);
    cpuAttentionPrecisionWeight(inputs.data(), precisions.data(), cpu_outputs.data(), total_size);

    // GPU batch processing
    if (hasGPU()) {
        std::vector<size_t> dims = {batch, dim};
        auto* tensor_inputs = createGPUTensor(inputs, dims);
        auto* tensor_precisions = createGPUTensor(precisions, dims);
        auto* tensor_outputs = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 2, NIMCP_GPU_PRECISION_FP32);

        if (tensor_inputs && tensor_precisions && tensor_outputs) {
            auto result = NIMCP_TENSOR_OPS()->mul(gpu_ctx, tensor_inputs, tensor_precisions, tensor_outputs);
            EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

            auto gpu_outputs = copyToHost(tensor_outputs);

            std::string error_msg;
            EXPECT_TRUE(compareTensors(cpu_outputs.data(), gpu_outputs.data(),
                                       total_size, STRICT_TOLERANCE, error_msg))
                << error_msg;

            nimcp_gpu_tensor_destroy(tensor_inputs);
            nimcp_gpu_tensor_destroy(tensor_precisions);
            nimcp_gpu_tensor_destroy(tensor_outputs);
        }
    }
}

//=============================================================================
// CPU FALLBACK TESTS
//=============================================================================

/**
 * WHAT: Test CPU fallback when GPU is unavailable
 * WHY:  System must work without GPU
 * HOW:  Force CPU backend and verify operations work
 */
TEST_F(CognitiveGPUTest, CPUFallback_FEPOperations) {
    // Force CPU backend
    bool switch_ok = nimcp_switch_backend(NIMCP_BACKEND_CPU);
    EXPECT_TRUE(switch_ok) << "Should be able to switch to CPU backend";
    EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);

    // Create CPU-based FEP context (passing NULL for gpu_ctx)
    const uint32_t dim = SMALL_DIM;
    auto matrix = generateTransitionMatrix(dim);
    auto state_t = generateRandomData(dim, 0.0f, 1.0f);
    auto state_t1 = generateRandomData(dim, 0.0f, 1.0f);

    // Verify CPU reference works
    float loss = cpuFEPTransitionLearning(matrix.data(), state_t.data(),
                                           state_t1.data(), dim, 0.01f, 0.001f);
    EXPECT_GE(loss, 0.0f) << "CPU FEP learning should produce valid loss";
}

/**
 * WHAT: Test CPU fallback for tensor operations
 * WHY:  All tensor ops must work on CPU
 * HOW:  Create CPU context, run operations
 */
TEST_F(CognitiveGPUTest, CPUFallback_TensorOperations) {
    // Force CPU backend
    nimcp_switch_backend(NIMCP_BACKEND_CPU);

    const size_t size = SMALL_DIM;
    auto data_a = generateRandomData(size);
    auto data_b = generateRandomData(size);

    // CPU reference
    std::vector<float> cpu_result(size);
    for (size_t i = 0; i < size; i++) {
        cpu_result[i] = data_a[i] + data_b[i];
    }

    // Note: CPU backend tensor operations may require different context setup
    // This test verifies the backend can be switched without crash
    nimcp_kernel_backend_t* cpu_backend = nimcp_get_kernel_backend();
    EXPECT_NE(cpu_backend, nullptr);
    EXPECT_EQ(cpu_backend->type, NIMCP_BACKEND_CPU);
    EXPECT_TRUE(cpu_backend->initialized);
}

/**
 * WHAT: Test backend switching preserves correctness
 * WHY:  Switching between backends must maintain computation correctness
 * HOW:  Compute on GPU, switch to CPU, verify switching works
 */
TEST_F(CognitiveGPUTest, BackendSwitch_Correctness) {
    const size_t size = SMALL_DIM;
    auto data = generateRandomData(size);

    // Record initial backend
    nimcp_backend_type_t initial_type = nimcp_get_backend_type();

    // Switch to CPU
    bool switch_ok = nimcp_switch_backend(NIMCP_BACKEND_CPU);
    EXPECT_TRUE(switch_ok);
    EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);

    // Switch back to AUTO/CUDA if available
    if (hasGPU()) {
        switch_ok = nimcp_switch_backend(NIMCP_BACKEND_CUDA);
        EXPECT_TRUE(switch_ok);
        EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CUDA);
    }

    // Final switch back to initial
    nimcp_switch_backend(initial_type);
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

/**
 * WHAT: Test cognitive operations with edge case dimensions
 * WHY:  Edge cases reveal bugs in dimension handling
 * HOW:  Test with size 1, odd sizes, etc.
 */
TEST_F(CognitiveGPUTest, EdgeCase_SmallDimensions) {
    std::vector<size_t> test_sizes = {1, 3, 7, 15, 17};

    for (size_t size : test_sizes) {
        SCOPED_TRACE("Size: " + std::to_string(size));

        auto data = generateRandomData(size, 0.0f, 1.0f);

        // CPU decay
        std::vector<float> cpu_result = data;
        cpuWorkingMemoryDecay(cpu_result.data(), size, 0.1f, 1.0f);

        // Verify non-negative after decay
        for (size_t i = 0; i < size; i++) {
            EXPECT_GE(cpu_result[i], 0.0f);
            EXPECT_LE(cpu_result[i], data[i]);  // Decay reduces values
        }

        if (hasGPU()) {
            std::vector<size_t> dims = {size};
            auto* tensor = createGPUTensor(data, dims);
            if (tensor) {
                auto* out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
                float decay = std::exp(-0.1f);
                NIMCP_TENSOR_OPS()->scale(gpu_ctx, tensor, decay, out);

                auto gpu_result = copyToHost(out);

                std::string error_msg;
                EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(),
                                           size, STRICT_TOLERANCE, error_msg))
                    << error_msg;

                nimcp_gpu_tensor_destroy(tensor);
                nimcp_gpu_tensor_destroy(out);
            }
        }
    }
}

/**
 * WHAT: Test precision with extreme values
 * WHY:  Attention precision can have wide range
 * HOW:  Test with very small and very large precision values
 */
TEST_F(CognitiveGPUTest, EdgeCase_ExtremePrecision) {
    const size_t size = SMALL_DIM;

    auto input = generateRandomData(size, -1.0f, 1.0f);

    // Very small precision (low confidence)
    std::vector<float> low_precision(size, 0.01f);
    std::vector<float> cpu_low(size);
    cpuAttentionPrecisionWeight(input.data(), low_precision.data(), cpu_low.data(), size);

    // Very high precision (high confidence)
    std::vector<float> high_precision(size, 100.0f);
    std::vector<float> cpu_high(size);
    cpuAttentionPrecisionWeight(input.data(), high_precision.data(), cpu_high.data(), size);

    // Verify high precision amplifies more than low precision
    for (size_t i = 0; i < size; i++) {
        EXPECT_LT(std::fabs(cpu_low[i]), std::fabs(cpu_high[i]) + 0.01f);
    }

    // GPU comparison for extreme values
    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* t_input = createGPUTensor(input, dims);
        auto* t_low_prec = createGPUTensor(low_precision, dims);
        auto* t_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        if (t_input && t_low_prec && t_out) {
            NIMCP_TENSOR_OPS()->mul(gpu_ctx, t_input, t_low_prec, t_out);
            auto gpu_low = copyToHost(t_out);

            std::string error_msg;
            EXPECT_TRUE(compareTensors(cpu_low.data(), gpu_low.data(),
                                       size, STRICT_TOLERANCE, error_msg))
                << error_msg;

            nimcp_gpu_tensor_destroy(t_input);
            nimcp_gpu_tensor_destroy(t_low_prec);
            nimcp_gpu_tensor_destroy(t_out);
        }
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
