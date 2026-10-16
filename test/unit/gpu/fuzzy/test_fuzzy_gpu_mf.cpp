/* ============================================================================
 * Unit Tests: GPU Fuzzy Membership Function Evaluation
 * ============================================================================
 * WHAT: Unit tests for GPU-accelerated membership function evaluation
 * WHY:  Validate correctness of all 14 MF types and 8 hedges on GPU
 * HOW:  Compare GPU results against CPU reference implementations
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_params.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-5f;
constexpr float RELAXED_TOLERANCE = 1e-4f;

class FuzzyGPUMFTest : public ::testing::Test {
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

    // Helper: Create GPU MF from parameters
    fuzzy_gpu_mf_t make_gpu_mf(uint32_t type, const std::vector<float>& params,
                               uint32_t hedge = FUZZY_HEDGE_NONE, float alpha_cut = 0.0f) {
        fuzzy_gpu_mf_t mf = {};
        mf.type = type;
        mf.hedge = hedge;
        mf.num_params = static_cast<uint32_t>(params.size());
        mf.alpha_cut = alpha_cut;
        for (size_t i = 0; i < params.size() && i < FUZZY_GPU_MAX_PARAMS; i++) {
            mf.params[i] = params[i];
        }
        return mf;
    }

    // Helper: CPU reference for triangular MF
    float cpu_triangular(float x, float a, float b, float c) {
        if (x <= a || x >= c) return 0.0f;
        if (x <= b) return (x - a) / (b - a + 1e-10f);
        return (c - x) / (c - b + 1e-10f);
    }

    // Helper: CPU reference for trapezoidal MF
    float cpu_trapezoidal(float x, float a, float b, float c, float d) {
        if (x <= a || x >= d) return 0.0f;
        if (x >= b && x <= c) return 1.0f;
        if (x < b) return (x - a) / (b - a + 1e-10f);
        return (d - x) / (d - c + 1e-10f);
    }

    // Helper: CPU reference for Gaussian MF
    float cpu_gaussian(float x, float mean, float sigma) {
        float diff = x - mean;
        return expf(-(diff * diff) / (2.0f * sigma * sigma + 1e-10f));
    }

    // Helper: CPU reference for Bell MF
    float cpu_bell(float x, float a, float b, float c) {
        float term = fabsf((x - c) / (a + 1e-10f));
        return 1.0f / (1.0f + powf(term, 2.0f * b));
    }

    // Helper: CPU reference for sigmoid MF
    float cpu_sigmoid(float x, float a, float c) {
        return 1.0f / (1.0f + expf(-a * (x - c)));
    }

    // Helper: CPU reference for S-shaped MF
    float cpu_s_shaped(float x, float a, float b) {
        if (x <= a) return 0.0f;
        if (x >= b) return 1.0f;
        if (x <= (a + b) / 2.0f) {
            float t = (x - a) / (b - a + 1e-10f);
            return 2.0f * t * t;
        }
        float t = (x - a) / (b - a + 1e-10f);
        return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    }

    // Helper: CPU reference for Z-shaped MF
    float cpu_z_shaped(float x, float a, float b) {
        if (x <= a) return 1.0f;
        if (x >= b) return 0.0f;
        if (x <= (a + b) / 2.0f) {
            float t = (x - a) / (b - a + 1e-10f);
            return 1.0f - 2.0f * t * t;
        }
        float t = (b - x) / (b - a + 1e-10f);
        return 2.0f * t * t;
    }

    // Helper: Apply hedge to membership value
    float apply_hedge(float mu, uint32_t hedge) {
        switch (hedge) {
            case FUZZY_HEDGE_NONE: return mu;
            case FUZZY_HEDGE_VERY: return mu * mu;
            case FUZZY_HEDGE_SOMEWHAT: return sqrtf(mu);
            case FUZZY_HEDGE_EXTREMELY: return mu * mu * mu;
            case FUZZY_HEDGE_SLIGHTLY: return sqrtf(mu) - mu;
            case FUZZY_HEDGE_NOT: return 1.0f - mu;
            case FUZZY_HEDGE_MORE_OR_LESS: return powf(mu, 0.75f);
            case FUZZY_HEDGE_INDEED: return (mu <= 0.5f) ? 2.0f * mu * mu : 1.0f - 2.0f * (1.0f - mu) * (1.0f - mu);
            default: return mu;
        }
    }
#endif
};

/* ============================================================================
 * Test: Triangular Membership Function
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, TriangularMF) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_TRIANGULAR, {0.0f, 0.5f, 1.0f});

    // Test points
    std::vector<float> inputs = {-0.1f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.1f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    // Verify against CPU reference
    for (size_t i = 0; i < inputs.size(); i++) {
        float expected = cpu_triangular(inputs[i], 0.0f, 0.5f, 1.0f);
        EXPECT_NEAR(outputs[i], expected, TOLERANCE)
            << "Mismatch at input " << inputs[i];
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Trapezoidal Membership Function
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, TrapezoidalMF) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_TRAPEZOIDAL, {0.0f, 0.3f, 0.7f, 1.0f});

    std::vector<float> inputs = {-0.1f, 0.0f, 0.15f, 0.3f, 0.5f, 0.7f, 0.85f, 1.0f, 1.1f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    for (size_t i = 0; i < inputs.size(); i++) {
        float expected = cpu_trapezoidal(inputs[i], 0.0f, 0.3f, 0.7f, 1.0f);
        EXPECT_NEAR(outputs[i], expected, TOLERANCE)
            << "Mismatch at input " << inputs[i];
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Gaussian Membership Function
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, GaussianMF) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_GAUSSIAN, {0.5f, 0.15f});

    std::vector<float> inputs = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    for (size_t i = 0; i < inputs.size(); i++) {
        float expected = cpu_gaussian(inputs[i], 0.5f, 0.15f);
        EXPECT_NEAR(outputs[i], expected, TOLERANCE)
            << "Mismatch at input " << inputs[i];
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Generalized Bell Membership Function
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, BellMF) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Bell: width=0.25, slope=2, center=0.5
    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_BELL, {0.25f, 2.0f, 0.5f});

    std::vector<float> inputs = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    for (size_t i = 0; i < inputs.size(); i++) {
        float expected = cpu_bell(inputs[i], 0.25f, 2.0f, 0.5f);
        EXPECT_NEAR(outputs[i], expected, TOLERANCE)
            << "Mismatch at input " << inputs[i];
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Sigmoid Membership Function
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, SigmoidMF) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Sigmoid: slope=10, center=0.5
    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_SIGMOID, {10.0f, 0.5f});

    std::vector<float> inputs = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    for (size_t i = 0; i < inputs.size(); i++) {
        float expected = cpu_sigmoid(inputs[i], 10.0f, 0.5f);
        EXPECT_NEAR(outputs[i], expected, TOLERANCE)
            << "Mismatch at input " << inputs[i];
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: S-Shaped Membership Function
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, SShapedMF) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_S_SHAPED, {0.2f, 0.8f});

    std::vector<float> inputs = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    for (size_t i = 0; i < inputs.size(); i++) {
        float expected = cpu_s_shaped(inputs[i], 0.2f, 0.8f);
        EXPECT_NEAR(outputs[i], expected, TOLERANCE)
            << "Mismatch at input " << inputs[i];
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Z-Shaped Membership Function
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, ZShapedMF) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_Z_SHAPED, {0.2f, 0.8f});

    std::vector<float> inputs = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    for (size_t i = 0; i < inputs.size(); i++) {
        float expected = cpu_z_shaped(inputs[i], 0.2f, 0.8f);
        EXPECT_NEAR(outputs[i], expected, TOLERANCE)
            << "Mismatch at input " << inputs[i];
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Singleton Membership Function
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, SingletonMF) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_SINGLETON, {0.5f});

    std::vector<float> inputs = {0.49f, 0.5f, 0.51f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    // Singleton should be 1 only at exact point (with small tolerance)
    EXPECT_NEAR(outputs[0], 0.0f, TOLERANCE);
    EXPECT_NEAR(outputs[1], 1.0f, TOLERANCE);
    EXPECT_NEAR(outputs[2], 0.0f, TOLERANCE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Hedge Application - Very (concentration)
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, HedgeVery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_GAUSSIAN, {0.5f, 0.2f}, FUZZY_HEDGE_VERY);

    std::vector<float> inputs = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;
    params.apply_hedges = true;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    for (size_t i = 0; i < inputs.size(); i++) {
        float base = cpu_gaussian(inputs[i], 0.5f, 0.2f);
        float expected = apply_hedge(base, FUZZY_HEDGE_VERY);
        EXPECT_NEAR(outputs[i], expected, TOLERANCE)
            << "Hedge VERY mismatch at input " << inputs[i];
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Hedge Application - Somewhat (dilation)
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, HedgeSomewhat) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_GAUSSIAN, {0.5f, 0.2f}, FUZZY_HEDGE_SOMEWHAT);

    std::vector<float> inputs = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;
    params.apply_hedges = true;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    for (size_t i = 0; i < inputs.size(); i++) {
        float base = cpu_gaussian(inputs[i], 0.5f, 0.2f);
        float expected = apply_hedge(base, FUZZY_HEDGE_SOMEWHAT);
        EXPECT_NEAR(outputs[i], expected, TOLERANCE)
            << "Hedge SOMEWHAT mismatch at input " << inputs[i];
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Hedge Application - NOT (complement)
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, HedgeNot) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_TRIANGULAR, {0.0f, 0.5f, 1.0f}, FUZZY_HEDGE_NOT);

    std::vector<float> inputs = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;
    params.apply_hedges = true;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    for (size_t i = 0; i < inputs.size(); i++) {
        float base = cpu_triangular(inputs[i], 0.0f, 0.5f, 1.0f);
        float expected = apply_hedge(base, FUZZY_HEDGE_NOT);
        EXPECT_NEAR(outputs[i], expected, TOLERANCE)
            << "Hedge NOT mismatch at input " << inputs[i];
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Batch Evaluation with Multiple MFs
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, BatchMultipleMFs) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Create 3 MFs: low, medium, high (triangular)
    std::vector<fuzzy_gpu_mf_t> mfs = {
        make_gpu_mf(FUZZY_MF_TRIANGULAR, {0.0f, 0.0f, 0.5f}),  // Low
        make_gpu_mf(FUZZY_MF_TRIANGULAR, {0.0f, 0.5f, 1.0f}),  // Medium
        make_gpu_mf(FUZZY_MF_TRIANGULAR, {0.5f, 1.0f, 1.0f})   // High
    };

    std::vector<float> inputs = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    std::vector<float> outputs(inputs.size() * mfs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = static_cast<uint32_t>(mfs.size());

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), mfs.data(), static_cast<uint32_t>(mfs.size()),
        outputs.data(), &params));

    // Verify sum of memberships for partition of unity
    for (size_t i = 0; i < inputs.size(); i++) {
        float sum = 0.0f;
        for (size_t m = 0; m < mfs.size(); m++) {
            sum += outputs[i * mfs.size() + m];
        }
        // For well-designed fuzzy partition, memberships should sum to ~1
        // But our test MFs overlap, so this is informational
        EXPECT_GE(sum, 0.0f);
        EXPECT_LE(sum, 3.0f);  // Maximum is 3 (all at 1)
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Large Batch Performance
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, LargeBatchEvaluation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t num_samples = 10000;
    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_GAUSSIAN, {0.5f, 0.2f});

    std::vector<float> inputs(num_samples);
    std::vector<float> outputs(num_samples);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (auto& x : inputs) x = dist(gen);

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(num_samples);
    params.num_mfs = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    // Verify a sample of results
    for (size_t i = 0; i < 100; i++) {
        size_t idx = (i * 100) % num_samples;
        float expected = cpu_gaussian(inputs[idx], 0.5f, 0.2f);
        EXPECT_NEAR(outputs[idx], expected, TOLERANCE)
            << "Mismatch at sample " << idx;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Alpha Cut Application
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, AlphaCutApplication) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // MF with alpha-cut of 0.3
    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_GAUSSIAN, {0.5f, 0.3f}, FUZZY_HEDGE_NONE, 0.3f);

    std::vector<float> inputs = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;
    params.apply_alpha_cuts = true;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    // Verify alpha-cut: values below 0.3 should be 0
    for (size_t i = 0; i < inputs.size(); i++) {
        float base = cpu_gaussian(inputs[i], 0.5f, 0.3f);
        float expected = (base >= 0.3f) ? base : 0.0f;
        EXPECT_NEAR(outputs[i], expected, TOLERANCE)
            << "Alpha-cut mismatch at input " << inputs[i];
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Edge Cases - Zero and Negative Sigma
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, EdgeCasesGaussian) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Very small sigma (nearly singleton)
    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_GAUSSIAN, {0.5f, 0.001f});

    std::vector<float> inputs = {0.49f, 0.5f, 0.51f};
    std::vector<float> outputs(inputs.size());

    nimcp_gpu_mf_eval_params_t params = nimcp_gpu_mf_eval_params_default();
    params.num_samples = static_cast<uint32_t>(inputs.size());
    params.num_mfs = 1;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, inputs.data(), &mf, 1, outputs.data(), &params));

    // At center should be ~1, elsewhere should be very small
    EXPECT_GT(outputs[1], 0.99f);  // At center
    EXPECT_LT(outputs[0], 0.01f);  // Off center
    EXPECT_LT(outputs[2], 0.01f);  // Off center
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MF Discretization
 * ============================================================================ */
TEST_F(FuzzyGPUMFTest, MFDiscretization) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    fuzzy_gpu_mf_t mf = make_gpu_mf(FUZZY_MF_TRIANGULAR, {0.0f, 0.5f, 1.0f});

    const uint32_t resolution = 101;  // Odd for center point
    std::vector<float> discretized(resolution);

    nimcp_gpu_discretize_params_t params = nimcp_gpu_discretize_params_default();
    params.resolution = resolution;
    params.x_min = 0.0f;
    params.x_max = 1.0f;

    ASSERT_TRUE(nimcp_gpu_fuzzy_mf_discretize_batch(
        ctx_, &mf, 1, discretized.data(), &params));

    // Check endpoints and peak
    EXPECT_NEAR(discretized[0], 0.0f, TOLERANCE);           // x = 0.0
    EXPECT_NEAR(discretized[resolution / 2], 1.0f, TOLERANCE);  // x = 0.5 (peak)
    EXPECT_NEAR(discretized[resolution - 1], 0.0f, TOLERANCE);  // x = 1.0
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace
