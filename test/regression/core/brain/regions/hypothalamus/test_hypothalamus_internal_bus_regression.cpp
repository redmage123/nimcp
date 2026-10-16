/**
 * @file test_hypothalamus_internal_bus_regression.cpp
 * @brief Regression tests for Hypothalamus Internal Bus
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Regression tests ensuring internal bus behavior remains stable
 *       across code changes and performance doesn't degrade
 * WHY:  Prevent regressions in pub-sub communication, modulation system,
 *       event delivery, and cross-module interaction
 * HOW:  Test performance bounds, memory stability, numerical accuracy,
 *       and boundary conditions
 *
 * TEST COVERAGE:
 * - Performance tests (publish latency, subscribe throughput, modulation lookup)
 * - Stability tests (long-running operations, memory stability, queue handling)
 * - Accuracy tests (modulation precision, event delivery, statistics)
 * - Boundary tests (max subscriptions, max data size, concurrent access)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>
#include <set>
#include <mutex>

// Header has its own extern "C" guard
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_internal_bus.h"

/* ============================================================================
 * Test Constants - Known Good Baselines
 * ============================================================================ */

/* Performance baselines (microseconds) */
#define REGRESSION_MAX_PUBLISH_TIME_US          200     /* 200us max per publish */
#define REGRESSION_AVG_PUBLISH_TIME_US          50      /* 50us average target */
#define REGRESSION_MAX_SUBSCRIBE_TIME_US        100     /* 100us max per subscribe */
#define REGRESSION_MAX_MODULATION_LOOKUP_US     20      /* 20us max for modulation lookup */

/* Load test parameters */
#define REGRESSION_PUBLISH_LOAD_COUNT           1000    /* Events for load test */
#define REGRESSION_LONG_RUN_CYCLES              10000   /* Long running test cycles */

/* Memory baselines */
#define REGRESSION_MEMORY_TEST_ITERATIONS       1000    /* Create/destroy cycles */
#define REGRESSION_CYCLES_PER_CHECK             100     /* Check state every N cycles */

/* Numerical accuracy */
#define REGRESSION_FLOAT_EPSILON                1e-5f   /* Float comparison tolerance */
#define REGRESSION_MODULATION_PRECISION         0.0001f /* Modulation factor precision */

/* Thread safety */
#define REGRESSION_NUM_THREADS                  4       /* Concurrent threads */
#define REGRESSION_THREAD_ITERATIONS            250     /* Iterations per thread */

/* ============================================================================
 * Callback Tracking
 * ============================================================================ */

static std::atomic<int> g_callback_count{0};
static std::atomic<int> g_circadian_event_count{0};
static std::atomic<int> g_stress_event_count{0};
static std::atomic<int> g_drive_event_count{0};
static std::atomic<int> g_autonomic_event_count{0};
static std::atomic<uint64_t> g_last_sequence_id{0};
static std::atomic<uint64_t> g_total_latency_us{0};
static std::mutex g_callback_mutex;
static std::vector<uint32_t> g_received_sequence_ids;

static int regression_event_callback(
    const hypo_internal_event_t* event,
    void* user_data
) {
    (void)user_data;
    if (event) {
        g_callback_count++;
        g_last_sequence_id.store(event->sequence_id);

        /* Calculate delivery latency */
        auto now_us = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (event->timestamp_us > 0 && now_us > event->timestamp_us) {
            g_total_latency_us += (now_us - event->timestamp_us);
        }

        /* Track by event type */
        if (event->type >= HYPO_IEVT_CIRCADIAN_PHASE_CHANGE &&
            event->type <= HYPO_IEVT_CORTISOL_AWAKENING) {
            g_circadian_event_count++;
        } else if (event->type >= HYPO_IEVT_STRESS_ONSET &&
                   event->type <= HYPO_IEVT_CORTISOL_NORMALIZED) {
            g_stress_event_count++;
        } else if (event->type >= HYPO_IEVT_DRIVE_THRESHOLD_CROSSED &&
                   event->type <= HYPO_IEVT_SAFETY_THREAT) {
            g_drive_event_count++;
        } else if (event->type >= HYPO_IEVT_SYMPATHETIC_ACTIVATION &&
                   event->type <= HYPO_IEVT_AUTONOMIC_BALANCE_SHIFT) {
            g_autonomic_event_count++;
        }

        /* Track sequence IDs for completeness check */
        {
            std::lock_guard<std::mutex> lock(g_callback_mutex);
            g_received_sequence_ids.push_back(event->sequence_id);
        }
    }
    return 0;
}

static int slow_callback(
    const hypo_internal_event_t* event,
    void* user_data
) {
    (void)event;
    (void)user_data;
    /* Simulate slow processing */
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    g_callback_count++;
    return 0;
}

