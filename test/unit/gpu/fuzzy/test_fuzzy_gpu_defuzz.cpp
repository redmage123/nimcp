/* ============================================================================
 * Unit Tests: GPU Fuzzy Defuzzification Methods
 * ============================================================================
 * WHAT: Unit tests for GPU-accelerated defuzzification
 * WHY:  Validate correctness of all 7 defuzzification methods on GPU
 * HOW:  Compare GPU results against CPU reference implementations
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_params.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;
constexpr float RELAXED_TOLERANCE = 1e-3f;

class FuzzyGPUDefuzzTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(0);
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = NULL;
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = NULL;

    // Helper: Create triangular fuzzy output for testing
    std::vector<float> createTriangularOutput(uint32_t resolution, float peak_pos, float peak_val) {
        std::vector<float> output(resolution, 0.0f);
        for (uint32_t i = 0; i < resolution; i++) {
            float x = static_cast<float>(i) / (resolution - 1);
            if (x <= peak_pos) {
                output[i] = peak_val * (x / peak_pos);
            } else {
                output[i] = peak_val * (1.0f - x) / (1.0f - peak_pos);
            }
            output[i] = std::max(0.0f, output[i]);
        }
        return output;
    }

    // Helper: Create uniform fuzzy output
    std::vector<float> createUniformOutput(uint32_t resolution, float left, float right, float height) {
        std::vector<float> output(resolution, 0.0f);
        for (uint32_t i = 0; i < resolution; i++) {
            float x = static_cast<float>(i) / (resolution - 1);
            if (x >= left && x <= right) {
                output[i] = height;
            }
        }
        return output;
    }

    // CPU reference: Centroid defuzzification
    float cpu_centroid(const std::vector<float>& values, float x_min, float x_max) {
        float numerator = 0.0f;
        float denominator = 0.0f;
        float dx = (x_max - x_min) / (values.size() - 1);

        for (size_t i = 0; i < values.size(); i++) {
            float x = x_min + i * dx;
            numerator += x * values[i];
            denominator += values[i];
        }

        if (denominator < 1e-10f) return (x_min + x_max) / 2.0f;
        return numerator / denominator;
    }

    // CPU reference: Bisector defuzzification
    float cpu_bisector(const std::vector<float>& values, float x_min, float x_max) {
        float total_area = std::accumulate(values.begin(), values.end(), 0.0f);
        float half_area = total_area / 2.0f;
        float cumulative = 0.0f;
        float dx = (x_max - x_min) / (values.size() - 1);

        for (size_t i = 0; i < values.size(); i++) {
            cumulative += values[i];
            if (cumulative >= half_area) {
                return x_min + i * dx;
            }
        }
        return x_max;
    }

    // CPU reference: Mean of Maximum (MOM)
    float cpu_mom(const std::vector<float>& values, float x_min, float x_max) {
        float max_val = *std::max_element(values.begin(), values.end());
        if (max_val < 1e-10f) return (x_min + x_max) / 2.0f;

        float sum_x = 0.0f;
        int count = 0;
        float dx = (x_max - x_min) / (values.size() - 1);

        for (size_t i = 0; i < values.size(); i++) {
            if (std::abs(values[i] - max_val) < 1e-6f) {
                sum_x += x_min + i * dx;
                count++;
            }
        }

        return (count > 0) ? sum_x / count : (x_min + x_max) / 2.0f;
    }

    // CPU reference: Smallest of Maximum (SOM)
    float cpu_som(const std::vector<float>& values, float x_min, float x_max) {
        float max_val = *std::max_element(values.begin(), values.end());
        if (max_val < 1e-10f) return x_min;

        float dx = (x_max - x_min) / (values.size() - 1);

        for (size_t i = 0; i < values.size(); i++) {
            if (std::abs(values[i] - max_val) < 1e-6f) {
                return x_min + i * dx;
            }
        }
        return x_min;
    }

    // CPU reference: Largest of Maximum (LOM)
    float cpu_lom(const std::vector<float>& values, float x_min, float x_max) {
        float max_val = *std::max_element(values.begin(), values.end());
        if (max_val < 1e-10f) return x_max;

        float dx = (x_max - x_min) / (values.size() - 1);

        for (int i = static_cast<int>(values.size()) - 1; i >= 0; i--) {
            if (std::abs(values[i] - max_val) < 1e-6f) {
                return x_min + i * dx;
            }
        }
        return x_max;
    }

    // CPU reference: Weighted Average (for Sugeno-style)
    float cpu_weighted_average(const std::vector<float>& weights, const std::vector<float>& values) {
        float numerator = 0.0f;
        float denominator = 0.0f;

        for (size_t i = 0; i < weights.size() && i < values.size(); i++) {
            numerator += weights[i] * values[i];
            denominator += weights[i];
        }

        if (denominator < 1e-10f) return 0.0f;
        return numerator / denominator;
    }
#endif
};

/* ============================================================================
 * Test: Centroid Defuzzification
 * ============================================================================ */
