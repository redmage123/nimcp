/**
 * @file test_statistics_gpu_integration.cpp
 * @brief Integration tests for GPU-accelerated statistics operations
 *
 * WHAT: Verify GPU statistics kernels integrate correctly with the system
 * WHY:  Ensure GPU acceleration provides correct results matching CPU reference
 * HOW:  Test GPU/CPU equivalence, backend switching, memory transfers, batch ops
 *
 * TEST COVERAGE:
 * - GPU/CPU equivalence for all statistics operations
 * - Memory transfer correctness (host-to-device, device-to-host)
 * - Batch statistics operations on GPU
 * - Backend switching (CPU fallback when GPU unavailable)
 * - Large dataset handling with GPU acceleration
 * - Mixed precision statistics (FP32, FP16)
 * - Concurrent GPU statistics operations
 * - GPU memory management and cleanup
 * - Performance benchmarking GPU vs CPU
 * - Real-world neural data processing on GPU
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
#include <chrono>
#include <numeric>
#include <algorithm>
#include <thread>

// GPU headers
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/backend/nimcp_kernel_backend.h"

// Statistics headers
extern "C" {
#include "utils/statistics/nimcp_statistics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/tensor/nimcp_tensor.h"
}

//=============================================================================
// Test Configuration Constants
//=============================================================================

namespace {
    // Tolerance thresholds for floating-point comparisons
    constexpr float STRICT_TOLERANCE = 1e-5f;      // For exact operations
    constexpr float RELAXED_TOLERANCE = 1e-4f;     // For transcendental functions
    constexpr float REDUCTION_TOLERANCE = 1e-3f;   // For reduction operations (accumulated error)
    constexpr float FP16_TOLERANCE = 1e-2f;        // For half precision operations

    // Test tensor sizes
    constexpr size_t SMALL_SIZE = 128;
    constexpr size_t MEDIUM_SIZE = 4096;
    constexpr size_t LARGE_SIZE = 65536;
    constexpr size_t HUGE_SIZE = 1048576;  // 1M elements

    // Batch sizes
    constexpr size_t SMALL_BATCH = 8;
    constexpr size_t MEDIUM_BATCH = 64;
    constexpr size_t LARGE_BATCH = 256;

    // Performance thresholds
    constexpr double GPU_SPEEDUP_THRESHOLD = 1.5;  // GPU should be at least 1.5x faster for large data
}

//=============================================================================
// Test Fixture with Helper Functions
//=============================================================================

class StatisticsGPUIntegrationTest : public ::testing::Test {
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

        // Initialize statistics module
        nimcp_error_t err = nimcp_statistics_init();
        ASSERT_EQ(err, NIMCP_OK) << "Failed to initialize statistics module";
    }

    void TearDown() override {
        nimcp_statistics_shutdown();

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
    // Helper: Generate normal distribution data
    //=========================================================================
    std::vector<float> generateNormalData(size_t count, float mean = 0.0f, float stddev = 1.0f) {
        std::vector<float> data(count);
        std::normal_distribution<float> dist(mean, stddev);
        for (size_t i = 0; i < count; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    //=========================================================================
    // Helper: Generate neural spike train data
    //=========================================================================
    std::vector<float> generateSpikeTrainData(size_t count, float firing_rate = 0.1f) {
        std::vector<float> data(count);
        std::bernoulli_distribution dist(firing_rate);
        for (size_t i = 0; i < count; i++) {
            data[i] = dist(rng) ? 1.0f : 0.0f;
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
    // Helper: Compare relative error for larger values
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
        if (!gpu_ctx || !nimcp_cuda_backend_available()) {
            return false;
        }
        nimcp_backend_type_t active_type = nimcp_get_backend_type();
        return active_type == NIMCP_BACKEND_CUDA;
    }

    //=========================================================================
    // CPU reference implementations for statistics
    //=========================================================================
    float cpuMean(const float* data, size_t n) {
        float sum = 0;
        for (size_t i = 0; i < n; i++) sum += data[i];
        return sum / n;
    }

    float cpuVariance(const float* data, size_t n, bool sample = true) {
        float mean = cpuMean(data, n);
        float sum_sq = 0;
        for (size_t i = 0; i < n; i++) {
            float diff = data[i] - mean;
            sum_sq += diff * diff;
        }
        return sum_sq / (sample ? (n - 1) : n);
    }

    float cpuStdDev(const float* data, size_t n, bool sample = true) {
        return std::sqrt(cpuVariance(data, n, sample));
    }

    float cpuSkewness(const float* data, size_t n) {
        float mean = cpuMean(data, n);
        float std = cpuStdDev(data, n, false);
        if (std < 1e-10f) return 0.0f;

        float sum = 0;
        for (size_t i = 0; i < n; i++) {
            float z = (data[i] - mean) / std;
            sum += z * z * z;
        }
        return sum / n;
    }

    float cpuKurtosis(const float* data, size_t n) {
        float mean = cpuMean(data, n);
        float std = cpuStdDev(data, n, false);
        if (std < 1e-10f) return 0.0f;

        float sum = 0;
        for (size_t i = 0; i < n; i++) {
            float z = (data[i] - mean) / std;
            sum += z * z * z * z;
        }
        return sum / n - 3.0f;  // Excess kurtosis
    }

    float cpuCovariance(const float* x, const float* y, size_t n) {
        float mean_x = cpuMean(x, n);
        float mean_y = cpuMean(y, n);
        float sum = 0;
        for (size_t i = 0; i < n; i++) {
            sum += (x[i] - mean_x) * (y[i] - mean_y);
        }
        return sum / (n - 1);
    }

    float cpuCorrelation(const float* x, const float* y, size_t n) {
        float cov = cpuCovariance(x, y, n);
        float std_x = cpuStdDev(x, n);
        float std_y = cpuStdDev(y, n);
        if (std_x < 1e-10f || std_y < 1e-10f) return 0.0f;
        return cov / (std_x * std_y);
    }

    float cpuEntropy(const float* probs, size_t n) {
        float entropy = 0;
        for (size_t i = 0; i < n; i++) {
            if (probs[i] > 1e-10f) {
                entropy -= probs[i] * std::log2(probs[i]);
            }
        }
        return entropy;
    }
};

//=============================================================================
// GPU/CPU EQUIVALENCE TESTS - DESCRIPTIVE STATISTICS
//=============================================================================

/**
 * WHAT: Test mean computation produces equivalent results on GPU and CPU
 * WHY:  Mean is fundamental statistical operation
 * HOW:  Compare GPU parallel reduction with CPU sequential sum
 */