static void reset_callback_counters() {
    g_callback_count = 0;
    g_circadian_event_count = 0;
    g_stress_event_count = 0;
    g_drive_event_count = 0;
    g_autonomic_event_count = 0;
    g_last_sequence_id = 0;
    g_total_latency_us = 0;
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    g_received_sequence_ids.clear();
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HypothalamusInternalBusRegressionTest : public ::testing::Test {
protected:
    hypo_ibus_t bus = nullptr;
    hypo_ibus_config_t config;

    void SetUp() override {
        reset_callback_counters();
        hypo_ibus_default_config(&config);
        config.enable_logging = false;
        bus = hypo_ibus_create(&config);
        ASSERT_NE(bus, nullptr);
    }

    void TearDown() override {
        if (bus) {
            hypo_ibus_destroy(bus);
            bus = nullptr;
        }
    }

    /**
     * Helper to measure publish time in microseconds
     */
    uint64_t measure_publish_time_us(const hypo_internal_event_t* event) {
        auto start = std::chrono::high_resolution_clock::now();
        hypo_ibus_publish(bus, event);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    /**
     * Helper to create a test event
     */
    void create_test_event(hypo_internal_event_t* event,
                          hypo_internal_event_type_t type,
                          hypo_internal_module_t source) {
        memset(event, 0, sizeof(*event));
        event->type = type;
        event->source = source;
        event->timestamp_us = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        event->sequence_id = 0;  /* Will be assigned by bus */

        /* Fill in type-specific data */
        switch (type) {
            case HYPO_IEVT_CIRCADIAN_PHASE_CHANGE:
                event->data.circadian.old_phase = 3;
                event->data.circadian.new_phase = 4;
                event->data.circadian.melatonin_level = 0.2f;
                event->data.circadian.cortisol_level = 0.5f;
                event->data.circadian.alertness = 0.8f;
                event->data.circadian.sleep_pressure = 0.3f;
                break;
            case HYPO_IEVT_STRESS_ONSET:
            case HYPO_IEVT_STRESS_PEAK:
            case HYPO_IEVT_STRESS_RECOVERY:
                event->data.stress.stress_level = 0.6f;
                event->data.stress.cortisol_level = 0.7f;
                event->data.stress.crh_level = 0.5f;
                event->data.stress.acth_level = 0.4f;
                event->data.stress.is_acute = true;
                event->data.stress.stressor_type = 1;
                break;
            case HYPO_IEVT_DRIVE_THRESHOLD_CROSSED:
            case HYPO_IEVT_DRIVE_SATISFIED:
                event->data.drive.drive_type = 0;
                event->data.drive.drive_level = 0.75f;
                event->data.drive.urgency = 0.6f;
                event->data.drive.deviation = 0.25f;
                event->data.drive.is_satisfied = false;
                event->data.drive.competing_drive = 0;
                break;
            case HYPO_IEVT_SYMPATHETIC_ACTIVATION:
            case HYPO_IEVT_PARASYMPATHETIC_ACTIVATION:
                event->data.autonomic.sympathetic_tone = 0.7f;
                event->data.autonomic.parasympathetic_tone = 0.3f;
                event->data.autonomic.balance = 0.4f;
                event->data.autonomic.heart_rate_mod = 1.2f;
                break;
            default:
                break;
        }
    }
};

/* ============================================================================
 * PERF-001: Publish latency under load (1000 events)
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, PERF001_PublishLatencyUnderLoad) {
    /* Subscribe to events */
    for (int i = 0; i < 5; i++) {
        int sub_id = hypo_ibus_subscribe(
            bus,
            (hypo_internal_module_t)i,
            HYPO_IEVT_STRESS_ONSET,
            regression_event_callback,
            nullptr
        );
        EXPECT_GE(sub_id, 0) << "Failed to subscribe module " << i;
    }

    /* Warm up */
    hypo_internal_event_t event;
    create_test_event(&event, HYPO_IEVT_STRESS_ONSET, HYPO_IMOD_HPA_AXIS);
    for (int i = 0; i < 10; i++) {
        hypo_ibus_publish(bus, &event);
    }
    reset_callback_counters();

    /* Collect timing samples under load */
    std::vector<uint64_t> publish_times;
    publish_times.reserve(REGRESSION_PUBLISH_LOAD_COUNT);

    for (int i = 0; i < REGRESSION_PUBLISH_LOAD_COUNT; i++) {
        event.data.stress.stress_level = 0.5f + 0.3f * sinf((float)i * 0.01f);
        publish_times.push_back(measure_publish_time_us(&event));
    }

    /* Calculate statistics */
    uint64_t max_time = *std::max_element(publish_times.begin(), publish_times.end());
    uint64_t total_time = std::accumulate(publish_times.begin(), publish_times.end(), 0ULL);
    double avg_time = (double)total_time / REGRESSION_PUBLISH_LOAD_COUNT;

    /* Sort for percentile calculation */
    std::sort(publish_times.begin(), publish_times.end());
    uint64_t p95_time = publish_times[(size_t)(REGRESSION_PUBLISH_LOAD_COUNT * 0.95)];
    uint64_t p99_time = publish_times[(size_t)(REGRESSION_PUBLISH_LOAD_COUNT * 0.99)];

    /* Verify performance bounds */
    EXPECT_LT(max_time, REGRESSION_MAX_PUBLISH_TIME_US * 2)
        << "Maximum publish time exceeded: " << max_time << "us";
    EXPECT_LT(avg_time, REGRESSION_AVG_PUBLISH_TIME_US)
        << "Average publish time exceeded: " << avg_time << "us";
    EXPECT_LT(p95_time, REGRESSION_MAX_PUBLISH_TIME_US)
        << "P95 publish time exceeded: " << p95_time << "us";

    /* All events should have been delivered */
    EXPECT_EQ(g_callback_count.load(), REGRESSION_PUBLISH_LOAD_COUNT * 5)
        << "Expected all callbacks to fire for " << REGRESSION_PUBLISH_LOAD_COUNT << " events to 5 subscribers";

    /* Print performance summary */
    printf("  Publish latency under load (n=%d):\n", REGRESSION_PUBLISH_LOAD_COUNT);
    printf("    Average: %.2f us\n", avg_time);
    printf("    P95: %lu us\n", (unsigned long)p95_time);
    printf("    P99: %lu us\n", (unsigned long)p99_time);
    printf("    Max: %lu us\n", (unsigned long)max_time);
}

/* ============================================================================
 * PERF-002: Subscribe/unsubscribe throughput
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, PERF002_SubscribeUnsubscribeThroughput) {
    const int NUM_CYCLES = 500;
    std::vector<uint64_t> subscribe_times;
    std::vector<uint64_t> unsubscribe_times;
    subscribe_times.reserve(NUM_CYCLES);
    unsubscribe_times.reserve(NUM_CYCLES);

    for (int i = 0; i < NUM_CYCLES; i++) {
        /* Measure subscribe time */
        auto start = std::chrono::high_resolution_clock::now();
        int sub_id = hypo_ibus_subscribe(
            bus,
            HYPO_IMOD_DRIVES,
            (hypo_internal_event_type_t)(i % HYPO_IEVT_COUNT),
            regression_event_callback,
            nullptr
        );
        auto end = std::chrono::high_resolution_clock::now();
        subscribe_times.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
        );

        ASSERT_GE(sub_id, 0) << "Failed to subscribe at iteration " << i;

        /* Measure unsubscribe time */
        start = std::chrono::high_resolution_clock::now();
        int ret = hypo_ibus_unsubscribe(bus, sub_id);
        end = std::chrono::high_resolution_clock::now();
        unsubscribe_times.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
        );

        ASSERT_EQ(ret, 0) << "Failed to unsubscribe at iteration " << i;
    }

    /* Calculate statistics */
    uint64_t sub_max = *std::max_element(subscribe_times.begin(), subscribe_times.end());
    uint64_t sub_total = std::accumulate(subscribe_times.begin(), subscribe_times.end(), 0ULL);
    double sub_avg = (double)sub_total / NUM_CYCLES;

    uint64_t unsub_max = *std::max_element(unsubscribe_times.begin(), unsubscribe_times.end());
    uint64_t unsub_total = std::accumulate(unsubscribe_times.begin(), unsubscribe_times.end(), 0ULL);
    double unsub_avg = (double)unsub_total / NUM_CYCLES;

    /* Verify performance bounds */
    EXPECT_LT(sub_max, REGRESSION_MAX_SUBSCRIBE_TIME_US)
        << "Subscribe max time exceeded: " << sub_max << "us";
    EXPECT_LT(unsub_max, REGRESSION_MAX_SUBSCRIBE_TIME_US)
        << "Unsubscribe max time exceeded: " << unsub_max << "us";

    printf("  Subscribe/Unsubscribe throughput (n=%d):\n", NUM_CYCLES);
    printf("    Subscribe avg: %.2f us, max: %lu us\n", sub_avg, (unsigned long)sub_max);
    printf("    Unsubscribe avg: %.2f us, max: %lu us\n", unsub_avg, (unsigned long)unsub_max);
}