TEST_F(FuzzyGPUDefuzzTest, CentroidMethod) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t resolution = 256;
    const float x_min = 0.0f;
    const float x_max = 10.0f;

    // Create triangular output with peak at 0.6 (normalized), height 1.0
    auto aggregated = createTriangularOutput(resolution, 0.6f, 1.0f);

    std::vector<float> output(1);

    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_CENTROID;
    params.resolution = resolution;
    params.x_min = x_min;
    params.x_max = x_max;
    params.num_samples = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_defuzzify_batch(
        ctx_, aggregated.data(), output.data(), &params));

    float expected = cpu_centroid(aggregated, x_min, x_max);
    EXPECT_NEAR(output[0], expected, RELAXED_TOLERANCE)
        << "Centroid defuzzification mismatch";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Bisector Defuzzification
 * ============================================================================ */
TEST_F(FuzzyGPUDefuzzTest, BisectorMethod) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t resolution = 256;
    const float x_min = 0.0f;
    const float x_max = 10.0f;

    // Asymmetric output for more interesting bisector
    auto aggregated = createTriangularOutput(resolution, 0.3f, 1.0f);

    std::vector<float> output(1);

    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_BISECTOR;
    params.resolution = resolution;
    params.x_min = x_min;
    params.x_max = x_max;
    params.num_samples = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_defuzzify_batch(
        ctx_, aggregated.data(), output.data(), &params));

    float expected = cpu_bisector(aggregated, x_min, x_max);
    EXPECT_NEAR(output[0], expected, RELAXED_TOLERANCE)
        << "Bisector defuzzification mismatch";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Mean of Maximum (MOM) Defuzzification
 * ============================================================================ */
