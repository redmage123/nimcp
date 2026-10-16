/**
 * @file test_recovery_observability.cpp
 * @brief Unit tests for recovery observability module
 *
 * Tests MTTR/MTBF tracking, distributed tracing, Prometheus-style metrics,
 * and histograms.
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_recovery_observability.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class RecoveryObservabilityTest : public ::testing::Test {
protected:
    ro_context_t* ctx;
    ro_config_t config;

    void SetUp() override {
        config = ro_default_config();
        ctx = ro_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            ro_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(RoLifecycleTest, DefaultConfig) {
    ro_config_t config = ro_default_config();

    EXPECT_TRUE(config.enable_tracing);
    EXPECT_TRUE(config.enable_metrics);
    EXPECT_TRUE(config.enable_events);
    EXPECT_GT(config.metrics_interval_ms, 0);
}

TEST(RoLifecycleTest, CreateAndDestroy) {
    ro_config_t config = ro_default_config();

    ro_context_t* ctx = ro_create(&config);
    ASSERT_NE(ctx, nullptr);

    ro_destroy(ctx);
}

TEST(RoLifecycleTest, CreateWithNullConfig) {
    ro_context_t* ctx = ro_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(RecoveryObservabilityTest, StartAndStop) {
    EXPECT_TRUE(ro_start(ctx));
    EXPECT_TRUE(ro_stop(ctx));
}

//=============================================================================
// Counter Tests
//=============================================================================

TEST_F(RecoveryObservabilityTest, CreateCounter) {
    ro_counter_t* counter = ro_create_counter(ctx, "test_counter", nullptr, 0);
    ASSERT_NE(counter, nullptr);
}

TEST_F(RecoveryObservabilityTest, IncrementCounter) {
    ro_counter_t* counter = ro_create_counter(ctx, "test_counter", nullptr, 0);

    EXPECT_EQ(ro_counter_inc(counter, 5), 5);
    EXPECT_EQ(ro_counter_inc(counter, 3), 8);
    EXPECT_EQ(ro_counter_get(counter), 8);
}

TEST_F(RecoveryObservabilityTest, CounterWithLabels) {
    ro_label_t labels[2];
    strncpy(labels[0].key, "service", sizeof(labels[0].key) - 1);
    strncpy(labels[0].value, "brain", sizeof(labels[0].value) - 1);
    strncpy(labels[1].key, "region", sizeof(labels[1].key) - 1);
    strncpy(labels[1].value, "us-west", sizeof(labels[1].value) - 1);

    ro_counter_t* counter = ro_create_counter(ctx, "labeled_counter", labels, 2);
    ASSERT_NE(counter, nullptr);
    EXPECT_EQ(counter->label_count, 2);
}

//=============================================================================
// Gauge Tests
//=============================================================================

TEST_F(RecoveryObservabilityTest, CreateGauge) {
    ro_gauge_t* gauge = ro_create_gauge(ctx, "test_gauge", nullptr, 0);
    ASSERT_NE(gauge, nullptr);
}

TEST_F(RecoveryObservabilityTest, SetGauge) {
    ro_gauge_t* gauge = ro_create_gauge(ctx, "test_gauge", nullptr, 0);

    ro_gauge_set(gauge, 42.5);
    EXPECT_NEAR(ro_gauge_get(gauge), 42.5, 0.01);
}

TEST_F(RecoveryObservabilityTest, IncDecGauge) {
    ro_gauge_t* gauge = ro_create_gauge(ctx, "test_gauge", nullptr, 0);

    ro_gauge_set(gauge, 10.0);
    ro_gauge_inc(gauge, 5.0);
    EXPECT_NEAR(ro_gauge_get(gauge), 15.0, 0.01);

    ro_gauge_dec(gauge, 3.0);
    EXPECT_NEAR(ro_gauge_get(gauge), 12.0, 0.01);
}

//=============================================================================
// Histogram Tests
//=============================================================================

TEST_F(RecoveryObservabilityTest, CreateHistogram) {
    double buckets[] = {10, 50, 100, 500, 1000};
    ro_histogram_t* hist = ro_create_histogram(ctx, "test_histogram", buckets, 5, nullptr, 0);
    ASSERT_NE(hist, nullptr);
}

TEST_F(RecoveryObservabilityTest, ObserveHistogram) {
    double buckets[] = {10, 50, 100, 500, 1000};
    ro_histogram_t* hist = ro_create_histogram(ctx, "latency_ms", buckets, 5, nullptr, 0);

    ro_histogram_observe(hist, 25);
    ro_histogram_observe(hist, 75);
    ro_histogram_observe(hist, 200);
    ro_histogram_observe(hist, 800);

    EXPECT_EQ(hist->sample_count, 4);
}

TEST_F(RecoveryObservabilityTest, HistogramPercentile) {
    double buckets[] = {10, 50, 100, 500, 1000};
    ro_histogram_t* hist = ro_create_histogram(ctx, "latency_ms", buckets, 5, nullptr, 0);

    for (int i = 0; i < 100; i++) {
        ro_histogram_observe(hist, i * 10);  // 0, 10, 20, ..., 990
    }

    double p50 = ro_histogram_percentile(hist, 0.5);
    double p99 = ro_histogram_percentile(hist, 0.99);

    EXPECT_GT(p50, 0);
    EXPECT_GT(p99, p50);
}

TEST_F(RecoveryObservabilityTest, HistogramMean) {
    double buckets[] = {10, 50, 100};
    ro_histogram_t* hist = ro_create_histogram(ctx, "test_hist", buckets, 3, nullptr, 0);

    ro_histogram_observe(hist, 10);
    ro_histogram_observe(hist, 20);
    ro_histogram_observe(hist, 30);

    EXPECT_NEAR(ro_histogram_mean(hist), 20.0, 0.01);
}

//=============================================================================
// Tracing Tests
//=============================================================================

TEST_F(RecoveryObservabilityTest, StartTrace) {
    ro_span_t* span = ro_start_trace(ctx, "test_operation");
    ASSERT_NE(span, nullptr);
    EXPECT_TRUE(span->is_recording);
}

TEST_F(RecoveryObservabilityTest, StartChildSpan) {
    ro_span_t* parent = ro_start_trace(ctx, "parent_op");
    ro_span_t* child = ro_start_span(ctx, parent, "child_op");

    ASSERT_NE(child, nullptr);
    EXPECT_EQ(memcmp(child->trace_id, parent->trace_id, RO_TRACE_ID_SIZE), 0);
}

TEST_F(RecoveryObservabilityTest, EndSpan) {
    ro_span_t* span = ro_start_trace(ctx, "test_op");

    ro_end_span(span, RO_SPAN_OK, "completed");

    EXPECT_FALSE(span->is_recording);
    EXPECT_EQ(span->status, RO_SPAN_OK);
}

TEST_F(RecoveryObservabilityTest, SpanDuration) {
    ro_span_t* span = ro_start_trace(ctx, "timed_op");

    // Small delay
    struct timespec ts = {0, 1000000};  // 1ms
    nanosleep(&ts, NULL);

    ro_end_span(span, RO_SPAN_OK, nullptr);

    uint64_t duration = ro_span_duration_ns(span);
    EXPECT_GT(duration, 0);
}

TEST_F(RecoveryObservabilityTest, SpanAttributes) {
    ro_span_t* span = ro_start_trace(ctx, "attributed_op");

    ro_span_set_attribute(span, "service", "brain");
    ro_span_set_attribute(span, "node_id", "42");

    EXPECT_EQ(span->attribute_count, 2);
    EXPECT_STREQ(span->attributes[0].key, "service");
    EXPECT_STREQ(span->attributes[0].value, "brain");

    ro_end_span(span, RO_SPAN_OK, nullptr);
}

//=============================================================================
// Recovery Metrics Tests
//=============================================================================

TEST_F(RecoveryObservabilityTest, StartRecovery) {
    EXPECT_TRUE(ro_start(ctx));

    ro_recovery_context_t* recovery = ro_start_recovery(ctx, 1);
    ASSERT_NE(recovery, nullptr);
    EXPECT_GT(recovery->recovery_id, 0);
    EXPECT_EQ(recovery->fault_type, 1);

    ro_end_recovery(ctx, recovery, true);

    EXPECT_TRUE(ro_stop(ctx));
}

TEST_F(RecoveryObservabilityTest, RecoveryAttempt) {
    EXPECT_TRUE(ro_start(ctx));

    ro_recovery_context_t* recovery = ro_start_recovery(ctx, 1);

    ro_record_recovery_attempt(recovery, "restart", false);
    ro_record_recovery_attempt(recovery, "reload", true);

    EXPECT_EQ(recovery->attempt_count, 2);

    ro_end_recovery(ctx, recovery, true);

    EXPECT_TRUE(ro_stop(ctx));
}

TEST_F(RecoveryObservabilityTest, MttrStats) {
    EXPECT_TRUE(ro_start(ctx));

    // Simulate recoveries
    for (int i = 0; i < 5; i++) {
        ro_recovery_context_t* recovery = ro_start_recovery(ctx, 1);

        // Small delay
        struct timespec ts = {0, 10000000};  // 10ms
        nanosleep(&ts, NULL);

        ro_end_recovery(ctx, recovery, i % 2 == 0);  // Alternating success/failure
    }

    ro_mttr_stats_t stats;
    EXPECT_TRUE(ro_get_mttr_stats(ctx, &stats));

    EXPECT_EQ(stats.total_recoveries, 5);
    EXPECT_GT(stats.mttr_ms, 0);

    EXPECT_TRUE(ro_stop(ctx));
}

TEST_F(RecoveryObservabilityTest, MtbfStats) {
    EXPECT_TRUE(ro_start(ctx));

    // Record failures
    ro_record_failure(ctx, 1, 1);

    struct timespec ts = {0, 50000000};  // 50ms
    nanosleep(&ts, NULL);

    ro_record_failure(ctx, 1, 2);

    ro_mtbf_stats_t stats;
    EXPECT_TRUE(ro_get_mtbf_stats(ctx, &stats));

    EXPECT_EQ(stats.total_failures, 2);

    EXPECT_TRUE(ro_stop(ctx));
}

//=============================================================================
// Event Logging Tests
//=============================================================================

TEST_F(RecoveryObservabilityTest, LogEvent) {
    ro_event_t event; memset(&event, 0, sizeof(event));
    event.type = RO_EVENT_FAILURE_DETECTED;
    event.severity = RO_SEVERITY_ERROR;
    event.timestamp_ns = ro_timestamp_ns();
    event.node_id = 1;
    event.fault_type = 1;
    strncpy(event.message, "Test failure", sizeof(event.message) - 1);

    EXPECT_TRUE(ro_log_event(ctx, &event));
}

TEST_F(RecoveryObservabilityTest, GetEvents) {
    // Log some events
    for (int i = 0; i < 5; i++) {
        ro_event_t event; memset(&event, 0, sizeof(event));
        event.type = RO_EVENT_RECOVERY_STARTED;
        event.severity = RO_SEVERITY_INFO;
        event.timestamp_ns = ro_timestamp_ns();
        ro_log_event(ctx, &event);
    }

    ro_event_t events[10];
    uint32_t count = ro_get_events(ctx, events, 10, 0);
    EXPECT_EQ(count, 5);
}

TEST_F(RecoveryObservabilityTest, GetEventsByType) {
    ro_event_t success_event; memset(&success_event, 0, sizeof(success_event));
    success_event.type = RO_EVENT_RECOVERY_SUCCESS;
    ro_log_event(ctx, &success_event);

    ro_event_t failed_event; memset(&failed_event, 0, sizeof(failed_event));
    failed_event.type = RO_EVENT_RECOVERY_FAILED;
    ro_log_event(ctx, &failed_event);
    ro_log_event(ctx, &failed_event);

    ro_event_t events[10];
    uint32_t count = ro_get_events_by_type(ctx, RO_EVENT_RECOVERY_FAILED, events, 10);
    EXPECT_EQ(count, 2);
}

//=============================================================================
// Export Tests
//=============================================================================

TEST_F(RecoveryObservabilityTest, ExportMetricsJson) {
    ro_create_counter(ctx, "test_counter", nullptr, 0);
    ro_create_gauge(ctx, "test_gauge", nullptr, 0);

    char buffer[4096];
    size_t written = ro_export_metrics(ctx, RO_EXPORT_JSON, buffer, sizeof(buffer));

    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buffer, "test_counter"), nullptr);
}

TEST_F(RecoveryObservabilityTest, ExportMetricsPrometheus) {
    ro_counter_t* counter = ro_create_counter(ctx, "requests_total", nullptr, 0);
    ro_counter_inc(counter, 100);

    char buffer[4096];
    size_t written = ro_export_metrics(ctx, RO_EXPORT_PROMETHEUS, buffer, sizeof(buffer));

    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buffer, "# TYPE requests_total counter"), nullptr);
}

TEST_F(RecoveryObservabilityTest, ExportTraces) {
    ro_span_t* span = ro_start_trace(ctx, "test_trace");
    ro_end_span(span, RO_SPAN_OK, nullptr);

    char buffer[4096];
    size_t written = ro_export_traces(ctx, RO_EXPORT_JSON, buffer, sizeof(buffer));

    EXPECT_GT(written, 0);
}

TEST_F(RecoveryObservabilityTest, RegisterExporter) {
    static bool exporter_called = false;

    auto callback = [](const void* data, size_t size, ro_export_format_t fmt, void* ud) -> bool {
        (void)data; (void)size; (void)fmt; (void)ud;
        exporter_called = true;
        return true;
    };

    EXPECT_TRUE(ro_register_exporter(ctx, callback, RO_EXPORT_JSON, nullptr));
    EXPECT_TRUE(ro_flush(ctx));
    EXPECT_TRUE(exporter_called);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST(RoUtilityTest, GenerateTraceId) {
    uint8_t trace_id[RO_TRACE_ID_SIZE];
    ro_generate_trace_id(trace_id);

    // Check it's not all zeros
    bool has_nonzero = false;
    for (int i = 0; i < RO_TRACE_ID_SIZE; i++) {
        if (trace_id[i] != 0) has_nonzero = true;
    }
    EXPECT_TRUE(has_nonzero);
}

TEST(RoUtilityTest, GenerateSpanId) {
    uint8_t span_id[RO_SPAN_ID_SIZE];
    ro_generate_span_id(span_id);

    bool has_nonzero = false;
    for (int i = 0; i < RO_SPAN_ID_SIZE; i++) {
        if (span_id[i] != 0) has_nonzero = true;
    }
    EXPECT_TRUE(has_nonzero);
}

TEST(RoUtilityTest, Timestamp) {
    uint64_t ts1 = ro_timestamp_ns();
    uint64_t ts2 = ro_timestamp_ns();

    EXPECT_GT(ts1, 0);
    EXPECT_GE(ts2, ts1);
}

TEST_F(RecoveryObservabilityTest, ResetMetrics) {
    ro_counter_t* counter = ro_create_counter(ctx, "test_counter", nullptr, 0);
    ro_counter_inc(counter, 100);

    ro_reset_metrics(ctx);

    EXPECT_EQ(ro_counter_get(counter), 0);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST(RoStringTest, MetricTypeToString) {
    EXPECT_STREQ("Counter", ro_metric_type_to_string(RO_METRIC_COUNTER));
    EXPECT_STREQ("Gauge", ro_metric_type_to_string(RO_METRIC_GAUGE));
    EXPECT_STREQ("Histogram", ro_metric_type_to_string(RO_METRIC_HISTOGRAM));
}

TEST(RoStringTest, SpanStatusToString) {
    EXPECT_STREQ("Unset", ro_span_status_to_string(RO_SPAN_UNSET));
    EXPECT_STREQ("OK", ro_span_status_to_string(RO_SPAN_OK));
    EXPECT_STREQ("Error", ro_span_status_to_string(RO_SPAN_ERROR));
}

TEST(RoStringTest, SeverityToString) {
    EXPECT_STREQ("Trace", ro_severity_to_string(RO_SEVERITY_TRACE));
    EXPECT_STREQ("Error", ro_severity_to_string(RO_SEVERITY_ERROR));
    EXPECT_STREQ("Fatal", ro_severity_to_string(RO_SEVERITY_FATAL));
}

TEST(RoStringTest, EventTypeToString) {
    EXPECT_STREQ("FailureDetected", ro_event_type_to_string(RO_EVENT_FAILURE_DETECTED));
    EXPECT_STREQ("RecoverySuccess", ro_event_type_to_string(RO_EVENT_RECOVERY_SUCCESS));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(RecoveryObservabilityTest, ExportEmptyBuffer) {
    char buffer[10];  // Too small
    size_t written = ro_export_metrics(ctx, RO_EXPORT_JSON, buffer, 10);
    // Should handle gracefully
    EXPECT_LT(written, 10);
}

TEST_F(RecoveryObservabilityTest, MaxMetrics) {
    // Try to create more than max metrics
    for (int i = 0; i < RO_MAX_METRICS + 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "counter_%d", i);
        ro_counter_t* c = ro_create_counter(ctx, name, nullptr, 0);

        if (i >= RO_MAX_METRICS) {
            EXPECT_EQ(c, nullptr);
        }
    }
}