/* ============================================================================
 * PERF-003: Modulation lookup performance
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, PERF003_ModulationLookupPerformance) {
    /* Register default modulations */
    int rules = hypo_ibus_register_default_modulations(bus);
    EXPECT_GT(rules, 0) << "Should register default modulation rules";

    /* Warm up */
    for (int i = 0; i < 10; i++) {
        hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    }

    /* Measure lookup times across all modules */
    const int NUM_LOOKUPS = 1000;
    std::vector<uint64_t> lookup_times;
    lookup_times.reserve(NUM_LOOKUPS);

    for (int i = 0; i < NUM_LOOKUPS; i++) {
        hypo_internal_module_t module = (hypo_internal_module_t)(i % HYPO_IMOD_COUNT);
        uint32_t param = (uint32_t)(i % 5);

        auto start = std::chrono::high_resolution_clock::now();
        float mod = hypo_ibus_get_modulation(bus, module, param);
        auto end = std::chrono::high_resolution_clock::now();

        lookup_times.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
        );

        /* Modulation should be in valid range */
        EXPECT_GE(mod, 0.0f) << "Modulation should be >= 0";
        EXPECT_LE(mod, 2.0f) << "Modulation should be <= 2";
    }

    uint64_t max_time = *std::max_element(lookup_times.begin(), lookup_times.end());
    uint64_t total_time = std::accumulate(lookup_times.begin(), lookup_times.end(), 0ULL);
    double avg_time = (double)total_time / NUM_LOOKUPS;

    EXPECT_LT(max_time, REGRESSION_MAX_MODULATION_LOOKUP_US)
        << "Modulation lookup max time exceeded: " << max_time << "us";

    printf("  Modulation lookup performance (n=%d):\n", NUM_LOOKUPS);
    printf("    Average: %.2f us, Max: %lu us\n", avg_time, (unsigned long)max_time);
}