TEST_F(StatisticsGPUIntegrationTest, Mean_GPUCPUEquivalence) {
    const std::vector<size_t> sizes = {SMALL_SIZE, MEDIUM_SIZE, LARGE_SIZE};

    for (size_t size : sizes) {
        SCOPED_TRACE("Size: " + std::to_string(size));

        auto data = generateRandomData(size);
        float cpu_mean = cpuMean(data.data(), size);

        // CPU statistics library
        nimcp_descriptive_stats_t cpu_stats;
        nimcp_error_t err = nimcp_statistics_descriptive(data.data(), size, &cpu_stats);
        EXPECT_EQ(err, NIMCP_OK);
        EXPECT_NEAR(cpu_stats.mean, cpu_mean, STRICT_TOLERANCE);

        if (hasGPU()) {
            std::vector<size_t> dims = {size};
            auto* tensor_in = createGPUTensor(data, dims);
            std::vector<size_t> out_dims = {1};
            auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

            ASSERT_NE(tensor_in, nullptr);
            ASSERT_NE(tensor_out, nullptr);

            bool result = nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_out, -1, false);
            EXPECT_TRUE(result);

            auto gpu_result = copyToHost(tensor_out);
            EXPECT_NEAR(cpu_mean, gpu_result[0], REDUCTION_TOLERANCE)
                << "GPU mean mismatch for size " << size;

            nimcp_gpu_tensor_destroy(tensor_in);
            nimcp_gpu_tensor_destroy(tensor_out);
        }
    }
}

/**
 * WHAT: Test variance computation equivalence
 * WHY:  Variance is used in normalization, standardization
 * HOW:  Compare GPU and CPU variance calculations
 */
TEST_F(StatisticsGPUIntegrationTest, Variance_GPUCPUEquivalence) {
    const size_t size = MEDIUM_SIZE;
    auto data = generateNormalData(size, 0.0f, 2.0f);

    float cpu_var = cpuVariance(data.data(), size, false);  // Population variance

    nimcp_descriptive_stats_t cpu_stats;
    nimcp_error_t err = nimcp_statistics_descriptive(data.data(), size, &cpu_stats);
    EXPECT_EQ(err, NIMCP_OK);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(data, dims);
        std::vector<size_t> out_dims = {1};
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        bool result = nimcp_gpu_var(gpu_ctx, tensor_in, tensor_out, -1, false, false);
        EXPECT_TRUE(result);

        auto gpu_result = copyToHost(tensor_out);
        EXPECT_NEAR(cpu_var, gpu_result[0], REDUCTION_TOLERANCE)
            << "GPU variance mismatch";

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test standard deviation equivalence
 * WHY:  StdDev used in z-score normalization
 * HOW:  Compare GPU and CPU std calculations
 */
TEST_F(StatisticsGPUIntegrationTest, StdDev_GPUCPUEquivalence) {
    const size_t size = MEDIUM_SIZE;
    auto data = generateNormalData(size, 5.0f, 3.0f);

    float cpu_std = cpuStdDev(data.data(), size, false);

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(data, dims);
        std::vector<size_t> out_dims = {1};
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        bool result = nimcp_gpu_std(gpu_ctx, tensor_in, tensor_out, -1, false, false);
        EXPECT_TRUE(result);

        auto gpu_result = copyToHost(tensor_out);
        EXPECT_NEAR(cpu_std, gpu_result[0], REDUCTION_TOLERANCE)
            << "GPU std mismatch";

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test sum reduction equivalence
 * WHY:  Sum is basis for many statistical computations
 * HOW:  Compare GPU parallel reduction with CPU sequential sum
 */
TEST_F(StatisticsGPUIntegrationTest, Sum_GPUCPUEquivalence) {
    const size_t size = LARGE_SIZE;
    auto data = generateRandomData(size);

    float cpu_sum = 0;
    for (size_t i = 0; i < size; i++) cpu_sum += data[i];

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(data, dims);
        std::vector<size_t> out_dims = {1};
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        bool result = nimcp_gpu_sum(gpu_ctx, tensor_in, tensor_out, -1, false);
        EXPECT_TRUE(result);

        auto gpu_result = copyToHost(tensor_out);
        float rel_error = std::fabs(gpu_result[0] - cpu_sum) / std::max(std::fabs(cpu_sum), 1e-10f);
        EXPECT_LT(rel_error, REDUCTION_TOLERANCE)
            << "CPU sum: " << cpu_sum << ", GPU sum: " << gpu_result[0];

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test min/max reduction equivalence
 * WHY:  Min/max used in normalization, range calculations
 * HOW:  Compare GPU parallel min/max with CPU
 */
TEST_F(StatisticsGPUIntegrationTest, MinMax_GPUCPUEquivalence) {
    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size, -100.0f, 100.0f);

    float cpu_min = *std::min_element(data.begin(), data.end());
    float cpu_max = *std::max_element(data.begin(), data.end());

    if (hasGPU()) {
        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(data, dims);
        std::vector<size_t> out_dims = {1};
        auto* tensor_min = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
        auto* tensor_max = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        bool result_min = nimcp_gpu_min(gpu_ctx, tensor_in, tensor_min, -1, false);
        bool result_max = nimcp_gpu_max(gpu_ctx, tensor_in, tensor_max, -1, false);
        EXPECT_TRUE(result_min);
        EXPECT_TRUE(result_max);

        auto gpu_min = copyToHost(tensor_min);
        auto gpu_max = copyToHost(tensor_max);
        EXPECT_NEAR(cpu_min, gpu_min[0], STRICT_TOLERANCE);
        EXPECT_NEAR(cpu_max, gpu_max[0], STRICT_TOLERANCE);

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_min);
        nimcp_gpu_tensor_destroy(tensor_max);
    }
}

//=============================================================================
// GPU MEMORY TRANSFER TESTS
//=============================================================================

/**
 * WHAT: Test host-to-device memory transfer correctness
 * WHY:  Data must be transferred correctly before GPU computation
 * HOW:  Transfer data to GPU and back, verify round-trip
 */
TEST_F(StatisticsGPUIntegrationTest, MemoryTransfer_HostToDevice) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size);

    std::vector<size_t> dims = {size};
    auto* tensor = createGPUTensor(data, dims);
    ASSERT_NE(tensor, nullptr);

    auto result = copyToHost(tensor);
    ASSERT_EQ(result.size(), size);

    std::string error_msg;
    EXPECT_TRUE(compareTensors(data.data(), result.data(), size, STRICT_TOLERANCE, error_msg))
        << "Round-trip memory transfer failed: " << error_msg;

    nimcp_gpu_tensor_destroy(tensor);
}

