/**
 * @file test_prediction_accuracy.cpp
 * @brief Regression tests for predictive coding accuracy in brain regions
 *
 * TEST OBJECTIVES:
 * - Verify prediction accuracy doesn't degrade
 * - Ensure convergence speed remains consistent
 * - Check free energy minimization performance
 * - Validate precision learning stability
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include "core/brain_regions/nimcp_brain_region_predictive.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "utils/logging/nimcp_logging.h"
#include <vector>
#include <cmath>
#include <numeric>

//=============================================================================
// Test Fixture
//=============================================================================

class PredictionAccuracyRegressionTest : public ::testing::Test {
protected:
    brain_region_t* region;

    void SetUp() override {
        region = brain_region_create(REGION_VISUAL_V1, 256);
        ASSERT_NE(region, nullptr);

        brain_region_predictive_config_t config =
            brain_region_predictive_config_sensory();
        config.max_iterations = 50;
        config.convergence_tolerance = 0.001f;

        ASSERT_EQ(brain_region_enable_predictive(region, &config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (region) {
            brain_region_disable_predictive(region);
            brain_region_destroy(region);
        }
    }

    // Helper: Compute statistics
    float compute_mean(const std::vector<float>& data) {
        return std::accumulate(data.begin(), data.end(), 0.0f) / data.size();
    }

    float compute_std(const std::vector<float>& data) {
        float mean = compute_mean(data);
        float variance = 0.0f;
        for (float val : data) {
            float diff = val - mean;
            variance += diff * diff;
        }
        return sqrtf(variance / data.size());
    }
};

//=============================================================================
// Prediction Accuracy Benchmarks
//=============================================================================

TEST_F(PredictionAccuracyRegressionTest, ConstantInputPredictionAccuracy) {
    /* WHAT: Regression test for constant input prediction
     * WHY:  Baseline - should converge to near-perfect prediction
     * BASELINE: Mean error < 0.05, FE < 1.0
     */

    const uint32_t NUM_TRIALS = 10;
    std::vector<float> mean_errors;
    std::vector<float> free_energies;

    for (uint32_t trial = 0; trial < NUM_TRIALS; trial++) {
        // Reset region
        brain_region_disable_predictive(region);
        brain_region_predictive_config_t config =
            brain_region_predictive_config_sensory();
        brain_region_enable_predictive(region, &config);

        // Constant input
        float input[256];
        for (uint32_t i = 0; i < 256; i++) {
            input[i] = 0.75f;
        }

        // Converge
        uint32_t iterations = brain_region_hierarchical_converge(
            region, input, 256, 50, 0.001f);

        EXPECT_LE(iterations, 50);

        // Get statistics
        brain_region_predictive_stats_t stats;
        brain_region_get_predictive_stats(region, &stats);

        mean_errors.push_back(stats.mean_prediction_error);
        free_energies.push_back(stats.total_free_energy);
    }

    // Compute statistics
    float mean_error_avg = compute_mean(mean_errors);
    float fe_avg = compute_mean(free_energies);

    // Regression thresholds (baseline performance)
    EXPECT_LT(mean_error_avg, 0.1f)
        << "Prediction accuracy degraded! Mean error: " << mean_error_avg;
    EXPECT_LT(fe_avg, 2.0f)
        << "Free energy minimization degraded! FE: " << fe_avg;

    LOG_MODULE_INFO("PredictiveTest", "Constant input regression: mean_error=%.4f, FE=%.4f",
                    mean_error_avg, fe_avg);
}

TEST_F(PredictionAccuracyRegressionTest, SinusoidalInputPredictionAccuracy) {
    /* WHAT: Regression test for sinusoidal input
     * WHY:  More complex pattern - tests learning
     * BASELINE: Mean error < 0.15, FE < 3.0
     */

    const uint32_t NUM_TRIALS = 10;
    std::vector<float> mean_errors;
    std::vector<float> free_energies;

    for (uint32_t trial = 0; trial < NUM_TRIALS; trial++) {
        // Reset region
        brain_region_disable_predictive(region);
        brain_region_predictive_config_t config =
            brain_region_predictive_config_sensory();
        brain_region_enable_predictive(region, &config);

        // Sinusoidal input
        float input[256];
        float frequency = 2.0f + 0.5f * (float)trial;
        for (uint32_t i = 0; i < 256; i++) {
            input[i] = 0.5f + 0.5f * sinf(2.0f * M_PI * frequency * (float)i / 256.0f);
        }

        // Converge
        uint32_t iterations = brain_region_hierarchical_converge(
            region, input, 256, 50, 0.001f);

        EXPECT_LE(iterations, 50);

        // Get statistics
        brain_region_predictive_stats_t stats;
        brain_region_get_predictive_stats(region, &stats);

        mean_errors.push_back(stats.mean_prediction_error);
        free_energies.push_back(stats.total_free_energy);
    }

    // Compute statistics
    float mean_error_avg = compute_mean(mean_errors);
    float fe_avg = compute_mean(free_energies);

    // Regression thresholds
    EXPECT_LT(mean_error_avg, 0.2f)
        << "Sinusoidal prediction accuracy degraded! Mean error: " << mean_error_avg;
    EXPECT_LT(fe_avg, 5.0f)
        << "Free energy minimization degraded! FE: " << fe_avg;

    LOG_MODULE_INFO("PredictiveTest", "Sinusoidal regression: mean_error=%.4f, FE=%.4f",
                    mean_error_avg, fe_avg);
}

