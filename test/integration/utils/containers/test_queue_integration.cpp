/**
 * @file test_queue_integration.cpp
 * @brief Integration tests for NIMCP queue types in realistic scenarios
 *
 * WHAT: Comprehensive integration testing of BLOCKING, SPSC, and MPMC queues
 * WHY:  Verify queue behavior under realistic multi-threaded workloads
 * HOW:  Producer-consumer patterns, throughput tests, stress tests, and real-world simulations
 *
 * TEST SCENARIOS:
 * 1. Producer-Consumer Patterns (SPSC, MPMC, BLOCKING)
 * 2. Throughput Benchmarks (operations per second)
 * 3. Stress Tests (10K+ items, data integrity)
 * 4. Mixed Operations (concurrent enqueue/dequeue)
 * 5. Backpressure Handling (queue full/empty scenarios)
 * 6. Real-world Simulations (event queue, work queue)
 * 7. Memory Pressure (large items, many cycles)
 * 8. Queue Type Selection (demonstrate optimal use cases)
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <algorithm>
#include <set>
#include "utils/containers/nimcp_queue.h"

//=============================================================================
// Performance Thresholds (relaxed for debug builds)
//=============================================================================

// Performance targets - relaxed for hardware variance and different system loads
// These are regression guards, not strict performance requirements
#ifdef NDEBUG
// Release build targets (with headroom for system variance, atomic stats, and different hardware)
static constexpr double kSPSCNsPerOp = 1000.0;   // 1000ns per SPSC operation
static constexpr double kMPMCNsPerOp = 5000.0;   // 5000ns per MPMC operation (accounts for contention)
#else
// Debug build targets (relaxed for atomic statistics overhead and debug instrumentation)
static constexpr double kSPSCNsPerOp = 5000.0;   // 5000ns per SPSC operation (debug + atomics)
static constexpr double kMPMCNsPerOp = 15000.0;  // 15000ns per MPMC operation (debug + atomics)
#endif

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/**
 * @brief Base fixture for queue integration tests
 *
 * WHAT: Common setup/teardown for queue tests
 * WHY:  Reduce code duplication, ensure proper cleanup
 * HOW:  RAII pattern for queue lifecycle
 */
class QueueIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Queues created in individual tests
    }

    void TearDown() override {
        // Clean up any remaining queues
        for (auto queue : queues_) {
            if (queue) {
                nimcp_queue_destroy(queue);
            }
        }
        queues_.clear();
    }

    /**
     * @brief Create queue and register for cleanup
     *
     * WHAT: Factory method with automatic cleanup
     * WHY:  Prevent resource leaks in tests
     * HOW:  Track created queues, destroy in teardown
     */
    nimcp_queue_handle_t CreateQueue(const nimcp_queue_config_t& config) {
        nimcp_queue_handle_t queue = nullptr;
        nimcp_result_t result = nimcp_queue_create(&config, &queue);
        EXPECT_EQ(NIMCP_SUCCESS, result);
        if (queue) {
            queues_.push_back(queue);
        }
        return queue;
    }

    std::vector<nimcp_queue_handle_t> queues_;
};

/**
 * @brief Test data item structure
 *
 * WHAT: Simple payload for queue operations
 * WHY:  Verify data integrity through queue operations
 * HOW:  ID for ordering, value for content verification
 */
struct TestItem {
    uint64_t id;
    uint64_t value;
    uint64_t timestamp;
    uint64_t checksum;

    TestItem() : id(0), value(0), timestamp(0), checksum(0) {}
    TestItem(uint64_t i, uint64_t v) : id(i), value(v) {
        timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        checksum = id ^ value ^ timestamp;
    }

    bool IsValid() const {
        return checksum == (id ^ value ^ timestamp);
    }
};

/**
 * @brief Large test item for memory pressure tests
 */
struct LargeTestItem {
    uint64_t id;
    uint8_t padding[1024];  // 1KB item
    uint64_t checksum;

    LargeTestItem() : id(0), checksum(0) {
        memset(padding, 0, sizeof(padding));
    }

    explicit LargeTestItem(uint64_t i) : id(i) {
        // Fill with pattern
        for (size_t j = 0; j < sizeof(padding); j++) {
            padding[j] = static_cast<uint8_t>((i + j) % 256);
        }
        checksum = ComputeChecksum();
    }

    uint64_t ComputeChecksum() const {
        uint64_t sum = id;
        for (size_t i = 0; i < sizeof(padding); i++) {
            sum ^= padding[i] << (i % 8);
        }
        return sum;
    }

    bool IsValid() const {
        return checksum == ComputeChecksum();
    }
};

