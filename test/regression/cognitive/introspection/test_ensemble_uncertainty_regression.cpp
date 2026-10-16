/**
 * @file test_ensemble_uncertainty_regression.cpp
 * @brief Regression tests for ensemble uncertainty stability and performance
 *
 * TEST COVERAGE:
 * - Uncertainty stability across runs
 * - Performance overhead of ensemble
 * - Memory usage
 * - Consistency of predictions
 * - Numerical stability
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>
#include <vector>

#include "cognitive/introspection/nimcp_ensemble_uncertainty.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EnsembleRegressionTest : public ::testing::Test {
protected:
    brain_t brain;
    introspection_context_t intro;
    ensemble_context_t ensemble;

    void SetUp() override {
        brain = brain_create(
            "test_ensemble_regression",
            BRAIN_SIZE_MEDIUM,
            BRAIN_TASK_CLASSIFICATION,
            50,  // inputs
            10   // outputs
        );

        intro = nullptr;
        ensemble = nullptr;

        if (brain != nullptr) {
            intro = brain_get_introspection(brain);
        }
    }

    void TearDown() override {
        if (ensemble != nullptr) {
            ensemble_destroy(ensemble);
            ensemble = nullptr;
        }

        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Measure execution time in microseconds
    template <typename Func>
    double measure_time_us(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }
};

//=============================================================================
// 1. Uncertainty Stability Tests
//=============================================================================

TEST_F(EnsembleRegressionTest, UncertaintyStabilityAcrossRuns) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);
    introspection_set_ensemble(intro, ensemble);

    // Same input multiple times should give consistent uncertainty
    float features[50];
    for (int i = 0; i < 50; i++) {
        features[i] = 0.5f;
    }

    std::vector<float> epistemic_values;
    std::vector<float> aleatoric_values;

    // Run 10 times
    for (int run = 0; run < 10; run++) {
        brain_uncertainty_t unc = brain_get_uncertainty(intro, features, 50);

        epistemic_values.push_back(unc.epistemic);
        aleatoric_values.push_back(unc.aleatoric);

        brain_uncertainty_free(&unc);
    }

    // Compute variance of uncertainty values
    float epistemic_mean = 0.0f;
    float aleatoric_mean = 0.0f;
    for (size_t i = 0; i < epistemic_values.size(); i++) {
        epistemic_mean += epistemic_values[i];
        aleatoric_mean += aleatoric_values[i];
    }
    epistemic_mean /= epistemic_values.size();
    aleatoric_mean /= aleatoric_values.size();

    float epistemic_var = 0.0f;
    float aleatoric_var = 0.0f;
    for (size_t i = 0; i < epistemic_values.size(); i++) {
        float diff_e = epistemic_values[i] - epistemic_mean;
        float diff_a = aleatoric_values[i] - aleatoric_mean;
        epistemic_var += diff_e * diff_e;
        aleatoric_var += diff_a * diff_a;
    }
    epistemic_var /= epistemic_values.size();
    aleatoric_var /= aleatoric_values.size();

    // Uncertainty should be relatively stable (low variance)
    // Allow some variance due to ensemble stochasticity
    EXPECT_LT(epistemic_var, 0.01f) << "Epistemic uncertainty too unstable";
    EXPECT_LT(aleatoric_var, 0.01f) << "Aleatoric uncertainty too unstable";
}

TEST_F(EnsembleRegressionTest, UncertaintyBoundedAcrossManyInputs) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);
    introspection_set_ensemble(intro, ensemble);

    // Test 100 random inputs
    for (int test = 0; test < 100; test++) {
        float features[50];
        for (int i = 0; i < 50; i++) {
            features[i] = static_cast<float>(rand()) / RAND_MAX;
        }

        brain_uncertainty_t unc = brain_get_uncertainty(intro, features, 50);

        // All uncertainties must be in valid range
        EXPECT_GE(unc.epistemic, 0.0f) << "Test " << test;
        EXPECT_LE(unc.epistemic, 1.0f) << "Test " << test;
        EXPECT_GE(unc.aleatoric, 0.0f) << "Test " << test;
        EXPECT_GE(unc.total, 0.0f) << "Test " << test;
        EXPECT_LE(unc.total, 1.0f) << "Test " << test;
        EXPECT_GE(unc.confidence, 0.0f) << "Test " << test;
        EXPECT_LE(unc.confidence, 1.0f) << "Test " << test;

        // No NaN or Inf
        EXPECT_FALSE(std::isnan(unc.epistemic)) << "Test " << test;
        EXPECT_FALSE(std::isnan(unc.aleatoric)) << "Test " << test;
        EXPECT_FALSE(std::isnan(unc.total)) << "Test " << test;
        EXPECT_FALSE(std::isinf(unc.epistemic)) << "Test " << test;
        EXPECT_FALSE(std::isinf(unc.aleatoric)) << "Test " << test;
        EXPECT_FALSE(std::isinf(unc.total)) << "Test " << test;

        brain_uncertainty_free(&unc);
    }
}

//=============================================================================
// 2. Performance Tests
//=============================================================================

TEST_F(EnsembleRegressionTest, UncertaintyPerformanceWithEnsemble) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);
    introspection_set_ensemble(intro, ensemble);

    float features[50];
    for (int i = 0; i < 50; i++) {
        features[i] = 0.5f;
    }

    // Measure time for ensemble-based uncertainty
    double total_time = measure_time_us([&]() {
        for (int i = 0; i < 10; i++) {
            brain_uncertainty_t unc = brain_get_uncertainty(intro, features, 50);
            brain_uncertainty_free(&unc);
        }
    });

    double avg_time = total_time / 10.0;

    // Should complete in reasonable time (< 50ms per call for MEDIUM brain)
    EXPECT_LT(avg_time, 50000.0) << "Avg time: " << avg_time << " us";

    // Log performance for monitoring
    std::cout << "Average ensemble uncertainty time: " << avg_time << " us\n";
}

TEST_F(EnsembleRegressionTest, PerformanceComparisonWithAndWithoutEnsemble) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    float features[50];
    for (int i = 0; i < 50; i++) {
        features[i] = 0.5f;
    }

    // Time without ensemble (fallback)
    double time_without = measure_time_us([&]() {
        for (int i = 0; i < 10; i++) {
            brain_uncertainty_t unc = brain_get_uncertainty(intro, features, 50);
            brain_uncertainty_free(&unc);
        }
    });

    // Time with ensemble
    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);
    introspection_set_ensemble(intro, ensemble);

    double time_with = measure_time_us([&]() {
        for (int i = 0; i < 10; i++) {
            brain_uncertainty_t unc = brain_get_uncertainty(intro, features, 50);
            brain_uncertainty_free(&unc);
        }
    });

    double overhead_ratio = time_with / time_without;

    // Ensemble should add overhead but not be prohibitively slow
    // Expect 2-10x slower due to multiple model evaluations
    EXPECT_LT(overhead_ratio, 50.0) << "Ensemble overhead too high";

    std::cout << "Performance comparison:\n";
    std::cout << "  Without ensemble: " << (time_without / 10.0) << " us\n";
    std::cout << "  With ensemble: " << (time_with / 10.0) << " us\n";
    std::cout << "  Overhead ratio: " << overhead_ratio << "x\n";
}

//=============================================================================
// 3. Memory Tests
//=============================================================================

TEST_F(EnsembleRegressionTest, MemoryUsageReasonable) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    ensemble_stats_t stats;
    bool result = ensemble_get_stats(ensemble, &stats);
    ASSERT_TRUE(result);

    // Memory usage should be reasonable
    // For MEDIUM brain (~10K neurons), ensemble of 5 models
    // Should be < 100MB
    size_t memory_mb = stats.memory_used_bytes / (1024 * 1024);
    EXPECT_LT(memory_mb, 100u) << "Ensemble using " << memory_mb << " MB";

    std::cout << "Ensemble memory usage: " << memory_mb << " MB\n";
    std::cout << "Number of models: " << stats.num_models << "\n";
}

TEST_F(EnsembleRegressionTest, NoMemoryLeakInRepeatedUncertainty) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);
    introspection_set_ensemble(intro, ensemble);

    float features[50];
    for (int i = 0; i < 50; i++) {
        features[i] = 0.5f;
    }

    ensemble_stats_t stats_before;
    ensemble_get_stats(ensemble, &stats_before);

    // Run many iterations
    for (int i = 0; i < 1000; i++) {
        brain_uncertainty_t unc = brain_get_uncertainty(intro, features, 50);
        brain_uncertainty_free(&unc);
    }

    ensemble_stats_t stats_after;
    ensemble_get_stats(ensemble, &stats_after);

    // Memory should not grow significantly
    // Allow some growth for caching, etc.
    size_t growth = stats_after.memory_used_bytes - stats_before.memory_used_bytes;
    size_t growth_mb = growth / (1024 * 1024);

    EXPECT_LT(growth_mb, 10u) << "Memory grew by " << growth_mb << " MB";
}

//=============================================================================
// 4. Numerical Stability Tests
//=============================================================================

TEST_F(EnsembleRegressionTest, NumericalStabilityExtremeInputs) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);
    introspection_set_ensemble(intro, ensemble);

    // Test extreme inputs
    float all_zeros[50] = {0};
    float all_ones[50];
    float all_large[50];

    for (int i = 0; i < 50; i++) {
        all_ones[i] = 1.0f;
        all_large[i] = 10.0f;  // Out of normal range
    }

    // All zeros
    brain_uncertainty_t unc_zeros = brain_get_uncertainty(intro, all_zeros, 50);
    EXPECT_FALSE(std::isnan(unc_zeros.total));
    EXPECT_FALSE(std::isinf(unc_zeros.total));
    brain_uncertainty_free(&unc_zeros);

    // All ones
    brain_uncertainty_t unc_ones = brain_get_uncertainty(intro, all_ones, 50);
    EXPECT_FALSE(std::isnan(unc_ones.total));
    EXPECT_FALSE(std::isinf(unc_ones.total));
    brain_uncertainty_free(&unc_ones);

    // Large values
    brain_uncertainty_t unc_large = brain_get_uncertainty(intro, all_large, 50);
    EXPECT_FALSE(std::isnan(unc_large.total));
    EXPECT_FALSE(std::isinf(unc_large.total));
    brain_uncertainty_free(&unc_large);
}

TEST_F(EnsembleRegressionTest, EntropyComputationStability) {
    // Test entropy with edge cases
    float uniform[4] = {0.25f, 0.25f, 0.25f, 0.25f};
    float deterministic[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float near_zero[4] = {1e-10f, 1e-10f, 1e-10f, 1e-10f};
    float very_small[4] = {0.0001f, 0.0001f, 0.0001f, 0.0001f};

    float h1 = ensemble_compute_entropy(uniform, 4);
    float h2 = ensemble_compute_entropy(deterministic, 4);
    float h3 = ensemble_compute_entropy(near_zero, 4);
    float h4 = ensemble_compute_entropy(very_small, 4);

    // All should be finite
    EXPECT_FALSE(std::isnan(h1));
    EXPECT_FALSE(std::isnan(h2));
    EXPECT_FALSE(std::isnan(h3));
    EXPECT_FALSE(std::isnan(h4));
    EXPECT_FALSE(std::isinf(h1));
    EXPECT_FALSE(std::isinf(h2));
    EXPECT_FALSE(std::isinf(h3));
    EXPECT_FALSE(std::isinf(h4));

    // Uniform should have high entropy
    EXPECT_GT(h1, 1.5f);

    // Deterministic should have low entropy
    EXPECT_LT(h2, 0.1f);
}

//=============================================================================
// 5. Consistency Tests
//=============================================================================

TEST_F(EnsembleRegressionTest, PredictionConsistencyAcrossModels) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    float features[50];
    for (int i = 0; i < 50; i++) {
        features[i] = 0.5f;
    }

    // Get predictions multiple times
    ensemble_prediction_t* preds1 = nullptr;
    ensemble_prediction_t* preds2 = nullptr;

    uint32_t count1 = ensemble_predict(ensemble, features, 50, &preds1);
    uint32_t count2 = ensemble_predict(ensemble, features, 50, &preds2);

    // Should get same number of predictions
    EXPECT_EQ(count1, count2);

    if (count1 > 0 && preds1 != nullptr && preds2 != nullptr) {
        // Predictions should be similar (not identical due to randomness)
        for (uint32_t i = 0; i < count1; i++) {
            EXPECT_EQ(preds1[i].size, preds2[i].size);

            if (preds1[i].size > 0 && preds1[i].prediction != nullptr &&
                preds2[i].prediction != nullptr) {
                // First output should be reasonably close
                float diff = std::abs(preds1[i].prediction[0] - preds2[i].prediction[0]);
                EXPECT_LT(diff, 0.5f) << "Model " << i << " predictions differ too much";
            }
        }

        ensemble_predictions_free(preds1, count1);
        ensemble_predictions_free(preds2, count2);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