/* ============================================================================
 * PERF-004: Concurrent access performance
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, PERF004_ConcurrentAccessPerformance) {
    std::atomic<int> completed_threads{0};
    std::atomic<int> errors{0};
    std::atomic<uint64_t> total_ops{0};
    std::vector<std::thread> threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    /* Create threads that concurrently access the bus */
    for (int t = 0; t < REGRESSION_NUM_THREADS; t++) {
        threads.emplace_back([this, &completed_threads, &errors, &total_ops, t]() {
            try {
                for (int i = 0; i < REGRESSION_THREAD_ITERATIONS; i++) {
                    /* Subscribe */
                    int sub_id = hypo_ibus_subscribe(
                        bus,
                        (hypo_internal_module_t)(t % HYPO_IMOD_COUNT),
                        (hypo_internal_event_type_t)(i % HYPO_IEVT_COUNT),
                        regression_event_callback,
                        nullptr
                    );

                    if (sub_id >= 0) {
                        /* Publish */
                        hypo_internal_event_t event;
                        memset(&event, 0, sizeof(event));
                        event.type = (hypo_internal_event_type_t)(i % HYPO_IEVT_COUNT);
                        event.source = (hypo_internal_module_t)(t % HYPO_IMOD_COUNT);
                        event.timestamp_us = (uint64_t)std::chrono::duration_cast<
                            std::chrono::microseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();

                        hypo_ibus_publish(bus, &event);

                        /* Query modulation */
                        float mod = hypo_ibus_get_modulation(
                            bus,
                            (hypo_internal_module_t)((t + 1) % HYPO_IMOD_COUNT),
                            0
                        );
                        if (mod < 0.0f || mod > 2.0f) {
                            errors++;
                        }

                        /* Unsubscribe */
                        hypo_ibus_unsubscribe(bus, sub_id);
                        total_ops += 4;  /* subscribe, publish, get_modulation, unsubscribe */
                    }
                }
                completed_threads++;
            } catch (...) {
                errors++;
            }
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    EXPECT_EQ(completed_threads.load(), REGRESSION_NUM_THREADS)
        << "All threads should complete";
    EXPECT_EQ(errors.load(), 0)
        << "No errors should occur during concurrent access";

    double ops_per_sec = (double)total_ops.load() * 1000.0 / elapsed_ms;
    printf("  Concurrent access performance (%d threads, %d iterations each):\n",
           REGRESSION_NUM_THREADS, REGRESSION_THREAD_ITERATIONS);
    printf("    Total ops: %lu, Time: %ld ms, Ops/sec: %.0f\n",
           (unsigned long)total_ops.load(), (long)elapsed_ms, ops_per_sec);
}

/* ============================================================================
 * STAB-001: Long-running operation (10000 publish cycles)
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, STAB001_LongRunningOperation) {
    /* Subscribe to various event types */
    for (int i = 0; i < 5; i++) {
        hypo_ibus_subscribe(
            bus,
            (hypo_internal_module_t)(i % HYPO_IMOD_COUNT),
            (hypo_internal_event_type_t)(i * 5),
            regression_event_callback,
            nullptr
        );
    }

    hypo_ibus_stats_t initial_stats;
    hypo_ibus_get_stats(bus, &initial_stats);

    /* Run for many cycles */
    for (int i = 0; i < REGRESSION_LONG_RUN_CYCLES; i++) {
        hypo_internal_event_t event;
        create_test_event(&event, (hypo_internal_event_type_t)(i % HYPO_IEVT_COUNT),
                         (hypo_internal_module_t)(i % HYPO_IMOD_COUNT));

        int delivered = hypo_ibus_publish(bus, &event);
        ASSERT_GE(delivered, 0) << "Publish failed at cycle " << i;

        /* Update modulations periodically */
        if (i % 100 == 0) {
            hypo_ibus_update_modulations(bus, 100000);  /* 100ms */
        }

        /* Periodic state verification */
        if (i % REGRESSION_CYCLES_PER_CHECK == 0) {
            hypo_ibus_stats_t stats;
            int ret = hypo_ibus_get_stats(bus, &stats);
            ASSERT_EQ(ret, 0) << "Get stats failed at cycle " << i;
            ASSERT_GE(stats.events_published, (uint64_t)i)
                << "Event count mismatch at cycle " << i;
        }
    }

    /* Final verification */
    hypo_ibus_stats_t final_stats;
    hypo_ibus_get_stats(bus, &final_stats);

    EXPECT_GE(final_stats.events_published,
              initial_stats.events_published + REGRESSION_LONG_RUN_CYCLES);
    EXPECT_EQ(final_stats.events_dropped, 0u)
        << "No events should be dropped in steady state";

    printf("  Long-running test completed:\n");
    printf("    Cycles: %d\n", REGRESSION_LONG_RUN_CYCLES);
    printf("    Events published: %lu\n", (unsigned long)final_stats.events_published);
    printf("    Events delivered: %lu\n", (unsigned long)final_stats.events_delivered);
    printf("    Events dropped: %lu\n", (unsigned long)final_stats.events_dropped);
}

/* ============================================================================
 * STAB-002: Memory stability (no leaks over repeated create/destroy)
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, STAB002_MemoryStabilityCreateDestroy) {
    /* Destroy the fixture's bus first */
    hypo_ibus_destroy(bus);
    bus = nullptr;

    for (int i = 0; i < REGRESSION_MEMORY_TEST_ITERATIONS; i++) {
        /* Create bus */
        hypo_ibus_config_t cfg;
        hypo_ibus_default_config(&cfg);
        hypo_ibus_t test_bus = hypo_ibus_create(&cfg);
        ASSERT_NE(test_bus, nullptr) << "Create failed at iteration " << i;

        /* Do some operations */
        int sub_id = hypo_ibus_subscribe(
            test_bus, HYPO_IMOD_DRIVES, HYPO_IEVT_DRIVE_THRESHOLD_CROSSED,
            regression_event_callback, nullptr
        );
        ASSERT_GE(sub_id, 0);

        hypo_internal_event_t event;
        memset(&event, 0, sizeof(event));
        event.type = HYPO_IEVT_DRIVE_THRESHOLD_CROSSED;
        event.source = HYPO_IMOD_DRIVES;
        hypo_ibus_publish(test_bus, &event);

        hypo_ibus_register_default_modulations(test_bus);

        /* Destroy bus */
        hypo_ibus_destroy(test_bus);

        /* Periodic verification that we haven't corrupted global state */
        if (i % REGRESSION_CYCLES_PER_CHECK == 0) {
            /* Create a fresh bus to verify we can still do so */
            hypo_ibus_t verify_bus = hypo_ibus_create(nullptr);
            ASSERT_NE(verify_bus, nullptr)
                << "Verification create failed at iteration " << i;
            hypo_ibus_destroy(verify_bus);
        }
    }

    /* Recreate fixture bus */
    bus = hypo_ibus_create(&config);
    ASSERT_NE(bus, nullptr);

    printf("  Memory stability test completed: %d create/destroy cycles\n",
           REGRESSION_MEMORY_TEST_ITERATIONS);
}

/* ============================================================================
 * STAB-003: Queue full handling
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, STAB003_QueueFullHandling) {
    /* Create bus with async enabled and small queue */
    hypo_ibus_destroy(bus);
    hypo_ibus_config_t small_queue_config;
    hypo_ibus_default_config(&small_queue_config);
    small_queue_config.enable_async = true;
    small_queue_config.max_queue_size = 8;  /* Small queue */
    bus = hypo_ibus_create(&small_queue_config);
    ASSERT_NE(bus, nullptr);

    /* Subscribe with slow callback */
    int sub_id = hypo_ibus_subscribe(
        bus, HYPO_IMOD_DRIVES, HYPO_IEVT_DRIVE_THRESHOLD_CROSSED,
        slow_callback, nullptr
    );
    EXPECT_GE(sub_id, 0);

    /* Publish many events quickly */
    hypo_internal_event_t event;
    create_test_event(&event, HYPO_IEVT_DRIVE_THRESHOLD_CROSSED, HYPO_IMOD_DRIVES);

    int successful_publishes = 0;
    for (int i = 0; i < 100; i++) {
        int ret = hypo_ibus_publish(bus, &event);
        if (ret >= 0) {
            successful_publishes++;
        }
    }

    /* Should have published at least some events */
    EXPECT_GT(successful_publishes, 0)
        << "Should successfully publish at least some events";

    /* Get stats to verify queue handling */
    hypo_ibus_stats_t stats;
    hypo_ibus_get_stats(bus, &stats);

    printf("  Queue full handling test:\n");
    printf("    Successful publishes: %d\n", successful_publishes);
    printf("    Events dropped: %lu\n", (unsigned long)stats.events_dropped);
    printf("    Peak queue depth: %u\n", stats.peak_queue_depth);
}

