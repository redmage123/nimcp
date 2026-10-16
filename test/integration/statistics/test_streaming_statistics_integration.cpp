/**
 * @file test_streaming_statistics_integration.cpp
 * @brief Integration tests for streaming statistics with batch statistics
 *
 * WHAT: Verify streaming statistics produce equivalent results to batch
 * WHY:  Ensure online algorithms maintain numerical stability and accuracy
 * HOW:  Compare streaming and batch computations on identical data
 *
 * TEST COVERAGE:
 * - Streaming + batch statistics equivalence
 * - Welford's algorithm numerical stability
 * - Running statistics merge operations
 * - Memory efficiency for large streams
 * - Real-time sensor data simulation
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <deque>

// Statistics headers
#include "utils/statistics/nimcp_statistics.h"

// Memory management
#include "utils/memory/nimcp_memory.h"

// Core types

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr float STRICT_TOLERANCE = 1e-5f;
    constexpr float RELAXED_TOLERANCE = 1e-4f;
    constexpr float STREAMING_TOLERANCE = 1e-3f;

    constexpr uint32_t SMALL_SIZE = 100;
    constexpr uint32_t MEDIUM_SIZE = 1000;
    constexpr uint32_t LARGE_SIZE = 100000;
    constexpr uint32_t VERY_LARGE_SIZE = 1000000;
}

//=============================================================================
// Test Fixture
//=============================================================================

class StreamingStatisticsIntegrationTest : public ::testing::Test {
protected:
    std::mt19937 rng{42};

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        nimcp_stats_config_t config = nimcp_stats_default_config();
        config.random_seed = 42;
        ASSERT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
    }

    void TearDown() override {
        nimcp_stats_shutdown();

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Memory leak: " << stats.current_allocated << " bytes";
    }

    //=========================================================================
    // Helper: Generate random data
    //=========================================================================
    std::vector<float> generateData(uint32_t n, float mean = 0.0f, float std = 1.0f) {
        std::vector<float> data(n);
        std::normal_distribution<float> dist(mean, std);
        for (uint32_t i = 0; i < n; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    //=========================================================================
    // Helper: Generate sensor-like streaming data
    //=========================================================================
    std::vector<float> generateSensorData(uint32_t n, float baseline = 100.0f,
                                           float drift_rate = 0.001f) {
        std::vector<float> data(n);
        std::normal_distribution<float> noise(0.0f, 2.0f);

        float current = baseline;
        for (uint32_t i = 0; i < n; i++) {
            current += drift_rate + noise(rng) * 0.1f;
            data[i] = current + noise(rng);
        }
        return data;
    }

    //=========================================================================
    // Helper: Compare statistics
    //=========================================================================
    bool statsEqual(float batch, float streaming, float tolerance) {
        if (std::isnan(batch) && std::isnan(streaming)) return true;
        if (std::isnan(batch) || std::isnan(streaming)) return false;
        return std::fabs(batch - streaming) < tolerance;
    }
};

//=============================================================================
// Basic Streaming vs Batch Equivalence Tests
//=============================================================================

TEST_F(StreamingStatisticsIntegrationTest, MeanEquivalence) {
    auto data = generateData(MEDIUM_SIZE, 50.0f, 10.0f);

    // Batch computation
    float batch_mean = nimcp_stats_mean(data.data(), MEDIUM_SIZE);

    // Streaming computation
    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    for (uint32_t i = 0; i < MEDIUM_SIZE; i++) {
        nimcp_stats_running_add(&running, data[i]);
    }
    double streaming_mean = nimcp_stats_running_mean(&running);

    EXPECT_NEAR(batch_mean, static_cast<float>(streaming_mean), RELAXED_TOLERANCE)
        << "Streaming mean should match batch mean";
}

TEST_F(StreamingStatisticsIntegrationTest, VarianceEquivalence) {
    auto data = generateData(MEDIUM_SIZE, 0.0f, 5.0f);

    float batch_var = nimcp_stats_variance(data.data(), MEDIUM_SIZE);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), MEDIUM_SIZE);
    double streaming_var = nimcp_stats_running_variance(&running);

    EXPECT_NEAR(batch_var, static_cast<float>(streaming_var), RELAXED_TOLERANCE)
        << "Streaming variance should match batch variance";
}

TEST_F(StreamingStatisticsIntegrationTest, StdDevEquivalence) {
    auto data = generateData(MEDIUM_SIZE, 100.0f, 20.0f);

    float batch_std = nimcp_stats_std_dev(data.data(), MEDIUM_SIZE);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), MEDIUM_SIZE);
    double streaming_std = nimcp_stats_running_std_dev(&running);

    EXPECT_NEAR(batch_std, static_cast<float>(streaming_std), RELAXED_TOLERANCE)
        << "Streaming std dev should match batch std dev";
}

TEST_F(StreamingStatisticsIntegrationTest, SkewnessEquivalence) {
    auto data = generateData(LARGE_SIZE, 0.0f, 1.0f);

    float batch_skew = nimcp_stats_skewness(data.data(), LARGE_SIZE);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), LARGE_SIZE);
    double streaming_skew = nimcp_stats_running_skewness(&running);

    EXPECT_NEAR(batch_skew, static_cast<float>(streaming_skew), STREAMING_TOLERANCE)
        << "Streaming skewness should match batch skewness";
}

TEST_F(StreamingStatisticsIntegrationTest, KurtosisEquivalence) {
    auto data = generateData(LARGE_SIZE, 0.0f, 1.0f);

    float batch_kurt = nimcp_stats_kurtosis(data.data(), LARGE_SIZE);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), LARGE_SIZE);
    double streaming_kurt = nimcp_stats_running_kurtosis(&running);

    EXPECT_NEAR(batch_kurt, static_cast<float>(streaming_kurt), STREAMING_TOLERANCE)
        << "Streaming kurtosis should match batch kurtosis";
}

TEST_F(StreamingStatisticsIntegrationTest, MinMaxTracking) {
    auto data = generateData(MEDIUM_SIZE, 0.0f, 10.0f);

    float batch_min = nimcp_stats_min(data.data(), MEDIUM_SIZE);
    float batch_max = nimcp_stats_max(data.data(), MEDIUM_SIZE);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), MEDIUM_SIZE);

    EXPECT_NEAR(batch_min, static_cast<float>(running.min), STRICT_TOLERANCE);
    EXPECT_NEAR(batch_max, static_cast<float>(running.max), STRICT_TOLERANCE);
}

//=============================================================================
// Incremental Update Tests
//=============================================================================

TEST_F(StreamingStatisticsIntegrationTest, IncrementalMeanStability) {
    auto data = generateData(LARGE_SIZE, 1000.0f, 1.0f);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    // Add one at a time and verify intermediate means
    std::vector<double> intermediate_means;
    for (uint32_t i = 0; i < LARGE_SIZE; i++) {
        nimcp_stats_running_add(&running, data[i]);
        intermediate_means.push_back(nimcp_stats_running_mean(&running));
    }

    // Final mean should match batch
    float batch_mean = nimcp_stats_mean(data.data(), LARGE_SIZE);
    EXPECT_NEAR(batch_mean, static_cast<float>(intermediate_means.back()), RELAXED_TOLERANCE);

    // Intermediate means should converge smoothly
    for (size_t i = 100; i < intermediate_means.size(); i++) {
        float delta = std::fabs(intermediate_means[i] - intermediate_means[i-1]);
        EXPECT_LT(delta, 1.0f) << "Mean should change smoothly";
    }
}

TEST_F(StreamingStatisticsIntegrationTest, ChunkedUpdates) {
    auto data = generateData(LARGE_SIZE, 0.0f, 5.0f);

    // Add in chunks of various sizes
    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    std::vector<uint32_t> chunk_sizes = {10, 50, 100, 500, 1000, 5000};
    uint32_t offset = 0;

    for (size_t c = 0; c < chunk_sizes.size() && offset < LARGE_SIZE; c++) {
        uint32_t size = std::min(chunk_sizes[c], LARGE_SIZE - offset);
        nimcp_stats_running_add_array(&running, data.data() + offset, size);
        offset += size;
    }

    // Add remaining
    if (offset < LARGE_SIZE) {
        nimcp_stats_running_add_array(&running, data.data() + offset, LARGE_SIZE - offset);
    }

    // Should match batch exactly
    float batch_mean = nimcp_stats_mean(data.data(), LARGE_SIZE);
    float batch_var = nimcp_stats_variance(data.data(), LARGE_SIZE);

    EXPECT_NEAR(batch_mean, static_cast<float>(nimcp_stats_running_mean(&running)), RELAXED_TOLERANCE);
    EXPECT_NEAR(batch_var, static_cast<float>(nimcp_stats_running_variance(&running)), RELAXED_TOLERANCE);
}

//=============================================================================
// Merge Operation Tests
//=============================================================================

TEST_F(StreamingStatisticsIntegrationTest, MergeTwoPartitions) {
    auto data = generateData(MEDIUM_SIZE, 50.0f, 10.0f);

    // Split into two halves
    uint32_t split = MEDIUM_SIZE / 2;

    nimcp_running_stats_t part1, part2;
    nimcp_stats_running_init(&part1);
    nimcp_stats_running_init(&part2);

    nimcp_stats_running_add_array(&part1, data.data(), split);
    nimcp_stats_running_add_array(&part2, data.data() + split, MEDIUM_SIZE - split);

    // Merge
    nimcp_stats_running_merge(&part1, &part2);

    // Should match full computation
    float batch_mean = nimcp_stats_mean(data.data(), MEDIUM_SIZE);
    float batch_var = nimcp_stats_variance(data.data(), MEDIUM_SIZE);

    EXPECT_NEAR(batch_mean, static_cast<float>(nimcp_stats_running_mean(&part1)), RELAXED_TOLERANCE);
    EXPECT_NEAR(batch_var, static_cast<float>(nimcp_stats_running_variance(&part1)), RELAXED_TOLERANCE);
}

TEST_F(StreamingStatisticsIntegrationTest, MergeMultiplePartitions) {
    auto data = generateData(LARGE_SIZE, 0.0f, 1.0f);

    // Split into 10 partitions
    uint32_t n_parts = 10;
    uint32_t part_size = LARGE_SIZE / n_parts;

    std::vector<nimcp_running_stats_t> partitions(n_parts);
    for (uint32_t p = 0; p < n_parts; p++) {
        nimcp_stats_running_init(&partitions[p]);
        uint32_t start = p * part_size;
        uint32_t size = (p == n_parts - 1) ? LARGE_SIZE - start : part_size;
        nimcp_stats_running_add_array(&partitions[p], data.data() + start, size);
    }

    // Merge all into first
    for (uint32_t p = 1; p < n_parts; p++) {
        nimcp_stats_running_merge(&partitions[0], &partitions[p]);
    }

    // Verify
    float batch_mean = nimcp_stats_mean(data.data(), LARGE_SIZE);
    float batch_var = nimcp_stats_variance(data.data(), LARGE_SIZE);
    float batch_skew = nimcp_stats_skewness(data.data(), LARGE_SIZE);

    EXPECT_NEAR(batch_mean, static_cast<float>(nimcp_stats_running_mean(&partitions[0])),
                RELAXED_TOLERANCE);
    EXPECT_NEAR(batch_var, static_cast<float>(nimcp_stats_running_variance(&partitions[0])),
                RELAXED_TOLERANCE);
    EXPECT_NEAR(batch_skew, static_cast<float>(nimcp_stats_running_skewness(&partitions[0])),
                STREAMING_TOLERANCE);
}

TEST_F(StreamingStatisticsIntegrationTest, MergeAssociativity) {
    auto data = generateData(MEDIUM_SIZE, 100.0f, 20.0f);

    // Split into 3 parts
    uint32_t p1_end = MEDIUM_SIZE / 3;
    uint32_t p2_end = 2 * MEDIUM_SIZE / 3;

    nimcp_running_stats_t a1, a2, a3;
    nimcp_running_stats_t b1, b2, b3;
    nimcp_stats_running_init(&a1); nimcp_stats_running_init(&a2); nimcp_stats_running_init(&a3);
    nimcp_stats_running_init(&b1); nimcp_stats_running_init(&b2); nimcp_stats_running_init(&b3);

    nimcp_stats_running_add_array(&a1, data.data(), p1_end);
    nimcp_stats_running_add_array(&a2, data.data() + p1_end, p2_end - p1_end);
    nimcp_stats_running_add_array(&a3, data.data() + p2_end, MEDIUM_SIZE - p2_end);

    nimcp_stats_running_add_array(&b1, data.data(), p1_end);
    nimcp_stats_running_add_array(&b2, data.data() + p1_end, p2_end - p1_end);
    nimcp_stats_running_add_array(&b3, data.data() + p2_end, MEDIUM_SIZE - p2_end);

    // Merge in different orders: (1+2)+3 vs 1+(2+3)
    nimcp_stats_running_merge(&a1, &a2);
    nimcp_stats_running_merge(&a1, &a3);

    nimcp_stats_running_merge(&b2, &b3);
    nimcp_stats_running_merge(&b1, &b2);

    // Results should be identical
    EXPECT_NEAR(nimcp_stats_running_mean(&a1), nimcp_stats_running_mean(&b1), STRICT_TOLERANCE)
        << "Merge should be associative";
    EXPECT_NEAR(nimcp_stats_running_variance(&a1), nimcp_stats_running_variance(&b1), RELAXED_TOLERANCE)
        << "Merge should be associative";
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(StreamingStatisticsIntegrationTest, LargeValuesStability) {
    // Test with large values that could cause overflow in naive algorithms
    std::vector<float> data(MEDIUM_SIZE);
    std::normal_distribution<float> dist(1e6f, 1.0f);

    for (uint32_t i = 0; i < MEDIUM_SIZE; i++) {
        data[i] = dist(rng);
    }

    float batch_mean = nimcp_stats_mean(data.data(), MEDIUM_SIZE);
    float batch_var = nimcp_stats_variance(data.data(), MEDIUM_SIZE);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), MEDIUM_SIZE);

    EXPECT_NEAR(batch_mean, static_cast<float>(nimcp_stats_running_mean(&running)), 1.0f);
    EXPECT_NEAR(batch_var, static_cast<float>(nimcp_stats_running_variance(&running)), RELAXED_TOLERANCE);
}

TEST_F(StreamingStatisticsIntegrationTest, SmallVarianceStability) {
    // Test with very small variance
    std::vector<float> data(MEDIUM_SIZE);
    std::normal_distribution<float> dist(100.0f, 0.001f);

    for (uint32_t i = 0; i < MEDIUM_SIZE; i++) {
        data[i] = dist(rng);
    }

    float batch_var = nimcp_stats_variance(data.data(), MEDIUM_SIZE);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), MEDIUM_SIZE);
    double streaming_var = nimcp_stats_running_variance(&running);

    // Should capture small variance without numerical issues
    EXPECT_GT(batch_var, 0.0f);
    EXPECT_GT(streaming_var, 0.0);
    EXPECT_NEAR(batch_var, static_cast<float>(streaming_var), batch_var * 0.1f);
}

TEST_F(StreamingStatisticsIntegrationTest, ConstantDataStability) {
    // Constant data should give zero variance
    std::vector<float> constant(MEDIUM_SIZE, 42.0f);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, constant.data(), MEDIUM_SIZE);

    double mean = nimcp_stats_running_mean(&running);
    double var = nimcp_stats_running_variance(&running);

    EXPECT_NEAR(mean, 42.0, STRICT_TOLERANCE);
    EXPECT_NEAR(var, 0.0, STRICT_TOLERANCE);
}

TEST_F(StreamingStatisticsIntegrationTest, AlternatingSignsStability) {
    // Alternating positive/negative values
    std::vector<float> data(MEDIUM_SIZE);
    for (uint32_t i = 0; i < MEDIUM_SIZE; i++) {
        data[i] = (i % 2 == 0) ? 1000.0f : -1000.0f;
    }

    float batch_mean = nimcp_stats_mean(data.data(), MEDIUM_SIZE);
    float batch_var = nimcp_stats_variance(data.data(), MEDIUM_SIZE);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), MEDIUM_SIZE);

    EXPECT_NEAR(batch_mean, static_cast<float>(nimcp_stats_running_mean(&running)), RELAXED_TOLERANCE);
    EXPECT_NEAR(batch_var, static_cast<float>(nimcp_stats_running_variance(&running)), 1.0f);
}

//=============================================================================
// Sliding Window Statistics Tests
//=============================================================================

TEST_F(StreamingStatisticsIntegrationTest, SlidingWindowMean) {
    auto data = generateSensorData(LARGE_SIZE);
    uint32_t window_size = 100;

    // Compute sliding window means
    std::vector<float> window_means;

    for (uint32_t i = window_size; i <= LARGE_SIZE; i++) {
        float mean = nimcp_stats_mean(data.data() + i - window_size, window_size);
        window_means.push_back(mean);
    }

    // Verify means are finite and reasonable
    for (float m : window_means) {
        EXPECT_TRUE(std::isfinite(m));
    }

    // Mean should drift upward (due to drift_rate in sensor data)
    float first_avg = nimcp_stats_mean(window_means.data(), 100);
    float last_avg = nimcp_stats_mean(window_means.data() + window_means.size() - 100, 100);
    EXPECT_GT(last_avg, first_avg) << "Sensor data should show positive drift";
}

TEST_F(StreamingStatisticsIntegrationTest, ExponentialMovingAverage) {
    auto data = generateSensorData(MEDIUM_SIZE);
    float alpha = 0.1f;  // Smoothing factor

    // Compute EMA manually
    std::vector<float> ema(MEDIUM_SIZE);
    ema[0] = data[0];
    for (uint32_t i = 1; i < MEDIUM_SIZE; i++) {
        ema[i] = alpha * data[i] + (1.0f - alpha) * ema[i-1];
    }

    // EMA should be smoother than raw data
    float data_var = nimcp_stats_variance(data.data(), MEDIUM_SIZE);
    float ema_var = nimcp_stats_variance(ema.data(), MEDIUM_SIZE);

    EXPECT_LT(ema_var, data_var) << "EMA should be smoother than raw data";
}

//=============================================================================
// Real-Time Sensor Simulation Tests
//=============================================================================

TEST_F(StreamingStatisticsIntegrationTest, RealTimeSensorMonitoring) {
    // Simulate real-time sensor readings
    uint32_t n_readings = 10000;
    float sample_rate = 100.0f;  // 100 Hz

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    std::normal_distribution<float> noise(0.0f, 0.5f);
    float baseline = 25.0f;  // Temperature in Celsius

    std::vector<float> anomaly_scores;

    for (uint32_t i = 0; i < n_readings; i++) {
        // Generate reading
        float reading = baseline + noise(rng);

        // Inject anomaly at i=5000
        if (i >= 5000 && i < 5010) {
            reading += 10.0f;  // Temperature spike
        }

        // Update stats
        double prev_mean = (running.n > 0) ? nimcp_stats_running_mean(&running) : reading;
        double prev_std = (running.n > 1) ? nimcp_stats_running_std_dev(&running) : 1.0f;

        nimcp_stats_running_add(&running, reading);

        // Compute anomaly score (z-score)
        if (running.n > 10) {
            float z_score = std::fabs(reading - prev_mean) / prev_std;
            anomaly_scores.push_back(z_score);
        }
    }

    // Should detect anomalies around i=5000
    float max_score = *std::max_element(anomaly_scores.begin(), anomaly_scores.end());
    EXPECT_GT(max_score, 3.0f) << "Should detect anomaly with high z-score";

    // Most scores should be small
    float median_score = nimcp_stats_median(anomaly_scores.data(),
                                             static_cast<uint32_t>(anomaly_scores.size()));
    EXPECT_LT(median_score, 1.5f) << "Most readings should be normal";
}

TEST_F(StreamingStatisticsIntegrationTest, MultiSensorFusion) {
    // Simulate multiple sensors measuring same quantity
    uint32_t n = MEDIUM_SIZE;
    uint32_t n_sensors = 5;

    std::vector<nimcp_running_stats_t> sensor_stats(n_sensors);
    for (auto& s : sensor_stats) {
        nimcp_stats_running_init(&s);
    }

    std::normal_distribution<float> signal(100.0f, 0.1f);
    std::vector<std::normal_distribution<float>> sensor_noise;
    for (uint32_t s = 0; s < n_sensors; s++) {
        sensor_noise.emplace_back(0.0f, 0.5f + 0.1f * s);  // Varying noise levels
    }

    for (uint32_t i = 0; i < n; i++) {
        float true_value = signal(rng);
        for (uint32_t s = 0; s < n_sensors; s++) {
            float reading = true_value + sensor_noise[s](rng);
            nimcp_stats_running_add(&sensor_stats[s], reading);
        }
    }

    // Compute weighted average based on inverse variance
    double weighted_sum = 0.0;
    double weight_sum = 0.0;
    for (const auto& s : sensor_stats) {
        double var = nimcp_stats_running_variance(&s);
        if (var > 0) {
            double weight = 1.0 / var;
            weighted_sum += weight * nimcp_stats_running_mean(&s);
            weight_sum += weight;
        }
    }

    double fused_estimate = weighted_sum / weight_sum;

    // Fused estimate should be close to true mean (100.0)
    EXPECT_NEAR(fused_estimate, 100.0, 0.5);
}

//=============================================================================
// Memory Efficiency Tests
//=============================================================================

TEST_F(StreamingStatisticsIntegrationTest, MemoryConstantForStreaming) {
    nimcp_memory_clear_stats();

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    // Process large stream without storing data
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (uint32_t i = 0; i < VERY_LARGE_SIZE; i++) {
        float value = dist(rng);
        nimcp_stats_running_add(&running, value);
    }

    // Memory should be constant (just the running stats struct)
    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 1024)
        << "Streaming should use constant memory";

    // Stats should still be valid
    EXPECT_TRUE(std::isfinite(nimcp_stats_running_mean(&running)));
    EXPECT_TRUE(std::isfinite(nimcp_stats_running_variance(&running)));
}

TEST_F(StreamingStatisticsIntegrationTest, NoMemoryLeaksRepeatedInit) {
    nimcp_memory_clear_stats();

    for (int trial = 0; trial < 1000; trial++) {
        nimcp_running_stats_t running;
        nimcp_stats_running_init(&running);

        // Add some data
        for (int i = 0; i < 100; i++) {
            nimcp_stats_running_add(&running, static_cast<float>(i));
        }
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 1024);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(StreamingStatisticsIntegrationTest, StreamingPerformance) {
    auto data = generateData(VERY_LARGE_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), VERY_LARGE_SIZE);

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t streaming_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    EXPECT_LT(streaming_us, 100000u)
        << "1M point streaming should complete in <100ms";

    // Verify results are valid
    EXPECT_TRUE(std::isfinite(nimcp_stats_running_mean(&running)));
    EXPECT_TRUE(std::isfinite(nimcp_stats_running_variance(&running)));
}

TEST_F(StreamingStatisticsIntegrationTest, BatchVsStreamingSpeed) {
    auto data = generateData(LARGE_SIZE);

    // Time batch computation
    auto start_batch = std::chrono::high_resolution_clock::now();
    float batch_mean = nimcp_stats_mean(data.data(), LARGE_SIZE);
    float batch_var = nimcp_stats_variance(data.data(), LARGE_SIZE);
    auto end_batch = std::chrono::high_resolution_clock::now();
    uint64_t batch_us = std::chrono::duration_cast<std::chrono::microseconds>(end_batch - start_batch).count();

    // Time streaming computation
    auto start_stream = std::chrono::high_resolution_clock::now();
    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), LARGE_SIZE);
    double stream_mean = nimcp_stats_running_mean(&running);
    double stream_var = nimcp_stats_running_variance(&running);
    auto end_stream = std::chrono::high_resolution_clock::now();
    uint64_t stream_us = std::chrono::duration_cast<std::chrono::microseconds>(end_stream - start_stream).count();

    // Results should match
    EXPECT_NEAR(batch_mean, static_cast<float>(stream_mean), RELAXED_TOLERANCE);
    EXPECT_NEAR(batch_var, static_cast<float>(stream_var), RELAXED_TOLERANCE);

    // Streaming should be comparable in speed (single pass)
    EXPECT_LT(stream_us, batch_us * 3) << "Streaming should not be much slower than batch";
}

TEST_F(StreamingStatisticsIntegrationTest, MergePerformance) {
    auto data = generateData(LARGE_SIZE);
    uint32_t n_partitions = 100;
    uint32_t part_size = LARGE_SIZE / n_partitions;

    // Create partitions
    std::vector<nimcp_running_stats_t> partitions(n_partitions);
    for (uint32_t p = 0; p < n_partitions; p++) {
        nimcp_stats_running_init(&partitions[p]);
        nimcp_stats_running_add_array(&partitions[p], data.data() + p * part_size, part_size);
    }

    // Time merge operations
    auto start = std::chrono::high_resolution_clock::now();
    for (uint32_t p = 1; p < n_partitions; p++) {
        nimcp_stats_running_merge(&partitions[0], &partitions[p]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    uint64_t merge_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    EXPECT_LT(merge_us, 1000u) << "100 merges should be fast";

    // Verify merged result
    float batch_mean = nimcp_stats_mean(data.data(), LARGE_SIZE);
    EXPECT_NEAR(batch_mean, static_cast<float>(nimcp_stats_running_mean(&partitions[0])),
                RELAXED_TOLERANCE);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(StreamingStatisticsIntegrationTest, SingleValue) {
    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add(&running, 42.0f);

    EXPECT_EQ(running.n, 1u);
    EXPECT_NEAR(nimcp_stats_running_mean(&running), 42.0, STRICT_TOLERANCE);
    EXPECT_TRUE(std::isnan(nimcp_stats_running_variance(&running)))
        << "Variance undefined for n=1";
}

TEST_F(StreamingStatisticsIntegrationTest, TwoValues) {
    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add(&running, 1.0f);
    nimcp_stats_running_add(&running, 3.0f);

    EXPECT_EQ(running.n, 2u);
    EXPECT_NEAR(nimcp_stats_running_mean(&running), 2.0, STRICT_TOLERANCE);
    EXPECT_NEAR(nimcp_stats_running_variance(&running), 2.0, STRICT_TOLERANCE);  // (1-2)^2 + (3-2)^2 = 2
}

TEST_F(StreamingStatisticsIntegrationTest, EmptyStats) {
    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    EXPECT_EQ(running.n, 0u);
    EXPECT_TRUE(std::isnan(nimcp_stats_running_mean(&running)));
    EXPECT_TRUE(std::isnan(nimcp_stats_running_variance(&running)));
}

TEST_F(StreamingStatisticsIntegrationTest, MergeWithEmpty) {
    nimcp_running_stats_t full, empty;
    nimcp_stats_running_init(&full);
    nimcp_stats_running_init(&empty);

    nimcp_stats_running_add(&full, 1.0f);
    nimcp_stats_running_add(&full, 2.0f);
    nimcp_stats_running_add(&full, 3.0f);

    double mean_before = nimcp_stats_running_mean(&full);
    double var_before = nimcp_stats_running_variance(&full);

    nimcp_stats_running_merge(&full, &empty);

    // Should be unchanged
    EXPECT_NEAR(nimcp_stats_running_mean(&full), mean_before, STRICT_TOLERANCE);
    EXPECT_NEAR(nimcp_stats_running_variance(&full), var_before, STRICT_TOLERANCE);
}