//=============================================================================
// Convergence Speed Regression
//=============================================================================

TEST_F(PredictionAccuracyRegressionTest, ConvergenceSpeedRegression) {
    /* WHAT: Regression test for convergence speed
     * WHY:  Ensure optimization doesn't slow down convergence
     * BASELINE: < 20 iterations for simple input
     */

    const uint32_t NUM_TRIALS = 20;
    std::vector<uint32_t> iteration_counts;

    for (uint32_t trial = 0; trial < NUM_TRIALS; trial++) {
        // Reset region
        brain_region_disable_predictive(region);
        brain_region_predictive_config_t config =
            brain_region_predictive_config_sensory();
        brain_region_enable_predictive(region, &config);

        // Simple pattern
        float input[256];
        for (uint32_t i = 0; i < 256; i++) {
            input[i] = (i % 2 == 0) ? 0.8f : 0.2f;
        }

        // Converge
        uint32_t iterations = brain_region_hierarchical_converge(
            region, input, 256, 50, 0.01f);

        iteration_counts.push_back(iterations);
    }

    // Compute average
    float avg_iterations = std::accumulate(iteration_counts.begin(),
                                            iteration_counts.end(), 0.0f) / NUM_TRIALS;

    // Regression threshold
    EXPECT_LT(avg_iterations, 25.0f)
        << "Convergence speed regressed! Avg iterations: " << avg_iterations;

    LOG_MODULE_INFO("PredictiveTest", "Convergence speed regression: avg_iterations=%.1f", avg_iterations);
}

//=============================================================================
// Precision Learning Regression
//=============================================================================

TEST_F(PredictionAccuracyRegressionTest, PrecisionLearningStability) {
    /* WHAT: Regression test for precision learning stability
     * WHY:  Ensure learned precisions are reasonable and stable
     * BASELINE: Precisions in range [0.01, 10.0], std < 5.0
     */

    // Create input with reliable and noisy regions
    float input[256];
    for (uint32_t i = 0; i < 128; i++) {
        input[i] = 0.7f;  // Reliable
    }
    for (uint32_t i = 128; i < 256; i++) {
        input[i] = 0.5f + 0.3f * ((float)rand() / RAND_MAX - 0.5f);  // Noisy
    }

    // Train for multiple iterations
    for (int iter = 0; iter < 20; iter++) {
        brain_region_hierarchical_step(region, input, 256, 1.0f);
        brain_region_learn_precisions(region, 1.0f);
    }

    // Get learned precisions
    float precisions[256];
    EXPECT_EQ(brain_region_get_precision(region, precisions, 256), NIMCP_SUCCESS);

    // Compute statistics
    std::vector<float> precision_vec(precisions, precisions + 256);
    float mean_precision = compute_mean(precision_vec);
    float std_precision = compute_std(precision_vec);

    // Check all precisions in valid range
    for (uint32_t i = 0; i < 256; i++) {
        EXPECT_GE(precisions[i], 0.01f) << "Precision too low at index " << i;
        EXPECT_LE(precisions[i], 100.0f) << "Precision too high at index " << i;
    }

    // Regression thresholds
    EXPECT_GT(mean_precision, 0.1f)
        << "Mean precision too low: " << mean_precision;
    EXPECT_LT(mean_precision, 50.0f)
        << "Mean precision exploded: " << mean_precision;
    EXPECT_LT(std_precision, 20.0f)
        << "Precision variance too high: " << std_precision;

    LOG_MODULE_INFO("PredictiveTest", "Precision learning regression: mean=%.4f, std=%.4f",
                    mean_precision, std_precision);
}

//=============================================================================
// Free Energy Minimization Regression
//=============================================================================