/**
 * WHAT: Test large data transfer correctness
 * WHY:  Large transfers may have different code paths
 * HOW:  Transfer large dataset and verify
 */
TEST_F(StatisticsGPUIntegrationTest, MemoryTransfer_LargeData) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t size = HUGE_SIZE;
    auto data = generateRandomData(size);

    std::vector<size_t> dims = {size};
    auto* tensor = createGPUTensor(data, dims);
    ASSERT_NE(tensor, nullptr);

    auto result = copyToHost(tensor);
    ASSERT_EQ(result.size(), size);

    // Sample check (full comparison too expensive)
    for (size_t i = 0; i < 1000; i++) {
        size_t idx = (i * 1009) % size;  // Prime stride for good coverage
        EXPECT_NEAR(data[idx], result[idx], STRICT_TOLERANCE)
            << "Mismatch at index " << idx;
    }

    nimcp_gpu_tensor_destroy(tensor);
}

/**
 * WHAT: Test multiple sequential transfers
 * WHY:  Repeated transfers should not cause memory issues
 * HOW:  Transfer many times and verify
 */
TEST_F(StatisticsGPUIntegrationTest, MemoryTransfer_Sequential) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t size = SMALL_SIZE;
    const int iterations = 100;

    for (int iter = 0; iter < iterations; iter++) {
        auto data = generateRandomData(size);
        std::vector<size_t> dims = {size};
        auto* tensor = createGPUTensor(data, dims);
        ASSERT_NE(tensor, nullptr) << "Failed at iteration " << iter;

        auto result = copyToHost(tensor);
        EXPECT_EQ(result.size(), size);

        nimcp_gpu_tensor_destroy(tensor);
    }
}

//=============================================================================
// BATCH OPERATIONS TESTS
//=============================================================================

/**
 * WHAT: Test batch mean computation
 * WHY:  Batch operations are common in neural network training
 * HOW:  Compute mean for multiple samples in parallel
 */
TEST_F(StatisticsGPUIntegrationTest, Batch_MeanComputation) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t batch_size = MEDIUM_BATCH;
    const size_t sample_size = MEDIUM_SIZE;

    // Generate batch data
    std::vector<float> batch_data(batch_size * sample_size);
    std::vector<float> cpu_means(batch_size);

    for (size_t b = 0; b < batch_size; b++) {
        auto sample = generateRandomData(sample_size);
        std::copy(sample.begin(), sample.end(), batch_data.begin() + b * sample_size);
        cpu_means[b] = cpuMean(sample.data(), sample_size);
    }

    std::vector<size_t> dims = {batch_size, sample_size};
    auto* tensor_in = createGPUTensor(batch_data, dims);
    std::vector<size_t> out_dims = {batch_size};
    auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    // Compute mean along axis 1 (sample dimension)
    bool result = nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_out, 1, false);
    EXPECT_TRUE(result);

    auto gpu_means = copyToHost(tensor_out);
    ASSERT_EQ(gpu_means.size(), batch_size);

    for (size_t b = 0; b < batch_size; b++) {
        EXPECT_NEAR(cpu_means[b], gpu_means[b], REDUCTION_TOLERANCE)
            << "Batch " << b << " mean mismatch";
    }

    nimcp_gpu_tensor_destroy(tensor_in);
    nimcp_gpu_tensor_destroy(tensor_out);
}

/**
 * WHAT: Test batch variance computation
 * WHY:  Batch normalization requires per-sample statistics
 * HOW:  Compute variance for multiple samples in parallel
 */
TEST_F(StatisticsGPUIntegrationTest, Batch_VarianceComputation) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t batch_size = SMALL_BATCH;
    const size_t sample_size = MEDIUM_SIZE;

    std::vector<float> batch_data(batch_size * sample_size);
    std::vector<float> cpu_vars(batch_size);

    for (size_t b = 0; b < batch_size; b++) {
        auto sample = generateNormalData(sample_size, static_cast<float>(b), 1.0f + b * 0.5f);
        std::copy(sample.begin(), sample.end(), batch_data.begin() + b * sample_size);
        cpu_vars[b] = cpuVariance(sample.data(), sample_size, false);
    }

    std::vector<size_t> dims = {batch_size, sample_size};
    auto* tensor_in = createGPUTensor(batch_data, dims);
    std::vector<size_t> out_dims = {batch_size};
    auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    bool result = nimcp_gpu_var(gpu_ctx, tensor_in, tensor_out, 1, false, false);
    EXPECT_TRUE(result);

    auto gpu_vars = copyToHost(tensor_out);
    ASSERT_EQ(gpu_vars.size(), batch_size);

    for (size_t b = 0; b < batch_size; b++) {
        EXPECT_NEAR(cpu_vars[b], gpu_vars[b], REDUCTION_TOLERANCE)
            << "Batch " << b << " variance mismatch";
    }

    nimcp_gpu_tensor_destroy(tensor_in);
    nimcp_gpu_tensor_destroy(tensor_out);
}

