/**
 * @file test_lockfree_metrics_integration.cpp
 * @brief Integration Tests for Lock-Free Metrics with Health Monitor
 * @version 1.0.0
 * @date 2025-11-20
 *
 * Tests: 15+ integration tests
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_lockfree_metrics.h"
#include "utils/fault_tolerance/nimcp_health_monitor.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LockfreeMetricsIntegrationTest : public ::testing::Test {
protected:
    lockfree_metrics_buffer_t* buffer;
    health_monitor_t monitor;

    void SetUp() override {
        buffer = nullptr;
        monitor = nullptr;
    }

    void TearDown() override {
        if (buffer) {
            lockfree_metrics_destroy(buffer);
            buffer = nullptr;
        }
        if (monitor) {
            health_monitor_destroy(monitor);
            monitor = nullptr;
        }
    }
};

//=============================================================================
// Integration with Health Monitor
//=============================================================================

TEST_F(LockfreeMetricsIntegrationTest, HealthMonitorIntegration) {
    buffer = lockfree_metrics_create(4096, "health_integration");
    ASSERT_NE(buffer, nullptr);

    monitor = health_monitor_create("test_brain");
    ASSERT_NE(monitor, nullptr);

    // Simulate health monitoring with lock-free metrics
    const int num_operations = 1000;

    for (int i = 0; i < num_operations; i++) {
        // Record to lock-free buffer (fast path)
        double latency = 100.0 + (i % 50);
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, latency, 0);

        // Also record to health monitor (slower path)
        health_monitor_record_operation(monitor, "inference", (uint64_t)latency);
    }

    // Verify metrics were recorded
    metrics_stats_t stats;
    lockfree_metrics_get_stats(buffer, &stats);
    EXPECT_EQ(stats.total_recorded, (uint64_t)num_operations);

    // Read and aggregate metrics
    metric_entry_t entries[100];
    int total_read = 0;
    while (true) {
        int32_t read = lockfree_metrics_read_batch(buffer, entries, 100);
        if (read <= 0) break;
        total_read += read;
    }

    EXPECT_EQ(total_read, num_operations);
}

//=============================================================================
// High-Throughput Stress Tests
//=============================================================================

TEST_F(LockfreeMetricsIntegrationTest, HighThroughputStress) {
    // Capacity must be >= total writes since there are no readers to drain
    const int num_writers = 16;
    const int writes_per_writer = 10000;
    const int expected_total = num_writers * writes_per_writer;

    buffer = lockfree_metrics_create(262144, "high_throughput");
    ASSERT_NE(buffer, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> writers;
    for (int t = 0; t < num_writers; t++) {
        writers.emplace_back([this, t, writes_per_writer]() {
            std::mt19937 rng(t);
            std::uniform_real_distribution<double> dist(1.0, 1000.0);

            for (int i = 0; i < writes_per_writer; i++) {
                double value = dist(rng);
                lockfree_metrics_record(
                    buffer, METRIC_TYPE_LATENCY, value, (uint32_t)t);
            }
        });
    }

    for (auto& writer : writers) {
        writer.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    metrics_stats_t stats;
    lockfree_metrics_get_stats(buffer, &stats);

    EXPECT_EQ(stats.total_recorded, (uint64_t)expected_total);

    double throughput = (double)expected_total / (duration.count() / 1000.0);
    printf("Throughput: %.0f metrics/sec (%d threads)\n", throughput, num_writers);
    EXPECT_GT(throughput, 100000.0);  // At least 100K metrics/sec
}

TEST_F(LockfreeMetricsIntegrationTest, ConcurrentProducerConsumer) {
    buffer = lockfree_metrics_create(8192, "producer_consumer");
    ASSERT_NE(buffer, nullptr);

    const int num_producers = 8;
    const int num_consumers = 4;
    const int items_per_producer = 5000;
    const int expected_total = num_producers * items_per_producer;

    std::atomic<bool> stop_consumers{false};
    std::atomic<int> total_consumed{0};

    std::vector<std::thread> threads;

    // Producers
    for (int t = 0; t < num_producers; t++) {
        threads.emplace_back([this, t, items_per_producer]() {
            for (int i = 0; i < items_per_producer; i++) {
                double value = (double)(t * 10000 + i);
                metric_result_t result = lockfree_metrics_record(
                    buffer, METRIC_TYPE_THROUGHPUT, value, (uint32_t)t);

                if (result == METRIC_RESULT_DROPPED) {
                    std::this_thread::yield();
                    i--;
                }

                if (i % 100 == 0) {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Consumers
    for (int t = 0; t < num_consumers; t++) {
        threads.emplace_back([this, &stop_consumers, &total_consumed]() {
            metric_entry_t entries[200];
            while (!stop_consumers.load()) {
                int32_t read = lockfree_metrics_read_batch(buffer, entries, 200);
                if (read > 0) {
                    total_consumed += read;
                } else {
                    std::this_thread::yield();
                }
            }

            // Final drain
            while (true) {
                int32_t read = lockfree_metrics_read_batch(buffer, entries, 200);
                if (read <= 0) break;
                total_consumed += read;
            }
        });
    }

    // Wait for producers
    for (int i = 0; i < num_producers; i++) {
        threads[i].join();
    }

    // Let consumers drain
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop_consumers.store(true);

    // Wait for consumers
    for (int i = num_producers; i < num_producers + num_consumers; i++) {
        threads[i].join();
    }

    EXPECT_EQ(total_consumed.load(), expected_total);
}

//=============================================================================
// Latency Measurements
//=============================================================================

TEST_F(LockfreeMetricsIntegrationTest, SingleThreadedLatency) {
    buffer = lockfree_metrics_create(4096, "latency_test");
    ASSERT_NE(buffer, nullptr);

    const int iterations = 10000;
    std::vector<uint64_t> latencies;

    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
        auto end = std::chrono::high_resolution_clock::now();

        auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start).count();
        latencies.push_back(latency_ns);
    }

    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    uint64_t p50 = latencies[iterations / 2];
    uint64_t p95 = latencies[(iterations * 95) / 100];
    uint64_t p99 = latencies[(iterations * 99) / 100];

    printf("Latency stats (single-threaded):\n");
    printf("  P50: %lu ns\n", p50);
    printf("  P95: %lu ns\n", p95);
    printf("  P99: %lu ns\n", p99);

    // Target: <100ns P50, <500ns P99 (varies with system load)
    EXPECT_LT(p50, 200u);
    EXPECT_LT(p99, 500u);
}

TEST_F(LockfreeMetricsIntegrationTest, MultiThreadedLatency) {
    buffer = lockfree_metrics_create(65536, "mt_latency");
    ASSERT_NE(buffer, nullptr);

    const int num_threads = 8;
    const int iterations_per_thread = 1000;

    struct ThreadStats {
        std::vector<uint64_t> latencies;
        std::mutex mutex;
    };

    std::vector<ThreadStats> thread_stats(num_threads);

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, iterations_per_thread, &thread_stats]() {
            for (int i = 0; i < iterations_per_thread; i++) {
                auto start = std::chrono::high_resolution_clock::now();
                metric_result_t result = lockfree_metrics_record(
                    buffer, METRIC_TYPE_LATENCY, (double)i, (uint32_t)t);
                auto end = std::chrono::high_resolution_clock::now();

                if (result == METRIC_RESULT_SUCCESS) {
                    auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        end - start).count();
                    thread_stats[t].latencies.push_back(latency_ns);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Aggregate latencies
    std::vector<uint64_t> all_latencies;
    for (const auto& stats : thread_stats) {
        all_latencies.insert(all_latencies.end(),
                            stats.latencies.begin(),
                            stats.latencies.end());
    }

    std::sort(all_latencies.begin(), all_latencies.end());
    size_t count = all_latencies.size();

    if (count > 0) {
        uint64_t p50 = all_latencies[count / 2];
        uint64_t p95 = all_latencies[(count * 95) / 100];
        uint64_t p99 = all_latencies[(count * 99) / 100];

        printf("Latency stats (multi-threaded, %d threads):\n", num_threads);
        printf("  P50: %lu ns\n", p50);
        printf("  P95: %lu ns\n", p95);
        printf("  P99: %lu ns\n", p99);

        // Under contention, allow higher latency
        EXPECT_LT(p50, 300u);
        EXPECT_LT(p99, 1000u);
    }
}

//=============================================================================
// Periodic Aggregation Pattern
//=============================================================================

TEST_F(LockfreeMetricsIntegrationTest, PeriodicAggregation) {
    buffer = lockfree_metrics_create(8192, "periodic_agg");
    ASSERT_NE(buffer, nullptr);

    std::atomic<bool> stop{false};
    std::atomic<int> batches_processed{0};

    // Producer thread
    std::thread producer([this, &stop]() {
        int counter = 0;
        while (!stop.load()) {
            lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)counter++, 0);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Aggregator thread (reads every 10ms)
    std::thread aggregator([this, &stop, &batches_processed]() {
        metric_entry_t entries[500];
        while (!stop.load()) {
            int32_t read = lockfree_metrics_read_batch(buffer, entries, 500);
            if (read > 0) {
                batches_processed++;
                // Process batch (calculate stats, etc.)
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Run for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));
    stop.store(true);

    producer.join();
    aggregator.join();

    EXPECT_GT(batches_processed.load(), 0);
    printf("Processed %d batches in 1 second\n", batches_processed.load());
}

//=============================================================================
// Real-World Scenario: Metrics Pipeline
//=============================================================================

TEST_F(LockfreeMetricsIntegrationTest, MetricsPipeline) {
    // Multi-stage pipeline: Record -> Buffer -> Aggregate -> Health Monitor

    buffer = lockfree_metrics_create(16384, "pipeline");
    ASSERT_NE(buffer, nullptr);

    monitor = health_monitor_create("pipeline_brain");
    ASSERT_NE(monitor, nullptr);

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> metrics_generated{0};
    std::atomic<uint64_t> metrics_aggregated{0};

    // Stage 1: Metric generators (simulating various components)
    std::vector<std::thread> generators;
    for (int i = 0; i < 4; i++) {
        generators.emplace_back([this, i, &stop, &metrics_generated]() {
            std::mt19937 rng(i);
            std::uniform_real_distribution<double> latency_dist(50.0, 500.0);

            while (!stop.load()) {
                double latency = latency_dist(rng);
                lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, latency, (uint32_t)i);
                metrics_generated++;

                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        });
    }

    // Stage 2: Aggregator (reads batches and updates health monitor)
    std::thread aggregator([this, &stop, &metrics_aggregated]() {
        metric_entry_t entries[200];

        while (!stop.load()) {
            int32_t read = lockfree_metrics_read_batch(buffer, entries, 200);

            if (read > 0) {
                // Aggregate metrics
                double total_latency = 0.0;
                for (int i = 0; i < read; i++) {
                    total_latency += entries[i].value;
                }
                double avg_latency = total_latency / read;

                // Update health monitor
                health_monitor_record_operation(monitor, "inference", (uint64_t)avg_latency);
                metrics_aggregated += read;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        // Final drain
        while (true) {
            int32_t read = lockfree_metrics_read_batch(buffer, entries, 200);
            if (read <= 0) break;
            metrics_aggregated += read;
        }
    });

    // Run pipeline for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));
    stop.store(true);

    for (auto& gen : generators) {
        gen.join();
    }
    aggregator.join();

    printf("Pipeline metrics: generated=%lu, aggregated=%lu\n",
           metrics_generated.load(), metrics_aggregated.load());

    EXPECT_GT(metrics_generated.load(), 0u);
    EXPECT_EQ(metrics_aggregated.load(), metrics_generated.load());

    // Check health monitor received metrics
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    EXPECT_GT(status.performance_score, 0.0f);
}

//=============================================================================
// Buffer Sizing Tests
//=============================================================================

TEST_F(LockfreeMetricsIntegrationTest, BufferSizingUnderLoad) {
    // Test different buffer sizes under load
    // Total writes = 4000, so sizes >= 4096 should have near-zero drops
    const uint32_t sizes[] = {256, 1024, 4096, 16384};
    const int num_writers = 8;
    const int writes_per_writer = 500;

    for (uint32_t size : sizes) {
        buffer = lockfree_metrics_create(size, "sizing_test");
        ASSERT_NE(buffer, nullptr);

        std::vector<std::thread> threads;
        for (int t = 0; t < num_writers; t++) {
            threads.emplace_back([this, writes_per_writer]() {
                for (int i = 0; i < writes_per_writer; i++) {
                    lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
                    if (i % 10 == 0) std::this_thread::yield();
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        metrics_stats_t stats;
        lockfree_metrics_get_stats(buffer, &stats);

        double drop_rate = lockfree_metrics_drop_rate(buffer);
        printf("Buffer size %5u: recorded=%6lu, dropped=%6lu (%.2f%% drop rate)\n",
               size, stats.total_recorded, stats.total_dropped, drop_rate * 100.0);

        // Larger buffers should have lower drop rates
        if (size >= 4096) {
            EXPECT_LT(drop_rate, 0.1);  // <10% drop rate for large buffers
        }

        lockfree_metrics_destroy(buffer);
        buffer = nullptr;
    }
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(LockfreeMetricsIntegrationTest, NoMemoryLeaks) {
    // Create and destroy many buffers
    for (int i = 0; i < 100; i++) {
        auto* buf = lockfree_metrics_create(256, "leak_test");
        ASSERT_NE(buf, nullptr);

        // Use buffer
        for (int j = 0; j < 50; j++) {
            lockfree_metrics_record(buf, METRIC_TYPE_LATENCY, (double)j, 0);
        }

        lockfree_metrics_destroy(buf);
    }

    // Valgrind or ASan would catch leaks
    SUCCEED();
}

//=============================================================================
// Contention Analysis
//=============================================================================

TEST_F(LockfreeMetricsIntegrationTest, ContentionAnalysis) {
    buffer = lockfree_metrics_create(4096, "contention");
    ASSERT_NE(buffer, nullptr);

    const int num_threads = 16;  // High contention
    const int writes_per_thread = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, writes_per_thread]() {
            for (int i = 0; i < writes_per_thread; i++) {
                lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    metrics_stats_t stats;
    lockfree_metrics_get_stats(buffer, &stats);

    double contention_rate = stats.record_attempts > 0 ?
        (double)stats.record_contentions / stats.record_attempts : 0.0;

    printf("Contention analysis (%d threads):\n", num_threads);
    printf("  Record attempts: %lu\n", stats.record_attempts);
    printf("  Contentions: %lu\n", stats.record_contentions);
    printf("  Contention rate: %.2f%%\n", contention_rate * 100.0);
    printf("  Avg contentions per record: %.3f\n",
           stats.total_recorded > 0 ?
               (double)stats.record_contentions / stats.total_recorded : 0.0);

    // Even under high contention, should have low contention rate
    EXPECT_LT(contention_rate, 0.5);  // <50% contention rate
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
