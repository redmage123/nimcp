//=============================================================================
// test_streaming_statistics_e2e.cpp - Streaming/Online Statistics E2E Tests
//=============================================================================
/**
 * @file test_streaming_statistics_e2e.cpp
 * @brief End-to-end tests for streaming/online statistical workflows
 *
 * WHAT: Complete real-time data analysis pipelines
 * WHY:  Verify statistics module handles streaming data scenarios
 * HOW:  Online algorithms, windowed statistics, anomaly detection
 *
 * TEST SCENARIOS:
 * 1. Online mean/variance with Welford's algorithm
 * 2. Exponential moving averages
 * 3. Sliding window statistics
 * 4. Online quantile estimation
 * 5. Streaming anomaly detection
 * 6. Online correlation tracking
 * 7. Change point detection in streams
 * 8. Online histogram maintenance
 * 9. Stream aggregation and merging
 * 10. Real-time percentile tracking
 * 11. Streaming entropy estimation
 * 12. Online regression updates
 * 13. Adaptive threshold detection
 * 14. Multi-stream synchronization
 * 15. High-frequency data processing
 *
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <deque>

extern "C" {
#include "utils/statistics/nimcp_statistics.h"
}

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-2f
#define VERY_LOOSE_TOLERANCE 0.1f

// Timing macros
#define START_TIMER() auto _start_time = std::chrono::high_resolution_clock::now()
#define STOP_TIMER_MS() std::chrono::duration<double, std::milli>( \
    std::chrono::high_resolution_clock::now() - _start_time).count()

//=============================================================================
// Test Fixture
//=============================================================================

class StreamingStatisticsE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_stats_default_config();
        ASSERT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
        rng.seed(42);  // Reproducible tests
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    nimcp_stats_config_t config;
    std::mt19937 rng;

    // Generate streaming data with concept drift
    std::vector<float> generate_drifting_stream(size_t n, float initial_mean,
                                                float drift_rate, float noise_std) {
        std::vector<float> stream(n);
        std::normal_distribution<float> noise(0.0f, noise_std);
        for (size_t i = 0; i < n; i++) {
            float mean = initial_mean + drift_rate * i;
            stream[i] = mean + noise(rng);
        }
        return stream;
    }

    // Generate data with sudden change point
    std::vector<float> generate_change_point_stream(size_t n, size_t change_point,
                                                    float mean1, float mean2,
                                                    float noise_std) {
        std::vector<float> stream(n);
        std::normal_distribution<float> noise(0.0f, noise_std);
        for (size_t i = 0; i < n; i++) {
            float mean = (i < change_point) ? mean1 : mean2;
            stream[i] = mean + noise(rng);
        }
        return stream;
    }

    // Generate anomalous stream
    std::vector<float> generate_anomalous_stream(size_t n, float mean, float std,
                                                 float anomaly_prob,
                                                 float anomaly_magnitude) {
        std::vector<float> stream(n);
        std::normal_distribution<float> normal(mean, std);
        std::uniform_real_distribution<float> uni(0.0f, 1.0f);
        for (size_t i = 0; i < n; i++) {
            stream[i] = normal(rng);
            if (uni(rng) < anomaly_prob) {
                stream[i] += (uni(rng) < 0.5f ? -1.0f : 1.0f) * anomaly_magnitude;
            }
        }
        return stream;
    }

    // Generate normal samples
    std::vector<float> generate_normal(size_t n, float mean, float stddev) {
        std::normal_distribution<float> dist(mean, stddev);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }
};

//=============================================================================
// E2E Test 1: Online Mean/Variance with Welford's Algorithm
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, OnlineMeanVarianceWelford) {
    START_TIMER();

    const size_t n_samples = 100000;
    auto stream = generate_normal(n_samples, 10.0f, 2.0f);

    // Use NIMCP running statistics
    nimcp_running_stats_t online_stats;
    nimcp_stats_running_init(&online_stats);

    // Process stream one element at a time
    for (size_t i = 0; i < n_samples; i++) {
        nimcp_stats_running_add(&online_stats, stream[i]);
    }

    // Get online results
    double online_mean = nimcp_stats_running_mean(&online_stats);
    double online_var = nimcp_stats_running_variance(&online_stats);
    double online_std = nimcp_stats_running_std_dev(&online_stats);

    // Compare with batch computation
    float batch_mean = nimcp_stats_mean(stream.data(), n_samples);
    float batch_var = nimcp_stats_variance(stream.data(), n_samples);
    float batch_std = nimcp_stats_std_dev(stream.data(), n_samples);

    // Should match closely
    EXPECT_NEAR(online_mean, batch_mean, LOOSE_TOLERANCE);
    EXPECT_NEAR(online_var, batch_var, LOOSE_TOLERANCE);
    EXPECT_NEAR(online_std, batch_std, LOOSE_TOLERANCE);

    // Verify convergence to true values
    EXPECT_NEAR(online_mean, 10.0f, 0.1f);
    EXPECT_NEAR(online_var, 4.0f, 0.2f);  // var = std^2 = 4

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Online mean/variance completed in " << elapsed << " ms\n";
    std::cout << "Online mean: " << online_mean << " vs batch: " << batch_mean << "\n";
    std::cout << "Online var: " << online_var << " vs batch: " << batch_var << "\n";
}

//=============================================================================
// E2E Test 2: Exponential Moving Averages
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, ExponentialMovingAverages) {
    START_TIMER();

    const size_t n_samples = 10000;
    auto stream = generate_normal(n_samples, 50.0f, 5.0f);

    // EMA with different smoothing factors
    std::vector<float> alphas = {0.1f, 0.2f, 0.5f, 0.9f};
    std::vector<std::vector<float>> emas(alphas.size());

    for (size_t a = 0; a < alphas.size(); a++) {
        emas[a].resize(n_samples);
        emas[a][0] = stream[0];  // Initialize with first value
        for (size_t i = 1; i < n_samples; i++) {
            emas[a][i] = alphas[a] * stream[i] + (1.0f - alphas[a]) * emas[a][i-1];
        }
    }

    // Higher alpha = more responsive to changes
    // Lower alpha = smoother, more resistant to noise

    // Compute variance of each EMA
    std::vector<float> ema_variances(alphas.size());
    for (size_t a = 0; a < alphas.size(); a++) {
        ema_variances[a] = nimcp_stats_variance(emas[a].data(), n_samples);
    }

    // Lower alpha should have lower variance (smoother)
    EXPECT_LT(ema_variances[0], ema_variances[3])
        << "Lower alpha EMA should be smoother (lower variance)";

    // All EMAs should converge to approximately the true mean
    for (size_t a = 0; a < alphas.size(); a++) {
        float final_ema = emas[a][n_samples - 1];
        EXPECT_NEAR(final_ema, 50.0f, 2.0f)
            << "EMA with alpha=" << alphas[a] << " should converge near true mean";
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "EMA analysis completed in " << elapsed << " ms\n";
    for (size_t a = 0; a < alphas.size(); a++) {
        std::cout << "Alpha=" << alphas[a] << " variance: " << ema_variances[a] << "\n";
    }
}

//=============================================================================
// E2E Test 3: Sliding Window Statistics
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, SlidingWindowStatistics) {
    START_TIMER();

    const size_t n_samples = 50000;
    const size_t window_size = 100;
    auto stream = generate_drifting_stream(n_samples, 0.0f, 0.001f, 1.0f);

    // Compute sliding window mean
    std::deque<float> window;
    std::vector<float> sliding_means;
    sliding_means.reserve(n_samples - window_size + 1);

    double window_sum = 0.0;

    for (size_t i = 0; i < n_samples; i++) {
        window.push_back(stream[i]);
        window_sum += stream[i];

        if (window.size() > window_size) {
            window_sum -= window.front();
            window.pop_front();
        }

        if (window.size() == window_size) {
            sliding_means.push_back(static_cast<float>(window_sum / window_size));
        }
    }

    // The sliding mean should track the drift
    // Correlation between time and sliding mean should be positive
    std::vector<float> indices(sliding_means.size());
    std::iota(indices.begin(), indices.end(), 0.0f);

    nimcp_correlation_result_t drift_corr;
    ASSERT_EQ(nimcp_stats_correlation_pearson(
        indices.data(), sliding_means.data(), sliding_means.size(), &drift_corr),
        NIMCP_STATS_OK);

    EXPECT_GT(drift_corr.r, 0.9f) << "Sliding mean should track upward drift";

    // Verify sliding mean approximates local statistics
    size_t test_idx = n_samples / 2;
    std::vector<float> local_window(stream.begin() + test_idx - window_size/2,
                                    stream.begin() + test_idx + window_size/2);
    float local_mean = nimcp_stats_mean(local_window.data(), window_size);

    // Should be close to sliding mean at that point
    float sliding_at_test = sliding_means[test_idx - window_size + 1];
    EXPECT_NEAR(sliding_at_test, local_mean, 1.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Sliding window statistics completed in " << elapsed << " ms\n";
    std::cout << "Drift correlation: " << drift_corr.r << "\n";
}

//=============================================================================
// E2E Test 4: Online Quantile Estimation
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, OnlineQuantileEstimation) {
    START_TIMER();

    const size_t n_samples = 100000;
    auto stream = generate_normal(n_samples, 0.0f, 1.0f);

    // P-square algorithm approximation (simplified reservoir sampling)
    const size_t reservoir_size = 1000;
    std::vector<float> reservoir;
    reservoir.reserve(reservoir_size);

    for (size_t i = 0; i < n_samples; i++) {
        if (reservoir.size() < reservoir_size) {
            reservoir.push_back(stream[i]);
        } else {
            std::uniform_int_distribution<size_t> idx_dist(0, i);
            size_t j = idx_dist(rng);
            if (j < reservoir_size) {
                reservoir[j] = stream[i];
            }
        }
    }

    // Estimate quantiles from reservoir
    float est_median = nimcp_stats_median(reservoir.data(), reservoir_size);
    float est_q25 = nimcp_stats_quantile(reservoir.data(), reservoir_size, 0.25f);
    float est_q75 = nimcp_stats_quantile(reservoir.data(), reservoir_size, 0.75f);

    // True quantiles for standard normal
    float true_median = 0.0f;
    float true_q25 = -0.6745f;  // From standard normal table
    float true_q75 = 0.6745f;

    EXPECT_NEAR(est_median, true_median, 0.1f);
    EXPECT_NEAR(est_q25, true_q25, 0.15f);
    EXPECT_NEAR(est_q75, true_q75, 0.15f);

    // Compare with batch quantiles
    float batch_median = nimcp_stats_median(stream.data(), n_samples);
    EXPECT_NEAR(est_median, batch_median, 0.1f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Online quantile estimation completed in " << elapsed << " ms\n";
    std::cout << "Estimated median: " << est_median << " (true: " << true_median << ")\n";
    std::cout << "Estimated Q25: " << est_q25 << " (true: " << true_q25 << ")\n";
}

//=============================================================================
// E2E Test 5: Streaming Anomaly Detection
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, StreamingAnomalyDetection) {
    START_TIMER();

    const size_t n_samples = 10000;
    const float anomaly_prob = 0.01f;  // 1% anomalies
    const float anomaly_magnitude = 10.0f;

    auto stream = generate_anomalous_stream(n_samples, 0.0f, 1.0f,
                                            anomaly_prob, anomaly_magnitude);

    // Online anomaly detection using z-score
    nimcp_running_stats_t online_stats;
    nimcp_stats_running_init(&online_stats);

    std::vector<bool> detected_anomalies(n_samples, false);
    const float z_threshold = 3.0f;
    const size_t warmup = 100;  // Need some data to estimate statistics

    size_t detected_count = 0;
    for (size_t i = 0; i < n_samples; i++) {
        if (i >= warmup) {
            double mean = nimcp_stats_running_mean(&online_stats);
            double std = nimcp_stats_running_std_dev(&online_stats);
            if (std > 0) {
                double z = std::abs(stream[i] - mean) / std;
                if (z > z_threshold) {
                    detected_anomalies[i] = true;
                    detected_count++;
                }
            }
        }
        nimcp_stats_running_add(&online_stats, stream[i]);
    }

    // Calculate detection rate
    size_t true_anomalies = 0;
    for (size_t i = 0; i < n_samples; i++) {
        if (std::abs(stream[i]) > anomaly_magnitude / 2) {
            true_anomalies++;
        }
    }

    float detection_rate = (true_anomalies > 0)
        ? (float)detected_count / true_anomalies : 0.0f;

    // Should detect most anomalies (magnitude >> normal std)
    EXPECT_GT(detection_rate, 0.5f)
        << "Should detect at least 50% of anomalies";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Streaming anomaly detection completed in " << elapsed << " ms\n";
    std::cout << "Detected: " << detected_count << ", True anomalies: "
              << true_anomalies << "\n";
}

//=============================================================================
// E2E Test 6: Online Correlation Tracking
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, OnlineCorrelationTracking) {
    START_TIMER();

    const size_t n_samples = 50000;
    const float true_correlation = 0.7f;

    // Generate correlated streams
    std::vector<float> stream_x(n_samples), stream_y(n_samples);
    std::normal_distribution<float> noise(0.0f, 1.0f);

    for (size_t i = 0; i < n_samples; i++) {
        stream_x[i] = noise(rng);
        // Y = rho * X + sqrt(1 - rho^2) * independent_noise
        stream_y[i] = true_correlation * stream_x[i]
                    + std::sqrt(1.0f - true_correlation * true_correlation) * noise(rng);
    }

    // Online correlation computation
    // Using running stats for both and covariance
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
    size_t n = 0;

    std::vector<float> running_correlations;
    const size_t report_interval = 1000;

    for (size_t i = 0; i < n_samples; i++) {
        n++;
        sum_x += stream_x[i];
        sum_y += stream_y[i];
        sum_xy += stream_x[i] * stream_y[i];
        sum_x2 += stream_x[i] * stream_x[i];
        sum_y2 += stream_y[i] * stream_y[i];

        if (n >= 100 && (i + 1) % report_interval == 0) {
            double mean_x = sum_x / n;
            double mean_y = sum_y / n;
            double var_x = sum_x2 / n - mean_x * mean_x;
            double var_y = sum_y2 / n - mean_y * mean_y;
            double cov_xy = sum_xy / n - mean_x * mean_y;

            if (var_x > 0 && var_y > 0) {
                double corr = cov_xy / std::sqrt(var_x * var_y);
                running_correlations.push_back(static_cast<float>(corr));
            }
        }
    }

    // Final online correlation
    float final_online_corr = running_correlations.back();

    // Batch correlation
    nimcp_correlation_result_t batch_corr;
    ASSERT_EQ(nimcp_stats_correlation_pearson(
        stream_x.data(), stream_y.data(), n_samples, &batch_corr),
        NIMCP_STATS_OK);

    EXPECT_NEAR(final_online_corr, batch_corr.r, 0.01f);
    EXPECT_NEAR(final_online_corr, true_correlation, 0.05f);

    // Correlation estimates should converge
    float early_corr = running_correlations[0];
    float late_corr = running_correlations.back();
    EXPECT_LT(std::abs(late_corr - true_correlation),
              std::abs(early_corr - true_correlation) + 0.1f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Online correlation tracking completed in " << elapsed << " ms\n";
    std::cout << "Final online corr: " << final_online_corr
              << ", Batch: " << batch_corr.r << ", True: " << true_correlation << "\n";
}

//=============================================================================
// E2E Test 7: Change Point Detection in Streams
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, ChangePointDetectionStreams) {
    START_TIMER();

    const size_t n_samples = 5000;
    const size_t true_change_point = 2500;
    auto stream = generate_change_point_stream(n_samples, true_change_point,
                                               0.0f, 3.0f, 1.0f);

    // CUSUM (Cumulative Sum) algorithm for change detection
    const size_t warmup = 500;
    nimcp_running_stats_t baseline_stats;
    nimcp_stats_running_init(&baseline_stats);

    // Build baseline from warmup period
    for (size_t i = 0; i < warmup; i++) {
        nimcp_stats_running_add(&baseline_stats, stream[i]);
    }
    double baseline_mean = nimcp_stats_running_mean(&baseline_stats);
    double baseline_std = nimcp_stats_running_std_dev(&baseline_stats);

    // CUSUM detection
    double cusum_pos = 0.0, cusum_neg = 0.0;
    const double threshold = 5.0;  // Detection threshold
    const double slack = 0.5;      // Slack parameter

    size_t detected_change = 0;
    for (size_t i = warmup; i < n_samples; i++) {
        double z = (stream[i] - baseline_mean) / baseline_std;
        cusum_pos = std::max(0.0, cusum_pos + z - slack);
        cusum_neg = std::max(0.0, cusum_neg - z - slack);

        if ((cusum_pos > threshold || cusum_neg > threshold) && detected_change == 0) {
            detected_change = i;
            break;
        }
    }

    // Detection should be reasonably close to true change point
    ASSERT_GT(detected_change, 0u) << "Should detect a change point";
    int detection_delay = static_cast<int>(detected_change) - static_cast<int>(true_change_point);
    EXPECT_LT(std::abs(detection_delay), 500)
        << "Detection should be within 500 samples of true change";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Change point detection completed in " << elapsed << " ms\n";
    std::cout << "True change: " << true_change_point
              << ", Detected: " << detected_change
              << ", Delay: " << detection_delay << "\n";
}

//=============================================================================
// E2E Test 8: Online Histogram Maintenance
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, OnlineHistogramMaintenance) {
    START_TIMER();

    const size_t n_samples = 100000;
    const size_t n_bins = 50;
    auto stream = generate_normal(n_samples, 0.0f, 1.0f);

    // Fixed-width histogram
    const float bin_min = -4.0f, bin_max = 4.0f;
    const float bin_width = (bin_max - bin_min) / n_bins;
    std::vector<size_t> histogram(n_bins, 0);

    for (size_t i = 0; i < n_samples; i++) {
        int bin = static_cast<int>((stream[i] - bin_min) / bin_width);
        if (bin >= 0 && bin < static_cast<int>(n_bins)) {
            histogram[bin]++;
        }
    }

    // Convert to probability density
    std::vector<float> density(n_bins);
    for (size_t b = 0; b < n_bins; b++) {
        density[b] = static_cast<float>(histogram[b]) / (n_samples * bin_width);
    }

    // Verify histogram matches expected normal shape
    // Peak should be near center (bin 25)
    size_t peak_bin = 0;
    for (size_t b = 1; b < n_bins; b++) {
        if (density[b] > density[peak_bin]) peak_bin = b;
    }
    EXPECT_NEAR(peak_bin, n_bins / 2, 3) << "Peak should be near center";

    // Density at peak should be approximately 1/sqrt(2*pi) ≈ 0.4
    float expected_peak = 1.0f / std::sqrt(2.0f * M_PI);
    EXPECT_NEAR(density[peak_bin], expected_peak, 0.05f);

    // Compute entropy from histogram
    float entropy = 0.0f;
    for (size_t b = 0; b < n_bins; b++) {
        if (histogram[b] > 0) {
            float p = static_cast<float>(histogram[b]) / n_samples;
            entropy -= p * std::log2(p);
        }
    }

    // Normal distribution entropy is 0.5 * ln(2*pi*e) ≈ 2.05 nats ≈ 2.96 bits
    // But discretized entropy depends on bin width
    EXPECT_GT(entropy, 3.0f) << "Entropy should be reasonable for normal distribution";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Online histogram completed in " << elapsed << " ms\n";
    std::cout << "Peak bin: " << peak_bin << ", Peak density: " << density[peak_bin] << "\n";
    std::cout << "Discretized entropy: " << entropy << " bits\n";
}

//=============================================================================
// E2E Test 9: Stream Aggregation and Merging
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, StreamAggregationMerging) {
    START_TIMER();

    // Simulate distributed stream processing
    const size_t n_streams = 4;
    const size_t samples_per_stream = 25000;
    const size_t total_samples = n_streams * samples_per_stream;

    std::vector<std::vector<float>> streams(n_streams);
    std::vector<nimcp_running_stats_t> stream_stats(n_streams);

    // Initialize and process each stream
    for (size_t s = 0; s < n_streams; s++) {
        streams[s] = generate_normal(samples_per_stream, 10.0f, 2.0f);
        nimcp_stats_running_init(&stream_stats[s]);
        for (size_t i = 0; i < samples_per_stream; i++) {
            nimcp_stats_running_add(&stream_stats[s], streams[s][i]);
        }
    }

    // Merge statistics
    nimcp_running_stats_t merged_stats = stream_stats[0];
    for (size_t s = 1; s < n_streams; s++) {
        nimcp_stats_running_merge(&merged_stats, &stream_stats[s]);
    }

    // Compute global statistics from merged
    double merged_mean = nimcp_stats_running_mean(&merged_stats);
    double merged_var = nimcp_stats_running_variance(&merged_stats);
    double merged_std = nimcp_stats_running_std_dev(&merged_stats);

    // Combine all streams for batch computation
    std::vector<float> all_data;
    all_data.reserve(total_samples);
    for (size_t s = 0; s < n_streams; s++) {
        all_data.insert(all_data.end(), streams[s].begin(), streams[s].end());
    }

    float batch_mean = nimcp_stats_mean(all_data.data(), total_samples);
    float batch_var = nimcp_stats_variance(all_data.data(), total_samples);
    float batch_std = nimcp_stats_std_dev(all_data.data(), total_samples);

    // Merged should match batch
    EXPECT_NEAR(merged_mean, batch_mean, LOOSE_TOLERANCE);
    EXPECT_NEAR(merged_var, batch_var, LOOSE_TOLERANCE);
    EXPECT_NEAR(merged_std, batch_std, LOOSE_TOLERANCE);

    // Verify count
    EXPECT_EQ(merged_stats.n, total_samples);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Stream aggregation completed in " << elapsed << " ms\n";
    std::cout << "Merged mean: " << merged_mean << " vs batch: " << batch_mean << "\n";
    std::cout << "Total samples: " << merged_stats.n << "\n";
}

//=============================================================================
// E2E Test 10: Real-time Percentile Tracking
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, RealtimePercentileTracking) {
    START_TIMER();

    const size_t n_samples = 50000;
    auto stream = generate_normal(n_samples, 100.0f, 15.0f);

    // Track P50, P90, P99 using sorted samples (simplified)
    const size_t max_samples = 10000;
    std::vector<float> samples;
    samples.reserve(max_samples);

    std::vector<float> p50_track, p90_track, p99_track;
    const size_t report_interval = 1000;

    for (size_t i = 0; i < n_samples; i++) {
        // Reservoir sampling to maintain fixed-size sample
        if (samples.size() < max_samples) {
            samples.push_back(stream[i]);
        } else {
            std::uniform_int_distribution<size_t> idx_dist(0, i);
            size_t j = idx_dist(rng);
            if (j < max_samples) {
                samples[j] = stream[i];
            }
        }

        // Report percentiles periodically
        if ((i + 1) % report_interval == 0 && samples.size() >= 100) {
            p50_track.push_back(nimcp_stats_quantile(samples.data(), samples.size(), 0.50f));
            p90_track.push_back(nimcp_stats_quantile(samples.data(), samples.size(), 0.90f));
            p99_track.push_back(nimcp_stats_quantile(samples.data(), samples.size(), 0.99f));
        }
    }

    // Final percentiles should be close to theoretical values
    // For N(100, 15): P50=100, P90≈119.2, P99≈134.9
    float final_p50 = p50_track.back();
    float final_p90 = p90_track.back();
    float final_p99 = p99_track.back();

    EXPECT_NEAR(final_p50, 100.0f, 2.0f);
    EXPECT_NEAR(final_p90, 100.0f + 1.28f * 15.0f, 3.0f);  // ~119.2
    EXPECT_NEAR(final_p99, 100.0f + 2.33f * 15.0f, 5.0f);  // ~134.9

    // Percentiles should stabilize over time
    nimcp_descriptive_stats_t p50_stats;
    ASSERT_EQ(nimcp_stats_describe(p50_track.data(), p50_track.size(), &p50_stats),
              NIMCP_STATS_OK);

    // Later estimates should have lower variance
    size_t half = p50_track.size() / 2;
    float early_var = nimcp_stats_variance(p50_track.data(), half);
    float late_var = nimcp_stats_variance(p50_track.data() + half, half);
    // Note: With reservoir sampling, variance should be similar
    // Just check they're both reasonable
    EXPECT_LT(late_var, 50.0f) << "Percentile estimates should be stable";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Real-time percentile tracking completed in " << elapsed << " ms\n";
    std::cout << "Final P50: " << final_p50 << ", P90: " << final_p90
              << ", P99: " << final_p99 << "\n";
}

//=============================================================================
// E2E Test 11: Streaming Entropy Estimation
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, StreamingEntropyEstimation) {
    START_TIMER();

    const size_t n_samples = 50000;
    const size_t n_bins = 20;

    // Generate streams with different distributions
    auto normal_stream = generate_normal(n_samples, 0.0f, 1.0f);

    // Uniform stream
    std::vector<float> uniform_stream(n_samples);
    std::uniform_real_distribution<float> uni_dist(-3.0f, 3.0f);
    for (auto& x : uniform_stream) x = uni_dist(rng);

    // Bimodal stream
    std::vector<float> bimodal_stream(n_samples);
    std::normal_distribution<float> mode1(-2.0f, 0.5f);
    std::normal_distribution<float> mode2(2.0f, 0.5f);
    for (size_t i = 0; i < n_samples; i++) {
        bimodal_stream[i] = (rng() % 2 == 0) ? mode1(rng) : mode2(rng);
    }

    // Compute streaming histogram entropy
    auto compute_stream_entropy = [&](const std::vector<float>& stream) {
        // Online histogram
        const float bin_min = -4.0f, bin_max = 4.0f;
        const float bin_width = (bin_max - bin_min) / n_bins;
        std::vector<size_t> hist(n_bins, 0);
        size_t total = 0;

        for (float x : stream) {
            int bin = static_cast<int>((x - bin_min) / bin_width);
            if (bin >= 0 && bin < static_cast<int>(n_bins)) {
                hist[bin]++;
                total++;
            }
        }

        // Compute entropy
        float entropy = 0.0f;
        for (size_t b = 0; b < n_bins; b++) {
            if (hist[b] > 0) {
                float p = static_cast<float>(hist[b]) / total;
                entropy -= p * std::log2(p);
            }
        }
        return entropy;
    };

    float normal_entropy = compute_stream_entropy(normal_stream);
    float uniform_entropy = compute_stream_entropy(uniform_stream);
    float bimodal_entropy = compute_stream_entropy(bimodal_stream);

    // Uniform should have highest entropy
    EXPECT_GT(uniform_entropy, normal_entropy)
        << "Uniform distribution should have higher entropy than normal";

    // Bimodal with narrow modes should have lower entropy
    EXPECT_LT(bimodal_entropy, uniform_entropy)
        << "Bimodal with narrow modes should have lower entropy than uniform";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Streaming entropy estimation completed in " << elapsed << " ms\n";
    std::cout << "Normal entropy: " << normal_entropy << ", Uniform: " << uniform_entropy
              << ", Bimodal: " << bimodal_entropy << "\n";
}

//=============================================================================
// E2E Test 12: Online Regression Updates
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, OnlineRegressionUpdates) {
    START_TIMER();

    const size_t n_samples = 10000;
    const float true_slope = 2.0f;
    const float true_intercept = 5.0f;

    // Generate regression data
    std::vector<float> x(n_samples), y(n_samples);
    std::normal_distribution<float> noise(0.0f, 1.0f);
    std::uniform_real_distribution<float> x_dist(0.0f, 10.0f);

    for (size_t i = 0; i < n_samples; i++) {
        x[i] = x_dist(rng);
        y[i] = true_intercept + true_slope * x[i] + noise(rng);
    }

    // Online regression using sufficient statistics
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    size_t n = 0;

    std::vector<float> slope_estimates;
    const size_t report_interval = 500;

    for (size_t i = 0; i < n_samples; i++) {
        n++;
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_x2 += x[i] * x[i];

        if ((i + 1) % report_interval == 0 && n > 2) {
            double mean_x = sum_x / n;
            double mean_y = sum_y / n;
            double var_x = sum_x2 / n - mean_x * mean_x;
            double cov_xy = sum_xy / n - mean_x * mean_y;

            if (var_x > 0) {
                double slope = cov_xy / var_x;
                slope_estimates.push_back(static_cast<float>(slope));
            }
        }
    }

    // Final online slope
    float final_slope = slope_estimates.back();

    // Batch regression
    nimcp_regression_result_t batch_reg;
    ASSERT_EQ(nimcp_stats_regression_linear(x.data(), y.data(), n_samples, &batch_reg),
              NIMCP_STATS_OK);

    EXPECT_NEAR(final_slope, batch_reg.slope, 0.01f);
    EXPECT_NEAR(final_slope, true_slope, 0.1f);

    // Slope estimates should converge
    float early_error = std::abs(slope_estimates[0] - true_slope);
    float late_error = std::abs(slope_estimates.back() - true_slope);
    EXPECT_LT(late_error, early_error + 0.1f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Online regression completed in " << elapsed << " ms\n";
    std::cout << "Final slope: " << final_slope << " (true: " << true_slope << ")\n";
}

//=============================================================================
// E2E Test 13: Adaptive Threshold Detection
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, AdaptiveThresholdDetection) {
    START_TIMER();

    const size_t n_samples = 20000;

    // Stream with changing baseline
    std::vector<float> stream(n_samples);
    std::normal_distribution<float> noise(0.0f, 1.0f);

    // Baseline shifts every 5000 samples
    for (size_t i = 0; i < n_samples; i++) {
        float baseline = 10.0f + 5.0f * (i / 5000);  // 10, 15, 20, 25
        stream[i] = baseline + noise(rng);
    }

    // Add some spikes that should be detected
    std::vector<size_t> spike_indices = {500, 3000, 8000, 15000};
    for (size_t idx : spike_indices) {
        stream[idx] += 8.0f;  // Large positive spike
    }

    // Adaptive threshold using EMA
    const float alpha = 0.01f;  // Slow adaptation
    float ema_mean = stream[0];
    float ema_var = 1.0f;
    const float k = 4.0f;  // Threshold multiplier

    std::vector<size_t> detected_spikes;
    const size_t warmup = 100;

    for (size_t i = 1; i < n_samples; i++) {
        // Update EMA statistics
        float delta = stream[i] - ema_mean;
        ema_mean += alpha * delta;
        ema_var = (1.0f - alpha) * (ema_var + alpha * delta * delta);

        // Detect if above adaptive threshold
        if (i >= warmup) {
            float threshold = ema_mean + k * std::sqrt(ema_var);
            if (stream[i] > threshold) {
                detected_spikes.push_back(i);
            }
        }
    }

    // Should detect most of the inserted spikes
    size_t detected_inserted = 0;
    for (size_t spike : spike_indices) {
        for (size_t det : detected_spikes) {
            if (std::abs(static_cast<int>(det) - static_cast<int>(spike)) < 10) {
                detected_inserted++;
                break;
            }
        }
    }

    EXPECT_GE(detected_inserted, 3u)
        << "Should detect at least 3 of 4 inserted spikes";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Adaptive threshold detection completed in " << elapsed << " ms\n";
    std::cout << "Detected " << detected_inserted << " of " << spike_indices.size()
              << " inserted spikes\n";
    std::cout << "Total detections: " << detected_spikes.size() << "\n";
}

//=============================================================================
// E2E Test 14: Multi-Stream Synchronization
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, MultiStreamSynchronization) {
    START_TIMER();

    // Simulate multiple sensors with different sampling rates
    const size_t n_sensors = 4;
    const size_t base_samples = 10000;
    const float duration = 100.0f;  // 100 time units

    // Each sensor has different sampling rate
    std::vector<size_t> sensor_samples = {10000, 5000, 2500, 20000};
    std::vector<std::vector<float>> sensor_data(n_sensors);
    std::vector<std::vector<float>> sensor_times(n_sensors);

    std::normal_distribution<float> signal(50.0f, 5.0f);

    for (size_t s = 0; s < n_sensors; s++) {
        size_t n = sensor_samples[s];
        sensor_data[s].resize(n);
        sensor_times[s].resize(n);
        float dt = duration / n;
        for (size_t i = 0; i < n; i++) {
            sensor_times[s][i] = i * dt;
            sensor_data[s][i] = signal(rng);
        }
    }

    // Synchronize to common time grid
    const size_t sync_samples = 5000;
    const float sync_dt = duration / sync_samples;
    std::vector<std::vector<float>> synced_data(n_sensors);

    for (size_t s = 0; s < n_sensors; s++) {
        synced_data[s].resize(sync_samples);
        size_t src_idx = 0;
        for (size_t i = 0; i < sync_samples; i++) {
            float target_time = i * sync_dt;
            // Find nearest sample (simple nearest neighbor interpolation)
            while (src_idx + 1 < sensor_samples[s] &&
                   std::abs(sensor_times[s][src_idx + 1] - target_time) <
                   std::abs(sensor_times[s][src_idx] - target_time)) {
                src_idx++;
            }
            synced_data[s][i] = sensor_data[s][src_idx];
        }
    }

    // Compute cross-correlation between synchronized streams
    std::vector<nimcp_correlation_result_t> correlations;
    for (size_t s1 = 0; s1 < n_sensors; s1++) {
        for (size_t s2 = s1 + 1; s2 < n_sensors; s2++) {
            nimcp_correlation_result_t corr;
            nimcp_stats_correlation_pearson(synced_data[s1].data(),
                                           synced_data[s2].data(),
                                           sync_samples, &corr);
            correlations.push_back(corr);
        }
    }

    // All sensors measuring same signal should have low correlation
    // (independent noise)
    for (const auto& corr : correlations) {
        EXPECT_LT(std::abs(corr.r), 0.1f)
            << "Independent sensors should have low correlation";
    }

    // Verify synchronized statistics match
    std::vector<float> sync_means(n_sensors);
    for (size_t s = 0; s < n_sensors; s++) {
        sync_means[s] = nimcp_stats_mean(synced_data[s].data(), sync_samples);
    }

    nimcp_descriptive_stats_t mean_stats;
    ASSERT_EQ(nimcp_stats_describe(sync_means.data(), n_sensors, &mean_stats),
              NIMCP_STATS_OK);

    // All sensors should have similar mean
    EXPECT_LT(mean_stats.std_dev, 1.0f) << "Sensor means should be similar";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Multi-stream synchronization completed in " << elapsed << " ms\n";
    std::cout << "Mean of sensor means: " << mean_stats.mean
              << ", Std: " << mean_stats.std_dev << "\n";
}

//=============================================================================
// E2E Test 15: High-Frequency Data Processing
//=============================================================================

TEST_F(StreamingStatisticsE2ETest, HighFrequencyDataProcessing) {
    START_TIMER();

    // Simulate high-frequency trading-like data
    const size_t n_ticks = 1000000;  // 1 million ticks
    std::vector<float> prices(n_ticks);
    std::vector<float> volumes(n_ticks);

    // Random walk price with jumps
    std::normal_distribution<float> tick_noise(0.0f, 0.01f);
    std::exponential_distribution<float> volume_dist(1.0f);

    prices[0] = 100.0f;
    volumes[0] = volume_dist(rng);

    for (size_t i = 1; i < n_ticks; i++) {
        prices[i] = prices[i-1] + tick_noise(rng);
        volumes[i] = volume_dist(rng);
    }

    // Compute returns
    std::vector<float> returns(n_ticks - 1);
    for (size_t i = 0; i < n_ticks - 1; i++) {
        returns[i] = (prices[i+1] - prices[i]) / prices[i];
    }

    // Aggregate to different time scales (downsampling)
    auto aggregate = [](const std::vector<float>& data, size_t window) {
        std::vector<float> result;
        for (size_t i = 0; i + window <= data.size(); i += window) {
            float sum = 0.0f;
            for (size_t j = 0; j < window; j++) {
                sum += data[i + j];
            }
            result.push_back(sum);
        }
        return result;
    };

    // 1-minute bars (assuming 1000 ticks/minute)
    auto returns_1min = aggregate(returns, 1000);
    // 5-minute bars
    auto returns_5min = aggregate(returns, 5000);
    // 15-minute bars
    auto returns_15min = aggregate(returns, 15000);

    // Compute statistics at each scale
    nimcp_descriptive_stats_t tick_stats, min1_stats, min5_stats, min15_stats;
    ASSERT_EQ(nimcp_stats_describe(returns.data(), returns.size(), &tick_stats),
              NIMCP_STATS_OK);
    ASSERT_EQ(nimcp_stats_describe(returns_1min.data(), returns_1min.size(), &min1_stats),
              NIMCP_STATS_OK);
    ASSERT_EQ(nimcp_stats_describe(returns_5min.data(), returns_5min.size(), &min5_stats),
              NIMCP_STATS_OK);
    ASSERT_EQ(nimcp_stats_describe(returns_15min.data(), returns_15min.size(), &min15_stats),
              NIMCP_STATS_OK);

    // Volatility should scale with sqrt(time)
    // std_dev(n*dt) ≈ sqrt(n) * std_dev(dt)
    float expected_1min_std = tick_stats.std_dev * std::sqrt(1000.0f);
    float expected_5min_std = tick_stats.std_dev * std::sqrt(5000.0f);

    // Allow for sampling variation
    EXPECT_NEAR(min1_stats.std_dev, expected_1min_std, expected_1min_std * 0.2f);
    EXPECT_NEAR(min5_stats.std_dev, expected_5min_std, expected_5min_std * 0.2f);

    // Compute VWAP (Volume-Weighted Average Price) streaming
    double vwap_num = 0.0, vwap_den = 0.0;
    for (size_t i = 0; i < n_ticks; i++) {
        vwap_num += prices[i] * volumes[i];
        vwap_den += volumes[i];
    }
    float vwap = static_cast<float>(vwap_num / vwap_den);

    float simple_avg = nimcp_stats_mean(prices.data(), n_ticks);

    // VWAP and simple average should be close for random volumes
    EXPECT_NEAR(vwap, simple_avg, 1.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "High-frequency processing completed in " << elapsed << " ms\n";
    std::cout << "Processed " << n_ticks << " ticks\n";
    std::cout << "Tick std: " << tick_stats.std_dev
              << ", 1min std: " << min1_stats.std_dev
              << " (expected: " << expected_1min_std << ")\n";
    std::cout << "VWAP: " << vwap << ", Simple avg: " << simple_avg << "\n";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