/* ============================================================================
 * STAB-004: Max subscribers reached
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, STAB004_MaxSubscribersReached) {
    int successful_subscriptions = 0;
    std::vector<int> sub_ids;

    /* Try to subscribe more than MAX_SUBSCRIBERS */
    for (int i = 0; i < HYPO_IBUS_MAX_SUBSCRIBERS + 10; i++) {
        int sub_id = hypo_ibus_subscribe(
            bus,
            (hypo_internal_module_t)(i % HYPO_IMOD_COUNT),
            (hypo_internal_event_type_t)(i % HYPO_IEVT_COUNT),
            regression_event_callback,
            nullptr
        );

        if (sub_id >= 0) {
            successful_subscriptions++;
            sub_ids.push_back(sub_id);
        }
    }

    /* Should have subscribed up to max */
    EXPECT_LE(successful_subscriptions, HYPO_IBUS_MAX_SUBSCRIBERS)
        << "Should not exceed max subscribers";
    EXPECT_GT(successful_subscriptions, 0)
        << "Should have at least some successful subscriptions";

    /* Verify bus is still functional */
    hypo_internal_event_t event;
    create_test_event(&event, HYPO_IEVT_STRESS_ONSET, HYPO_IMOD_HPA_AXIS);
    int delivered = hypo_ibus_publish(bus, &event);
    EXPECT_GE(delivered, 0) << "Bus should still be functional after max reached";

    /* Clean up */
    for (int sub_id : sub_ids) {
        hypo_ibus_unsubscribe(bus, sub_id);
    }

    printf("  Max subscribers test: %d/%d subscriptions succeeded\n",
           successful_subscriptions, HYPO_IBUS_MAX_SUBSCRIBERS + 10);
}

/* ============================================================================
 * ACC-001: Modulation factor precision (floating point)
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, ACC001_ModulationFactorPrecision) {
    /* Register a modulation rule with a precise value */
    hypo_ibus_modulation_t mod;
    mod.target = HYPO_IMOD_APPETITE;
    mod.modulation_factor = 0.12345f;
    mod.parameter_id = 1;
    mod.duration_us = 1000000;  /* 1 second */
    mod.is_additive = false;

    int rule_id = hypo_ibus_register_modulation(bus, HYPO_IEVT_STRESS_ONSET, &mod);
    EXPECT_GE(rule_id, 0);

    /* Trigger the modulation */
    hypo_internal_event_t event;
    create_test_event(&event, HYPO_IEVT_STRESS_ONSET, HYPO_IMOD_HPA_AXIS);
    hypo_ibus_publish(bus, &event);

    /* Query the modulation */
    float retrieved_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 1);

    /* Check precision */
    float diff = fabsf(retrieved_mod - mod.modulation_factor);
    EXPECT_LT(diff, REGRESSION_MODULATION_PRECISION)
        << "Modulation precision lost: expected " << mod.modulation_factor
        << ", got " << retrieved_mod << ", diff " << diff;

    /* Test modulation decay precision */
    hypo_ibus_update_modulations(bus, 500000);  /* Half the duration */

    /* Modulation should still be active but potentially decayed */
    float after_decay = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 1);
    EXPECT_GE(after_decay, 0.0f);
    EXPECT_LE(after_decay, 2.0f);

    printf("  Modulation precision test:\n");
    printf("    Set: %f, Retrieved: %f, Diff: %e\n",
           mod.modulation_factor, retrieved_mod, diff);
}

/* ============================================================================
 * ACC-002: Event delivery completeness (no drops)
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, ACC002_EventDeliveryCompleteness) {
    reset_callback_counters();

    /* Subscribe to all stress events */
    int sub_id = hypo_ibus_subscribe(
        bus, HYPO_IMOD_DRIVES, HYPO_IEVT_STRESS_ONSET,
        regression_event_callback, nullptr
    );
    EXPECT_GE(sub_id, 0);

    /* Publish events with sequence numbers */
    const int NUM_EVENTS = 500;
    for (int i = 0; i < NUM_EVENTS; i++) {
        hypo_internal_event_t event;
        create_test_event(&event, HYPO_IEVT_STRESS_ONSET, HYPO_IMOD_HPA_AXIS);
        event.sequence_id = (uint32_t)i;
        hypo_ibus_publish(bus, &event);
    }

    /* Verify all events were delivered */
    EXPECT_EQ(g_callback_count.load(), NUM_EVENTS)
        << "All events should be delivered";

    /* Verify no duplicates and no drops */
    {
        std::lock_guard<std::mutex> lock(g_callback_mutex);
        std::set<uint32_t> unique_ids(g_received_sequence_ids.begin(),
                                       g_received_sequence_ids.end());
        EXPECT_EQ(unique_ids.size(), (size_t)NUM_EVENTS)
            << "Should receive exactly " << NUM_EVENTS << " unique events";
    }

    /* Verify statistics */
    hypo_ibus_stats_t stats;
    hypo_ibus_get_stats(bus, &stats);
    EXPECT_EQ(stats.events_published, (uint64_t)NUM_EVENTS);
    EXPECT_EQ(stats.events_delivered, (uint64_t)NUM_EVENTS);
    EXPECT_EQ(stats.events_dropped, 0u);
}

/* ============================================================================
 * ACC-003: Statistics accuracy
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, ACC003_StatisticsAccuracy) {
    hypo_ibus_reset_stats(bus);

    /* Subscribe multiple modules to multiple event types */
    int num_circadian_subs = 0;
    int num_stress_subs = 0;

    for (int i = 0; i < 3; i++) {
        if (hypo_ibus_subscribe(bus, (hypo_internal_module_t)i,
            HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, regression_event_callback, nullptr) >= 0) {
            num_circadian_subs++;
        }
    }
    for (int i = 0; i < 2; i++) {
        if (hypo_ibus_subscribe(bus, (hypo_internal_module_t)(i + 3),
            HYPO_IEVT_STRESS_ONSET, regression_event_callback, nullptr) >= 0) {
            num_stress_subs++;
        }
    }

    /* Publish known quantities */
    const int CIRCADIAN_EVENTS = 50;
    const int STRESS_EVENTS = 30;

    for (int i = 0; i < CIRCADIAN_EVENTS; i++) {
        hypo_internal_event_t event;
        create_test_event(&event, HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, HYPO_IMOD_CIRCADIAN);
        hypo_ibus_publish(bus, &event);
    }

    for (int i = 0; i < STRESS_EVENTS; i++) {
        hypo_internal_event_t event;
        create_test_event(&event, HYPO_IEVT_STRESS_ONSET, HYPO_IMOD_HPA_AXIS);
        hypo_ibus_publish(bus, &event);
    }

    /* Verify statistics */
    hypo_ibus_stats_t stats;
    hypo_ibus_get_stats(bus, &stats);

    EXPECT_EQ(stats.events_published, (uint64_t)(CIRCADIAN_EVENTS + STRESS_EVENTS))
        << "Total events published should match";
    EXPECT_EQ(stats.events_delivered,
              (uint64_t)(CIRCADIAN_EVENTS * num_circadian_subs +
                        STRESS_EVENTS * num_stress_subs))
        << "Total events delivered should match subscribers * events";
    EXPECT_EQ(stats.active_subscribers, (uint32_t)(num_circadian_subs + num_stress_subs))
        << "Active subscriber count should match";

    /* Per-module statistics */
    EXPECT_EQ(stats.module_events[HYPO_IMOD_CIRCADIAN], (uint64_t)CIRCADIAN_EVENTS);
    EXPECT_EQ(stats.module_events[HYPO_IMOD_HPA_AXIS], (uint64_t)STRESS_EVENTS);

    printf("  Statistics accuracy test:\n");
    printf("    Events published: %lu (expected %d)\n",
           (unsigned long)stats.events_published, CIRCADIAN_EVENTS + STRESS_EVENTS);
    printf("    Events delivered: %lu (expected %d)\n",
           (unsigned long)stats.events_delivered,
           CIRCADIAN_EVENTS * num_circadian_subs + STRESS_EVENTS * num_stress_subs);
}