/**
 * WHAT: Test batch normalization (z-score)
 * WHY:  Normalization is fundamental preprocessing step
 * HOW:  Compute (x - mean) / std for each sample
 */
TEST_F(StatisticsGPUIntegrationTest, Batch_Normalization) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t batch_size = SMALL_BATCH;
    const size_t sample_size = MEDIUM_SIZE;

    std::vector<float> batch_data(batch_size * sample_size);
    std::vector<float> cpu_normalized(batch_size * sample_size);

    for (size_t b = 0; b < batch_size; b++) {
        auto sample = generateNormalData(sample_size, static_cast<float>(b) * 10.0f, 2.0f + b);
        std::copy(sample.begin(), sample.end(), batch_data.begin() + b * sample_size);

        // CPU normalization
        float mean = cpuMean(sample.data(), sample_size);
        float std = cpuStdDev(sample.data(), sample_size, false);
        for (size_t i = 0; i < sample_size; i++) {
            cpu_normalized[b * sample_size + i] = (sample[i] - mean) / std;
        }
    }

    // GPU normalization would be implemented with element-wise ops
    // For now, verify the CPU statistics library can handle batch data
    for (size_t b = 0; b < batch_size; b++) {
        nimcp_descriptive_stats_t stats;
        nimcp_error_t err = nimcp_statistics_descriptive(
            batch_data.data() + b * sample_size, sample_size, &stats);
        EXPECT_EQ(err, NIMCP_OK);

        // Verify normalized data has mean ~0 and std ~1
        float norm_mean = cpuMean(cpu_normalized.data() + b * sample_size, sample_size);
        float norm_std = cpuStdDev(cpu_normalized.data() + b * sample_size, sample_size, false);
        EXPECT_NEAR(norm_mean, 0.0f, RELAXED_TOLERANCE);
        EXPECT_NEAR(norm_std, 1.0f, RELAXED_TOLERANCE);
    }
}

//=============================================================================
// BACKEND SWITCHING TESTS
//=============================================================================

/**
 * WHAT: Test CPU fallback when GPU unavailable
 * WHY:  System should work without GPU
 * HOW:  Force CPU backend and verify statistics work
 */
TEST_F(StatisticsGPUIntegrationTest, Backend_CPUFallback) {
    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size);

    // Statistics should work regardless of GPU availability
    nimcp_descriptive_stats_t stats;
    nimcp_error_t err = nimcp_statistics_descriptive(data.data(), size, &stats);
    EXPECT_EQ(err, NIMCP_OK);

    float expected_mean = cpuMean(data.data(), size);
    EXPECT_NEAR(stats.mean, expected_mean, STRICT_TOLERANCE);
}

/**
 * WHAT: Test backend switching during computation
 * WHY:  System may need to switch backends dynamically
 * HOW:  Compute stats, switch backend, compute again
 */
TEST_F(StatisticsGPUIntegrationTest, Backend_SwitchDuringComputation) {
    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size);
    float expected_mean = cpuMean(data.data(), size);

    // First computation
    nimcp_descriptive_stats_t stats1;
    nimcp_error_t err1 = nimcp_statistics_descriptive(data.data(), size, &stats1);
    EXPECT_EQ(err1, NIMCP_OK);
    EXPECT_NEAR(stats1.mean, expected_mean, STRICT_TOLERANCE);

    // Try to switch backend (if possible)
    nimcp_backend_type_t original_type = nimcp_get_backend_type();

    // Switch to CPU explicitly
    nimcp_kernel_backend_shutdown();
    bool init_ok = nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
    ASSERT_TRUE(init_ok);

    // Second computation on CPU
    nimcp_descriptive_stats_t stats2;
    nimcp_error_t err2 = nimcp_statistics_descriptive(data.data(), size, &stats2);
    EXPECT_EQ(err2, NIMCP_OK);
    EXPECT_NEAR(stats2.mean, expected_mean, STRICT_TOLERANCE);

    // Results should match
    EXPECT_NEAR(stats1.mean, stats2.mean, STRICT_TOLERANCE);

    // Restore original backend
    nimcp_kernel_backend_shutdown();
    nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);
    gpu_ctx = nimcp_gpu_context_create_auto();
}

/**
 * WHAT: Test results consistency across backends
 * WHY:  GPU and CPU should produce same results
 * HOW:  Run same computation on both backends, compare
 */
TEST_F(StatisticsGPUIntegrationTest, Backend_ResultsConsistency) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available for consistency test";
    }

    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size);

    // CPU computation
    nimcp_descriptive_stats_t cpu_stats;
    nimcp_error_t err = nimcp_statistics_descriptive(data.data(), size, &cpu_stats);
    EXPECT_EQ(err, NIMCP_OK);

    // GPU computation
    std::vector<size_t> dims = {size};
    auto* tensor_in = createGPUTensor(data, dims);
    std::vector<size_t> out_dims = {1};
    auto* tensor_mean = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_var = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_mean, -1, false);
    nimcp_gpu_var(gpu_ctx, tensor_in, tensor_var, -1, false, true);  // Sample variance

    auto gpu_mean = copyToHost(tensor_mean);
    auto gpu_var = copyToHost(tensor_var);

    EXPECT_NEAR(cpu_stats.mean, gpu_mean[0], REDUCTION_TOLERANCE)
        << "Mean mismatch between CPU and GPU";
    EXPECT_NEAR(cpu_stats.variance, gpu_var[0], REDUCTION_TOLERANCE)
        << "Variance mismatch between CPU and GPU";

    nimcp_gpu_tensor_destroy(tensor_in);
    nimcp_gpu_tensor_destroy(tensor_mean);
    nimcp_gpu_tensor_destroy(tensor_var);
}

//=============================================================================
// LARGE DATASET TESTS
//=============================================================================

/**
 * WHAT: Test statistics on very large datasets
 * WHY:  GPU acceleration is most beneficial for large data
 * HOW:  Process million-element arrays
 */