//=============================================================================
// 1. Producer-Consumer Pattern Tests
//=============================================================================

/**
 * TEST: Single Producer Single Consumer (SPSC optimal)
 *
 * WHAT: One thread produces, one thread consumes
 * WHY:  Verify SPSC queue works correctly in its ideal use case
 * HOW:  Producer sends N items, consumer receives all N items
 *
 * EXPECTED: All items received in order, ultra-low latency (<100ns)
 */
TEST_F(QueueIntegrationTest, SPSC_SingleProducerSingleConsumer) {
    const size_t NUM_ITEMS = 100000;

    nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_SPSC);
    config.max_size = 1024;
    config.item_size = sizeof(TestItem);
    config.is_blocking = true;

    nimcp_queue_handle_t queue = CreateQueue(config);
    ASSERT_NE(nullptr, queue);

    std::atomic<bool> producer_done{false};
    std::vector<TestItem> received_items;
    received_items.reserve(NUM_ITEMS);

    // Consumer thread
    std::thread consumer([&]() {
        TestItem item;
        while (!producer_done.load() || nimcp_queue_get_size(queue) > 0) {
            if (nimcp_queue_try_dequeue(queue, &item)) {
                EXPECT_TRUE(item.IsValid());
                received_items.push_back(item);
            } else {
                std::this_thread::yield();
            }
        }
    });

    // Producer thread
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < NUM_ITEMS; i++) {
        TestItem item(i, i * 2);
        while (!nimcp_queue_try_enqueue(queue, &item)) {
            std::this_thread::yield();
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    producer_done = true;

    consumer.join();

    // Verify results
    EXPECT_EQ(NUM_ITEMS, received_items.size());

    // Verify order (SPSC guarantees FIFO)
    for (size_t i = 0; i < received_items.size(); i++) {
        EXPECT_EQ(i, received_items[i].id);
        EXPECT_EQ(i * 2, received_items[i].value);
    }

    // Check throughput
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    double ns_per_op = static_cast<double>(duration.count()) / NUM_ITEMS;
    EXPECT_LT(ns_per_op, kSPSCNsPerOp) << "SPSC should be < " << kSPSCNsPerOp << "ns per operation";

    std::cout << "SPSC throughput: " << ns_per_op << " ns/op" << std::endl;
}

/**
 * TEST: Multiple Producers Single Consumer
 *
 * WHAT: N producers, 1 consumer (MPMC or BLOCKING required)
 * WHY:  Verify queue handles multiple producers correctly
 * HOW:  Each producer sends unique items, consumer collects all
 *
 * EXPECTED: All items received, no duplicates, no data loss
 */
TEST_F(QueueIntegrationTest, MPMC_MultipleProducersSingleConsumer) {
    const size_t NUM_PRODUCERS = 4;
    const size_t ITEMS_PER_PRODUCER = 10000;
    const size_t TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_MPMC);
    config.max_size = 1024;
    config.item_size = sizeof(TestItem);
    config.is_blocking = false;  // Use non-blocking for stress test

    nimcp_queue_handle_t queue = CreateQueue(config);
    ASSERT_NE(nullptr, queue);

    std::atomic<size_t> producers_done{0};
    std::vector<TestItem> received_items;
    received_items.reserve(TOTAL_ITEMS);
    std::mutex received_mutex;

    // Consumer thread
    std::thread consumer([&]() {
        TestItem item;
        while (producers_done.load() < NUM_PRODUCERS || nimcp_queue_get_size(queue) > 0) {
            if (nimcp_queue_try_dequeue(queue, &item)) {
                EXPECT_TRUE(item.IsValid());
                std::lock_guard<std::mutex> lock(received_mutex);
                received_items.push_back(item);
            } else {
                std::this_thread::yield();
            }
        }
    });

    // Producer threads
    std::vector<std::thread> producers;
    for (size_t p = 0; p < NUM_PRODUCERS; p++) {
        producers.emplace_back([&, p]() {
            for (size_t i = 0; i < ITEMS_PER_PRODUCER; i++) {
                uint64_t item_id = p * ITEMS_PER_PRODUCER + i;
                TestItem item(item_id, item_id * 3);

                while (!nimcp_queue_try_enqueue(queue, &item)) {
                    std::this_thread::yield();
                }
            }
            producers_done.fetch_add(1);
        });
    }

    for (auto& t : producers) {
        t.join();
    }
    consumer.join();

    // Verify all items received
    EXPECT_EQ(TOTAL_ITEMS, received_items.size());

    // Verify no duplicates
    std::set<uint64_t> ids;
    for (const auto& item : received_items) {
        ids.insert(item.id);
    }
    EXPECT_EQ(TOTAL_ITEMS, ids.size()) << "Duplicate items detected";

    // Verify statistics
    nimcp_queue_status_t status;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_queue_get_status(queue, &status));
    EXPECT_EQ(TOTAL_ITEMS, status.total_enqueued);
    EXPECT_EQ(TOTAL_ITEMS, status.total_dequeued);
}