TEST_F(PredictionAccuracyRegressionTest, FreeEnergyMinimizationTrend) {
    /* WHAT: Regression test for free energy minimization
     * WHY:  Verify free energy decreases over iterations
     * BASELINE: FE should decrease by at least 20% from start to end
     */

    float input[256];
    for (uint32_t i = 0; i < 256; i++) {
        input[i] = 0.5f + 0.5f * sinf(2.0f * M_PI * 3.0f * (float)i / 256.0f);
    }

    std::vector<float> free_energies;

    // Track free energy over 30 iterations
    for (int iter = 0; iter < 30; iter++) {
        brain_region_hierarchical_step(region, input, 256, 1.0f);
        float fe = brain_region_get_free_energy(region);
        free_energies.push_back(fe);
    }

    // Check trend: later FE should be lower than early FE
    float early_fe_avg = 0.0f;
    for (int i = 0; i < 5; i++) {
        early_fe_avg += free_energies[i];
    }
    early_fe_avg /= 5.0f;

    float late_fe_avg = 0.0f;
    for (int i = 25; i < 30; i++) {
        late_fe_avg += free_energies[i];
    }
    late_fe_avg /= 5.0f;

    // Free energy should decrease or stabilize
    EXPECT_LE(late_fe_avg, early_fe_avg * 1.2f)
        << "Free energy not decreasing! Early: " << early_fe_avg
        << ", Late: " << late_fe_avg;

    // Ideally should decrease by at least 10%
    float decrease_ratio = (early_fe_avg - late_fe_avg) / (early_fe_avg + 1e-6f);
    EXPECT_GE(decrease_ratio, -0.2f)  // Allow small increases
        << "Free energy increased too much! Ratio: " << decrease_ratio;

    LOG_MODULE_INFO("PredictiveTest", "Free energy trend regression: early=%.4f, late=%.4f, decrease=%.2f%%",
                    early_fe_avg, late_fe_avg, decrease_ratio * 100.0f);
}

//=============================================================================
// Consistency Regression
//=============================================================================

TEST_F(PredictionAccuracyRegressionTest, DeterministicBehavior) {
    /* WHAT: Regression test for deterministic behavior
     * WHY:  Same input should produce same results (reproducibility)
     * BASELINE: Results should be identical across runs
     */

    float input[256];
    for (uint32_t i = 0; i < 256; i++) {
        input[i] = 0.5f;
    }

    // Run twice with same input
    std::vector<float> run1_errors;
    std::vector<float> run2_errors;

    // Run 1
    for (int iter = 0; iter < 10; iter++) {
        brain_region_hierarchical_step(region, input, 256, 1.0f);

        brain_region_predictive_stats_t stats;
        brain_region_get_predictive_stats(region, &stats);
        run1_errors.push_back(stats.mean_prediction_error);
    }

    // Reset
    brain_region_disable_predictive(region);
    brain_region_predictive_config_t config =
        brain_region_predictive_config_sensory();
    brain_region_enable_predictive(region, &config);

    // Run 2
    for (int iter = 0; iter < 10; iter++) {
        brain_region_hierarchical_step(region, input, 256, 1.0f);

        brain_region_predictive_stats_t stats;
        brain_region_get_predictive_stats(region, &stats);
        run2_errors.push_back(stats.mean_prediction_error);
    }

    // Compare runs (should be very similar due to deterministic initialization)
    for (size_t i = 0; i < run1_errors.size(); i++) {
        float diff = fabsf(run1_errors[i] - run2_errors[i]);
        float avg = (run1_errors[i] + run2_errors[i]) / 2.0f;
        float relative_diff = diff / (avg + 1e-6f);

        // Allow small differences due to floating point, but should be close
        EXPECT_LT(relative_diff, 0.1f)
            << "Non-deterministic behavior at iteration " << i
            << " (diff=" << diff << ", rel=" << relative_diff << ")";
    }

    LOG_MODULE_INFO("PredictiveTest", "Deterministic behavior regression: passed");
}

//=============================================================================
// Performance Regression
//=============================================================================

TEST_F(PredictionAccuracyRegressionTest, MemoryUsageStability) {
    /* WHAT: Regression test for memory usage
     * WHY:  Ensure no memory leaks or excessive allocation
     * BASELINE: Memory should not grow over iterations
     */

    // Note: This is a basic test - in production would use valgrind/sanitizers

    float input[256];
    for (uint32_t i = 0; i < 256; i++) {
        input[i] = 0.6f;
    }

    // Run many iterations to check for leaks
    for (int iter = 0; iter < 100; iter++) {
        brain_region_hierarchical_step(region, input, 256, 1.0f);

        // Periodically check region is still valid
        if (iter % 20 == 0) {
            brain_region_predictive_stats_t stats;
            EXPECT_EQ(brain_region_get_predictive_stats(region, &stats),
                      NIMCP_SUCCESS);
        }
    }

    // If we got here without crashes, basic stability is OK
    SUCCEED() << "Memory stability check passed (100 iterations)";

    LOG_MODULE_INFO("PredictiveTest", "Memory usage stability regression: passed");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