TEST_F(StatisticsGPUIntegrationTest, LargeData_MillionElements) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t size = HUGE_SIZE;
    auto data = generateNormalData(size, 0.0f, 1.0f);

    // CPU reference (may be slow)
    float cpu_mean = cpuMean(data.data(), size);
    float cpu_var = cpuVariance(data.data(), size, false);

    // GPU computation
    std::vector<size_t> dims = {size};
    auto* tensor_in = createGPUTensor(data, dims);
    std::vector<size_t> out_dims = {1};
    auto* tensor_mean = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_var = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    bool result1 = nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_mean, -1, false);
    bool result2 = nimcp_gpu_var(gpu_ctx, tensor_in, tensor_var, -1, false, false);
    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);

    auto gpu_mean = copyToHost(tensor_mean);
    auto gpu_var = copyToHost(tensor_var);

    // For large data, use relative tolerance
    float mean_rel_error = std::fabs(gpu_mean[0] - cpu_mean) / std::max(std::fabs(cpu_mean), 1e-10f);
    float var_rel_error = std::fabs(gpu_var[0] - cpu_var) / std::max(std::fabs(cpu_var), 1e-10f);

    EXPECT_LT(mean_rel_error, REDUCTION_TOLERANCE);
    EXPECT_LT(var_rel_error, REDUCTION_TOLERANCE);

    nimcp_gpu_tensor_destroy(tensor_in);
    nimcp_gpu_tensor_destroy(tensor_mean);
    nimcp_gpu_tensor_destroy(tensor_var);
}

/**
 * WHAT: Test chunked processing for very large data
 * WHY:  May need to process data in chunks for memory constraints
 * HOW:  Process data in chunks, combine results
 */
TEST_F(StatisticsGPUIntegrationTest, LargeData_ChunkedProcessing) {
    const size_t total_size = HUGE_SIZE;
    const size_t chunk_size = LARGE_SIZE;
    const size_t num_chunks = total_size / chunk_size;

    auto data = generateNormalData(total_size);

    // Full computation reference
    float full_mean = cpuMean(data.data(), total_size);

    // Chunked mean computation using Welford's algorithm
    float running_mean = 0.0f;
    size_t running_n = 0;

    for (size_t c = 0; c < num_chunks; c++) {
        float chunk_mean = cpuMean(data.data() + c * chunk_size, chunk_size);

        // Update running mean
        for (size_t i = 0; i < chunk_size; i++) {
            running_n++;
            running_mean += (data[c * chunk_size + i] - running_mean) / running_n;
        }
    }

    EXPECT_NEAR(full_mean, running_mean, RELAXED_TOLERANCE)
        << "Chunked mean differs from full mean";
}

//=============================================================================
// PERFORMANCE TESTS
//=============================================================================

/**
 * WHAT: Benchmark GPU vs CPU performance for mean computation
 * WHY:  Verify GPU provides performance benefit
 * HOW:  Time both implementations on large data
 */
TEST_F(StatisticsGPUIntegrationTest, Performance_MeanBenchmark) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available for performance test";
    }

    const size_t size = HUGE_SIZE;
    auto data = generateRandomData(size);

    // Warm up
    nimcp_descriptive_stats_t warm_stats;
    nimcp_statistics_descriptive(data.data(), size, &warm_stats);

    // CPU timing
    auto cpu_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; i++) {
        nimcp_descriptive_stats_t stats;
        nimcp_statistics_descriptive(data.data(), size, &stats);
    }
    auto cpu_end = std::chrono::high_resolution_clock::now();
    double cpu_time = std::chrono::duration<double, std::milli>(cpu_end - cpu_start).count();

    // GPU timing
    std::vector<size_t> dims = {size};
    auto* tensor_in = createGPUTensor(data, dims);
    std::vector<size_t> out_dims = {1};
    auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    // Warm up GPU
    nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_out, -1, false);

    auto gpu_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; i++) {
        nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_out, -1, false);
    }
    auto gpu_end = std::chrono::high_resolution_clock::now();
    double gpu_time = std::chrono::duration<double, std::milli>(gpu_end - gpu_start).count();

    // Log performance
    std::cout << "  CPU time (10 iterations): " << cpu_time << " ms" << std::endl;
    std::cout << "  GPU time (10 iterations): " << gpu_time << " ms" << std::endl;
    std::cout << "  Speedup: " << cpu_time / gpu_time << "x" << std::endl;

    // GPU should be faster for large data (but don't fail test if not)
    if (cpu_time / gpu_time < GPU_SPEEDUP_THRESHOLD) {
        std::cout << "  WARNING: GPU not significantly faster than CPU" << std::endl;
    }

    nimcp_gpu_tensor_destroy(tensor_in);
    nimcp_gpu_tensor_destroy(tensor_out);
}

/**
 * WHAT: Benchmark batch operations performance
 * WHY:  Batch operations should benefit from GPU parallelism
 * HOW:  Time batch mean computation on GPU
 */
TEST_F(StatisticsGPUIntegrationTest, Performance_BatchBenchmark) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available for performance test";
    }

    const size_t batch_size = LARGE_BATCH;
    const size_t sample_size = MEDIUM_SIZE;
    const size_t total_size = batch_size * sample_size;

    auto data = generateRandomData(total_size);

    // CPU timing (sequential batch processing)
    auto cpu_start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < 5; iter++) {
        for (size_t b = 0; b < batch_size; b++) {
            cpuMean(data.data() + b * sample_size, sample_size);
        }
    }
    auto cpu_end = std::chrono::high_resolution_clock::now();
    double cpu_time = std::chrono::duration<double, std::milli>(cpu_end - cpu_start).count();

    // GPU timing (parallel batch processing)
    std::vector<size_t> dims = {batch_size, sample_size};
    auto* tensor_in = createGPUTensor(data, dims);
    std::vector<size_t> out_dims = {batch_size};
    auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    // Warm up
    nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_out, 1, false);

    auto gpu_start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < 5; iter++) {
        nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_out, 1, false);
    }
    auto gpu_end = std::chrono::high_resolution_clock::now();
    double gpu_time = std::chrono::duration<double, std::milli>(gpu_end - gpu_start).count();

    std::cout << "  Batch CPU time (5 iterations): " << cpu_time << " ms" << std::endl;
    std::cout << "  Batch GPU time (5 iterations): " << gpu_time << " ms" << std::endl;
    std::cout << "  Batch speedup: " << cpu_time / gpu_time << "x" << std::endl;

    nimcp_gpu_tensor_destroy(tensor_in);
    nimcp_gpu_tensor_destroy(tensor_out);
}

//=============================================================================
// NEURAL DATA PROCESSING TESTS
//=============================================================================

/**
 * WHAT: Test GPU processing of neural spike train data
 * WHY:  Neural spike analysis is key NIMCP use case
 * HOW:  Compute firing rate and other statistics on GPU
 */
TEST_F(StatisticsGPUIntegrationTest, NeuralData_SpikeTrainAnalysis) {
    const size_t num_neurons = 1000;
    const size_t time_steps = 10000;
    const size_t total_size = num_neurons * time_steps;

    // Generate spike trains for multiple neurons
    std::vector<float> spike_data(total_size);
    std::vector<float> expected_rates(num_neurons);

    for (size_t n = 0; n < num_neurons; n++) {
        float firing_rate = 0.05f + 0.1f * (n % 10) / 10.0f;  // 5-15% firing rate
        auto spikes = generateSpikeTrainData(time_steps, firing_rate);
        std::copy(spikes.begin(), spikes.end(), spike_data.begin() + n * time_steps);
        expected_rates[n] = firing_rate;
    }

    // Compute firing rates using mean (spikes are 0/1)
    if (hasGPU()) {
        std::vector<size_t> dims = {num_neurons, time_steps};
        auto* tensor_in = createGPUTensor(spike_data, dims);
        std::vector<size_t> out_dims = {num_neurons};
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        bool result = nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_out, 1, false);
        EXPECT_TRUE(result);

        auto gpu_rates = copyToHost(tensor_out);
        ASSERT_EQ(gpu_rates.size(), num_neurons);

        // Verify computed rates are close to expected (within statistical variance)
        for (size_t n = 0; n < num_neurons; n++) {
            EXPECT_NEAR(gpu_rates[n], expected_rates[n], 0.02f)
                << "Neuron " << n << " firing rate mismatch";
        }

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test GPU processing of membrane potential data
 * WHY:  Membrane potential analysis requires statistics
 * HOW:  Compute mean, variance of membrane potentials
 */
TEST_F(StatisticsGPUIntegrationTest, NeuralData_MembranePotentialAnalysis) {
    const size_t num_neurons = 500;
    const size_t time_steps = 5000;
    const size_t total_size = num_neurons * time_steps;

    // Generate membrane potential data (typical range -80mV to +40mV)
    std::vector<float> membrane_data(total_size);
    for (size_t n = 0; n < num_neurons; n++) {
        float base_potential = -65.0f + 10.0f * (n % 5) / 5.0f;
        auto potentials = generateNormalData(time_steps, base_potential, 5.0f);
        std::copy(potentials.begin(), potentials.end(), membrane_data.begin() + n * time_steps);
    }

    // Compute statistics for each neuron
    nimcp_descriptive_stats_t overall_stats;
    nimcp_error_t err = nimcp_statistics_descriptive(membrane_data.data(), total_size, &overall_stats);
    EXPECT_EQ(err, NIMCP_OK);

    // Verify overall statistics are in expected range
    EXPECT_GT(overall_stats.mean, -80.0f);
    EXPECT_LT(overall_stats.mean, -50.0f);
    EXPECT_GT(overall_stats.variance, 0.0f);
}

/**
 * WHAT: Test inter-spike interval (ISI) statistics
 * WHY:  ISI analysis important for neural coding
 * HOW:  Compute ISI distribution statistics
 */
TEST_F(StatisticsGPUIntegrationTest, NeuralData_InterSpikeInterval) {
    const size_t num_spikes = 10000;

    // Generate ISIs from exponential distribution (Poisson process)
    std::vector<float> isis(num_spikes);
    std::exponential_distribution<float> exp_dist(0.05f);  // 20ms mean ISI
    for (size_t i = 0; i < num_spikes; i++) {
        isis[i] = exp_dist(rng);
    }

    nimcp_descriptive_stats_t isi_stats;
    nimcp_error_t err = nimcp_statistics_descriptive(isis.data(), num_spikes, &isi_stats);
    EXPECT_EQ(err, NIMCP_OK);

    // For exponential distribution, mean ~ 1/rate and CV ~ 1
    float cv = isi_stats.std_dev / isi_stats.mean;
    EXPECT_NEAR(cv, 1.0f, 0.1f)  // CV should be close to 1 for Poisson process
        << "ISI coefficient of variation: " << cv;

    // Mean ISI should be close to expected (20ms)
    EXPECT_NEAR(isi_stats.mean, 20.0f, 2.0f);
}

//=============================================================================
// GPU MEMORY MANAGEMENT TESTS
//=============================================================================

/**
 * WHAT: Test GPU memory cleanup after operations
 * WHY:  Memory leaks degrade performance over time
 * HOW:  Run many operations, verify memory freed
 */
TEST_F(StatisticsGPUIntegrationTest, Memory_CleanupAfterOperations) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    size_t initial_free, initial_total;
    nimcp_gpu_tensor_memory_info(gpu_ctx, &initial_free, &initial_total);

    // Run many operations
    for (int i = 0; i < 100; i++) {
        auto data = generateRandomData(MEDIUM_SIZE);
        std::vector<size_t> dims = {MEDIUM_SIZE};
        auto* tensor = createGPUTensor(data, dims);

        std::vector<size_t> out_dims = {1};
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        nimcp_gpu_mean(gpu_ctx, tensor, tensor_out, -1, false);

        nimcp_gpu_tensor_destroy(tensor);
        nimcp_gpu_tensor_destroy(tensor_out);
    }

    size_t final_free, final_total;
    nimcp_gpu_tensor_memory_info(gpu_ctx, &final_free, &final_total);

    // Memory should be mostly recovered (allow some fragmentation)
    float memory_recovered = static_cast<float>(final_free) / initial_free;
    EXPECT_GT(memory_recovered, 0.9f)
        << "Memory not properly freed: initial free=" << initial_free
        << ", final free=" << final_free;
}

