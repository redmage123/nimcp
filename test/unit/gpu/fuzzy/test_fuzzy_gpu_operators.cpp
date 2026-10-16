/* ============================================================================
 * Unit Tests: GPU Fuzzy Operators
 * ============================================================================
 * WHAT: Unit tests for GPU-accelerated fuzzy operators
 * WHY:  Validate T-norms, T-conorms, complements, and implications on GPU
 * HOW:  Compare GPU results against CPU reference implementations
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_params.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/fuzzy/nimcp_fuzzy_operators.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-5f;
constexpr float RELAXED_TOLERANCE = 1e-4f;

class FuzzyGPUOperatorsTest : public ::testing::Test {
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

    // CPU reference T-norms
    float cpu_tnorm_min(float a, float b) { return std::min(a, b); }
    float cpu_tnorm_product(float a, float b) { return a * b; }
    float cpu_tnorm_lukasiewicz(float a, float b) { return std::max(0.0f, a + b - 1.0f); }
    float cpu_tnorm_drastic(float a, float b) {
        if (a == 1.0f) return b;
        if (b == 1.0f) return a;
        return 0.0f;
    }
    float cpu_tnorm_nilpotent(float a, float b) {
        return (a + b > 1.0f) ? std::min(a, b) : 0.0f;
    }
    float cpu_tnorm_hamacher(float a, float b, float gamma = 0.0f) {
        if (a == 0.0f && b == 0.0f) return 0.0f;
        return (a * b) / (gamma + (1.0f - gamma) * (a + b - a * b) + 1e-10f);
    }

    // CPU reference T-conorms
    float cpu_tconorm_max(float a, float b) { return std::max(a, b); }
    float cpu_tconorm_probabilistic(float a, float b) { return a + b - a * b; }
    float cpu_tconorm_bounded(float a, float b) { return std::min(1.0f, a + b); }
    float cpu_tconorm_drastic(float a, float b) {
        if (a == 0.0f) return b;
        if (b == 0.0f) return a;
        return 1.0f;
    }
    float cpu_tconorm_nilpotent(float a, float b) {
        return (a + b < 1.0f) ? std::max(a, b) : 1.0f;
    }

    // CPU reference complements
    float cpu_complement_standard(float a) { return 1.0f - a; }
    float cpu_complement_sugeno(float a, float lambda) {
        return (1.0f - a) / (1.0f + lambda * a);
    }
    float cpu_complement_yager(float a, float w) {
        return powf(1.0f - powf(a, w), 1.0f / w);
    }

    // CPU reference implications
    float cpu_impl_mamdani(float ant, float con) { return std::min(ant, con); }
    float cpu_impl_larsen(float ant, float con) { return ant * con; }
    float cpu_impl_godel(float ant, float con) { return (ant <= con) ? 1.0f : con; }
    float cpu_impl_zadeh(float ant, float con) {
        return std::max(std::min(ant, con), 1.0f - ant);
    }
    float cpu_impl_lukasiewicz(float ant, float con) {
        return std::min(1.0f, 1.0f - ant + con);
    }
#endif
};

/* ============================================================================
 * Test: T-norm Min (Zadeh AND)
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, TnormMin) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 1000;
    std::vector<float> a(n), b(n), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
        b[i] = dist(gen);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_tnorm_batch(
        ctx_, a.data(), b.data(), result.data(), FUZZY_TNORM_MIN, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        float expected = cpu_tnorm_min(a[i], b[i]);
        EXPECT_NEAR(result[i], expected, TOLERANCE)
            << "T-norm MIN mismatch at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: T-norm Product (Algebraic AND)
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, TnormProduct) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 1000;
    std::vector<float> a(n), b(n), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
        b[i] = dist(gen);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_tnorm_batch(
        ctx_, a.data(), b.data(), result.data(), FUZZY_TNORM_ALGEBRAIC_PRODUCT, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        float expected = cpu_tnorm_product(a[i], b[i]);
        EXPECT_NEAR(result[i], expected, TOLERANCE)
            << "T-norm PRODUCT mismatch at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: T-norm Lukasiewicz (Bounded AND)
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, TnormLukasiewicz) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 1000;
    std::vector<float> a(n), b(n), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
        b[i] = dist(gen);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_tnorm_batch(
        ctx_, a.data(), b.data(), result.data(), FUZZY_TNORM_LUKASIEWICZ, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        float expected = cpu_tnorm_lukasiewicz(a[i], b[i]);
        EXPECT_NEAR(result[i], expected, TOLERANCE)
            << "T-norm LUKASIEWICZ mismatch at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: T-norm Drastic
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, TnormDrastic) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Test specific cases for drastic T-norm
    std::vector<float> a = {1.0f, 0.5f, 0.0f, 1.0f, 0.7f};
    std::vector<float> b = {0.3f, 1.0f, 0.5f, 1.0f, 0.8f};
    std::vector<float> result(a.size());

    ASSERT_TRUE(nimcp_gpu_fuzzy_tnorm_batch(
        ctx_, a.data(), b.data(), result.data(), FUZZY_TNORM_DRASTIC, static_cast<uint32_t>(a.size())));

    for (size_t i = 0; i < a.size(); i++) {
        float expected = cpu_tnorm_drastic(a[i], b[i]);
        EXPECT_NEAR(result[i], expected, TOLERANCE)
            << "T-norm DRASTIC mismatch at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: T-conorm Max (Zadeh OR)
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, TconormMax) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 1000;
    std::vector<float> a(n), b(n), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
        b[i] = dist(gen);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_tconorm_batch(
        ctx_, a.data(), b.data(), result.data(), FUZZY_TCONORM_MAX, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        float expected = cpu_tconorm_max(a[i], b[i]);
        EXPECT_NEAR(result[i], expected, TOLERANCE)
            << "T-conorm MAX mismatch at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: T-conorm Probabilistic Sum
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, TconormProbabilistic) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 1000;
    std::vector<float> a(n), b(n), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
        b[i] = dist(gen);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_tconorm_batch(
        ctx_, a.data(), b.data(), result.data(), FUZZY_TCONORM_ALGEBRAIC_SUM, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        float expected = cpu_tconorm_probabilistic(a[i], b[i]);
        EXPECT_NEAR(result[i], expected, TOLERANCE)
            << "T-conorm PROBABILISTIC mismatch at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: T-conorm Bounded Sum
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, TconormBounded) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 1000;
    std::vector<float> a(n), b(n), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
        b[i] = dist(gen);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_tconorm_batch(
        ctx_, a.data(), b.data(), result.data(), FUZZY_TCONORM_LUKASIEWICZ, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        float expected = cpu_tconorm_bounded(a[i], b[i]);
        EXPECT_NEAR(result[i], expected, TOLERANCE)
            << "T-conorm BOUNDED mismatch at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Standard Complement (NOT)
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, ComplementStandard) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 1000;
    std::vector<float> a(n), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_complement_batch(
        ctx_, a.data(), result.data(), FUZZY_COMPLEMENT_STANDARD, 0.0f, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        float expected = cpu_complement_standard(a[i]);
        EXPECT_NEAR(result[i], expected, TOLERANCE)
            << "Complement STANDARD mismatch at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Sugeno Complement
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, ComplementSugeno) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 100;
    const float lambda = 0.5f;
    std::vector<float> a(n), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_complement_batch(
        ctx_, a.data(), result.data(), FUZZY_COMPLEMENT_SUGENO, lambda, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        float expected = cpu_complement_sugeno(a[i], lambda);
        EXPECT_NEAR(result[i], expected, RELAXED_TOLERANCE)
            << "Complement SUGENO mismatch at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Implication Mamdani (min)
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, ImplicationMamdani) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 1000;
    std::vector<float> antecedent(n), consequent(n), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        antecedent[i] = dist(gen);
        consequent[i] = dist(gen);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_implication_batch(
        ctx_, antecedent.data(), consequent.data(), result.data(),
        FUZZY_IMPL_MAMDANI, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        float expected = cpu_impl_mamdani(antecedent[i], consequent[i]);
        EXPECT_NEAR(result[i], expected, TOLERANCE)
            << "Implication MAMDANI mismatch at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Implication Larsen (product)
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, ImplicationLarsen) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 1000;
    std::vector<float> antecedent(n), consequent(n), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        antecedent[i] = dist(gen);
        consequent[i] = dist(gen);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_implication_batch(
        ctx_, antecedent.data(), consequent.data(), result.data(),
        FUZZY_IMPL_LARSEN, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        float expected = cpu_impl_larsen(antecedent[i], consequent[i]);
        EXPECT_NEAR(result[i], expected, TOLERANCE)
            << "Implication LARSEN mismatch at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Batch Aggregation
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, BatchAggregation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Multiple arrays of varying lengths
    std::vector<float> values = {
        0.2f, 0.5f, 0.8f,           // Array 1: 3 elements
        0.1f, 0.9f,                  // Array 2: 2 elements
        0.3f, 0.4f, 0.5f, 0.6f      // Array 3: 4 elements
    };
    std::vector<uint32_t> lengths = {3, 2, 4};
    std::vector<float> results(lengths.size());

    ASSERT_TRUE(nimcp_gpu_fuzzy_aggregate_batch(
        ctx_, values.data(), lengths.data(), results.data(),
        FUZZY_AGG_MAX, static_cast<uint32_t>(lengths.size())));

    // Verify max aggregation
    EXPECT_NEAR(results[0], 0.8f, TOLERANCE);  // max(0.2, 0.5, 0.8)
    EXPECT_NEAR(results[1], 0.9f, TOLERANCE);  // max(0.1, 0.9)
    EXPECT_NEAR(results[2], 0.6f, TOLERANCE);  // max(0.3, 0.4, 0.5, 0.6)
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Operator Properties - T-norm Commutativity
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, TnormCommutativity) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 100;
    std::vector<float> a(n), b(n), ab_result(n), ba_result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
        b[i] = dist(gen);
    }

    // Test commutativity: T(a,b) = T(b,a)
    ASSERT_TRUE(nimcp_gpu_fuzzy_tnorm_batch(
        ctx_, a.data(), b.data(), ab_result.data(), FUZZY_TNORM_ALGEBRAIC_PRODUCT, static_cast<uint32_t>(n)));
    ASSERT_TRUE(nimcp_gpu_fuzzy_tnorm_batch(
        ctx_, b.data(), a.data(), ba_result.data(), FUZZY_TNORM_ALGEBRAIC_PRODUCT, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(ab_result[i], ba_result[i], TOLERANCE)
            << "T-norm not commutative at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Operator Properties - T-norm Identity
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, TnormIdentity) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 100;
    std::vector<float> a(n), ones(n, 1.0f), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
    }

    // Test identity: T(a, 1) = a
    ASSERT_TRUE(nimcp_gpu_fuzzy_tnorm_batch(
        ctx_, a.data(), ones.data(), result.data(), FUZZY_TNORM_MIN, static_cast<uint32_t>(n)));

    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(result[i], a[i], TOLERANCE)
            << "T-norm identity failed at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Large Batch Performance
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, LargeBatchPerformance) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 100000;
    std::vector<float> a(n), b(n), result(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
        b[i] = dist(gen);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_tnorm_batch(
        ctx_, a.data(), b.data(), result.data(), FUZZY_TNORM_ALGEBRAIC_PRODUCT, static_cast<uint32_t>(n)));

    // Verify a sample
    for (size_t i = 0; i < 100; i++) {
        size_t idx = (i * 1000) % n;
        float expected = cpu_tnorm_product(a[idx], b[idx]);
        EXPECT_NEAR(result[idx], expected, TOLERANCE);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: De Morgan's Laws
 * ============================================================================ */
TEST_F(FuzzyGPUOperatorsTest, DeMorgansLaws) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t n = 100;
    std::vector<float> a(n), b(n);
    std::vector<float> tnorm_result(n), tconorm_result(n);
    std::vector<float> not_a(n), not_b(n), demorgan(n);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(gen);
        b[i] = dist(gen);
    }

    // For standard complement and min/max:
    // NOT(a AND b) = (NOT a) OR (NOT b)
    // NOT(min(a,b)) = max(1-a, 1-b)

    // Compute min(a, b)
    ASSERT_TRUE(nimcp_gpu_fuzzy_tnorm_batch(
        ctx_, a.data(), b.data(), tnorm_result.data(), FUZZY_TNORM_MIN, static_cast<uint32_t>(n)));

    // Compute NOT(min(a,b))
    ASSERT_TRUE(nimcp_gpu_fuzzy_complement_batch(
        ctx_, tnorm_result.data(), demorgan.data(), FUZZY_COMPLEMENT_STANDARD, 0.0f, static_cast<uint32_t>(n)));

    // Compute NOT a and NOT b
    ASSERT_TRUE(nimcp_gpu_fuzzy_complement_batch(
        ctx_, a.data(), not_a.data(), FUZZY_COMPLEMENT_STANDARD, 0.0f, static_cast<uint32_t>(n)));
    ASSERT_TRUE(nimcp_gpu_fuzzy_complement_batch(
        ctx_, b.data(), not_b.data(), FUZZY_COMPLEMENT_STANDARD, 0.0f, static_cast<uint32_t>(n)));

    // Compute max(NOT a, NOT b)
    ASSERT_TRUE(nimcp_gpu_fuzzy_tconorm_batch(
        ctx_, not_a.data(), not_b.data(), tconorm_result.data(), FUZZY_TCONORM_MAX, static_cast<uint32_t>(n)));

    // Verify De Morgan's law
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(demorgan[i], tconorm_result[i], TOLERANCE)
            << "De Morgan's law failed at index " << i;
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace
