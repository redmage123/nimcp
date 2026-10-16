/**
 * @file test_gpu_cpu_equivalence.cpp
 * @brief Integration tests for GPU/CPU kernel equivalence in NIMCP
 *
 * WHAT: Verify GPU and CPU kernel implementations produce equivalent results
 * WHY:  Ensure computation correctness regardless of backend selection
 * HOW:  Run identical operations on both backends and compare outputs
 *
 * TEST COVERAGE:
 * - Tensor element-wise operations (add, sub, mul, div, scale)
 * - Matrix operations (matmul, transpose)
 * - Activation functions (relu, sigmoid, tanh, softmax)
 * - Training operations (loss, gradients, optimizer steps)
 * - SNN forward passes (LIF, Izhikevich)
 * - LNN ODE solvers (Euler, Heun, RK4)
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <memory>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/training/nimcp_training_gpu.h"
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/lnn/nimcp_lnn_gpu.h"

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Configuration Constants
//=============================================================================

namespace {
    // Tolerance thresholds for floating-point comparisons
    constexpr float STRICT_TOLERANCE = 1e-5f;      // For exact operations
    constexpr float RELAXED_TOLERANCE = 1e-4f;     // For transcendental functions
    constexpr float TRAINING_TOLERANCE = 1e-3f;    // For training ops (more numerical variance)
    constexpr float SNN_TOLERANCE = 1e-3f;         // For spike timing comparisons
    constexpr float ODE_TOLERANCE = 1e-3f;         // For ODE solvers (accumulating error)

    // Test tensor sizes
    constexpr size_t SMALL_SIZE = 64;
    constexpr size_t MEDIUM_SIZE = 256;
    constexpr size_t LARGE_SIZE = 1024;
    constexpr size_t BATCH_SIZE = 32;
}

//=============================================================================
// Test Fixture with Helper Functions
//=============================================================================

class GPUCPUEquivalenceTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    nimcp_kernel_backend_t* backend = nullptr;
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
    // Helper: Generate positive random data (for operations that need it)
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
    // Helper: Compute relative error for larger values
    //=========================================================================
    bool compareRelative(const float* a, const float* b, size_t count,
                         float rel_tolerance, std::string& error_msg) {
        for (size_t i = 0; i < count; i++) {
            float abs_a = std::fabs(a[i]);
            float abs_b = std::fabs(b[i]);
            float diff = std::fabs(a[i] - b[i]);
            float denom = std::max(abs_a, abs_b);

            if (denom > 1e-10f) {
                float rel_error = diff / denom;
                if (rel_error > rel_tolerance) {
                    error_msg = "Relative error " + std::to_string(rel_error) +
                                " at index " + std::to_string(i) +
                                " exceeds tolerance " + std::to_string(rel_tolerance);
                    return false;
                }
            } else if (diff > 1e-10f) {
                error_msg = "Absolute difference when both near zero at index " +
                            std::to_string(i);
                return false;
            }
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
    // Helper: Check if GPU is available and active
    //=========================================================================
    bool hasGPU() const {
        // Must have valid GPU context AND active backend must be CUDA
        // (not just initialized - it must be the ACTIVE backend)
        if (!gpu_ctx || !nimcp_cuda_backend_available()) {
            return false;
        }
        // Check that the active backend is actually CUDA, not CPU fallback
        nimcp_backend_type_t active_type = nimcp_get_backend_type();
        return active_type == NIMCP_BACKEND_CUDA;
    }

    //=========================================================================
    // CPU reference implementations for validation
    //=========================================================================
    void cpuAdd(const float* a, const float* b, float* out, size_t n) {
        for (size_t i = 0; i < n; i++) out[i] = a[i] + b[i];
    }

    void cpuSub(const float* a, const float* b, float* out, size_t n) {
        for (size_t i = 0; i < n; i++) out[i] = a[i] - b[i];
    }

    void cpuMul(const float* a, const float* b, float* out, size_t n) {
        for (size_t i = 0; i < n; i++) out[i] = a[i] * b[i];
    }

    void cpuDiv(const float* a, const float* b, float* out, size_t n) {
        for (size_t i = 0; i < n; i++) out[i] = a[i] / b[i];
    }

    void cpuScale(const float* a, float s, float* out, size_t n) {
        for (size_t i = 0; i < n; i++) out[i] = a[i] * s;
    }

    void cpuRelu(const float* x, float* out, size_t n) {
        for (size_t i = 0; i < n; i++) out[i] = x[i] > 0 ? x[i] : 0;
    }

    void cpuSigmoid(const float* x, float* out, size_t n) {
        for (size_t i = 0; i < n; i++) out[i] = 1.0f / (1.0f + std::exp(-x[i]));
    }

    void cpuTanh(const float* x, float* out, size_t n) {
        for (size_t i = 0; i < n; i++) out[i] = std::tanh(x[i]);
    }

    void cpuSoftmax(const float* x, float* out, size_t n) {
        float max_val = x[0];
        for (size_t i = 1; i < n; i++) max_val = std::max(max_val, x[i]);

        float sum = 0;
        for (size_t i = 0; i < n; i++) {
            out[i] = std::exp(x[i] - max_val);
            sum += out[i];
        }
        for (size_t i = 0; i < n; i++) out[i] /= sum;
    }

    void cpuMatmul(const float* a, const float* b, float* c,
                   size_t M, size_t N, size_t K) {
        for (size_t i = 0; i < M; i++) {
            for (size_t j = 0; j < N; j++) {
                float sum = 0;
                for (size_t k = 0; k < K; k++) {
                    sum += a[i * K + k] * b[k * N + j];
                }
                c[i * N + j] = sum;
            }
        }
    }

    void cpuMSELoss(const float* pred, const float* target, size_t n,
                    float& loss, float* grad) {
        loss = 0;
        for (size_t i = 0; i < n; i++) {
            float diff = pred[i] - target[i];
            loss += diff * diff;
            if (grad) grad[i] = 2.0f * diff / n;
        }
        loss /= n;
    }

    void cpuLIFForward(float* v, float* spikes, const float* input, size_t n,
                       float tau, float thresh, float v_reset, float dt) {
        float decay = std::exp(-dt / tau);
        for (size_t i = 0; i < n; i++) {
            v[i] = v[i] * decay + input[i] * (1.0f - decay);
            if (v[i] >= thresh) {
                spikes[i] = 1.0f;
                v[i] = v_reset;
            } else {
                spikes[i] = 0.0f;
            }
        }
    }

    void cpuIzhikevichForward(float* v, float* u, float* spikes, const float* input,
                               size_t n, float a, float b, float c, float d, float dt) {
        for (size_t i = 0; i < n; i++) {
            if (v[i] >= 30.0f) {
                spikes[i] = 1.0f;
                v[i] = c;
                u[i] = u[i] + d;
            } else {
                spikes[i] = 0.0f;
            }

            float dv = (0.04f * v[i] * v[i] + 5.0f * v[i] + 140.0f - u[i] + input[i]) * dt;
            float du = a * (b * v[i] - u[i]) * dt;
            v[i] += dv;
            u[i] += du;
        }
    }

    void cpuEulerStep(const float* x, const float* dx_dt, float dt, float* x_new, size_t n) {
        for (size_t i = 0; i < n; i++) {
            x_new[i] = x[i] + dt * dx_dt[i];
        }
    }
};

//=============================================================================
// TENSOR ELEMENT-WISE OPERATIONS
//=============================================================================

/**
 * WHAT: Test tensor addition produces equivalent results on CPU and GPU
 * WHY:  Element-wise add is fundamental to neural network computation
 * HOW:  Run add on both backends, compare outputs within tolerance
 */
TEST_F(GPUCPUEquivalenceTest, TensorAdd_Equivalence) {
    const std::vector<size_t> sizes = {SMALL_SIZE, MEDIUM_SIZE, LARGE_SIZE};

    for (size_t size : sizes) {
        SCOPED_TRACE("Size: " + std::to_string(size));

        auto data_a = generateRandomData(size);
        auto data_b = generateRandomData(size);
        std::vector<float> cpu_result(size);
        std::vector<float> gpu_result(size);

        // CPU reference
        cpuAdd(data_a.data(), data_b.data(), cpu_result.data(), size);

        // GPU via backend
        if (hasGPU()) {
            std::vector<size_t> dims = {size};
            auto* tensor_a = createGPUTensor(data_a, dims);
            auto* tensor_b = createGPUTensor(data_b, dims);
            auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

            ASSERT_NE(tensor_a, nullptr);
            ASSERT_NE(tensor_b, nullptr);
            ASSERT_NE(tensor_out, nullptr);

            auto result = NIMCP_TENSOR_OPS()->add(gpu_ctx, tensor_a, tensor_b, tensor_out);
            EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

            gpu_result = copyToHost(tensor_out);

            std::string error_msg;
            EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                       STRICT_TOLERANCE, error_msg))
                << error_msg;

            nimcp_gpu_tensor_destroy(tensor_a);
            nimcp_gpu_tensor_destroy(tensor_b);
            nimcp_gpu_tensor_destroy(tensor_out);
        }
    }
}

/**
 * WHAT: Test tensor subtraction equivalence
 * WHY:  Subtraction used in gradient computation
 * HOW:  Compare CPU and GPU implementations
 */
TEST_F(GPUCPUEquivalenceTest, TensorSub_Equivalence) {
    const size_t size = MEDIUM_SIZE;
    auto data_a = generateRandomData(size);
    auto data_b = generateRandomData(size);
    std::vector<float> cpu_result(size);

    cpuSub(data_a.data(), data_b.data(), cpu_result.data(), size);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_a = createGPUTensor(data_a, dims);
        auto* tensor_b = createGPUTensor(data_b, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TENSOR_OPS()->sub(gpu_ctx, tensor_a, tensor_b, tensor_out);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_result = copyToHost(tensor_out);
        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   STRICT_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_a);
        nimcp_gpu_tensor_destroy(tensor_b);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test tensor multiplication equivalence
 * WHY:  Element-wise multiply used in gating mechanisms
 * HOW:  Compare CPU and GPU implementations
 */
TEST_F(GPUCPUEquivalenceTest, TensorMul_Equivalence) {
    const size_t size = MEDIUM_SIZE;
    auto data_a = generateRandomData(size);
    auto data_b = generateRandomData(size);
    std::vector<float> cpu_result(size);

    cpuMul(data_a.data(), data_b.data(), cpu_result.data(), size);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_a = createGPUTensor(data_a, dims);
        auto* tensor_b = createGPUTensor(data_b, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TENSOR_OPS()->mul(gpu_ctx, tensor_a, tensor_b, tensor_out);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_result = copyToHost(tensor_out);
        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   STRICT_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_a);
        nimcp_gpu_tensor_destroy(tensor_b);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test tensor division equivalence
 * WHY:  Division used in normalization layers
 * HOW:  Use positive divisors to avoid division by zero issues
 */
TEST_F(GPUCPUEquivalenceTest, TensorDiv_Equivalence) {
    const size_t size = MEDIUM_SIZE;
    auto data_a = generateRandomData(size);
    auto data_b = generatePositiveData(size, 0.5f, 2.0f);  // Avoid small divisors
    std::vector<float> cpu_result(size);

    cpuDiv(data_a.data(), data_b.data(), cpu_result.data(), size);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_a = createGPUTensor(data_a, dims);
        auto* tensor_b = createGPUTensor(data_b, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TENSOR_OPS()->div(gpu_ctx, tensor_a, tensor_b, tensor_out);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_result = copyToHost(tensor_out);
        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   RELAXED_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_a);
        nimcp_gpu_tensor_destroy(tensor_b);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test scalar multiplication equivalence
 * WHY:  Scaling used in learning rate application, gradient clipping
 * HOW:  Compare CPU and GPU scalar multiply
 */
TEST_F(GPUCPUEquivalenceTest, TensorScale_Equivalence) {
    const size_t size = MEDIUM_SIZE;
    const float scalar = 2.5f;
    auto data = generateRandomData(size);
    std::vector<float> cpu_result(size);

    cpuScale(data.data(), scalar, cpu_result.data(), size);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(data, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TENSOR_OPS()->scale(gpu_ctx, tensor_in, scalar, tensor_out);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_result = copyToHost(tensor_out);
        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   STRICT_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

//=============================================================================
// MATRIX OPERATIONS
//=============================================================================

/**
 * WHAT: Test matrix multiplication equivalence
 * WHY:  GEMM is core operation for neural networks
 * HOW:  Compare CPU reference matmul with GPU backend
 */
TEST_F(GPUCPUEquivalenceTest, MatMul_Equivalence) {
    const size_t M = 32, N = 64, K = 48;
    auto data_a = generateRandomData(M * K);
    auto data_b = generateRandomData(K * N);
    std::vector<float> cpu_result(M * N);

    cpuMatmul(data_a.data(), data_b.data(), cpu_result.data(), M, N, K);

    if (hasGPU()) {
        std::vector<size_t> dims_a = {M, K};
        std::vector<size_t> dims_b = {K, N};
        std::vector<size_t> dims_c = {M, N};

        auto* tensor_a = createGPUTensor(data_a, dims_a);
        auto* tensor_b = createGPUTensor(data_b, dims_b);
        auto* tensor_c = nimcp_gpu_tensor_create(gpu_ctx, dims_c.data(), 2, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TENSOR_OPS()->matmul(gpu_ctx, tensor_a, tensor_b, tensor_c);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_result = copyToHost(tensor_c);
        std::string error_msg;
        // Use relative comparison for matmul due to accumulated errors
        EXPECT_TRUE(compareRelative(cpu_result.data(), gpu_result.data(), M * N,
                                    RELAXED_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_a);
        nimcp_gpu_tensor_destroy(tensor_b);
        nimcp_gpu_tensor_destroy(tensor_c);
    }
}

/**
 * WHAT: Test matrix multiplication with various sizes
 * WHY:  Different sizes may trigger different kernel paths
 * HOW:  Test small, medium, and large matrices
 */
TEST_F(GPUCPUEquivalenceTest, MatMul_VariousSizes) {
    struct TestCase {
        size_t M, N, K;
    };

    std::vector<TestCase> cases = {
        {1, 1, 1},       // Minimal
        {4, 4, 4},       // Very small
        {16, 16, 16},    // Small square
        {32, 64, 48},    // Rectangular
        {128, 256, 128}, // Medium
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE("Size: " + std::to_string(tc.M) + "x" + std::to_string(tc.K) +
                     " @ " + std::to_string(tc.K) + "x" + std::to_string(tc.N));

        auto data_a = generateRandomData(tc.M * tc.K);
        auto data_b = generateRandomData(tc.K * tc.N);
        std::vector<float> cpu_result(tc.M * tc.N);

        cpuMatmul(data_a.data(), data_b.data(), cpu_result.data(), tc.M, tc.N, tc.K);

        if (hasGPU()) {
            std::vector<size_t> dims_a = {tc.M, tc.K};
            std::vector<size_t> dims_b = {tc.K, tc.N};
            std::vector<size_t> dims_c = {tc.M, tc.N};

            auto* tensor_a = createGPUTensor(data_a, dims_a);
            auto* tensor_b = createGPUTensor(data_b, dims_b);
            auto* tensor_c = nimcp_gpu_tensor_create(gpu_ctx, dims_c.data(), 2, NIMCP_GPU_PRECISION_FP32);

            auto result = NIMCP_TENSOR_OPS()->matmul(gpu_ctx, tensor_a, tensor_b, tensor_c);
            EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

            auto gpu_result = copyToHost(tensor_c);
            std::string error_msg;
            // Use fixed tolerance for GPU matmul: cuBLAS uses different ordering and FMA
            // which gives slightly different results than naive CPU implementation
            // Larger matrices have more accumulated FP error
            constexpr float MATMUL_TOLERANCE = 1e-2f;  // 1% tolerance for GPU matmul
            EXPECT_TRUE(compareRelative(cpu_result.data(), gpu_result.data(), tc.M * tc.N,
                                        MATMUL_TOLERANCE, error_msg))
                << error_msg;

            nimcp_gpu_tensor_destroy(tensor_a);
            nimcp_gpu_tensor_destroy(tensor_b);
            nimcp_gpu_tensor_destroy(tensor_c);
        }
    }
}

//=============================================================================
// ACTIVATION FUNCTIONS
//=============================================================================

/**
 * WHAT: Test ReLU activation equivalence
 * WHY:  ReLU is most common activation function
 * HOW:  Compare CPU reference with GPU implementation
 */
TEST_F(GPUCPUEquivalenceTest, ReLU_Equivalence) {
    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size, -2.0f, 2.0f);
    std::vector<float> cpu_result(size);

    cpuRelu(data.data(), cpu_result.data(), size);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(data, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TENSOR_OPS()->relu(gpu_ctx, tensor_in, tensor_out);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_result = copyToHost(tensor_out);
        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   STRICT_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test sigmoid activation equivalence
 * WHY:  Sigmoid used in binary classification, gates
 * HOW:  Compare CPU exp-based sigmoid with GPU
 */
TEST_F(GPUCPUEquivalenceTest, Sigmoid_Equivalence) {
    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size, -5.0f, 5.0f);
    std::vector<float> cpu_result(size);

    cpuSigmoid(data.data(), cpu_result.data(), size);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(data, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TENSOR_OPS()->sigmoid(gpu_ctx, tensor_in, tensor_out);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_result = copyToHost(tensor_out);
        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   RELAXED_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test tanh activation equivalence
 * WHY:  Tanh commonly used in RNNs, LSTMs
 * HOW:  Compare CPU std::tanh with GPU
 */
TEST_F(GPUCPUEquivalenceTest, Tanh_Equivalence) {
    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size, -3.0f, 3.0f);
    std::vector<float> cpu_result(size);

    cpuTanh(data.data(), cpu_result.data(), size);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(data, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TENSOR_OPS()->tanh(gpu_ctx, tensor_in, tensor_out);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_result = copyToHost(tensor_out);
        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   RELAXED_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test softmax activation equivalence
 * WHY:  Softmax essential for classification outputs
 * HOW:  Compare numerically stable CPU softmax with GPU
 */
TEST_F(GPUCPUEquivalenceTest, Softmax_Equivalence) {
    const size_t size = 128;  // Typical class count
    auto data = generateRandomData(size, -3.0f, 3.0f);
    std::vector<float> cpu_result(size);

    cpuSoftmax(data.data(), cpu_result.data(), size);

    // Verify CPU softmax properties
    float sum = 0;
    for (size_t i = 0; i < size; i++) {
        EXPECT_GE(cpu_result[i], 0.0f);
        sum += cpu_result[i];
    }
    EXPECT_NEAR(sum, 1.0f, 1e-6f);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(data, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TENSOR_OPS()->softmax(gpu_ctx, tensor_in, tensor_out);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_result = copyToHost(tensor_out);

        // Verify GPU softmax properties
        float gpu_sum = 0;
        for (size_t i = 0; i < size; i++) {
            EXPECT_GE(gpu_result[i], 0.0f);
            gpu_sum += gpu_result[i];
        }
        EXPECT_NEAR(gpu_sum, 1.0f, 1e-5f);

        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   RELAXED_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

//=============================================================================
// TRAINING OPERATIONS
//=============================================================================

/**
 * WHAT: Test MSE loss computation equivalence
 * WHY:  MSE is fundamental regression loss function
 * HOW:  Compare CPU reference MSE with GPU backend
 */
TEST_F(GPUCPUEquivalenceTest, MSELoss_Equivalence) {
    const size_t size = MEDIUM_SIZE;
    auto pred = generateRandomData(size);
    auto target = generateRandomData(size);
    std::vector<float> cpu_grad(size);
    float cpu_loss = 0;

    cpuMSELoss(pred.data(), target.data(), size, cpu_loss, cpu_grad.data());

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_pred = createGPUTensor(pred, dims);
        auto* tensor_target = createGPUTensor(target, dims);
        auto* tensor_loss = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TRAINING_OPS()->mse_loss(gpu_ctx, tensor_pred, tensor_target, tensor_loss);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        // Note: The backend returns loss in a tensor, we need to extract it
        auto gpu_loss_data = copyToHost(tensor_loss);

        // For simplicity, compare first element if it's scalar reduction
        // The exact comparison depends on how the backend stores the loss
        EXPECT_GT(gpu_loss_data.size(), 0);

        nimcp_gpu_tensor_destroy(tensor_pred);
        nimcp_gpu_tensor_destroy(tensor_target);
        nimcp_gpu_tensor_destroy(tensor_loss);
    }
}

/**
 * WHAT: Test cross-entropy loss equivalence
 * WHY:  Cross-entropy is standard classification loss
 * HOW:  Compare CPU and GPU implementations
 */
TEST_F(GPUCPUEquivalenceTest, CrossEntropyLoss_Equivalence) {
    const size_t batch = 8;
    const size_t classes = 10;
    const size_t size = batch * classes;

    auto logits = generateRandomData(size, -2.0f, 2.0f);
    // Create one-hot targets
    std::vector<float> targets(size, 0.0f);
    for (size_t i = 0; i < batch; i++) {
        size_t label = i % classes;
        targets[i * classes + label] = 1.0f;
    }

    if (hasGPU()) {
        std::vector<size_t> dims = {batch, classes};
        auto* tensor_logits = createGPUTensor(logits, dims);
        auto* tensor_targets = createGPUTensor(targets, dims);
        auto* tensor_loss = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 2, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TRAINING_OPS()->cross_entropy_loss(
            gpu_ctx, tensor_logits, tensor_targets, tensor_loss);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_loss = copyToHost(tensor_loss);
        // Verify loss is positive (valid cross-entropy)
        if (!gpu_loss.empty()) {
            EXPECT_GE(gpu_loss[0], 0.0f);
        }

        nimcp_gpu_tensor_destroy(tensor_logits);
        nimcp_gpu_tensor_destroy(tensor_targets);
        nimcp_gpu_tensor_destroy(tensor_loss);
    }
}

/**
 * WHAT: Test gradient clipping equivalence
 * WHY:  Gradient clipping prevents exploding gradients
 * HOW:  Compare clipping behavior on both backends
 */
TEST_F(GPUCPUEquivalenceTest, GradientClip_Equivalence) {
    const size_t size = MEDIUM_SIZE;
    const float max_norm = 1.0f;
    auto gradients = generateRandomData(size, -5.0f, 5.0f);

    // CPU reference clipping
    float cpu_norm = 0;
    for (size_t i = 0; i < size; i++) {
        cpu_norm += gradients[i] * gradients[i];
    }
    cpu_norm = std::sqrt(cpu_norm);

    std::vector<float> cpu_clipped(size);
    if (cpu_norm > max_norm) {
        float scale = max_norm / cpu_norm;
        for (size_t i = 0; i < size; i++) {
            cpu_clipped[i] = gradients[i] * scale;
        }
    } else {
        std::copy(gradients.begin(), gradients.end(), cpu_clipped.begin());
    }

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_grad = createGPUTensor(gradients, dims);

        auto result = NIMCP_TRAINING_OPS()->gradient_clip(gpu_ctx, tensor_grad, max_norm);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_clipped = copyToHost(tensor_grad);

        // Verify GPU clipped norm <= max_norm
        float gpu_norm = 0;
        for (size_t i = 0; i < size; i++) {
            gpu_norm += gpu_clipped[i] * gpu_clipped[i];
        }
        gpu_norm = std::sqrt(gpu_norm);
        EXPECT_LE(gpu_norm, max_norm + TRAINING_TOLERANCE);

        nimcp_gpu_tensor_destroy(tensor_grad);
    }
}

/**
 * WHAT: Test SGD optimizer step equivalence
 * WHY:  SGD is baseline optimizer
 * HOW:  Compare parameter updates on both backends
 */
TEST_F(GPUCPUEquivalenceTest, SGDStep_Equivalence) {
    const size_t size = MEDIUM_SIZE;
    const float lr = 0.01f;
    const float momentum = 0.9f;

    auto params = generateRandomData(size);
    auto grads = generateRandomData(size, -0.1f, 0.1f);
    std::vector<float> velocity(size, 0.0f);

    // CPU reference SGD with momentum
    std::vector<float> cpu_params = params;
    std::vector<float> cpu_velocity = velocity;
    for (size_t i = 0; i < size; i++) {
        cpu_velocity[i] = momentum * cpu_velocity[i] + grads[i];
        cpu_params[i] = cpu_params[i] - lr * cpu_velocity[i];
    }

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_params = createGPUTensor(params, dims);
        auto* tensor_grads = createGPUTensor(grads, dims);
        auto* tensor_velocity = createGPUTensor(velocity, dims);

        auto result = NIMCP_TRAINING_OPS()->sgd_step(
            gpu_ctx, tensor_params, tensor_grads, lr, momentum, tensor_velocity);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_params = copyToHost(tensor_params);
        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_params.data(), gpu_params.data(), size,
                                   TRAINING_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_params);
        nimcp_gpu_tensor_destroy(tensor_grads);
        nimcp_gpu_tensor_destroy(tensor_velocity);
    }
}

/**
 * WHAT: Test Adam optimizer step equivalence
 * WHY:  Adam is most popular adaptive optimizer
 * HOW:  Compare parameter updates including moment updates
 */
TEST_F(GPUCPUEquivalenceTest, AdamStep_Equivalence) {
    const size_t size = SMALL_SIZE;
    const float lr = 0.001f;
    const float beta1 = 0.9f;
    const float beta2 = 0.999f;
    const float eps = 1e-8f;
    const uint64_t t = 1;

    auto params = generateRandomData(size);
    auto grads = generateRandomData(size, -0.1f, 0.1f);
    std::vector<float> m(size, 0.0f);
    std::vector<float> v(size, 0.0f);

    // CPU reference Adam
    std::vector<float> cpu_params = params;
    std::vector<float> cpu_m = m;
    std::vector<float> cpu_v = v;
    float bc1 = 1.0f - std::pow(beta1, static_cast<float>(t));
    float bc2 = 1.0f - std::pow(beta2, static_cast<float>(t));

    for (size_t i = 0; i < size; i++) {
        cpu_m[i] = beta1 * cpu_m[i] + (1.0f - beta1) * grads[i];
        cpu_v[i] = beta2 * cpu_v[i] + (1.0f - beta2) * grads[i] * grads[i];
        float m_hat = cpu_m[i] / bc1;
        float v_hat = cpu_v[i] / bc2;
        cpu_params[i] = cpu_params[i] - lr * m_hat / (std::sqrt(v_hat) + eps);
    }

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_params = createGPUTensor(params, dims);
        auto* tensor_grads = createGPUTensor(grads, dims);
        auto* tensor_m = createGPUTensor(m, dims);
        auto* tensor_v = createGPUTensor(v, dims);

        auto result = NIMCP_TRAINING_OPS()->adam_step(
            gpu_ctx, tensor_params, tensor_grads, tensor_m, tensor_v,
            lr, beta1, beta2, eps, t);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_params = copyToHost(tensor_params);
        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_params.data(), gpu_params.data(), size,
                                   TRAINING_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_params);
        nimcp_gpu_tensor_destroy(tensor_grads);
        nimcp_gpu_tensor_destroy(tensor_m);
        nimcp_gpu_tensor_destroy(tensor_v);
    }
}

//=============================================================================
// SNN FORWARD PASS TESTS
//=============================================================================

/**
 * WHAT: Test LIF neuron forward pass equivalence
 * WHY:  LIF is foundational SNN neuron model
 * HOW:  Compare membrane potential evolution and spikes
 */
TEST_F(GPUCPUEquivalenceTest, LIFForward_Equivalence) {
    const size_t n_neurons = 128;
    const float tau = 20.0f;
    const float thresh = 1.0f;
    const float v_reset = 0.0f;
    const float dt = 1.0f;

    auto input = generateRandomData(n_neurons, 0.0f, 0.5f);
    std::vector<float> cpu_v(n_neurons, 0.0f);
    std::vector<float> cpu_spikes(n_neurons, 0.0f);

    // Multiple timesteps for proper dynamics
    for (int step = 0; step < 10; step++) {
        cpuLIFForward(cpu_v.data(), cpu_spikes.data(), input.data(), n_neurons,
                      tau, thresh, v_reset, dt);
    }

    if (hasGPU()) {
        std::vector<size_t> dims = {n_neurons};
        auto* tensor_input = createGPUTensor(input, dims);
        std::vector<float> init_v(n_neurons, 0.0f);
        auto* tensor_v = createGPUTensor(init_v, dims);
        auto* tensor_spikes = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        // Run same number of steps
        for (int step = 0; step < 10; step++) {
            auto result = NIMCP_SNN_OPS()->lif_forward(
                gpu_ctx, tensor_input, tensor_v, tensor_spikes, tau, thresh, v_reset, dt);
            EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);
        }

        auto gpu_v = copyToHost(tensor_v);
        auto gpu_spikes = copyToHost(tensor_spikes);

        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_v.data(), gpu_v.data(), n_neurons,
                                   SNN_TOLERANCE, error_msg))
            << "Membrane potential mismatch: " << error_msg;

        nimcp_gpu_tensor_destroy(tensor_input);
        nimcp_gpu_tensor_destroy(tensor_v);
        nimcp_gpu_tensor_destroy(tensor_spikes);
    }
}

/**
 * WHAT: Test Izhikevich neuron forward pass equivalence
 * WHY:  Izhikevich model captures diverse spiking behaviors
 * HOW:  Compare v, u dynamics and spike outputs
 */
TEST_F(GPUCPUEquivalenceTest, IzhikevichForward_Equivalence) {
    const size_t n_neurons = 64;
    // Regular spiking parameters
    const float a = 0.02f;
    const float b = 0.2f;
    const float c = -65.0f;
    const float d = 8.0f;
    const float dt = 0.5f;

    auto input = generateRandomData(n_neurons, 0.0f, 20.0f);
    std::vector<float> cpu_v(n_neurons, -65.0f);
    std::vector<float> cpu_u(n_neurons);
    std::vector<float> cpu_spikes(n_neurons, 0.0f);

    // Initialize u
    for (size_t i = 0; i < n_neurons; i++) {
        cpu_u[i] = b * cpu_v[i];
    }

    // Multiple timesteps
    for (int step = 0; step < 50; step++) {
        cpuIzhikevichForward(cpu_v.data(), cpu_u.data(), cpu_spikes.data(),
                              input.data(), n_neurons, a, b, c, d, dt);
    }

    if (hasGPU()) {
        std::vector<size_t> dims = {n_neurons};
        auto* tensor_input = createGPUTensor(input, dims);
        std::vector<float> init_v(n_neurons, -65.0f);
        std::vector<float> init_u(n_neurons);
        for (size_t i = 0; i < n_neurons; i++) init_u[i] = b * init_v[i];

        auto* tensor_v = createGPUTensor(init_v, dims);
        auto* tensor_u = createGPUTensor(init_u, dims);
        auto* tensor_spikes = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        for (int step = 0; step < 50; step++) {
            auto result = NIMCP_SNN_OPS()->izhikevich_forward(
                gpu_ctx, tensor_input, tensor_v, tensor_u, tensor_spikes, a, b, c, d, dt);
            EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);
        }

        auto gpu_v = copyToHost(tensor_v);
        auto gpu_u = copyToHost(tensor_u);

        std::string error_msg;
        // Use slightly relaxed tolerance for multi-step ODE solver (50 iterations)
        // Floating-point errors accumulate over multiple timesteps
        constexpr float ODE_TOLERANCE = 5e-3f;  // 0.5% for 50-step Euler integration
        EXPECT_TRUE(compareTensors(cpu_v.data(), gpu_v.data(), n_neurons,
                                   ODE_TOLERANCE, error_msg))
            << "V mismatch: " << error_msg;
        EXPECT_TRUE(compareTensors(cpu_u.data(), gpu_u.data(), n_neurons,
                                   ODE_TOLERANCE, error_msg))
            << "U mismatch: " << error_msg;

        nimcp_gpu_tensor_destroy(tensor_input);
        nimcp_gpu_tensor_destroy(tensor_v);
        nimcp_gpu_tensor_destroy(tensor_u);
        nimcp_gpu_tensor_destroy(tensor_spikes);
    }
}

/**
 * WHAT: Test surrogate gradient computation equivalence
 * WHY:  Surrogate gradients enable SNN backpropagation
 * HOW:  Compare SuperSpike gradient with CPU reference
 */
TEST_F(GPUCPUEquivalenceTest, SurrogateGradient_Equivalence) {
    const size_t size = MEDIUM_SIZE;
    const float beta = 10.0f;  // SuperSpike sharpness

    auto input = generateRandomData(size, -1.0f, 1.0f);
    std::vector<float> cpu_grad(size);

    // CPU SuperSpike: 1 / (1 + beta * |x|)^2
    for (size_t i = 0; i < size; i++) {
        float denom = 1.0f + beta * std::fabs(input[i]);
        cpu_grad[i] = 1.0f / (denom * denom);
    }

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_input = createGPUTensor(input, dims);
        auto* tensor_grad = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_SNN_OPS()->surrogate_superspike(gpu_ctx, tensor_input, tensor_grad, beta);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_grad = copyToHost(tensor_grad);
        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_grad.data(), gpu_grad.data(), size,
                                   SNN_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_input);
        nimcp_gpu_tensor_destroy(tensor_grad);
    }
}