TEST_F(FuzzyGPUDefuzzTest, MeanOfMaximumMethod) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t resolution = 256;
    const float x_min = 0.0f;
    const float x_max = 10.0f;

    // Uniform plateau for clear MOM test
    auto aggregated = createUniformOutput(resolution, 0.3f, 0.7f, 1.0f);

    std::vector<float> output(1);

    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_MOM;
    params.resolution = resolution;
    params.x_min = x_min;
    params.x_max = x_max;
    params.num_samples = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_defuzzify_batch(
        ctx_, aggregated.data(), output.data(), &params));

    float expected = cpu_mom(aggregated, x_min, x_max);
    EXPECT_NEAR(output[0], expected, RELAXED_TOLERANCE)
        << "MOM defuzzification mismatch";

    // MOM should be approximately center of plateau (5.0 for [3,7] on [0,10])
    EXPECT_NEAR(output[0], 5.0f, 0.5f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Smallest of Maximum (SOM) Defuzzification
 * ============================================================================ */
TEST_F(FuzzyGPUDefuzzTest, SmallestOfMaximumMethod) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t resolution = 256;
    const float x_min = 0.0f;
    const float x_max = 10.0f;

    // Uniform plateau for SOM test
    auto aggregated = createUniformOutput(resolution, 0.3f, 0.7f, 1.0f);

    std::vector<float> output(1);

    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_SOM;
    params.resolution = resolution;
    params.x_min = x_min;
    params.x_max = x_max;
    params.num_samples = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_defuzzify_batch(
        ctx_, aggregated.data(), output.data(), &params));

    float expected = cpu_som(aggregated, x_min, x_max);
    EXPECT_NEAR(output[0], expected, RELAXED_TOLERANCE)
        << "SOM defuzzification mismatch";

    // SOM should be left edge of plateau (~3.0)
    EXPECT_NEAR(output[0], 3.0f, 0.5f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Largest of Maximum (LOM) Defuzzification
 * ============================================================================ */
TEST_F(FuzzyGPUDefuzzTest, LargestOfMaximumMethod) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t resolution = 256;
    const float x_min = 0.0f;
    const float x_max = 10.0f;

    // Uniform plateau for LOM test
    auto aggregated = createUniformOutput(resolution, 0.3f, 0.7f, 1.0f);

    std::vector<float> output(1);

    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_LOM;
    params.resolution = resolution;
    params.x_min = x_min;
    params.x_max = x_max;
    params.num_samples = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_defuzzify_batch(
        ctx_, aggregated.data(), output.data(), &params));

    float expected = cpu_lom(aggregated, x_min, x_max);
    EXPECT_NEAR(output[0], expected, RELAXED_TOLERANCE)
        << "LOM defuzzification mismatch";

    // LOM should be right edge of plateau (~7.0)
    EXPECT_NEAR(output[0], 7.0f, 0.5f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Batch Defuzzification
 * ============================================================================ */
TEST_F(FuzzyGPUDefuzzTest, BatchDefuzzification) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t resolution = 128;
    const uint32_t num_samples = 10;
    const float x_min = 0.0f;
    const float x_max = 10.0f;

    // Create multiple outputs with different peaks
    std::vector<float> aggregated(num_samples * resolution);
    std::vector<float> expected(num_samples);

    for (uint32_t s = 0; s < num_samples; s++) {
        float peak_pos = 0.1f + 0.8f * s / (num_samples - 1);  // [0.1, 0.9]
        auto sample_output = createTriangularOutput(resolution, peak_pos, 1.0f);
        std::copy(sample_output.begin(), sample_output.end(),
                  aggregated.begin() + s * resolution);
        expected[s] = cpu_centroid(sample_output, x_min, x_max);
    }

    std::vector<float> output(num_samples);

    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_CENTROID;
    params.resolution = resolution;
    params.x_min = x_min;
    params.x_max = x_max;
    params.num_samples = num_samples;

    ASSERT_TRUE(nimcp_gpu_fuzzy_defuzzify_batch(
        ctx_, aggregated.data(), output.data(), &params));

    for (uint32_t s = 0; s < num_samples; s++) {
        EXPECT_NEAR(output[s], expected[s], RELAXED_TOLERANCE)
            << "Batch defuzz mismatch at sample " << s;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Multi-Method Defuzzification
 * ============================================================================ */
TEST_F(FuzzyGPUDefuzzTest, MultiMethodDefuzzification) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t resolution = 256;
    const float x_min = 0.0f;
    const float x_max = 10.0f;

    // Create a test output
    auto aggregated = createUniformOutput(resolution, 0.3f, 0.7f, 1.0f);

    // Apply multiple methods
    std::vector<uint32_t> methods = {
        FUZZY_DEFUZZ_CENTROID,
        FUZZY_DEFUZZ_BISECTOR,
        FUZZY_DEFUZZ_MOM,
        FUZZY_DEFUZZ_SOM,
        FUZZY_DEFUZZ_LOM
    };

    std::vector<float> outputs(methods.size());

    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.resolution = resolution;
    params.x_min = x_min;
    params.x_max = x_max;
    params.num_samples = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_defuzzify_multi_method(
        ctx_, aggregated.data(), methods.data(),
        static_cast<uint32_t>(methods.size()), outputs.data(), &params));

    // Verify ordering: SOM < MOM < LOM for uniform plateau
    EXPECT_LT(outputs[3], outputs[2]);  // SOM < MOM
    EXPECT_LT(outputs[2], outputs[4]);  // MOM < LOM

    // Verify reasonable values
    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_GE(outputs[i], x_min);
        EXPECT_LE(outputs[i], x_max);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Empty Fuzzy Set Handling
 * ============================================================================ */
TEST_F(FuzzyGPUDefuzzTest, EmptyFuzzySetHandling) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t resolution = 128;
    const float x_min = 0.0f;
    const float x_max = 10.0f;

    // All zeros
    std::vector<float> aggregated(resolution, 0.0f);
    std::vector<float> output(1);

    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_CENTROID;
    params.resolution = resolution;
    params.x_min = x_min;
    params.x_max = x_max;
    params.num_samples = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_defuzzify_batch(
        ctx_, aggregated.data(), output.data(), &params));

    // Should return center of universe for empty set
    EXPECT_NEAR(output[0], (x_min + x_max) / 2.0f, RELAXED_TOLERANCE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: High Resolution Defuzzification
 * ============================================================================ */
TEST_F(FuzzyGPUDefuzzTest, HighResolutionDefuzz) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const uint32_t resolution = 1024;  // Higher resolution
    const float x_min = 0.0f;
    const float x_max = 10.0f;

    auto aggregated = createTriangularOutput(resolution, 0.4f, 1.0f);
    std::vector<float> output(1);

    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_CENTROID;
    params.resolution = resolution;
    params.x_min = x_min;
    params.x_max = x_max;
    params.num_samples = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_defuzzify_batch(
        ctx_, aggregated.data(), output.data(), &params));

    float expected = cpu_centroid(aggregated, x_min, x_max);
    EXPECT_NEAR(output[0], expected, TOLERANCE)
        << "High resolution defuzz mismatch";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Parallel Reduction Accuracy
 * ============================================================================ */
TEST_F(FuzzyGPUDefuzzTest, ParallelReductionAccuracy) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Test with size that requires multi-level reduction
    const uint32_t resolution = 512;
    const float x_min = 0.0f;
    const float x_max = 100.0f;

    // Create a known distribution
    std::vector<float> aggregated(resolution);
    float dx = (x_max - x_min) / (resolution - 1);
    for (uint32_t i = 0; i < resolution; i++) {
        float x = x_min + i * dx;
        // Gaussian centered at 50
        aggregated[i] = expf(-(x - 50.0f) * (x - 50.0f) / (2.0f * 10.0f * 10.0f));
    }

    std::vector<float> output(1);

    nimcp_gpu_defuzz_params_t params = nimcp_gpu_defuzz_params_default();
    params.method = FUZZY_DEFUZZ_CENTROID;
    params.resolution = resolution;
    params.x_min = x_min;
    params.x_max = x_max;
    params.num_samples = 1;
    params.use_parallel_reduction = true;

    ASSERT_TRUE(nimcp_gpu_fuzzy_defuzzify_batch(
        ctx_, aggregated.data(), output.data(), &params));

    // Centroid of symmetric Gaussian should be at center
    EXPECT_NEAR(output[0], 50.0f, 0.5f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace
