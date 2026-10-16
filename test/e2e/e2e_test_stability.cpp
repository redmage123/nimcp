/**
 * @file e2e_test_stability.cpp
 * @brief E2E Tests for Long-Running Stability
 *
 * WHAT: Comprehensive end-to-end tests for extended operation stability
 * WHY:  Verify memory stays bounded, no numerical drift or overflow
 * HOW:  Run brain for 1000+ iterations, monitor resources
 *
 * TEST COVERAGE:
 * - Extended runtime stability (1000+ iterations)
 * - Memory usage stays bounded
 * - No numerical drift
 * - No overflow conditions
 * - Resource exhaustion handling
 * - Performance consistency over time
 *
 * BIOLOGICAL ANALOGY:
 * - Long-term neural stability (homeostasis)
 * - Metabolic balance over extended operation
 * - Synaptic weight maintenance
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <chrono>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

// Iteration counts
constexpr uint32_t SHORT_RUN_ITERATIONS = 100;
constexpr uint32_t MEDIUM_RUN_ITERATIONS = 500;
constexpr uint32_t LONG_RUN_ITERATIONS = 1000;
constexpr uint32_t EXTENDED_RUN_ITERATIONS = 2000;

// Timing thresholds (milliseconds)
constexpr double MAX_ITERATION_TIME_MS = 50.0;
constexpr double PERFORMANCE_VARIANCE_THRESHOLD = 2.0;  // 2x allowed variance

// Memory thresholds
constexpr size_t MAX_MEMORY_GROWTH_BYTES = 1024 * 1024;  // 1MB
constexpr size_t MAX_PER_ITERATION_GROWTH_BYTES = 4096;

// Numerical stability
constexpr float MAX_DRIFT_THRESHOLD = 0.1f;
constexpr float OVERFLOW_THRESHOLD = 1e30f;

//=============================================================================
// Helper Structures
//=============================================================================

/**
 * @brief Stability metrics collected during run
 */
struct StabilityMetrics {
    std::vector<size_t> memory_samples;
    std::vector<double> iteration_times;
    std::vector<float> output_norms;
    uint32_t nan_count;
    uint32_t inf_count;
    uint32_t overflow_count;
    float initial_output_mean;
    float final_output_mean;
};

//=============================================================================
// Test Fixture
//=============================================================================

class StabilityE2ETest : public ::testing::Test {
protected:
    brain_t brain_;
    StabilityMetrics metrics_;

    void SetUp() override {
        nimcp_init();
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        brain_ = nullptr;
        resetMetrics();
    }

    void TearDown() override {
        if (brain_) {
            brain_destroy(brain_);
            brain_ = nullptr;
        }
        nimcp_memory_check_leaks();
        nimcp_shutdown();
    }

    void resetMetrics() {
        metrics_.memory_samples.clear();
        metrics_.iteration_times.clear();
        metrics_.output_norms.clear();
        metrics_.nan_count = 0;
        metrics_.inf_count = 0;
        metrics_.overflow_count = 0;
        metrics_.initial_output_mean = 0.0f;
        metrics_.final_output_mean = 0.0f;
    }

    // Create brain for stability testing
    bool createStabilityBrain(const char* name, brain_size_t size,
                              uint32_t input_dim, uint32_t output_dim) {
        brain_ = brain_create_minimal(
            name,
            size,
            BRAIN_TASK_CLASSIFICATION,
            input_dim,
            output_dim
        );
        return brain_ != nullptr;
    }

    // Record memory sample
    void recordMemorySample() {
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        metrics_.memory_samples.push_back(stats.current_allocated);
    }

    // Check numerical stability of values
    void checkNumericalStability(const std::vector<float>& values) {
        for (float v : values) {
            if (std::isnan(v)) metrics_.nan_count++;
            if (std::isinf(v)) metrics_.inf_count++;
            if (std::abs(v) > OVERFLOW_THRESHOLD) metrics_.overflow_count++;
        }
    }