/* ============================================================================
 * ACC-004: Modulation decay timing
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, ACC004_ModulationDecayTiming) {
    /* Register a timed modulation */
    hypo_ibus_modulation_t mod;
    mod.target = HYPO_IMOD_APPETITE;
    mod.modulation_factor = 0.5f;
    mod.parameter_id = 0;
    mod.duration_us = 1000000;  /* 1 second */
    mod.is_additive = false;

    int rule_id = hypo_ibus_register_modulation(bus, HYPO_IEVT_CORTISOL_ELEVATED, &mod);
    EXPECT_GE(rule_id, 0);

    /* Trigger the modulation */
    hypo_internal_event_t event;
    create_test_event(&event, HYPO_IEVT_CORTISOL_ELEVATED, HYPO_IMOD_HPA_AXIS);
    hypo_ibus_publish(bus, &event);

    /* Verify modulation is active */
    float initial_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_NE(initial_mod, 1.0f) << "Modulation should be active initially";

    /* Advance time past the duration */
    hypo_ibus_update_modulations(bus, 500000);   /* 0.5 seconds */
    float mid_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);

    hypo_ibus_update_modulations(bus, 600000);   /* 0.6 more seconds = 1.1 total */
    float final_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);

    /* After duration, modulation should return to neutral (1.0) */
    EXPECT_FLOAT_EQ(final_mod, 1.0f)
        << "Modulation should decay to 1.0 after duration expires";

    printf("  Modulation decay timing:\n");
    printf("    Initial: %f, Mid (0.5s): %f, Final (1.1s): %f\n",
           initial_mod, mid_mod, final_mod);
}

/* ============================================================================
 * BOUND-001: Max subscriptions reached boundary
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, BOUND001_MaxSubscriptionsBoundary) {
    /* Fill up subscriptions to exactly the max */
    std::vector<int> sub_ids;
    int count = 0;

    while (count < HYPO_IBUS_MAX_SUBSCRIBERS) {
        int sub_id = hypo_ibus_subscribe(
            bus,
            (hypo_internal_module_t)(count % HYPO_IMOD_COUNT),
            HYPO_IEVT_STRESS_ONSET,
            regression_event_callback,
            nullptr
        );
        if (sub_id < 0) break;
        sub_ids.push_back(sub_id);
        count++;
    }

    EXPECT_EQ(count, HYPO_IBUS_MAX_SUBSCRIBERS)
        << "Should reach exactly max subscriptions";

    /* Next subscription should fail */
    int overflow_id = hypo_ibus_subscribe(
        bus, HYPO_IMOD_DRIVES, HYPO_IEVT_STRESS_ONSET,
        regression_event_callback, nullptr
    );
    EXPECT_LT(overflow_id, 0)
        << "Subscription should fail when max reached";

    /* Verify subscriber count */
    uint32_t sub_count = hypo_ibus_subscriber_count(bus, HYPO_IEVT_STRESS_ONSET);
    EXPECT_EQ(sub_count, (uint32_t)HYPO_IBUS_MAX_SUBSCRIBERS);

    /* Unsubscribe one and verify we can subscribe again */
    hypo_ibus_unsubscribe(bus, sub_ids[0]);
    int new_sub = hypo_ibus_subscribe(
        bus, HYPO_IMOD_DRIVES, HYPO_IEVT_STRESS_ONSET,
        regression_event_callback, nullptr
    );
    EXPECT_GE(new_sub, 0) << "Should be able to subscribe after unsubscribe";
}

/* ============================================================================
 * BOUND-002: Event with max data size
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, BOUND002_EventMaxDataSize) {
    int sub_id = hypo_ibus_subscribe(
        bus, HYPO_IMOD_ALIGNMENT, HYPO_IEVT_ALIGNMENT_VIOLATION,
        regression_event_callback, nullptr
    );
    EXPECT_GE(sub_id, 0);

    /* Create event with alignment data (includes description string) */
    hypo_internal_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = HYPO_IEVT_ALIGNMENT_VIOLATION;
    event.source = HYPO_IMOD_ALIGNMENT;
    event.timestamp_us = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    event.data.alignment.constraint_id = 42;
    event.data.alignment.margin = 0.01f;
    event.data.alignment.is_locked = true;

    /* Fill description to max */
    memset(event.data.alignment.description, 'X', sizeof(event.data.alignment.description) - 1);
    event.data.alignment.description[sizeof(event.data.alignment.description) - 1] = '\0';

    reset_callback_counters();
    int result = hypo_ibus_publish(bus, &event);
    EXPECT_EQ(result, 0) << "Event with max data should succeed";
    EXPECT_EQ(g_callback_count.load(), 1) << "Event should be delivered";

    /* Test with raw data union filled */
    hypo_internal_event_t raw_event;
    memset(&raw_event, 0, sizeof(raw_event));
    raw_event.type = HYPO_IEVT_STRESS_ONSET;  /* Use type that doesn't check raw */
    raw_event.source = HYPO_IMOD_HPA_AXIS;
    raw_event.timestamp_us = event.timestamp_us;
    memset(raw_event.data.raw, 0xFF, sizeof(raw_event.data.raw));

    int sub_id2 = hypo_ibus_subscribe(
        bus, HYPO_IMOD_DRIVES, HYPO_IEVT_STRESS_ONSET,
        regression_event_callback, nullptr
    );
    EXPECT_GE(sub_id2, 0);

    reset_callback_counters();
    result = hypo_ibus_publish(bus, &raw_event);
    EXPECT_EQ(result, 0) << "Event with full raw data should succeed";
}