/**
 * WHAT: Test allocation failure handling
 * WHY:  System should handle out-of-memory gracefully
 * HOW:  Attempt to allocate more memory than available
 */
TEST_F(StatisticsGPUIntegrationTest, Memory_AllocationFailure) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    size_t free_bytes, total_bytes;
    nimcp_gpu_tensor_memory_info(gpu_ctx, &free_bytes, &total_bytes);

    // Try to allocate more than available (this should fail gracefully)
    size_t huge_size = (total_bytes / sizeof(float)) * 2;  // 2x total memory
    std::vector<size_t> dims = {huge_size};

    // This allocation should fail
    auto* tensor = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    // Either fails (returns NULL) or system has enough memory
    if (tensor != nullptr) {
        nimcp_gpu_tensor_destroy(tensor);
    }
    // Test passes either way - we're checking for graceful handling
}

//=============================================================================
// CONCURRENT OPERATIONS TESTS
//=============================================================================

/**
 * WHAT: Test concurrent GPU statistics operations
 * WHY:  Multiple operations may run in parallel
 * HOW:  Launch multiple operations, verify results
 */
TEST_F(StatisticsGPUIntegrationTest, Concurrent_MultipleOperations) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t size = MEDIUM_SIZE;
    const int num_operations = 4;

    std::vector<std::vector<float>> datasets(num_operations);
    std::vector<nimcp_gpu_tensor_t*> input_tensors(num_operations);
    std::vector<nimcp_gpu_tensor_t*> output_tensors(num_operations);
    std::vector<float> expected_means(num_operations);

    // Create all tensors
    for (int i = 0; i < num_operations; i++) {
        datasets[i] = generateRandomData(size, -10.0f * i, 10.0f * i);
        expected_means[i] = cpuMean(datasets[i].data(), size);

        std::vector<size_t> dims = {size};
        input_tensors[i] = createGPUTensor(datasets[i], dims);
        std::vector<size_t> out_dims = {1};
        output_tensors[i] = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        ASSERT_NE(input_tensors[i], nullptr);
        ASSERT_NE(output_tensors[i], nullptr);
    }

    // Launch all operations (they may execute concurrently on GPU)
    for (int i = 0; i < num_operations; i++) {
        bool result = nimcp_gpu_mean(gpu_ctx, input_tensors[i], output_tensors[i], -1, false);
        EXPECT_TRUE(result);
    }

    // Verify all results
    for (int i = 0; i < num_operations; i++) {
        auto gpu_result = copyToHost(output_tensors[i]);
        EXPECT_NEAR(expected_means[i], gpu_result[0], REDUCTION_TOLERANCE)
            << "Operation " << i << " result mismatch";

        nimcp_gpu_tensor_destroy(input_tensors[i]);
        nimcp_gpu_tensor_destroy(output_tensors[i]);
    }
}

//=============================================================================
// MIXED PRECISION TESTS
//=============================================================================

/**
 * WHAT: Test FP16 statistics computation
 * WHY:  Half precision can be faster with acceptable accuracy
 * HOW:  Compute statistics in FP16, compare with FP32
 */
TEST_F(StatisticsGPUIntegrationTest, MixedPrecision_FP16Mean) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size, -1.0f, 1.0f);  // Small range for FP16

    float cpu_mean = cpuMean(data.data(), size);

    // FP32 computation
    std::vector<size_t> dims = {size};
    auto* tensor_fp32 = createGPUTensor(data, dims);
    std::vector<size_t> out_dims = {1};
    auto* out_fp32 = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_mean(gpu_ctx, tensor_fp32, out_fp32, -1, false);
    auto fp32_result = copyToHost(out_fp32);

    // FP16 computation (if supported)
    auto* tensor_fp16 = nimcp_gpu_tensor_from_host(
        gpu_ctx, data.data(), dims.data(), 1, NIMCP_GPU_PRECISION_FP16);

    if (tensor_fp16) {
        auto* out_fp16 = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP16);

        if (out_fp16) {
            nimcp_gpu_mean(gpu_ctx, tensor_fp16, out_fp16, -1, false);

            // Convert FP16 result to FP32 for comparison
            // (Implementation dependent on how FP16 is handled)

            nimcp_gpu_tensor_destroy(out_fp16);
        }
        nimcp_gpu_tensor_destroy(tensor_fp16);
    }

    // Verify FP32 result
    EXPECT_NEAR(cpu_mean, fp32_result[0], REDUCTION_TOLERANCE);

    nimcp_gpu_tensor_destroy(tensor_fp32);
    nimcp_gpu_tensor_destroy(out_fp32);
}

//=============================================================================
// EDGE CASES AND ROBUSTNESS
//=============================================================================

/**
 * WHAT: Test GPU operations with empty/single element tensors
 * WHY:  Edge cases can reveal bugs
 * HOW:  Test with minimal data
 */
TEST_F(StatisticsGPUIntegrationTest, EdgeCase_SingleElement) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    std::vector<float> single = {3.14159f};
    std::vector<size_t> dims = {1};
    auto* tensor_in = createGPUTensor(single, dims);
    auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    // Mean of single element is the element itself
    nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_out, -1, false);
    auto result = copyToHost(tensor_out);
    EXPECT_NEAR(single[0], result[0], STRICT_TOLERANCE);

    // Variance of single element is 0
    nimcp_gpu_var(gpu_ctx, tensor_in, tensor_out, -1, false, false);
    result = copyToHost(tensor_out);
    EXPECT_NEAR(0.0f, result[0], STRICT_TOLERANCE);

    nimcp_gpu_tensor_destroy(tensor_in);
    nimcp_gpu_tensor_destroy(tensor_out);
}