/**
 * TEST: Single Producer Multiple Consumers
 *
 * WHAT: 1 producer, N consumers
 * WHY:  Verify work distribution across consumers
 * HOW:  Producer sends items, multiple consumers compete
 *
 * EXPECTED: All items consumed exactly once, no data loss
 */
TEST_F(QueueIntegrationTest, MPMC_SingleProducerMultipleConsumers) {
    const size_t NUM_CONSUMERS = 4;
    const size_t NUM_ITEMS = 10000;

    nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_MPMC);
    config.max_size = 512;
    config.item_size = sizeof(TestItem);
    config.is_blocking = false;

    nimcp_queue_handle_t queue = CreateQueue(config);
    ASSERT_NE(nullptr, queue);

    std::atomic<bool> producer_done{false};
    std::vector<std::vector<TestItem>> consumer_items(NUM_CONSUMERS);
    std::vector<std::mutex> consumer_mutexes(NUM_CONSUMERS);

    // Consumer threads
    std::vector<std::thread> consumers;
    for (size_t c = 0; c < NUM_CONSUMERS; c++) {
        consumers.emplace_back([&, c]() {
            TestItem item;
            while (!producer_done.load() || nimcp_queue_get_size(queue) > 0) {
                if (nimcp_queue_try_dequeue(queue, &item)) {
                    EXPECT_TRUE(item.IsValid());
                    std::lock_guard<std::mutex> lock(consumer_mutexes[c]);
                    consumer_items[c].push_back(item);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Producer thread
    std::thread producer([&]() {
        for (size_t i = 0; i < NUM_ITEMS; i++) {
            TestItem item(i, i * 5);
            while (!nimcp_queue_try_enqueue(queue, &item)) {
                std::this_thread::yield();
            }
        }
        producer_done = true;
    });

    producer.join();
    for (auto& t : consumers) {
        t.join();
    }

    // Verify all items consumed
    size_t total_consumed = 0;
    std::set<uint64_t> all_ids;

    for (size_t c = 0; c < NUM_CONSUMERS; c++) {
        total_consumed += consumer_items[c].size();
        for (const auto& item : consumer_items[c]) {
            all_ids.insert(item.id);
        }
    }

    EXPECT_EQ(NUM_ITEMS, total_consumed);
    EXPECT_EQ(NUM_ITEMS, all_ids.size()) << "Some items consumed multiple times or lost";
}

/**
 * TEST: Multiple Producers Multiple Consumers (MPMC optimal)
 *
 * WHAT: N producers, M consumers (complex scenario)
 * WHY:  Verify MPMC handles full concurrency correctly
 * HOW:  Multiple threads producing and consuming simultaneously
 *
 * EXPECTED: All items processed exactly once, high throughput
 */
TEST_F(QueueIntegrationTest, MPMC_MultipleProducersMultipleConsumers) {
    const size_t NUM_PRODUCERS = 4;
    const size_t NUM_CONSUMERS = 4;
    const size_t ITEMS_PER_PRODUCER = 5000;
    const size_t TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_MPMC);
    config.max_size = 2048;
    config.item_size = sizeof(TestItem);
    config.is_blocking = false;

    nimcp_queue_handle_t queue = CreateQueue(config);
    ASSERT_NE(nullptr, queue);

    std::atomic<size_t> producers_done{0};
    std::atomic<size_t> items_consumed{0};
    std::set<uint64_t> consumed_ids;
    std::mutex consumed_mutex;

    // Consumer threads
    std::vector<std::thread> consumers;
    for (size_t c = 0; c < NUM_CONSUMERS; c++) {
        consumers.emplace_back([&]() {
            TestItem item;
            while (producers_done.load() < NUM_PRODUCERS || nimcp_queue_get_size(queue) > 0) {
                if (nimcp_queue_try_dequeue(queue, &item)) {
                    EXPECT_TRUE(item.IsValid());

                    std::lock_guard<std::mutex> lock(consumed_mutex);
                    consumed_ids.insert(item.id);
                    items_consumed.fetch_add(1);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Producer threads
    std::vector<std::thread> producers;
    for (size_t p = 0; p < NUM_PRODUCERS; p++) {
        producers.emplace_back([&, p]() {
            for (size_t i = 0; i < ITEMS_PER_PRODUCER; i++) {
                uint64_t item_id = p * ITEMS_PER_PRODUCER + i;
                TestItem item(item_id, item_id * 7);

                while (!nimcp_queue_try_enqueue(queue, &item)) {
                    std::this_thread::yield();
                }
            }
            producers_done.fetch_add(1);
        });
    }

    for (auto& t : producers) {
        t.join();
    }
    for (auto& t : consumers) {
        t.join();
    }

    // Verify results
    EXPECT_EQ(TOTAL_ITEMS, items_consumed.load());
    EXPECT_EQ(TOTAL_ITEMS, consumed_ids.size());

    nimcp_queue_status_t status;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_queue_get_status(queue, &status));
    EXPECT_EQ(TOTAL_ITEMS, status.total_enqueued);
    EXPECT_EQ(TOTAL_ITEMS, status.total_dequeued);
}

//=============================================================================
// 2. Throughput Tests
//=============================================================================

/**
 * TEST: Compare throughput across queue types
 *
 * WHAT: Measure operations per second for each queue type
 * WHY:  Verify performance characteristics match specifications
 * HOW:  Time fixed number of operations, calculate ops/sec
 *
 * EXPECTED: SPSC < 100ns, MPMC < 500ns, BLOCKING < 10us
 */
TEST_F(QueueIntegrationTest, ThroughputComparison) {
    const size_t NUM_OPERATIONS = 50000;

    struct BenchmarkResult {
        nimcp_queue_type_t type;
        double ns_per_op;
        uint64_t ops_per_sec;
    };

    std::vector<BenchmarkResult> results;

    // Test each queue type
    nimcp_queue_type_t types[] = {
        NIMCP_QUEUE_TYPE_SPSC,
        NIMCP_QUEUE_TYPE_MPMC,
        NIMCP_QUEUE_TYPE_BLOCKING
    };

    for (auto type : types) {
        nimcp_queue_config_t config = nimcp_queue_default_config(type);
        config.max_size = 1024;
        config.item_size = sizeof(TestItem);
        config.is_blocking = false;

        nimcp_queue_handle_t queue = CreateQueue(config);
        ASSERT_NE(nullptr, queue);

        std::atomic<bool> done{false};

        // Consumer thread
        std::thread consumer([&]() {
            TestItem item;
            while (!done.load() || nimcp_queue_get_size(queue) > 0) {
                nimcp_queue_try_dequeue(queue, &item);
            }
        });

        // Producer thread (timed)
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < NUM_OPERATIONS; i++) {
            TestItem item(i, i);
            while (!nimcp_queue_try_enqueue(queue, &item)) {
                std::this_thread::yield();
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        done = true;

        consumer.join();

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        double ns_per_op = static_cast<double>(duration.count()) / NUM_OPERATIONS;
        uint64_t ops_per_sec = static_cast<uint64_t>(1e9 / ns_per_op);

        results.push_back({type, ns_per_op, ops_per_sec});

        std::cout << nimcp_queue_type_name(type) << ": "
                  << ns_per_op << " ns/op, "
                  << ops_per_sec << " ops/sec" << std::endl;
    }

    // Verify performance expectations (thresholds relaxed for debug builds)
    for (const auto& result : results) {
        switch (result.type) {
            case NIMCP_QUEUE_TYPE_SPSC:
                EXPECT_LT(result.ns_per_op, kSPSCNsPerOp) << "SPSC should be ultra-low latency";
                break;
            case NIMCP_QUEUE_TYPE_MPMC:
                EXPECT_LT(result.ns_per_op, kMPMCNsPerOp) << "MPMC should be low latency";
                break;
            case NIMCP_QUEUE_TYPE_BLOCKING:
                EXPECT_LT(result.ns_per_op, 10000.0) << "BLOCKING should be moderate latency";
                break;
        }
    }
}

//=============================================================================
// 3. Stress Tests
//=============================================================================

/**
 * TEST: High-volume data integrity
 *
 * WHAT: Push 100K+ items through queue, verify all data
 * WHY:  Ensure no data loss or corruption under stress
 * HOW:  Send items with checksums, verify on receive
 *
 * EXPECTED: 100% data integrity, no loss, no corruption
 */
TEST_F(QueueIntegrationTest, StressTest_DataIntegrity) {
    const size_t NUM_ITEMS = 100000;

    nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_MPMC);
    config.max_size = 4096;
    config.item_size = sizeof(TestItem);
    config.is_blocking = false;

    nimcp_queue_handle_t queue = CreateQueue(config);
    ASSERT_NE(nullptr, queue);

    std::atomic<bool> producer_done{false};
    std::vector<TestItem> received;
    received.reserve(NUM_ITEMS);

    // Consumer
    std::thread consumer([&]() {
        TestItem item;
        while (!producer_done.load() || nimcp_queue_get_size(queue) > 0) {
            if (nimcp_queue_try_dequeue(queue, &item)) {
                EXPECT_TRUE(item.IsValid()) << "Corrupted item at id=" << item.id;
                received.push_back(item);
            }
        }
    });

    // Producer
    for (size_t i = 0; i < NUM_ITEMS; i++) {
        TestItem item(i, i * 11);
        while (!nimcp_queue_try_enqueue(queue, &item)) {
            std::this_thread::yield();
        }
    }
    producer_done = true;

    consumer.join();

    // Verify
    EXPECT_EQ(NUM_ITEMS, received.size());

    std::sort(received.begin(), received.end(),
              [](const TestItem& a, const TestItem& b) { return a.id < b.id; });

    for (size_t i = 0; i < received.size(); i++) {
        EXPECT_EQ(i, received[i].id);
        EXPECT_EQ(i * 11, received[i].value);
        EXPECT_TRUE(received[i].IsValid());
    }
}

//=============================================================================
// 4. Mixed Operations Tests
//=============================================================================

/**
 * TEST: Batch and single operations interleaved
 *
 * WHAT: Mix batch enqueue/dequeue with single operations
 * WHY:  Verify batch operations work correctly alongside single ops
 * HOW:  Randomly choose single or batch operations
 *
 * EXPECTED: All items processed, no interference between modes
 */
TEST_F(QueueIntegrationTest, MixedOperations_BatchAndSingle) {
    const size_t NUM_ITEMS = 10000;
    const size_t kMaxBatchSize = 100;

    nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_BLOCKING);
    config.max_size = 2048;
    config.item_size = sizeof(TestItem);
    config.is_blocking = true;
    config.timeout_ms = 100;

    nimcp_queue_handle_t queue = CreateQueue(config);
    ASSERT_NE(nullptr, queue);

    std::atomic<bool> producer_done{false};
    std::vector<TestItem> received;
    std::mutex received_mutex;

    std::mt19937 rng(12345);
    std::uniform_int_distribution<size_t> batch_dist(1, kMaxBatchSize);

    // Consumer with mixed operations
    std::thread consumer([&]() {
        std::mt19937 consumer_rng(54321);
        std::uniform_int_distribution<int> mode_dist(0, 1);

        while (!producer_done.load() || nimcp_queue_get_size(queue) > 0) {
            if (mode_dist(consumer_rng) == 0) {
                // Single dequeue
                TestItem item;
                if (nimcp_queue_try_dequeue(queue, &item)) {
                    EXPECT_TRUE(item.IsValid());
                    std::lock_guard<std::mutex> lock(received_mutex);
                    received.push_back(item);
                }
            } else {
                // Batch dequeue
                size_t batch_size = batch_dist(consumer_rng);
                std::vector<TestItem> batch(batch_size);
                size_t dequeued = 0;

                if (nimcp_queue_dequeue_batch(queue, batch.data(), batch_size,
                                              &dequeued, 0) == NIMCP_SUCCESS) {
                    std::lock_guard<std::mutex> lock(received_mutex);
                    for (size_t i = 0; i < dequeued; i++) {
                        EXPECT_TRUE(batch[i].IsValid());
                        received.push_back(batch[i]);
                    }
                }
            }
        }
    });

    // Producer with mixed operations
    size_t produced = 0;
    while (produced < NUM_ITEMS) {
        if (batch_dist(rng) % 2 == 0) {
            // Single enqueue
            TestItem item(produced, produced * 13);
            if (nimcp_queue_try_enqueue(queue, &item)) {
                produced++;
            }
        } else {
            // Batch enqueue
            size_t batch_size = std::min(batch_dist(rng), NUM_ITEMS - produced);
            std::vector<TestItem> batch;
            for (size_t i = 0; i < batch_size; i++) {
                batch.emplace_back(produced + i, (produced + i) * 13);
            }

            size_t enqueued = 0;
            nimcp_queue_enqueue_batch(queue, batch.data(), batch.size(),
                                     &enqueued, 100);
            produced += enqueued;
        }
    }
    producer_done = true;

    consumer.join();

    // Verify
    EXPECT_EQ(NUM_ITEMS, received.size());

    std::sort(received.begin(), received.end(),
              [](const TestItem& a, const TestItem& b) { return a.id < b.id; });

    for (size_t i = 0; i < received.size(); i++) {
        EXPECT_EQ(i, received[i].id);
    }
}

//=============================================================================
// 5. Backpressure Handling Tests
//=============================================================================

/**
 * TEST: Queue full behavior
 *
 * WHAT: Fill queue to capacity, verify blocking behavior
 * WHY:  Ensure backpressure mechanisms work correctly
 * HOW:  Producer faster than consumer, queue fills up
 *
 * EXPECTED: Blocking queues block, non-blocking return error
 */
TEST_F(QueueIntegrationTest, Backpressure_QueueFull) {
    const size_t QUEUE_SIZE = 128;

    nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_BLOCKING);
    config.max_size = QUEUE_SIZE;
    config.item_size = sizeof(TestItem);
    config.is_blocking = false;  // Test non-blocking behavior

    nimcp_queue_handle_t queue = CreateQueue(config);
    ASSERT_NE(nullptr, queue);

    // Fill queue
    for (size_t i = 0; i < QUEUE_SIZE; i++) {
        TestItem item(i, i);
        EXPECT_TRUE(nimcp_queue_try_enqueue(queue, &item));
    }

    EXPECT_TRUE(nimcp_queue_is_full(queue));

    // Try to enqueue when full
    TestItem overflow_item(999, 999);
    EXPECT_FALSE(nimcp_queue_try_enqueue(queue, &overflow_item));

    // Verify statistics
    nimcp_queue_status_t status;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_queue_get_status(queue, &status));
    EXPECT_GT(status.enqueue_failures, 0ULL);

    // Drain queue
    TestItem item;
    for (size_t i = 0; i < QUEUE_SIZE; i++) {
        EXPECT_TRUE(nimcp_queue_try_dequeue(queue, &item));
        EXPECT_EQ(i, item.id);
    }

    EXPECT_TRUE(nimcp_queue_is_empty(queue));
}

//=============================================================================
// 6. Real-world Simulation Tests
//=============================================================================

/**
 * TEST: Event queue pattern
 *
 * WHAT: Simulate event processing system
 * WHY:  Verify queue works in realistic event-driven architecture
 * HOW:  Multiple event producers, priority consumer
 *
 * EXPECTED: All events processed, low latency
 */
TEST_F(QueueIntegrationTest, RealWorld_EventQueue) {
    const size_t NUM_EVENT_SOURCES = 8;
    const size_t EVENTS_PER_SOURCE = 1000;
    const size_t TOTAL_EVENTS = NUM_EVENT_SOURCES * EVENTS_PER_SOURCE;

    nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_MPMC);
    config.max_size = 1024;
    config.item_size = sizeof(TestItem);
    config.is_blocking = true;

    nimcp_queue_handle_t queue = CreateQueue(config);
    ASSERT_NE(nullptr, queue);

    std::atomic<size_t> sources_done{0};
    std::atomic<size_t> events_processed{0};

    // Event processor (consumer)
    std::thread processor([&]() {
        TestItem event;
        while (sources_done.load() < NUM_EVENT_SOURCES || nimcp_queue_get_size(queue) > 0) {
            if (nimcp_queue_try_dequeue(queue, &event)) {
                EXPECT_TRUE(event.IsValid());
                events_processed.fetch_add(1);

                // Simulate event processing
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
    });

    // Event sources (producers)
    std::vector<std::thread> sources;
    for (size_t s = 0; s < NUM_EVENT_SOURCES; s++) {
        sources.emplace_back([&, s]() {
            std::mt19937 rng(s);
            std::uniform_int_distribution<int> delay_dist(10, 100);

            for (size_t i = 0; i < EVENTS_PER_SOURCE; i++) {
                uint64_t event_id = s * EVENTS_PER_SOURCE + i;
                TestItem event(event_id, s);

                while (!nimcp_queue_try_enqueue(queue, &event)) {
                    std::this_thread::yield();
                }

                // Simulate variable event rate
                std::this_thread::sleep_for(std::chrono::microseconds(delay_dist(rng)));
            }
            sources_done.fetch_add(1);
        });
    }

    for (auto& t : sources) {
        t.join();
    }
    processor.join();

    EXPECT_EQ(TOTAL_EVENTS, events_processed.load());
}

/**
 * TEST: Work queue pattern
 *
 * WHAT: Simulate task distribution to worker threads
 * WHY:  Verify queue works for work-stealing pattern
 * HOW:  Master enqueues tasks, workers compete to process
 *
 * EXPECTED: All tasks completed, balanced distribution
 */
TEST_F(QueueIntegrationTest, RealWorld_WorkQueue) {
    const size_t NUM_WORKERS = 4;
    const size_t NUM_TASKS = 10000;

    nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_MPMC);
    config.max_size = 512;
    config.item_size = sizeof(TestItem);
    config.is_blocking = true;

    nimcp_queue_handle_t queue = CreateQueue(config);
    ASSERT_NE(nullptr, queue);

    std::atomic<bool> all_tasks_submitted{false};
    std::atomic<size_t> tasks_completed{0};
    std::vector<size_t> tasks_per_worker(NUM_WORKERS, 0);
    std::vector<std::mutex> worker_mutexes(NUM_WORKERS);

    // Worker threads
    std::vector<std::thread> workers;
    for (size_t w = 0; w < NUM_WORKERS; w++) {
        workers.emplace_back([&, w]() {
            TestItem task;
            while (!all_tasks_submitted.load() || nimcp_queue_get_size(queue) > 0) {
                if (nimcp_queue_try_dequeue(queue, &task)) {
                    EXPECT_TRUE(task.IsValid());

                    // Simulate task processing
                    std::this_thread::sleep_for(std::chrono::microseconds(10));

                    tasks_per_worker[w]++;
                    tasks_completed.fetch_add(1);
                }
            }
        });
    }

    // Master thread submits tasks
    for (size_t i = 0; i < NUM_TASKS; i++) {
        TestItem task(i, i * 17);
        while (!nimcp_queue_try_enqueue(queue, &task)) {
            std::this_thread::yield();
        }
    }
    all_tasks_submitted = true;

    for (auto& t : workers) {
        t.join();
    }

    EXPECT_EQ(NUM_TASKS, tasks_completed.load());

    // Verify reasonable load distribution
    size_t min_tasks = *std::min_element(tasks_per_worker.begin(), tasks_per_worker.end());
    size_t max_tasks = *std::max_element(tasks_per_worker.begin(), tasks_per_worker.end());

    std::cout << "Work distribution - min: " << min_tasks << ", max: " << max_tasks << std::endl;

    // No worker should be idle (assuming tasks > workers)
    EXPECT_GT(min_tasks, 0UL);
}

//=============================================================================
// 7. Memory Pressure Tests
//=============================================================================

/**
 * TEST: Large item sizes
 *
 * WHAT: Queue operations with large (1KB+) items
 * WHY:  Verify queue handles large items efficiently
 * HOW:  Process items with significant memory footprint
 *
 * EXPECTED: Correct operation, no performance degradation
 */
TEST_F(QueueIntegrationTest, MemoryPressure_LargeItems) {
    const size_t NUM_ITEMS = 1000;

    nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_SPSC);
    config.max_size = 128;
    config.item_size = sizeof(LargeTestItem);
    config.is_blocking = false;

    nimcp_queue_handle_t queue = CreateQueue(config);
    ASSERT_NE(nullptr, queue);

    std::atomic<bool> producer_done{false};
    std::vector<LargeTestItem> received;

    // Consumer
    std::thread consumer([&]() {
        LargeTestItem item;
        while (!producer_done.load() || nimcp_queue_get_size(queue) > 0) {
            if (nimcp_queue_try_dequeue(queue, &item)) {
                EXPECT_TRUE(item.IsValid()) << "Corrupted large item at id=" << item.id;
                received.push_back(item);
            }
        }
    });

    // Producer
    for (size_t i = 0; i < NUM_ITEMS; i++) {
        LargeTestItem item(i);
        while (!nimcp_queue_try_enqueue(queue, &item)) {
            std::this_thread::yield();
        }
    }
    producer_done = true;

    consumer.join();

    EXPECT_EQ(NUM_ITEMS, received.size());

    for (size_t i = 0; i < received.size(); i++) {
        EXPECT_TRUE(received[i].IsValid());
    }
}

/**
 * TEST: Many create/destroy cycles
 *
 * WHAT: Repeatedly create and destroy queues
 * WHY:  Verify no memory leaks or resource exhaustion
 * HOW:  Create, use, destroy queue many times
 *
 * EXPECTED: Consistent behavior, no leaks
 */
TEST_F(QueueIntegrationTest, MemoryPressure_CreateDestroyCycles) {
    const size_t NUM_CYCLES = 100;
    const size_t ITEMS_PER_CYCLE = 200;  // Must be <= queue capacity

    for (size_t cycle = 0; cycle < NUM_CYCLES; cycle++) {
        nimcp_queue_config_t config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_MPMC);
        config.max_size = 256;  // Queue capacity
        config.item_size = sizeof(TestItem);
        config.is_blocking = false;

        nimcp_queue_handle_t queue = nullptr;
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_queue_create(&config, &queue));
        ASSERT_NE(nullptr, queue);

        // Use queue - enqueue/dequeue in batches that fit
        for (size_t i = 0; i < ITEMS_PER_CYCLE; i++) {
            TestItem item(i, i);
            EXPECT_TRUE(nimcp_queue_try_enqueue(queue, &item));
        }

        for (size_t i = 0; i < ITEMS_PER_CYCLE; i++) {
            TestItem item;
            EXPECT_TRUE(nimcp_queue_try_dequeue(queue, &item));
            EXPECT_EQ(i, item.id);
        }

        EXPECT_TRUE(nimcp_queue_is_empty(queue));

        // Destroy
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_queue_destroy(queue));
    }
}