/* ============================================================================
 * BOUND-003: All modules publishing simultaneously
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, BOUND003_AllModulesSimultaneous) {
    /* Subscribe to all event types from all modules */
    int total_subs = 0;
    for (int m = 0; m < HYPO_IMOD_COUNT && total_subs < HYPO_IBUS_MAX_SUBSCRIBERS - 1; m++) {
        int sub_id = hypo_ibus_subscribe_to_module(
            bus,
            (hypo_internal_module_t)((m + 1) % HYPO_IMOD_COUNT),
            (hypo_internal_module_t)m,
            regression_event_callback,
            nullptr
        );
        if (sub_id >= 0) total_subs++;
    }

    reset_callback_counters();
    hypo_ibus_reset_stats(bus);

    /* Publish from all modules simultaneously using threads */
    std::vector<std::thread> publishers;
    std::atomic<int> publish_errors{0};

    for (int m = 0; m < HYPO_IMOD_COUNT; m++) {
        publishers.emplace_back([this, m, &publish_errors]() {
            hypo_internal_event_t event;
            memset(&event, 0, sizeof(event));

            /* Each module publishes events appropriate to it */
            switch ((hypo_internal_module_t)m) {
                case HYPO_IMOD_CIRCADIAN:
                    event.type = HYPO_IEVT_CIRCADIAN_PHASE_CHANGE;
                    break;
                case HYPO_IMOD_HPA_AXIS:
                    event.type = HYPO_IEVT_STRESS_ONSET;
                    break;
                case HYPO_IMOD_DRIVES:
                    event.type = HYPO_IEVT_DRIVE_THRESHOLD_CROSSED;
                    break;
                case HYPO_IMOD_AUTONOMIC:
                    event.type = HYPO_IEVT_SYMPATHETIC_ACTIVATION;
                    break;
                default:
                    event.type = HYPO_IEVT_SETPOINT_DEVIATION;
                    break;
            }
            event.source = (hypo_internal_module_t)m;
            event.timestamp_us = (uint64_t)std::chrono::duration_cast<
                std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            int ret = hypo_ibus_publish(bus, &event);
            if (ret < 0) {
                publish_errors++;
            }
        });
    }

    for (auto& t : publishers) {
        t.join();
    }

    EXPECT_EQ(publish_errors.load(), 0)
        << "All modules should publish successfully";

    /* Verify events were received */
    hypo_ibus_stats_t stats;
    hypo_ibus_get_stats(bus, &stats);

    EXPECT_GE(stats.events_published, (uint64_t)HYPO_IMOD_COUNT)
        << "Should have published from all modules";

    printf("  All modules simultaneous publish:\n");
    printf("    Modules: %d, Subscribers: %d\n", HYPO_IMOD_COUNT, total_subs);
    printf("    Events published: %lu, delivered: %lu\n",
           (unsigned long)stats.events_published, (unsigned long)stats.events_delivered);
}

/* ============================================================================
 * BOUND-004: Convenience function boundary values
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, BOUND004_ConvenienceFunctionBoundaries) {
    int sub_id = hypo_ibus_subscribe(
        bus, HYPO_IMOD_DRIVES, HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
        regression_event_callback, nullptr
    );
    EXPECT_GE(sub_id, 0);

    /* Test boundary values for circadian phase */
    int ret = hypo_ibus_publish_circadian_phase(bus, 0, 7, 0.0f, 0.0f, 0.0f);
    EXPECT_EQ(ret, 0) << "Should accept minimum boundary values";

    ret = hypo_ibus_publish_circadian_phase(bus, 7, 0, 1.0f, 1.0f, 1.0f);
    EXPECT_EQ(ret, 0) << "Should accept maximum boundary values";

    /* Test stress boundary values */
    sub_id = hypo_ibus_subscribe(
        bus, HYPO_IMOD_APPETITE, HYPO_IEVT_STRESS_ONSET,
        regression_event_callback, nullptr
    );

    ret = hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET, 0.0f, 0.0f, false);
    EXPECT_EQ(ret, 0);

    ret = hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_PEAK, 1.0f, 1.0f, true);
    EXPECT_EQ(ret, 0);

    /* Test drive boundary values */
    sub_id = hypo_ibus_subscribe(
        bus, HYPO_IMOD_HPA_AXIS, HYPO_IEVT_DRIVE_THRESHOLD_CROSSED,
        regression_event_callback, nullptr
    );

    ret = hypo_ibus_publish_drive(bus, HYPO_IEVT_DRIVE_THRESHOLD_CROSSED, 0, 0.0f, 0.0f);
    EXPECT_EQ(ret, 0);

    ret = hypo_ibus_publish_drive(bus, HYPO_IEVT_DRIVE_SATISFIED, 8, 1.0f, 1.0f);
    EXPECT_EQ(ret, 0);

    /* Test autonomic boundary values */
    sub_id = hypo_ibus_subscribe(
        bus, HYPO_IMOD_HOMEOSTASIS, HYPO_IEVT_SYMPATHETIC_ACTIVATION,
        regression_event_callback, nullptr
    );

    ret = hypo_ibus_publish_autonomic(bus, HYPO_IEVT_SYMPATHETIC_ACTIVATION, 0.0f, 0.0f);
    EXPECT_EQ(ret, 0);

    ret = hypo_ibus_publish_autonomic(bus, HYPO_IEVT_PARASYMPATHETIC_ACTIVATION, 1.0f, 1.0f);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * NULL Safety Tests
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, NullSafety) {
    /* hypo_ibus_destroy should be NULL-safe */
    hypo_ibus_destroy(nullptr);  /* Should not crash */

    /* Operations with NULL bus should fail gracefully */
    EXPECT_LT(hypo_ibus_subscribe(nullptr, HYPO_IMOD_DRIVES,
        HYPO_IEVT_STRESS_ONSET, regression_event_callback, nullptr), 0);

    EXPECT_EQ(hypo_ibus_unsubscribe(nullptr, 0), -1);

    hypo_internal_event_t event;
    create_test_event(&event, HYPO_IEVT_STRESS_ONSET, HYPO_IMOD_HPA_AXIS);
    EXPECT_LT(hypo_ibus_publish(nullptr, &event), 0);
    EXPECT_LT(hypo_ibus_publish(bus, nullptr), 0);

    hypo_ibus_stats_t stats;
    EXPECT_EQ(hypo_ibus_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(hypo_ibus_get_stats(bus, nullptr), -1);

    EXPECT_EQ(hypo_ibus_reset(nullptr), -1);

    /* Modulation with NULL */
    EXPECT_LT(hypo_ibus_register_modulation(nullptr, HYPO_IEVT_STRESS_ONSET, nullptr), 0);
    EXPECT_LT(hypo_ibus_register_modulation(bus, HYPO_IEVT_STRESS_ONSET, nullptr), 0);

    EXPECT_EQ(hypo_ibus_get_modulation(nullptr, HYPO_IMOD_APPETITE, 0), 1.0f);

    EXPECT_EQ(hypo_ibus_update_modulations(nullptr, 1000), -1);
    EXPECT_EQ(hypo_ibus_clear_modulations(nullptr), -1);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, UtilityFunctionsNotNull) {
    /* Module names should never be NULL */
    for (int i = 0; i < HYPO_IMOD_COUNT; i++) {
        const char* name = hypo_ibus_module_name((hypo_internal_module_t)i);
        EXPECT_NE(name, nullptr) << "Module " << i << " has NULL name";
        EXPECT_GT(strlen(name), 0u) << "Module " << i << " has empty name";
    }

    /* Event names should never be NULL */
    for (int i = 0; i < HYPO_IEVT_COUNT; i++) {
        const char* name = hypo_ibus_event_name((hypo_internal_event_type_t)i);
        EXPECT_NE(name, nullptr) << "Event " << i << " has NULL name";
        EXPECT_GT(strlen(name), 0u) << "Event " << i << " has empty name";
    }

    /* Out of range values should return a valid string (e.g., "Unknown") */
    const char* unknown_mod = hypo_ibus_module_name((hypo_internal_module_t)999);
    EXPECT_NE(unknown_mod, nullptr);

    const char* unknown_evt = hypo_ibus_event_name((hypo_internal_event_type_t)999);
    EXPECT_NE(unknown_evt, nullptr);
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, DefaultConfigValid) {
    hypo_ibus_config_t default_config;
    int ret = hypo_ibus_default_config(&default_config);
    EXPECT_EQ(ret, 0);

    /* Verify limits are reasonable */
    EXPECT_GT(default_config.max_subscribers, 0u);
    EXPECT_LE(default_config.max_subscribers, 1024u);

    EXPECT_GT(default_config.max_queue_size, 0u);
    EXPECT_LE(default_config.max_queue_size, 4096u);

    /* Verify biological parameters are in valid ranges [0, 1] */
    EXPECT_GE(default_config.circadian_hunger_amplitude, 0.0f);
    EXPECT_LE(default_config.circadian_hunger_amplitude, 1.0f);

    EXPECT_GE(default_config.circadian_fatigue_amplitude, 0.0f);
    EXPECT_LE(default_config.circadian_fatigue_amplitude, 1.0f);

    EXPECT_GE(default_config.cortisol_appetite_suppression, 0.0f);
    EXPECT_LE(default_config.cortisol_appetite_suppression, 1.0f);

    EXPECT_GE(default_config.fatigue_curiosity_reduction, 0.0f);
    EXPECT_LE(default_config.fatigue_curiosity_reduction, 1.0f);

    EXPECT_GE(default_config.social_safety_modulation, 0.0f);
    EXPECT_LE(default_config.social_safety_modulation, 1.0f);

    EXPECT_GE(default_config.hunger_stress_threshold, 0.0f);
    EXPECT_LE(default_config.hunger_stress_threshold, 1.0f);

    /* NULL config should fail */
    EXPECT_EQ(hypo_ibus_default_config(nullptr), -1);
}

/* ============================================================================
 * Reset and Clear Tests
 * ============================================================================ */

TEST_F(HypothalamusInternalBusRegressionTest, ResetClearsState) {
    /* Set up some state */
    hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES, HYPO_IEVT_STRESS_ONSET,
                       regression_event_callback, nullptr);

    hypo_internal_event_t event;
    create_test_event(&event, HYPO_IEVT_STRESS_ONSET, HYPO_IMOD_HPA_AXIS);
    for (int i = 0; i < 10; i++) {
        hypo_ibus_publish(bus, &event);
    }

    hypo_ibus_register_default_modulations(bus);

    /* Reset */
    int ret = hypo_ibus_reset(bus);
    EXPECT_EQ(ret, 0);

    /* Verify state is cleared */
    hypo_ibus_stats_t stats;
    hypo_ibus_get_stats(bus, &stats);
    EXPECT_EQ(stats.events_published, 0u);
    EXPECT_EQ(stats.events_delivered, 0u);
    EXPECT_EQ(stats.queue_depth, 0u);

    /* Subscriptions should still be active (reset keeps subscriptions) */
    EXPECT_GT(stats.active_subscribers, 0u);
}

TEST_F(HypothalamusInternalBusRegressionTest, ClearModulations) {
    /* Register modulations */
    hypo_ibus_register_default_modulations(bus);

    /* Trigger a modulation */
    hypo_internal_event_t event;
    create_test_event(&event, HYPO_IEVT_STRESS_ONSET, HYPO_IMOD_HPA_AXIS);
    hypo_ibus_publish(bus, &event);

    /* Clear modulations */
    int ret = hypo_ibus_clear_modulations(bus);
    EXPECT_EQ(ret, 0);

    /* All modulations should be neutral (1.0) */
    for (int m = 0; m < HYPO_IMOD_COUNT; m++) {
        float mod = hypo_ibus_get_modulation(bus, (hypo_internal_module_t)m, 0);
        EXPECT_FLOAT_EQ(mod, 1.0f) << "Module " << m << " should have neutral modulation";
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