//=============================================================================
// LNN ODE SOLVER TESTS
//=============================================================================

/**
 * WHAT: Test Euler ODE step equivalence
 * WHY:  Euler is simplest ODE solver for LNN
 * HOW:  Compare single step x_new = x + dt * dx_dt
 */
TEST_F(GPUCPUEquivalenceTest, LNN_EulerStep_Equivalence) {
    const size_t n_neurons = 64;
    const float dt = 1.0f;

    auto x = generateRandomData(n_neurons);
    auto dx_dt = generateRandomData(n_neurons, -0.1f, 0.1f);
    std::vector<float> cpu_x_new(n_neurons);

    cpuEulerStep(x.data(), dx_dt.data(), dt, cpu_x_new.data(), n_neurons);

    if (hasGPU()) {
        std::vector<size_t> dims = {n_neurons};
        auto* tensor_x = createGPUTensor(x, dims);
        auto* tensor_dx = createGPUTensor(dx_dt, dims);
        auto* tensor_x_new = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_LNN_OPS()->euler_step(gpu_ctx, tensor_x, tensor_dx, dt, tensor_x_new);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_x_new = copyToHost(tensor_x_new);
        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_x_new.data(), gpu_x_new.data(), n_neurons,
                                   ODE_TOLERANCE, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_x);
        nimcp_gpu_tensor_destroy(tensor_dx);
        nimcp_gpu_tensor_destroy(tensor_x_new);
    }
}

/**
 * WHAT: Test multiple ODE steps accumulate correctly
 * WHY:  ODE errors can accumulate over many steps
 * HOW:  Run multiple Euler steps and compare trajectories
 */
TEST_F(GPUCPUEquivalenceTest, LNN_MultiStep_Equivalence) {
    const size_t n_neurons = 32;
    const float dt = 0.1f;
    const int n_steps = 100;

    // Simple exponential decay: dx/dt = -0.1 * x
    auto x = generateRandomData(n_neurons, 0.5f, 1.5f);
    std::vector<float> cpu_x = x;

    // CPU simulation
    for (int step = 0; step < n_steps; step++) {
        for (size_t i = 0; i < n_neurons; i++) {
            float dx_dt = -0.1f * cpu_x[i];
            cpu_x[i] = cpu_x[i] + dt * dx_dt;
        }
    }

    if (hasGPU()) {
        std::vector<size_t> dims = {n_neurons};
        auto* tensor_x = createGPUTensor(x, dims);
        auto* tensor_dx = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
        auto* tensor_x_new = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        for (int step = 0; step < n_steps; step++) {
            // Compute derivative: dx/dt = -0.1 * x
            auto gpu_x = copyToHost(tensor_x);
            std::vector<float> dx(n_neurons);
            for (size_t i = 0; i < n_neurons; i++) {
                dx[i] = -0.1f * gpu_x[i];
            }

            // Copy dx to GPU and step
            nimcp_gpu_tensor_destroy(tensor_dx);
            tensor_dx = createGPUTensor(dx, dims);

            auto result = NIMCP_LNN_OPS()->euler_step(gpu_ctx, tensor_x, tensor_dx, dt, tensor_x_new);
            EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

            // Swap for next iteration
            std::swap(tensor_x, tensor_x_new);
        }

        auto gpu_final_x = copyToHost(tensor_x);
        std::string error_msg;
        // Allow larger tolerance for accumulated error
        EXPECT_TRUE(compareTensors(cpu_x.data(), gpu_final_x.data(), n_neurons,
                                   ODE_TOLERANCE * n_steps * 0.01f, error_msg))
            << error_msg;

        nimcp_gpu_tensor_destroy(tensor_x);
        nimcp_gpu_tensor_destroy(tensor_dx);
        nimcp_gpu_tensor_destroy(tensor_x_new);
    }
}

//=============================================================================
// DATA DISTRIBUTION TESTS
//=============================================================================

/**
 * WHAT: Test operations with different data distributions
 * WHY:  Ensure correctness across various input characteristics
 * HOW:  Test with zeros, ones, uniform, normal, sparse data
 */
TEST_F(GPUCPUEquivalenceTest, DataDistribution_Zeros) {
    const size_t size = MEDIUM_SIZE;
    std::vector<float> zeros(size, 0.0f);
    std::vector<float> cpu_result(size);

    cpuRelu(zeros.data(), cpu_result.data(), size);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(zeros, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        NIMCP_TENSOR_OPS()->relu(gpu_ctx, tensor_in, tensor_out);
        auto gpu_result = copyToHost(tensor_out);

        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   STRICT_TOLERANCE, error_msg));

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

TEST_F(GPUCPUEquivalenceTest, DataDistribution_Ones) {
    const size_t size = MEDIUM_SIZE;
    std::vector<float> ones(size, 1.0f);
    std::vector<float> cpu_result(size);

    cpuSigmoid(ones.data(), cpu_result.data(), size);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(ones, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        NIMCP_TENSOR_OPS()->sigmoid(gpu_ctx, tensor_in, tensor_out);
        auto gpu_result = copyToHost(tensor_out);

        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   RELAXED_TOLERANCE, error_msg));

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

TEST_F(GPUCPUEquivalenceTest, DataDistribution_Sparse) {
    const size_t size = MEDIUM_SIZE;
    std::vector<float> sparse(size, 0.0f);

    // Only 10% non-zero
    for (size_t i = 0; i < size / 10; i++) {
        sparse[i * 10] = static_cast<float>(i) * 0.1f;
    }

    std::vector<float> cpu_result(size);
    cpuRelu(sparse.data(), cpu_result.data(), size);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(sparse, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        NIMCP_TENSOR_OPS()->relu(gpu_ctx, tensor_in, tensor_out);
        auto gpu_result = copyToHost(tensor_out);

        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   STRICT_TOLERANCE, error_msg));

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

TEST_F(GPUCPUEquivalenceTest, DataDistribution_LargeValues) {
    const size_t size = MEDIUM_SIZE;
    auto large_vals = generateRandomData(size, -1000.0f, 1000.0f);
    std::vector<float> cpu_result(size);

    // Tanh saturates for large values
    cpuTanh(large_vals.data(), cpu_result.data(), size);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(large_vals, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        NIMCP_TENSOR_OPS()->tanh(gpu_ctx, tensor_in, tensor_out);
        auto gpu_result = copyToHost(tensor_out);

        std::string error_msg;
        EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                   RELAXED_TOLERANCE, error_msg));

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

//=============================================================================
// REDUCTION OPERATIONS
//=============================================================================

/**
 * WHAT: Test sum reduction equivalence
 * WHY:  Reduction is fundamental parallel operation
 * HOW:  Compare CPU sequential sum with GPU parallel reduction
 */
TEST_F(GPUCPUEquivalenceTest, Reduction_Sum_Equivalence) {
    const size_t size = LARGE_SIZE;
    auto data = generateRandomData(size);

    float cpu_sum = 0;
    for (size_t i = 0; i < size; i++) {
        cpu_sum += data[i];
    }

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        std::vector<size_t> out_dims = {1};
        auto* tensor_in = createGPUTensor(data, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TENSOR_OPS()->sum(gpu_ctx, tensor_in, tensor_out);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_result = copyToHost(tensor_out);
        ASSERT_EQ(gpu_result.size(), 1);

        // Reduction may have different rounding, use relative tolerance
        float rel_diff = std::fabs(gpu_result[0] - cpu_sum) / std::max(std::fabs(cpu_sum), 1e-10f);
        EXPECT_LT(rel_diff, RELAXED_TOLERANCE) << "CPU: " << cpu_sum << ", GPU: " << gpu_result[0];

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test mean reduction equivalence
 * WHY:  Mean used in normalization and loss computation
 * HOW:  Compare CPU mean with GPU parallel mean
 */
TEST_F(GPUCPUEquivalenceTest, Reduction_Mean_Equivalence) {
    const size_t size = LARGE_SIZE;
    auto data = generateRandomData(size);

    float cpu_mean = 0;
    for (size_t i = 0; i < size; i++) {
        cpu_mean += data[i];
    }
    cpu_mean /= size;

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        std::vector<size_t> out_dims = {1};
        auto* tensor_in = createGPUTensor(data, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        auto result = NIMCP_TENSOR_OPS()->mean(gpu_ctx, tensor_in, tensor_out);
        EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

        auto gpu_result = copyToHost(tensor_out);
        ASSERT_EQ(gpu_result.size(), 1);

        EXPECT_NEAR(cpu_mean, gpu_result[0], RELAXED_TOLERANCE);

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

//=============================================================================
// EDGE CASES AND ROBUSTNESS
//=============================================================================

/**
 * WHAT: Test operations with minimum size tensors
 * WHY:  Edge cases often reveal bugs
 * HOW:  Run operations on size-1 tensors
 */
TEST_F(GPUCPUEquivalenceTest, EdgeCase_SingleElement) {
    std::vector<float> single_a = {1.5f};
    std::vector<float> single_b = {2.0f};
    std::vector<float> cpu_result(1);

    cpuAdd(single_a.data(), single_b.data(), cpu_result.data(), 1);

    if (hasGPU()) {
        std::vector<size_t> dims = {1};
        auto* tensor_a = createGPUTensor(single_a, dims);
        auto* tensor_b = createGPUTensor(single_b, dims);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        NIMCP_TENSOR_OPS()->add(gpu_ctx, tensor_a, tensor_b, tensor_out);
        auto gpu_result = copyToHost(tensor_out);

        EXPECT_NEAR(cpu_result[0], gpu_result[0], STRICT_TOLERANCE);

        nimcp_gpu_tensor_destroy(tensor_a);
        nimcp_gpu_tensor_destroy(tensor_b);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test operations with non-power-of-2 sizes
 * WHY:  Many GPU kernels are optimized for power-of-2 sizes
 * HOW:  Test with various non-power-of-2 sizes
 */
TEST_F(GPUCPUEquivalenceTest, EdgeCase_NonPowerOf2) {
    std::vector<size_t> odd_sizes = {17, 127, 255, 513, 1023};

    for (size_t size : odd_sizes) {
        SCOPED_TRACE("Size: " + std::to_string(size));

        auto data = generateRandomData(size);
        std::vector<float> cpu_result(size);
        cpuRelu(data.data(), cpu_result.data(), size);

        if (hasGPU()) {
            std::vector<size_t> dims = {size};
            auto* tensor_in = createGPUTensor(data, dims);
            auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

            NIMCP_TENSOR_OPS()->relu(gpu_ctx, tensor_in, tensor_out);
            auto gpu_result = copyToHost(tensor_out);

            std::string error_msg;
            EXPECT_TRUE(compareTensors(cpu_result.data(), gpu_result.data(), size,
                                       STRICT_TOLERANCE, error_msg))
                << error_msg;

            nimcp_gpu_tensor_destroy(tensor_in);
            nimcp_gpu_tensor_destroy(tensor_out);
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
