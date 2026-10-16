/**
 * @file test_metrics_aggregator.cpp
 * @brief Unit tests for Metrics Aggregator (100% coverage)
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_metrics_aggregator.h"
#include <thread>
#include <chrono>
#include <cmath>

/* =============================================================================
 * Test Fixtures
 * ============================================================================= */

class MetricsAggregatorTest : public ::testing::Test {
protected:
    nimcp_metrics_aggregator_t* agg;

    void SetUp() override {
        agg = nimcp_metrics_aggregator_create("test_metric");
        ASSERT_NE(agg, nullptr);
    }

    void TearDown() override {
        nimcp_metrics_aggregator_destroy(agg);
    }
};

/* =============================================================================
 * Creation and Destruction Tests
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, CreateWithValidName) {
    nimcp_metrics_aggregator_t* agg2 = nimcp_metrics_aggregator_create("latency_ms");
    ASSERT_NE(agg2, nullptr);
    nimcp_metrics_aggregator_destroy(agg2);
}

TEST_F(MetricsAggregatorTest, CreateWithNullName) {
    nimcp_metrics_aggregator_t* agg2 = nimcp_metrics_aggregator_create(nullptr);
    EXPECT_EQ(agg2, nullptr);
}

TEST_F(MetricsAggregatorTest, DestroyNull) {
    nimcp_metrics_aggregator_destroy(nullptr);
    // Should not crash
}

/* =============================================================================
 * Sample Addition Tests
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, AddSingleSample) {
    EXPECT_TRUE(nimcp_metrics_aggregator_add_sample(agg, 42.0, 0));

    nimcp_metrics_aggregator_aggregate(agg);

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    ASSERT_NE(stats, nullptr);
    EXPECT_EQ(stats->count, 1);
    EXPECT_DOUBLE_EQ(stats->min, 42.0);
    EXPECT_DOUBLE_EQ(stats->max, 42.0);
    EXPECT_DOUBLE_EQ(stats->avg, 42.0);
}

TEST_F(MetricsAggregatorTest, AddMultipleSamples) {
    double values[] = {10.0, 20.0, 30.0, 40.0, 50.0};

    for (double val : values) {
        EXPECT_TRUE(nimcp_metrics_aggregator_add_sample(agg, val, 0));
    }

    nimcp_metrics_aggregator_aggregate(agg);

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_EQ(stats->count, 5);
    EXPECT_DOUBLE_EQ(stats->min, 10.0);
    EXPECT_DOUBLE_EQ(stats->max, 50.0);
    EXPECT_DOUBLE_EQ(stats->avg, 30.0);
    EXPECT_DOUBLE_EQ(stats->sum, 150.0);
}

TEST_F(MetricsAggregatorTest, AddSampleNull) {
    EXPECT_FALSE(nimcp_metrics_aggregator_add_sample(nullptr, 42.0, 0));
}

TEST_F(MetricsAggregatorTest, AddNegativeSamples) {
    nimcp_metrics_aggregator_add_sample(agg, -10.0, 0);
    nimcp_metrics_aggregator_add_sample(agg, -5.0, 0);
    nimcp_metrics_aggregator_add_sample(agg, 5.0, 0);

    nimcp_metrics_aggregator_aggregate(agg);

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_DOUBLE_EQ(stats->min, -10.0);
    EXPECT_DOUBLE_EQ(stats->max, 5.0);
}

TEST_F(MetricsAggregatorTest, AddLargeSamples) {
    nimcp_metrics_aggregator_add_sample(agg, 1e6, 0);
    nimcp_metrics_aggregator_add_sample(agg, 2e6, 0);

    nimcp_metrics_aggregator_aggregate(agg);

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_DOUBLE_EQ(stats->min, 1e6);
    EXPECT_DOUBLE_EQ(stats->max, 2e6);
}

/* =============================================================================
 * Aggregation Tests
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, ManualAggregation) {
    nimcp_metrics_aggregator_add_sample(agg, 100.0, 0);
    EXPECT_TRUE(nimcp_metrics_aggregator_aggregate(agg));

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_NE(stats, nullptr);
    EXPECT_EQ(stats->count, 1);
}

TEST_F(MetricsAggregatorTest, AggregateNull) {
    EXPECT_FALSE(nimcp_metrics_aggregator_aggregate(nullptr));
}

TEST_F(MetricsAggregatorTest, AggregateEmptyMetric) {
    nimcp_metrics_aggregator_aggregate(agg);

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_EQ(stats->count, 0);
    EXPECT_DOUBLE_EQ(stats->min, 0.0);
    EXPECT_DOUBLE_EQ(stats->max, 0.0);
    EXPECT_DOUBLE_EQ(stats->avg, 0.0);
}

TEST_F(MetricsAggregatorTest, AutoAggregation) {
    nimcp_metrics_aggregator_set_auto_aggregate(agg, true, 0);

    nimcp_metrics_aggregator_add_sample(agg, 42.0, 0);

    // Auto-aggregation should have occurred
    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_EQ(stats->count, 1);
}

/* =============================================================================
 * Window Tests
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, MultipleWindows) {
    for (int i = 0; i < 10; i++) {
        nimcp_metrics_aggregator_add_sample(agg, i * 10.0, 0);
    }

    nimcp_metrics_aggregator_aggregate(agg);

    // All windows should have data
    for (int w = 0; w < NIMCP_WINDOW_COUNT; w++) {
        const auto* stats = nimcp_metrics_aggregator_get_stats(agg,
            static_cast<nimcp_time_window_t>(w));
        ASSERT_NE(stats, nullptr);
        EXPECT_GT(stats->count, 0);
    }
}

TEST_F(MetricsAggregatorTest, WindowSizeConstraints) {
    // Test that windows properly aggregate samples within their time ranges
    // All samples added at timestamp 0 should be in all windows
    for (int i = 0; i < 10; i++) {
        nimcp_metrics_aggregator_add_sample(agg, i * 1.0, 0);
    }

    nimcp_metrics_aggregator_aggregate(agg);

    const auto* stats_1s = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    const auto* stats_10s = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_10S);

    // Since all samples are at the same timestamp, both windows should contain all samples
    EXPECT_EQ(stats_1s->count, 10);
    EXPECT_EQ(stats_10s->count, 10);
    // Both windows should have the same statistics since they contain the same samples
    EXPECT_DOUBLE_EQ(stats_1s->min, stats_10s->min);
    EXPECT_DOUBLE_EQ(stats_1s->max, stats_10s->max);
}

/* =============================================================================
 * Query Function Tests
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, GetStats) {
    nimcp_metrics_aggregator_add_sample(agg, 100.0, 0);
    nimcp_metrics_aggregator_aggregate(agg);

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    ASSERT_NE(stats, nullptr);
    EXPECT_EQ(stats->count, 1);
}

TEST_F(MetricsAggregatorTest, GetStatsNull) {
    EXPECT_EQ(nimcp_metrics_aggregator_get_stats(nullptr, NIMCP_WINDOW_1S), nullptr);
}

TEST_F(MetricsAggregatorTest, GetStatsInvalidWindow) {
    EXPECT_EQ(nimcp_metrics_aggregator_get_stats(agg,
        static_cast<nimcp_time_window_t>(999)), nullptr);
}

TEST_F(MetricsAggregatorTest, GetMin) {
    nimcp_metrics_aggregator_add_sample(agg, 5.0, 0);
    nimcp_metrics_aggregator_add_sample(agg, 15.0, 0);
    nimcp_metrics_aggregator_aggregate(agg);

    EXPECT_DOUBLE_EQ(nimcp_metrics_aggregator_get_min(agg, NIMCP_WINDOW_1S), 5.0);
}

TEST_F(MetricsAggregatorTest, GetMax) {
    nimcp_metrics_aggregator_add_sample(agg, 5.0, 0);
    nimcp_metrics_aggregator_add_sample(agg, 15.0, 0);
    nimcp_metrics_aggregator_aggregate(agg);

    EXPECT_DOUBLE_EQ(nimcp_metrics_aggregator_get_max(agg, NIMCP_WINDOW_1S), 15.0);
}

TEST_F(MetricsAggregatorTest, GetAvg) {
    nimcp_metrics_aggregator_add_sample(agg, 10.0, 0);
    nimcp_metrics_aggregator_add_sample(agg, 20.0, 0);
    nimcp_metrics_aggregator_add_sample(agg, 30.0, 0);
    nimcp_metrics_aggregator_aggregate(agg);

    EXPECT_DOUBLE_EQ(nimcp_metrics_aggregator_get_avg(agg, NIMCP_WINDOW_1S), 20.0);
}

TEST_F(MetricsAggregatorTest, GetCount) {
    nimcp_metrics_aggregator_add_sample(agg, 1.0, 0);
    nimcp_metrics_aggregator_add_sample(agg, 2.0, 0);
    nimcp_metrics_aggregator_add_sample(agg, 3.0, 0);
    nimcp_metrics_aggregator_aggregate(agg);

    EXPECT_EQ(nimcp_metrics_aggregator_get_count(agg, NIMCP_WINDOW_1S), 3);
}

TEST_F(MetricsAggregatorTest, GetMinNull) {
    EXPECT_DOUBLE_EQ(nimcp_metrics_aggregator_get_min(nullptr, NIMCP_WINDOW_1S), 0.0);
}

TEST_F(MetricsAggregatorTest, GetMaxNull) {
    EXPECT_DOUBLE_EQ(nimcp_metrics_aggregator_get_max(nullptr, NIMCP_WINDOW_1S), 0.0);
}

TEST_F(MetricsAggregatorTest, GetAvgNull) {
    EXPECT_DOUBLE_EQ(nimcp_metrics_aggregator_get_avg(nullptr, NIMCP_WINDOW_1S), 0.0);
}

TEST_F(MetricsAggregatorTest, GetCountNull) {
    EXPECT_EQ(nimcp_metrics_aggregator_get_count(nullptr, NIMCP_WINDOW_1S), 0);
}

TEST_F(MetricsAggregatorTest, GetMinInvalidWindow) {
    EXPECT_DOUBLE_EQ(nimcp_metrics_aggregator_get_min(agg,
        static_cast<nimcp_time_window_t>(999)), 0.0);
}

/* =============================================================================
 * Percentile Tests
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, GetPercentileP50) {
    for (int i = 1; i <= 100; i++) {
        nimcp_metrics_aggregator_add_sample(agg, i * 1.0, 0);
    }
    nimcp_metrics_aggregator_aggregate(agg);

    double p50 = nimcp_metrics_aggregator_get_percentile(agg, NIMCP_WINDOW_1S, 0.50);
    // P50 should be around 50
    EXPECT_GT(p50, 40.0);
    EXPECT_LT(p50, 60.0);
}

TEST_F(MetricsAggregatorTest, GetPercentileP95) {
    for (int i = 1; i <= 100; i++) {
        nimcp_metrics_aggregator_add_sample(agg, i * 1.0, 0);
    }
    nimcp_metrics_aggregator_aggregate(agg);

    double p95 = nimcp_metrics_aggregator_get_percentile(agg, NIMCP_WINDOW_1S, 0.95);
    // P95 should be around 95
    EXPECT_GT(p95, 85.0);
    EXPECT_LT(p95, 105.0);
}

TEST_F(MetricsAggregatorTest, GetPercentileP99) {
    for (int i = 1; i <= 100; i++) {
        nimcp_metrics_aggregator_add_sample(agg, i * 1.0, 0);
    }
    nimcp_metrics_aggregator_aggregate(agg);

    double p99 = nimcp_metrics_aggregator_get_percentile(agg, NIMCP_WINDOW_1S, 0.99);
    // P99 should be around 99
    EXPECT_GT(p99, 89.0);
    EXPECT_LT(p99, 110.0);
}

TEST_F(MetricsAggregatorTest, GetPercentileNull) {
    EXPECT_DOUBLE_EQ(nimcp_metrics_aggregator_get_percentile(nullptr,
        NIMCP_WINDOW_1S, 0.5), 0.0);
}

TEST_F(MetricsAggregatorTest, GetPercentileInvalidWindow) {
    EXPECT_DOUBLE_EQ(nimcp_metrics_aggregator_get_percentile(agg,
        static_cast<nimcp_time_window_t>(999), 0.5), 0.0);
}

/* =============================================================================
 * Histogram Tests
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, HistogramAdd) {
    nimcp_histogram_t hist;
    nimcp_histogram_reset(&hist);

    nimcp_histogram_add(&hist, 10.0);
    nimcp_histogram_add(&hist, 20.0);
    nimcp_histogram_add(&hist, 30.0);

    EXPECT_EQ(hist.total_count, 3);
    EXPECT_DOUBLE_EQ(hist.min_value, 10.0);
    EXPECT_DOUBLE_EQ(hist.max_value, 30.0);
}

TEST_F(MetricsAggregatorTest, HistogramReset) {
    nimcp_histogram_t hist;
    nimcp_histogram_reset(&hist);

    nimcp_histogram_add(&hist, 42.0);
    EXPECT_EQ(hist.total_count, 1);

    nimcp_histogram_reset(&hist);
    EXPECT_EQ(hist.total_count, 0);
    EXPECT_TRUE(std::isinf(hist.min_value));
    EXPECT_TRUE(std::isinf(hist.max_value));
}

TEST_F(MetricsAggregatorTest, HistogramResetNull) {
    nimcp_histogram_reset(nullptr);
    // Should not crash
}

TEST_F(MetricsAggregatorTest, HistogramAddNull) {
    nimcp_histogram_add(nullptr, 42.0);
    // Should not crash
}

TEST_F(MetricsAggregatorTest, HistogramPercentileEmpty) {
    nimcp_histogram_t hist;
    nimcp_histogram_reset(&hist);

    double p50 = nimcp_histogram_percentile(&hist, 0.5);
    EXPECT_DOUBLE_EQ(p50, 0.0);
}

TEST_F(MetricsAggregatorTest, HistogramPercentileNull) {
    double p50 = nimcp_histogram_percentile(nullptr, 0.5);
    EXPECT_DOUBLE_EQ(p50, 0.0);
}

TEST_F(MetricsAggregatorTest, HistogramPercentileEdgeCases) {
    nimcp_histogram_t hist;
    nimcp_histogram_reset(&hist);

    for (int i = 1; i <= 100; i++) {
        nimcp_histogram_add(&hist, i * 1.0);
    }

    // P0 should be min
    double p0 = nimcp_histogram_percentile(&hist, 0.0);
    EXPECT_DOUBLE_EQ(p0, hist.min_value);

    // P100 should be max
    double p100 = nimcp_histogram_percentile(&hist, 1.0);
    EXPECT_DOUBLE_EQ(p100, hist.max_value);
}

TEST_F(MetricsAggregatorTest, HistogramDynamicBucketWidth) {
    nimcp_histogram_t hist;
    nimcp_histogram_reset(&hist);

    // Add values with increasing range
    nimcp_histogram_add(&hist, 1.0);
    nimcp_histogram_add(&hist, 100.0);
    nimcp_histogram_add(&hist, 1000.0);

    EXPECT_GT(hist.bucket_width, 0.0);
    EXPECT_EQ(hist.total_count, 3);
}

/* =============================================================================
 * Configuration Tests
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, SetAutoAggregate) {
    EXPECT_TRUE(nimcp_metrics_aggregator_set_auto_aggregate(agg, true, 5));
    EXPECT_TRUE(nimcp_metrics_aggregator_set_auto_aggregate(agg, false, 0));
}

TEST_F(MetricsAggregatorTest, SetAutoAggregateNull) {
    EXPECT_FALSE(nimcp_metrics_aggregator_set_auto_aggregate(nullptr, true, 1));
}

TEST_F(MetricsAggregatorTest, DisableAutoAggregate) {
    nimcp_metrics_aggregator_set_auto_aggregate(agg, false, 0);

    nimcp_metrics_aggregator_add_sample(agg, 42.0, 0);

    // Without manual aggregation, stats should be stale
    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_EQ(stats->count, 0);  // No aggregation occurred

    // Now aggregate manually
    nimcp_metrics_aggregator_aggregate(agg);
    EXPECT_EQ(stats->count, 1);
}

/* =============================================================================
 * Reset Tests
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, Reset) {
    nimcp_metrics_aggregator_add_sample(agg, 42.0, 0);
    nimcp_metrics_aggregator_aggregate(agg);

    nimcp_metrics_aggregator_reset(agg);

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_EQ(stats->count, 0);

    uint64_t total_samples = 999, aggregations = 999;
    nimcp_metrics_aggregator_get_statistics(agg, &total_samples, &aggregations);
    EXPECT_EQ(total_samples, 0);
    EXPECT_EQ(aggregations, 0);
}

TEST_F(MetricsAggregatorTest, ResetNull) {
    nimcp_metrics_aggregator_reset(nullptr);
    // Should not crash
}

/* =============================================================================
 * Statistics Tests
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, GetStatistics) {
    nimcp_metrics_aggregator_add_sample(agg, 1.0, 0);
    nimcp_metrics_aggregator_add_sample(agg, 2.0, 0);
    nimcp_metrics_aggregator_aggregate(agg);

    uint64_t total_samples = 0, aggregations = 0;
    EXPECT_TRUE(nimcp_metrics_aggregator_get_statistics(agg,
        &total_samples, &aggregations));

    EXPECT_EQ(total_samples, 2);
    EXPECT_EQ(aggregations, 1);
}

TEST_F(MetricsAggregatorTest, GetStatisticsNull) {
    uint64_t total = 0, aggs = 0;
    EXPECT_FALSE(nimcp_metrics_aggregator_get_statistics(nullptr, &total, &aggs));
    EXPECT_FALSE(nimcp_metrics_aggregator_get_statistics(agg, nullptr, &aggs));
    EXPECT_FALSE(nimcp_metrics_aggregator_get_statistics(agg, &total, nullptr));
}

/* =============================================================================
 * Utility Function Tests
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, WindowToString) {
    EXPECT_STREQ(nimcp_window_to_string(NIMCP_WINDOW_1S), "1s");
    EXPECT_STREQ(nimcp_window_to_string(NIMCP_WINDOW_10S), "10s");
    EXPECT_STREQ(nimcp_window_to_string(NIMCP_WINDOW_1M), "1m");
    EXPECT_STREQ(nimcp_window_to_string(NIMCP_WINDOW_1H), "1h");
    EXPECT_STREQ(nimcp_window_to_string(static_cast<nimcp_time_window_t>(999)),
                 "UNKNOWN");
}

TEST_F(MetricsAggregatorTest, WindowDuration) {
    EXPECT_EQ(nimcp_window_duration(NIMCP_WINDOW_1S), 1);
    EXPECT_EQ(nimcp_window_duration(NIMCP_WINDOW_10S), 10);
    EXPECT_EQ(nimcp_window_duration(NIMCP_WINDOW_1M), 60);
    EXPECT_EQ(nimcp_window_duration(NIMCP_WINDOW_1H), 3600);
    EXPECT_EQ(nimcp_window_duration(static_cast<nimcp_time_window_t>(999)), 0);
}

/* =============================================================================
 * Integration Scenarios
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, RealisticLatencyScenario) {
    // Simulate latency measurements
    double latencies[] = {1.2, 1.5, 1.3, 1.8, 2.1, 1.4, 1.6, 15.0, 1.5, 1.7};

    for (double lat : latencies) {
        nimcp_metrics_aggregator_add_sample(agg, lat, 0);
    }

    nimcp_metrics_aggregator_aggregate(agg);

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_DOUBLE_EQ(stats->min, 1.2);
    EXPECT_DOUBLE_EQ(stats->max, 15.0);
    EXPECT_EQ(stats->count, 10);

    // P95 should catch the outlier
    double p95 = nimcp_metrics_aggregator_get_percentile(agg, NIMCP_WINDOW_1S, 0.95);
    EXPECT_GT(p95, 2.0);
}

TEST_F(MetricsAggregatorTest, MemoryEfficiency) {
    // Add 1000 samples - should still be memory efficient
    for (int i = 0; i < 1000; i++) {
        nimcp_metrics_aggregator_add_sample(agg, i * 0.1, 0);
    }

    nimcp_metrics_aggregator_aggregate(agg);

    // Should have aggregated statistics available
    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1H);
    EXPECT_GT(stats->count, 0);
}

TEST_F(MetricsAggregatorTest, AllWindows) {
    for (int i = 0; i < 100; i++) {
        nimcp_metrics_aggregator_add_sample(agg, i * 1.0, 0);
    }

    nimcp_metrics_aggregator_aggregate(agg);

    // Verify all windows have data
    EXPECT_GT(nimcp_metrics_aggregator_get_count(agg, NIMCP_WINDOW_1S), 0);
    EXPECT_GT(nimcp_metrics_aggregator_get_count(agg, NIMCP_WINDOW_10S), 0);
    EXPECT_GT(nimcp_metrics_aggregator_get_count(agg, NIMCP_WINDOW_1M), 0);
    EXPECT_GT(nimcp_metrics_aggregator_get_count(agg, NIMCP_WINDOW_1H), 0);
}

/* =============================================================================
 * Edge Cases
 * ============================================================================= */

TEST_F(MetricsAggregatorTest, ZeroValues) {
    nimcp_metrics_aggregator_add_sample(agg, 0.0, 0);
    nimcp_metrics_aggregator_add_sample(agg, 0.0, 0);
    nimcp_metrics_aggregator_aggregate(agg);

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_DOUBLE_EQ(stats->min, 0.0);
    EXPECT_DOUBLE_EQ(stats->max, 0.0);
    EXPECT_DOUBLE_EQ(stats->avg, 0.0);
}

TEST_F(MetricsAggregatorTest, SingleValue) {
    nimcp_metrics_aggregator_add_sample(agg, 42.0, 0);
    nimcp_metrics_aggregator_aggregate(agg);

    double p50 = nimcp_metrics_aggregator_get_percentile(agg, NIMCP_WINDOW_1S, 0.5);
    EXPECT_GT(p50, 0.0);
}

TEST_F(MetricsAggregatorTest, VerySmallValues) {
    nimcp_metrics_aggregator_add_sample(agg, 1e-10, 0);
    nimcp_metrics_aggregator_add_sample(agg, 2e-10, 0);
    nimcp_metrics_aggregator_aggregate(agg);

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_GT(stats->max, stats->min);
}

TEST_F(MetricsAggregatorTest, IdenticalValues) {
    for (int i = 0; i < 50; i++) {
        nimcp_metrics_aggregator_add_sample(agg, 42.0, 0);
    }
    nimcp_metrics_aggregator_aggregate(agg);

    const auto* stats = nimcp_metrics_aggregator_get_stats(agg, NIMCP_WINDOW_1S);
    EXPECT_DOUBLE_EQ(stats->min, 42.0);
    EXPECT_DOUBLE_EQ(stats->max, 42.0);
    EXPECT_DOUBLE_EQ(stats->avg, 42.0);
}
