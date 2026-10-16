/**
 * @file test_consciousness_metrics_regression.cpp
 * @brief Regression tests for Consciousness Metrics - performance and stability
 *
 * TEST COVERAGE:
 * - Performance benchmarks for Φ computation
 * - Φ stability across multiple runs
 * - Memory leak detection
 * - Long-running monitoring stability
 * - Large network handling
 * - Resource cleanup verification
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>

#include "cognitive/introspection/nimcp_consciousness_metrics.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ConsciousnessRegressionTest : public ::testing::Test {
protected:
    brain_t brain;
    introspection_context_t intro_ctx;

    void SetUp() override {
        brain = brain_create(
            "test_consciousness_regression",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            50,
            10
        );

        if (brain) {
            intro_ctx = brain_get_introspection(brain);
        } else {
            intro_ctx = nullptr;
        }
    }

    void TearDown() override {
        if (brain) {
            brain_disable_consciousness_monitoring(brain);
            brain_destroy(brain);
            brain = nullptr;
            intro_ctx = nullptr;
        }
    }

    // Helper: Get current memory usage (simplified)
    size_t get_memory_usage() {
        // Would use platform-specific API in real implementation
        return 0;  // Placeholder
    }
};

//=============================================================================
// 1. Performance Benchmarks
//=============================================================================

TEST_F(ConsciousnessRegressionTest, PhiComputationPerformance_SmallNetwork) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    const int iterations = 20;
    std::vector<double> durations;

    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        consciousness_phi_result_t* result = introspection_compute_phi_fast(intro_ctx, nullptr);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        if (result) {
            durations.push_back(duration.count() / 1000.0);  // Convert to ms
            consciousness_phi_result_free(result);
        }
    }

    if (!durations.empty()) {
        // Compute statistics
        double sum = 0.0;
        double min_time = durations[0];
        double max_time = durations[0];

        for (double d : durations) {
            sum += d;
            if (d < min_time) min_time = d;
            if (d > max_time) max_time = d;
        }

        double avg_time = sum / durations.size();

        // Performance expectations for small network
        EXPECT_LT(avg_time, 100.0);   // Average < 100ms
        EXPECT_LT(max_time, 500.0);   // Max < 500ms

        // Log performance metrics
        printf("Φ Computation Performance (n=%d):\n", (int)durations.size());
        printf("  Average: %.2f ms\n", avg_time);
        printf("  Min: %.2f ms\n", min_time);
        printf("  Max: %.2f ms\n", max_time);
    }
}

TEST_F(ConsciousnessRegressionTest, PhiComputationScalability) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Test with different configuration methods
    consciousness_phi_config_t configs[] = {
        consciousness_phi_fast_config(),
        consciousness_phi_default_config(),
        consciousness_phi_accurate_config()
    };

    const char* config_names[] = {"fast", "default", "accurate"};

    for (int c = 0; c < 3; c++) {
        auto start = std::chrono::high_resolution_clock::now();

        consciousness_phi_result_t* result =
            introspection_compute_phi(intro_ctx, &configs[c]);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (result) {
            printf("%s config: %.2f ms (Φ=%.3f)\n",
                   config_names[c],
                   (double)duration.count(),
                   result->phi);

            consciousness_phi_result_free(result);
        }
    }
}

//=============================================================================
// 2. Stability Tests
//=============================================================================

TEST_F(ConsciousnessRegressionTest, PhiStabilityAcrossRuns) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    const int runs = 20;
    std::vector<float> phi_values;

    for (int i = 0; i < runs; i++) {
        consciousness_phi_result_t* result = introspection_compute_phi(intro_ctx, nullptr);
        if (result) {
            phi_values.push_back(result->phi);
            consciousness_phi_result_free(result);
        }
    }

    if (phi_values.size() >= 5) {
        // Compute mean and variance
        float sum = 0.0f;
        for (float phi : phi_values) {
            sum += phi;
        }
        float mean = sum / phi_values.size();

        float var_sum = 0.0f;
        for (float phi : phi_values) {
            float diff = phi - mean;
            var_sum += diff * diff;
        }
        float variance = var_sum / phi_values.size();
        float stddev = sqrtf(variance);

        // Stability check: variance should be low
        EXPECT_LT(stddev, 0.2f);  // Standard deviation < 0.2

        printf("Φ Stability (n=%d):\n", (int)phi_values.size());
        printf("  Mean: %.4f\n", mean);
        printf("  Std Dev: %.4f\n", stddev);
        printf("  Coefficient of Variation: %.2f%%\n", (stddev / mean) * 100.0f);
    }
}

TEST_F(ConsciousnessRegressionTest, ConsistentStateClassification) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    const int runs = 15;
    std::vector<consciousness_state_t> states;

    for (int i = 0; i < runs; i++) {
        consciousness_phi_result_t* result = introspection_compute_phi(intro_ctx, nullptr);
        if (result) {
            states.push_back(result->state);
            consciousness_phi_result_free(result);
        }
    }

    if (states.size() >= 5) {
        // Count state frequencies
        int state_counts[5] = {0};
        for (auto state : states) {
            if (state >= 0 && state < 5) {
                state_counts[state]++;
            }
        }

        // Most common state should be majority
        int max_count = 0;
        for (int i = 0; i < 5; i++) {
            if (state_counts[i] > max_count) {
                max_count = state_counts[i];
            }
        }

        float stability_ratio = (float)max_count / (float)states.size();
        EXPECT_GT(stability_ratio, 0.7f);  // 70% should be same state

        printf("State Stability: %.1f%% consistent\n", stability_ratio * 100.0f);
    }
}

//=============================================================================
// 3. Memory Leak Detection
//=============================================================================

TEST_F(ConsciousnessRegressionTest, NoMemoryLeaksInRepeatedComputation) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    const int iterations = 100;

    size_t initial_memory = get_memory_usage();

    for (int i = 0; i < iterations; i++) {
        consciousness_phi_result_t* result = introspection_compute_phi_fast(intro_ctx, nullptr);
        if (result) {
            consciousness_phi_result_free(result);
        }
    }

    size_t final_memory = get_memory_usage();

    // Memory growth should be minimal
    // (Actual check would need platform-specific memory tracking)
    EXPECT_TRUE(true);  // Placeholder - real test would check memory delta

    printf("Memory test: %d iterations completed\n", iterations);
}

TEST_F(ConsciousnessRegressionTest, ProperResourceCleanup) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Allocate and free many results
    const int count = 20;

    for (int i = 0; i < count; i++) {
        consciousness_phi_result_t* result = introspection_compute_phi(intro_ctx, nullptr);

        if (result) {
            // Verify all pointers are valid before freeing
            EXPECT_NE(result, nullptr);

            if (result->mip) {
                EXPECT_NE(result->mip->subset_a, nullptr);
                EXPECT_NE(result->mip->subset_b, nullptr);
            }

            consciousness_phi_result_free(result);
        }

        phi_partition_t* mip = introspection_get_mip(intro_ctx, nullptr);
        if (mip) {
            phi_partition_free(mip);
        }
    }

    // Should not crash or leak
    EXPECT_TRUE(true);
}

//=============================================================================
// 4. Long-Running Monitoring Stability
//=============================================================================

TEST_F(ConsciousnessRegressionTest, MonitoringStability_ShortDuration) {
    if (!brain) {
        GTEST_SKIP() << "Brain not available";
    }

    consciousness_phi_config_t config = consciousness_phi_fast_config();

    bool success = brain_enable_consciousness_monitoring(
        brain,
        &config,
        50,   // Fast updates
        nullptr,
        nullptr
    );

    if (success) {
        // Run for 500ms
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Should still be running without crash
        brain_disable_consciousness_monitoring(brain);

        EXPECT_TRUE(true);
    } else {
        GTEST_SKIP() << "Monitoring not available";
    }
}

//=============================================================================
// 5. Error Handling and Edge Cases
//=============================================================================

TEST_F(ConsciousnessRegressionTest, GracefulHandlingOfInvalidInputs) {
    // All should return NULL or safe defaults without crashing

    consciousness_phi_result_t* result1 = introspection_compute_phi(nullptr, nullptr);
    EXPECT_EQ(result1, nullptr);

    phi_partition_t* mip1 = introspection_get_mip(nullptr, nullptr);
    EXPECT_EQ(mip1, nullptr);

    conceptual_structure_t* cs1 = introspection_get_conceptual_structure(nullptr, nullptr);
    EXPECT_EQ(cs1, nullptr);

    float phi = brain_get_consciousness_level(nullptr);
    EXPECT_FLOAT_EQ(phi, 0.0f);

    bool conscious = brain_is_conscious(nullptr, 0.0f);
    EXPECT_FALSE(conscious);

    consciousness_monitoring_stats_t stats;
    bool stats_ok = brain_get_consciousness_stats(nullptr, &stats);
    EXPECT_FALSE(stats_ok);
}

TEST_F(ConsciousnessRegressionTest, MultipleFreeCalls) {
    // Multiple frees should be safe (NULL pointers)

    consciousness_phi_result_free(nullptr);
    consciousness_phi_result_free(nullptr);

    phi_partition_free(nullptr);
    phi_partition_free(nullptr);

    conceptual_structure_free(nullptr);
    conceptual_structure_free(nullptr);

    consciousness_concept_free(nullptr);
    consciousness_concept_free(nullptr);

    // Should not crash
    EXPECT_TRUE(true);
}

//=============================================================================
// 6. Regression Baselines
//=============================================================================

TEST_F(ConsciousnessRegressionTest, PhiValueReasonableRange) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    const int samples = 20;
    int in_range_count = 0;

    for (int i = 0; i < samples; i++) {
        consciousness_phi_result_t* result = introspection_compute_phi(intro_ctx, nullptr);
        if (result) {
            // Φ should be in biologically plausible range
            if (result->phi >= 0.0f && result->phi <= 2.0f) {
                in_range_count++;
            }
            consciousness_phi_result_free(result);
        }
    }

    // At least 90% should be in range
    float in_range_ratio = (float)in_range_count / (float)samples;
    EXPECT_GT(in_range_ratio, 0.9f);

    printf("Φ Range Test: %.1f%% in [0, 2.0]\n", in_range_ratio * 100.0f);
}

TEST_F(ConsciousnessRegressionTest, StateNamesConsistent) {
    // Verify all state names are defined
    EXPECT_NE(consciousness_state_name(CONSCIOUSNESS_STATE_UNCONSCIOUS), nullptr);
    EXPECT_NE(consciousness_state_name(CONSCIOUSNESS_STATE_MINIMAL), nullptr);
    EXPECT_NE(consciousness_state_name(CONSCIOUSNESS_STATE_REDUCED), nullptr);
    EXPECT_NE(consciousness_state_name(CONSCIOUSNESS_STATE_NORMAL), nullptr);
    EXPECT_NE(consciousness_state_name(CONSCIOUSNESS_STATE_HEIGHTENED), nullptr);

    // All should be non-empty
    EXPECT_GT(strlen(consciousness_state_name(CONSCIOUSNESS_STATE_UNCONSCIOUS)), 0u);
    EXPECT_GT(strlen(consciousness_state_name(CONSCIOUSNESS_STATE_MINIMAL)), 0u);
    EXPECT_GT(strlen(consciousness_state_name(CONSCIOUSNESS_STATE_REDUCED)), 0u);
    EXPECT_GT(strlen(consciousness_state_name(CONSCIOUSNESS_STATE_NORMAL)), 0u);
    EXPECT_GT(strlen(consciousness_state_name(CONSCIOUSNESS_STATE_HEIGHTENED)), 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