//=============================================================================
// 8. Queue Type Selection Tests
//=============================================================================

/**
 * TEST: Demonstrate optimal queue type selection
 *
 * WHAT: Show when each queue type is most appropriate
 * WHY:  Guide users to correct queue type for their use case
 * HOW:  Compare performance in different scenarios
 *
 * EXPECTED: Clear performance differences based on use case
 */
TEST_F(QueueIntegrationTest, QueueSelection_UseCaseOptimization) {
    const size_t NUM_ITEMS = 10000;

    std::cout << "\n=== Queue Type Selection Guide ===\n";

    // Scenario 1: Single producer, single consumer
    {
        std::cout << "\nScenario 1: Single Producer, Single Consumer\n";

        nimcp_queue_type_t types[] = {NIMCP_QUEUE_TYPE_SPSC, NIMCP_QUEUE_TYPE_MPMC};

        for (auto type : types) {
            nimcp_queue_config_t config = nimcp_queue_default_config(type);
            config.max_size = 512;
            config.item_size = sizeof(TestItem);
            config.is_blocking = false;

            nimcp_queue_handle_t queue = CreateQueue(config);

            auto start = std::chrono::high_resolution_clock::now();

            std::thread consumer([&]() {
                TestItem item;
                size_t count = 0;
                while (count < NUM_ITEMS) {
                    if (nimcp_queue_try_dequeue(queue, &item)) {
                        count++;
                    }
                }
            });

            for (size_t i = 0; i < NUM_ITEMS; i++) {
                TestItem item(i, i);
                while (!nimcp_queue_try_enqueue(queue, &item)) {
                    std::this_thread::yield();
                }
            }

            consumer.join();
            auto end = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            std::cout << "  " << nimcp_queue_type_name(type) << ": "
                      << duration.count() << " us";
            if (type == NIMCP_QUEUE_TYPE_SPSC) {
                std::cout << " (OPTIMAL)";
            }
            std::cout << "\n";
        }
    }

    // Scenario 2: Multiple producers, multiple consumers
    {
        std::cout << "\nScenario 2: Multiple Producers, Multiple Consumers\n";

        nimcp_queue_type_t types[] = {NIMCP_QUEUE_TYPE_MPMC, NIMCP_QUEUE_TYPE_BLOCKING};

        for (auto type : types) {
            nimcp_queue_config_t config = nimcp_queue_default_config(type);
            config.max_size = 512;
            config.item_size = sizeof(TestItem);
            config.is_blocking = false;

            nimcp_queue_handle_t queue = CreateQueue(config);

            auto start = std::chrono::high_resolution_clock::now();

            std::atomic<size_t> consumed{0};
            std::atomic<size_t> produced{0};

            std::vector<std::thread> threads;

            // 2 producers
            for (int p = 0; p < 2; p++) {
                threads.emplace_back([&, p]() {
                    for (size_t i = 0; i < NUM_ITEMS / 2; i++) {
                        TestItem item(produced.fetch_add(1), 0);
                        while (!nimcp_queue_try_enqueue(queue, &item)) {
                            std::this_thread::yield();
                        }
                    }
                });
            }

            // 2 consumers
            for (int c = 0; c < 2; c++) {
                threads.emplace_back([&]() {
                    TestItem item;
                    while (consumed.load() < NUM_ITEMS) {
                        if (nimcp_queue_try_dequeue(queue, &item)) {
                            consumed.fetch_add(1);
                        }
                    }
                });
            }

            for (auto& t : threads) {
                t.join();
            }

            auto end = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            std::cout << "  " << nimcp_queue_type_name(type) << ": "
                      << duration.count() << " us";
            if (type == NIMCP_QUEUE_TYPE_MPMC) {
                std::cout << " (OPTIMAL)";
            }
            std::cout << "\n";
        }
    }

    std::cout << "\n=== Recommendations ===\n";
    std::cout << "BLOCKING: General purpose, unknown thread patterns, simple synchronization\n";
    std::cout << "SPSC: Dedicated thread pairs, ultra-low latency, pipeline stages\n";
    std::cout << "MPMC: Multiple threads, work stealing, event distribution\n";
    std::cout << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