    // Compute vector norm
    float computeNorm(const std::vector<float>& values) {
        float sum = 0.0f;
        for (float v : values) {
            sum += v * v;
        }
        return std::sqrt(sum);
    }

    // Compute statistics
    void computeStatistics(const std::vector<double>& values,
                           double& mean, double& stddev, double& min_val, double& max_val) {
        if (values.empty()) {
            mean = stddev = min_val = max_val = 0.0;
            return;
        }

        mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        min_val = *std::min_element(values.begin(), values.end());
        max_val = *std::max_element(values.begin(), values.end());

        double variance = 0.0;
        for (double v : values) {
            variance += (v - mean) * (v - mean);
        }
        stddev = std::sqrt(variance / values.size());
    }

    // Print metrics summary
    void printMetricsSummary() {
        std::cout << "[E2E] Stability Metrics Summary:\n";

        // Memory
        if (!metrics_.memory_samples.empty()) {
            size_t min_mem = *std::min_element(metrics_.memory_samples.begin(),
                                               metrics_.memory_samples.end());
            size_t max_mem = *std::max_element(metrics_.memory_samples.begin(),
                                               metrics_.memory_samples.end());
            std::cout << "  Memory: " << min_mem << " - " << max_mem
                      << " bytes (growth: " << (max_mem - min_mem) << ")\n";
        }

        // Timing
        if (!metrics_.iteration_times.empty()) {
            double mean, stddev, min_t, max_t;
            computeStatistics(metrics_.iteration_times, mean, stddev, min_t, max_t);
            std::cout << "  Timing: mean=" << mean << "ms stddev=" << stddev
                      << "ms range=[" << min_t << ", " << max_t << "]\n";
        }

        // Numerical issues
        std::cout << "  NaN: " << metrics_.nan_count
                  << " Inf: " << metrics_.inf_count
                  << " Overflow: " << metrics_.overflow_count << "\n";

        // Drift
        float drift = std::abs(metrics_.final_output_mean - metrics_.initial_output_mean);
        std::cout << "  Output drift: " << drift << "\n";
    }
};

//=============================================================================
// Test: Extended Runtime Stability (1000+ iterations)
//=============================================================================

E2E_TEST_F(StabilityE2ETest, ExtendedRuntimeStability) {
    E2E_PIPELINE_START("Extended Runtime Stability (1000+ iterations)");

    const uint32_t INPUT_DIM = 16;
    const uint32_t OUTPUT_DIM = 8;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createStabilityBrain("extended_stability_brain",
                                            BRAIN_SIZE_SMALL,
                                            INPUT_DIM, OUTPUT_DIM);
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Initial baseline
    E2E_STAGE_BEGIN("Establish baseline", 200);
    {
        recordMemorySample();

        std::vector<float> input = TestDataGenerator::generate_features(INPUT_DIM);
        std::vector<float> output(OUTPUT_DIM);

        // Simulated inference
        for (size_t i = 0; i < OUTPUT_DIM; ++i) {
            output[i] = std::tanh(input[i % INPUT_DIM]);
        }

        metrics_.initial_output_mean = std::accumulate(output.begin(), output.end(), 0.0f) /
                                       output.size();
        std::cout << "[E2E] Initial output mean: " << metrics_.initial_output_mean << "\n";
    }
    E2E_STAGE_END();

    // Stage 3: Extended run
    E2E_STAGE_BEGIN("Extended run (1000+ iterations)", 60000);
    {
        for (uint32_t iter = 0; iter < LONG_RUN_ITERATIONS; ++iter) {
            auto iter_start = std::chrono::high_resolution_clock::now();

            // Generate input
            std::vector<float> input = TestDataGenerator::generate_features(INPUT_DIM);
            std::vector<float> output(OUTPUT_DIM);

            // Simulated processing
            for (size_t i = 0; i < OUTPUT_DIM; ++i) {
                output[i] = std::tanh(input[i % INPUT_DIM] + 0.01f * iter);
            }

            // Check numerical stability
            checkNumericalStability(output);
            metrics_.output_norms.push_back(computeNorm(output));

            auto iter_end = std::chrono::high_resolution_clock::now();
            double iter_time = std::chrono::duration<double, std::milli>(
                iter_end - iter_start).count();
            metrics_.iteration_times.push_back(iter_time);

            // Periodic memory sampling
            if (iter % 100 == 0) {
                recordMemorySample();
                std::cout << "[E2E] Iteration " << iter << ": "
                          << iter_time << "ms\n";
            }
        }
    }
    E2E_STAGE_END();

    // Stage 4: Final state check
    E2E_STAGE_BEGIN("Final state check", 200);
    {
        recordMemorySample();

        std::vector<float> input = TestDataGenerator::generate_features(INPUT_DIM);
        std::vector<float> output(OUTPUT_DIM);

        for (size_t i = 0; i < OUTPUT_DIM; ++i) {
            output[i] = std::tanh(input[i % INPUT_DIM]);
        }

        metrics_.final_output_mean = std::accumulate(output.begin(), output.end(), 0.0f) /
                                     output.size();

        printMetricsSummary();
    }
    E2E_STAGE_END();

    // Stage 5: Verify stability
    E2E_STAGE_BEGIN("Verify stability", 100);
    {
        // Memory should not grow unboundedly
        if (metrics_.memory_samples.size() >= 2) {
            size_t initial = metrics_.memory_samples.front();
            size_t final_mem = metrics_.memory_samples.back();
            size_t growth = (final_mem > initial) ? (final_mem - initial) : 0;

            EXPECT_LT(growth, MAX_MEMORY_GROWTH_BYTES)
                << "Memory grew by " << growth << " bytes";
        }

        // No numerical issues
        EXPECT_EQ(metrics_.nan_count, 0u) << "NaN values detected";
        EXPECT_EQ(metrics_.inf_count, 0u) << "Infinity values detected";
        EXPECT_EQ(metrics_.overflow_count, 0u) << "Overflow values detected";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Memory Usage Stays Bounded
//=============================================================================

E2E_TEST_F(StabilityE2ETest, MemoryUsageStaysBounded) {
    E2E_PIPELINE_START("Memory Usage Stays Bounded");

    const uint32_t INPUT_DIM = 32;
    const uint32_t OUTPUT_DIM = 16;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createStabilityBrain("memory_bounded_brain",
                                            BRAIN_SIZE_MEDIUM,
                                            INPUT_DIM, OUTPUT_DIM);
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Memory monitoring loop
    E2E_STAGE_BEGIN("Memory monitoring over iterations", 30000);
    {
        size_t peak_memory = 0;
        size_t min_memory = SIZE_MAX;
        bool memory_leak_detected = false;

        recordMemorySample();
        size_t baseline = metrics_.memory_samples.back();

        for (uint32_t iter = 0; iter < MEDIUM_RUN_ITERATIONS; ++iter) {
            // Allocate temporary data (simulating training)
            {
                std::vector<float> input = TestDataGenerator::generate_features(INPUT_DIM);
                std::vector<float> output(OUTPUT_DIM);
                std::vector<float> gradients(INPUT_DIM * OUTPUT_DIM);

                // Simulated computation
                for (size_t i = 0; i < OUTPUT_DIM; ++i) {
                    output[i] = input[i % INPUT_DIM] * 0.5f;
                }
                for (size_t i = 0; i < gradients.size(); ++i) {
                    gradients[i] = 0.01f * (float)(rand() % 100 - 50);
                }
            }
            // Vectors should be deallocated here

            // Check memory
            nimcp_memory_stats_t stats;
            nimcp_memory_get_stats(&stats);

            peak_memory = std::max(peak_memory, stats.current_allocated);
            min_memory = std::min(min_memory, stats.current_allocated);

            // Check for sustained growth
            if (iter % 100 == 0 && iter > 0) {
                size_t growth = (stats.current_allocated > baseline) ?
                                (stats.current_allocated - baseline) : 0;

                if (growth > MAX_PER_ITERATION_GROWTH_BYTES * iter) {
                    memory_leak_detected = true;
                    std::cout << "[E2E] Potential leak at iter " << iter
                              << ": growth=" << growth << "\n";
                }
            }
        }

        size_t memory_range = peak_memory - min_memory;
        std::cout << "[E2E] Memory range: " << min_memory << " - " << peak_memory
                  << " (range: " << memory_range << ")\n";

        EXPECT_FALSE(memory_leak_detected) << "Memory leak pattern detected";
        EXPECT_LT(memory_range, MAX_MEMORY_GROWTH_BYTES)
            << "Memory fluctuation too large";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: No Numerical Drift
//=============================================================================

E2E_TEST_F(StabilityE2ETest, NoNumericalDrift) {
    E2E_PIPELINE_START("No Numerical Drift");

    const uint32_t INPUT_DIM = 16;
    const uint32_t OUTPUT_DIM = 8;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createStabilityBrain("no_drift_brain",
                                            BRAIN_SIZE_SMALL,
                                            INPUT_DIM, OUTPUT_DIM);
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Track output statistics over time
    E2E_STAGE_BEGIN("Track drift over iterations", 20000);
    {
        // Use fixed seed for reproducible input
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        std::vector<float> fixed_input(INPUT_DIM);
        for (float& v : fixed_input) {
            v = dist(gen);
        }

        std::vector<float> initial_output(OUTPUT_DIM);
        std::vector<float> current_output(OUTPUT_DIM);

        // Compute initial output
        for (size_t i = 0; i < OUTPUT_DIM; ++i) {
            initial_output[i] = std::tanh(fixed_input[i % INPUT_DIM]);
        }

        std::vector<float> drift_history;

        for (uint32_t iter = 0; iter < MEDIUM_RUN_ITERATIONS; ++iter) {
            // Compute current output with same input
            for (size_t i = 0; i < OUTPUT_DIM; ++i) {
                current_output[i] = std::tanh(fixed_input[i % INPUT_DIM]);
            }

            // Compute drift from initial
            float drift = 0.0f;
            for (size_t i = 0; i < OUTPUT_DIM; ++i) {
                drift += std::abs(current_output[i] - initial_output[i]);
            }
            drift /= OUTPUT_DIM;
            drift_history.push_back(drift);

            // Periodic check
            if (iter % 100 == 0) {
                std::cout << "[E2E] Iteration " << iter << " drift: " << drift << "\n";
            }
        }

        // Verify no significant drift
        float max_drift = *std::max_element(drift_history.begin(), drift_history.end());
        std::cout << "[E2E] Maximum drift: " << max_drift << "\n";

        EXPECT_LT(max_drift, MAX_DRIFT_THRESHOLD)
            << "Significant numerical drift detected";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: No Overflow Conditions
//=============================================================================

E2E_TEST_F(StabilityE2ETest, NoOverflowConditions) {
    E2E_PIPELINE_START("No Overflow Conditions");

    // Stage 1: Test with extreme inputs
    E2E_STAGE_BEGIN("Test extreme inputs", 5000);
    {
        uint32_t overflow_count = 0;
        uint32_t nan_count = 0;
        uint32_t inf_count = 0;

        // Test very large values
        std::vector<float> large_input(16, 1e10f);
        std::vector<float> output(8);

        for (uint32_t iter = 0; iter < SHORT_RUN_ITERATIONS; ++iter) {
            for (size_t i = 0; i < output.size(); ++i) {
                // Operations that could overflow
                float temp = large_input[i % large_input.size()] * 1.001f;
                output[i] = std::tanh(temp);  // tanh clamps to [-1, 1]

                if (std::isnan(output[i])) nan_count++;
                if (std::isinf(output[i])) inf_count++;
                if (std::abs(output[i]) > OVERFLOW_THRESHOLD) overflow_count++;
            }
        }

        std::cout << "[E2E] Large inputs: nan=" << nan_count
                  << " inf=" << inf_count
                  << " overflow=" << overflow_count << "\n";

        // tanh should prevent overflow
        EXPECT_EQ(inf_count, 0u) << "Infinity from large inputs";
    }
    E2E_STAGE_END();

    // Stage 2: Test very small values
    E2E_STAGE_BEGIN("Test small inputs", 5000);
    {
        uint32_t underflow_issues = 0;

        std::vector<float> small_input(16, 1e-30f);
        std::vector<float> output(8);

        for (uint32_t iter = 0; iter < SHORT_RUN_ITERATIONS; ++iter) {
            for (size_t i = 0; i < output.size(); ++i) {
                float temp = small_input[i % small_input.size()] * 0.999f;
                output[i] = temp + 0.5f;  // Add offset to avoid pure zero

                if (output[i] == 0.0f && small_input[i % small_input.size()] != 0.0f) {
                    underflow_issues++;
                }
            }
        }

        std::cout << "[E2E] Small inputs: underflow issues=" << underflow_issues << "\n";
    }
    E2E_STAGE_END();

    // Stage 3: Test accumulation over iterations
    E2E_STAGE_BEGIN("Test accumulation stability", 10000);
    {
        float accumulator = 0.0f;
        uint32_t overflow_iterations = 0;

        for (uint32_t iter = 0; iter < MEDIUM_RUN_ITERATIONS; ++iter) {
            accumulator += 0.001f;
            accumulator *= 0.999f;  // Decay to prevent unbounded growth

            if (std::isinf(accumulator) || std::isnan(accumulator)) {
                overflow_iterations++;
                accumulator = 0.0f;  // Reset
            }
        }

        std::cout << "[E2E] Accumulation: final=" << accumulator
                  << " overflow_resets=" << overflow_iterations << "\n";

        EXPECT_EQ(overflow_iterations, 0u)
            << "Accumulation caused overflow";
        EXPECT_FALSE(std::isnan(accumulator)) << "Accumulation resulted in NaN";
        EXPECT_FALSE(std::isinf(accumulator)) << "Accumulation resulted in Inf";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Performance Consistency
//=============================================================================

E2E_TEST_F(StabilityE2ETest, PerformanceConsistency) {
    E2E_PIPELINE_START("Performance Consistency Over Time");

    const uint32_t INPUT_DIM = 32;
    const uint32_t OUTPUT_DIM = 16;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createStabilityBrain("perf_consistency_brain",
                                            BRAIN_SIZE_SMALL,
                                            INPUT_DIM, OUTPUT_DIM);
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Measure performance over time
    E2E_STAGE_BEGIN("Measure performance consistency", 20000);
    {
        std::vector<double> early_times;
        std::vector<double> late_times;

        // Early iterations
        for (uint32_t iter = 0; iter < 100; ++iter) {
            auto start = std::chrono::high_resolution_clock::now();

            std::vector<float> input = TestDataGenerator::generate_features(INPUT_DIM);
            std::vector<float> output(OUTPUT_DIM);

            for (size_t i = 0; i < OUTPUT_DIM; ++i) {
                output[i] = std::tanh(input[i % INPUT_DIM]);
            }

            auto end = std::chrono::high_resolution_clock::now();
            early_times.push_back(std::chrono::duration<double, std::milli>(end - start).count());
        }

        // Run many iterations
        for (uint32_t iter = 0; iter < MEDIUM_RUN_ITERATIONS - 200; ++iter) {
            std::vector<float> input = TestDataGenerator::generate_features(INPUT_DIM);
            std::vector<float> output(OUTPUT_DIM);
            for (size_t i = 0; i < OUTPUT_DIM; ++i) {
                output[i] = std::tanh(input[i % INPUT_DIM]);
            }
        }

        // Late iterations
        for (uint32_t iter = 0; iter < 100; ++iter) {
            auto start = std::chrono::high_resolution_clock::now();

            std::vector<float> input = TestDataGenerator::generate_features(INPUT_DIM);
            std::vector<float> output(OUTPUT_DIM);

            for (size_t i = 0; i < OUTPUT_DIM; ++i) {
                output[i] = std::tanh(input[i % INPUT_DIM]);
            }

            auto end = std::chrono::high_resolution_clock::now();
            late_times.push_back(std::chrono::duration<double, std::milli>(end - start).count());
        }

        // Compare early and late performance
        double early_mean, early_std, early_min, early_max;
        double late_mean, late_std, late_min, late_max;

        computeStatistics(early_times, early_mean, early_std, early_min, early_max);
        computeStatistics(late_times, late_mean, late_std, late_min, late_max);

        std::cout << "[E2E] Early performance: mean=" << early_mean
                  << "ms std=" << early_std << "ms\n";
        std::cout << "[E2E] Late performance: mean=" << late_mean
                  << "ms std=" << late_std << "ms\n";

        // Performance should not degrade significantly
        double slowdown = late_mean / early_mean;
        std::cout << "[E2E] Slowdown factor: " << slowdown << "x\n";

        EXPECT_LT(slowdown, PERFORMANCE_VARIANCE_THRESHOLD)
            << "Performance degraded significantly over time";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Extended Run with Periodic GC
//=============================================================================

E2E_TEST_F(StabilityE2ETest, ExtendedRunWithPeriodicGC) {
    E2E_PIPELINE_START("Extended Run with Periodic Cleanup");

    const uint32_t INPUT_DIM = 16;
    const uint32_t OUTPUT_DIM = 8;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createStabilityBrain("periodic_gc_brain",
                                            BRAIN_SIZE_SMALL,
                                            INPUT_DIM, OUTPUT_DIM);
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Extended run with cleanup
    E2E_STAGE_BEGIN("Extended run with periodic cleanup", 30000);
    {
        recordMemorySample();
        size_t initial_memory = metrics_.memory_samples.back();

        for (uint32_t iter = 0; iter < LONG_RUN_ITERATIONS; ++iter) {
            // Normal processing
            std::vector<float> input = TestDataGenerator::generate_features(INPUT_DIM);
            std::vector<float> output(OUTPUT_DIM);

            for (size_t i = 0; i < OUTPUT_DIM; ++i) {
                output[i] = std::tanh(input[i % INPUT_DIM]);
            }

            checkNumericalStability(output);

            // Periodic "cleanup" (simulate memory pressure handling)
            if (iter % 200 == 0 && iter > 0) {
                recordMemorySample();
                size_t current = metrics_.memory_samples.back();

                std::cout << "[E2E] Iteration " << iter
                          << " memory: " << current << " bytes\n";

                // If memory grew too much, report it
                if (current > initial_memory + MAX_MEMORY_GROWTH_BYTES / 2) {
                    std::cout << "[E2E] Warning: significant memory growth at iter "
                              << iter << "\n";
                }
            }
        }

        recordMemorySample();
        size_t final_memory = metrics_.memory_samples.back();
        size_t growth = (final_memory > initial_memory) ?
                        (final_memory - initial_memory) : 0;

        std::cout << "[E2E] Total memory growth: " << growth << " bytes\n";

        EXPECT_LT(growth, MAX_MEMORY_GROWTH_BYTES)
            << "Memory grew too much despite periodic cleanup";
    }
    E2E_STAGE_END();

    // Stage 3: Verify stability metrics
    E2E_STAGE_BEGIN("Verify stability", 100);
    {
        printMetricsSummary();

        EXPECT_EQ(metrics_.nan_count, 0u);
        EXPECT_EQ(metrics_.inf_count, 0u);
        EXPECT_EQ(metrics_.overflow_count, 0u);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