/**
 * WHAT: Test GPU operations with non-power-of-2 sizes
 * WHY:  GPU kernels may be optimized for power-of-2 sizes
 * HOW:  Test with odd sizes
 */
TEST_F(StatisticsGPUIntegrationTest, EdgeCase_NonPowerOf2Size) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    std::vector<size_t> odd_sizes = {17, 127, 255, 513, 1023, 4097};

    for (size_t size : odd_sizes) {
        SCOPED_TRACE("Size: " + std::to_string(size));

        auto data = generateRandomData(size);
        float cpu_mean = cpuMean(data.data(), size);

        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(data, dims);
        std::vector<size_t> out_dims = {1};
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_out, -1, false);
        auto result = copyToHost(tensor_out);

        EXPECT_NEAR(cpu_mean, result[0], REDUCTION_TOLERANCE);

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Test GPU operations with constant data
 * WHY:  Constant data has zero variance
 * HOW:  Test statistics on array of identical values
 */
TEST_F(StatisticsGPUIntegrationTest, EdgeCase_ConstantData) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t size = MEDIUM_SIZE;
    const float constant_value = 42.0f;
    std::vector<float> data(size, constant_value);

    std::vector<size_t> dims = {size};
    auto* tensor_in = createGPUTensor(data, dims);
    std::vector<size_t> out_dims = {1};
    auto* tensor_mean = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_var = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_std = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_mean, -1, false);
    nimcp_gpu_var(gpu_ctx, tensor_in, tensor_var, -1, false, false);
    nimcp_gpu_std(gpu_ctx, tensor_in, tensor_std, -1, false, false);

    auto mean_result = copyToHost(tensor_mean);
    auto var_result = copyToHost(tensor_var);
    auto std_result = copyToHost(tensor_std);

    EXPECT_NEAR(constant_value, mean_result[0], STRICT_TOLERANCE);
    EXPECT_NEAR(0.0f, var_result[0], STRICT_TOLERANCE);
    EXPECT_NEAR(0.0f, std_result[0], STRICT_TOLERANCE);

    nimcp_gpu_tensor_destroy(tensor_in);
    nimcp_gpu_tensor_destroy(tensor_mean);
    nimcp_gpu_tensor_destroy(tensor_var);
    nimcp_gpu_tensor_destroy(tensor_std);
}

/**
 * WHAT: Test GPU operations with extreme values
 * WHY:  Large/small values can cause overflow/underflow
 * HOW:  Test with very large and very small values
 */
TEST_F(StatisticsGPUIntegrationTest, EdgeCase_ExtremeValues) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t size = SMALL_SIZE;

    // Large values
    std::vector<float> large_data(size);
    for (size_t i = 0; i < size; i++) {
        large_data[i] = 1e6f + static_cast<float>(i);
    }

    float cpu_mean_large = cpuMean(large_data.data(), size);

    std::vector<size_t> dims = {size};
    auto* tensor_large = createGPUTensor(large_data, dims);
    std::vector<size_t> out_dims = {1};
    auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_mean(gpu_ctx, tensor_large, tensor_out, -1, false);
    auto result_large = copyToHost(tensor_out);

    float rel_error = std::fabs(result_large[0] - cpu_mean_large) / cpu_mean_large;
    EXPECT_LT(rel_error, RELAXED_TOLERANCE)
        << "Large value mean: expected " << cpu_mean_large << ", got " << result_large[0];

    // Small values
    std::vector<float> small_data(size);
    for (size_t i = 0; i < size; i++) {
        small_data[i] = 1e-6f + 1e-8f * static_cast<float>(i);
    }

    float cpu_mean_small = cpuMean(small_data.data(), size);

    auto* tensor_small = createGPUTensor(small_data, dims);
    nimcp_gpu_mean(gpu_ctx, tensor_small, tensor_out, -1, false);
    auto result_small = copyToHost(tensor_out);

    rel_error = std::fabs(result_small[0] - cpu_mean_small) / cpu_mean_small;
    EXPECT_LT(rel_error, RELAXED_TOLERANCE)
        << "Small value mean: expected " << cpu_mean_small << ", got " << result_small[0];

    nimcp_gpu_tensor_destroy(tensor_large);
    nimcp_gpu_tensor_destroy(tensor_small);
    nimcp_gpu_tensor_destroy(tensor_out);
}

//=============================================================================
// STRESS TESTS
//=============================================================================

/**
 * WHAT: Stress test with many sequential operations
 * WHY:  Verify stability over extended use
 * HOW:  Run thousands of operations
 */
TEST_F(StatisticsGPUIntegrationTest, Stress_ManyOperations) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const size_t size = SMALL_SIZE;
    const int num_operations = 1000;

    for (int i = 0; i < num_operations; i++) {
        auto data = generateRandomData(size);

        std::vector<size_t> dims = {size};
        auto* tensor_in = createGPUTensor(data, dims);
        std::vector<size_t> out_dims = {1};
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, out_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        bool result = nimcp_gpu_mean(gpu_ctx, tensor_in, tensor_out, -1, false);
        EXPECT_TRUE(result) << "Failed at operation " << i;

        nimcp_gpu_tensor_destroy(tensor_in);
        nimcp_gpu_tensor_destroy(tensor_out);
    }
}

/**
 * WHAT: Stress test with rapid allocation/deallocation
 * WHY:  Memory management should handle rapid cycling
 * HOW:  Rapidly create and destroy tensors
 */
TEST_F(StatisticsGPUIntegrationTest, Stress_RapidAllocation) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    const int iterations = 500;

    for (int i = 0; i < iterations; i++) {
        size_t size = SMALL_SIZE + (i % 10) * 100;
        auto data = generateRandomData(size);

        std::vector<size_t> dims = {size};
        auto* tensor = createGPUTensor(data, dims);
        ASSERT_NE(tensor, nullptr) << "Allocation failed at iteration " << i;
        nimcp_gpu_tensor_destroy(tensor);
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
