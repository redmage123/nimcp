/**
 * @file test_hypothalamus_orchestrator_regression.cpp
 * @brief Regression tests for Hypothalamus Orchestrator
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Regression tests ensuring orchestrator behavior remains stable
 *       across code changes and performance doesn't degrade
 * WHY:  Prevent regressions in orchestrator coordination, event delivery,
 *       and drive state management
 * HOW:  Test known good behaviors, performance bounds, memory stability,
 *       and thread safety
 *
 * TEST COVERAGE:
 * - Performance bounds (event delivery latency, drive assessment time)
 * - Value stability (drive levels consistent across multiple assessments)
 * - Numerical accuracy (statistics don't overflow, averages computed correctly)
 * - Memory stability (no leaks in repeated registration/unregistration cycles)
 * - Thread safety (concurrent bridge registration and event publishing)
 * - State machine regression (state transitions work correctly)
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

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"

/* ============================================================================
 * Test Constants - Known Good Baselines
 * ============================================================================ */

/* Performance baselines (microseconds) */
#define REGRESSION_MAX_PUBLISH_TIME_US        500     /* 500us max per publish */
#define REGRESSION_AVG_PUBLISH_TIME_US        200     /* 200us average target */
#define REGRESSION_MAX_DRIVE_STATE_TIME_US    100     /* 100us max for state query */

/* Memory baselines */
#define REGRESSION_MAX_ITERATIONS             500     /* Iterations for memory test */
#define REGRESSION_CYCLES_PER_CHECK           50      /* Check state every N cycles */

/* Numerical accuracy */
#define REGRESSION_FLOAT_EPSILON              1e-5f   /* Float comparison tolerance */
#define REGRESSION_DRIVE_LEVEL_TOLERANCE      0.001f  /* Drive level stability tolerance */

/* Thread safety */
#define REGRESSION_NUM_THREADS                4       /* Concurrent threads */
#define REGRESSION_THREAD_ITERATIONS          200     /* Iterations per thread */

/* ============================================================================
 * Callback Tracking
 * ============================================================================ */

static std::atomic<int> g_callback_count{0};
static std::atomic<int> g_drive_activated_count{0};
static std::atomic<int> g_stress_response_count{0};
static std::atomic<float> g_last_drive_level{0.0f};
static std::atomic<uint64_t> g_last_event_time{0};

static int regression_event_callback(
    const hypo_event_data_t* event,
    void* user_data
) {
    (void)user_data;
    if (event) {
        g_callback_count++;
        g_last_event_time.store(event->timestamp);

        if (event->event_type == HYPO_EVENT_DRIVE_ACTIVATED) {
            g_drive_activated_count++;
            g_last_drive_level.store(event->drive.drive_level);
        }
        if (event->event_type == HYPO_EVENT_STRESS_RESPONSE) {
            g_stress_response_count++;
        }
    }
    return 0;
}

static void reset_callback_counters() {
    g_callback_count = 0;
    g_drive_activated_count = 0;
    g_stress_response_count = 0;
    g_last_drive_level = 0.0f;
    g_last_event_time = 0;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HypothalamusOrchestratorRegressionTest : public ::testing::Test {
protected:
    hypo_orchestrator_t orchestrator = nullptr;
    hypo_orch_config_t config;

    void SetUp() override {
        reset_callback_counters();
        hypo_orch_default_config(&config);
        config.enable_logging = false;
        orchestrator = hypo_orch_create(&config);
        ASSERT_NE(orchestrator, nullptr);
    }

    void TearDown() override {
        if (orchestrator) {
            hypo_orch_destroy(orchestrator);
            orchestrator = nullptr;
        }
    }

    /**
     * Helper to measure publish time in microseconds
     */
    uint64_t measure_publish_time_us(const hypo_event_data_t* event, uint32_t bridge_id) {
        auto start = std::chrono::high_resolution_clock::now();
        hypo_orch_publish(orchestrator, bridge_id, event);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    /**
     * Helper to verify drive level is in valid range
     */
    bool verify_drive_level_bounds(float level) {
        return level >= 0.0f && level <= 1.0f;
    }

    /**
     * Helper to create a test event
     */
    void create_test_event(hypo_event_data_t* event, hypo_event_type_t type,
                          hypo_bridge_type_t source, float drive_level) {
        memset(event, 0, sizeof(*event));
        event->event_type = type;
        event->source = source;
        event->urgency = HYPO_URGENCY_MODERATE;
        event->timestamp = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        event->event_id = 0;

        if (type == HYPO_EVENT_DRIVE_ACTIVATED) {
            event->drive.drive_type = 0;
            event->drive.drive_level = drive_level;
            event->drive.deviation = 0.0f;
            event->drive.urgency_weight = 1.0f;
            strncpy(event->drive.description, "Test drive event",
                    sizeof(event->drive.description) - 1);
        }
    }
};

/* ============================================================================
 * REG-001: Bridge count remains consistent after multiple add/remove cycles
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG001_BridgeCountConsistency) {
    /* NOTE: Only one bridge per type is allowed. Use different bridge types.
     * HYPO_BRIDGE_COUNT includes UNKNOWN(0), so valid types are 1..HYPO_BRIDGE_COUNT-1 */
    const int NUM_BRIDGES = 10;  /* Use 10 different types */
    uint32_t ids[32];
    int handles[32];

    for (int cycle = 0; cycle < 5; cycle++) {
        /* Register bridges with different types */
        for (int i = 0; i < NUM_BRIDGES; i++) {
            handles[i] = i;
            hypo_bridge_type_t btype = (hypo_bridge_type_t)(i + 1); /* Skip UNKNOWN=0 */
            int ret = hypo_orch_register_bridge(
                orchestrator, btype, "test_bridge",
                &handles[i], nullptr, &ids[i]
            );
            EXPECT_EQ(ret, 0) << "Failed to register bridge " << i << " in cycle " << cycle;
        }

        hypo_orch_stats_t stats;
        hypo_orch_get_stats(orchestrator, &stats);
        EXPECT_EQ(stats.registered_bridges, (uint32_t)NUM_BRIDGES)
            << "Expected " << NUM_BRIDGES << " registered bridges in cycle " << cycle;

        /* Unregister all bridges */
        for (int i = 0; i < NUM_BRIDGES; i++) {
            hypo_orch_unregister_bridge(orchestrator, ids[i]);
        }

        hypo_orch_get_stats(orchestrator, &stats);
        EXPECT_EQ(stats.registered_bridges, 0u)
            << "Expected 0 registered bridges after unregister in cycle " << cycle;
    }
}

/* ============================================================================
 * REG-002: Statistics don't overflow with many events
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG002_StatisticsNoOverflow) {
    int handle = 42;
    uint32_t bridge_id;
    hypo_orch_register_bridge(orchestrator, HYPO_BRIDGE_EMOTION, "test_bridge",
                              &handle, nullptr, &bridge_id);

    hypo_event_data_t event;
    create_test_event(&event, HYPO_EVENT_DRIVE_ACTIVATED, HYPO_BRIDGE_EMOTION, 0.5f);

    for (int i = 0; i < 5000; i++) {
        hypo_orch_publish(orchestrator, bridge_id, &event);
    }

    hypo_orch_stats_t stats;
    hypo_orch_get_stats(orchestrator, &stats);

    EXPECT_EQ(stats.events_published, 5000u);
    EXPECT_FALSE(std::isnan((float)stats.avg_event_latency_us));
    EXPECT_FALSE(std::isinf((float)stats.avg_event_latency_us));
}

/* ============================================================================
 * REG-003: Drive state remains stable without input changes
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG003_DriveStateStability) {
    int handle = 42;
    uint32_t bridge_id;
    hypo_orch_register_bridge(orchestrator, HYPO_BRIDGE_EMOTION, "test_bridge",
                              &handle, nullptr, &bridge_id);

    /* Report a drive to establish baseline */
    hypo_orch_report_drive(orchestrator, bridge_id, 0, 0.5f,
                          HYPO_URGENCY_MODERATE, "Test drive");

    hypo_unified_drive_state_t initial_state;
    hypo_orch_get_drive_state(orchestrator, &initial_state);
    float reference_level = initial_state.unified_drive_level;

    /* Query state multiple times without changes */
    const int NUM_QUERIES = 50;
    float min_level = reference_level;
    float max_level = reference_level;

    for (int i = 0; i < NUM_QUERIES; i++) {
        hypo_unified_drive_state_t state;
        hypo_orch_get_drive_state(orchestrator, &state);
        min_level = std::min(min_level, state.unified_drive_level);
        max_level = std::max(max_level, state.unified_drive_level);
    }

    /* Drive level should remain stable */
    float variance = max_level - min_level;
    EXPECT_LT(variance, REGRESSION_DRIVE_LEVEL_TOLERANCE)
        << "Drive level should remain stable without input changes";
}

/* ============================================================================
 * REG-004: Subscription callback reliability
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG004_SubscriptionCallbackReliability) {
    int handle = 42;
    uint32_t bridge_id;
    hypo_orch_register_bridge(orchestrator, HYPO_BRIDGE_EMOTION, "publisher",
                              &handle, nullptr, &bridge_id);

    int subscriber_handle = 43;
    uint32_t subscriber_id;
    hypo_orch_register_bridge(orchestrator, HYPO_BRIDGE_ATTENTION, "subscriber",
                              &subscriber_handle, nullptr, &subscriber_id);

    /* Subscribe to drive events */
    hypo_orch_subscribe(orchestrator, subscriber_id, HYPO_EVENT_DRIVE_ACTIVATED,
                       regression_event_callback, nullptr);

    /* Publish events and count callbacks */
    const int NUM_EVENTS = 100;
    for (int i = 0; i < NUM_EVENTS; i++) {
        hypo_event_data_t event;
        create_test_event(&event, HYPO_EVENT_DRIVE_ACTIVATED,
                         HYPO_BRIDGE_EMOTION, 0.5f + (float)i / 200.0f);
        hypo_orch_publish(orchestrator, bridge_id, &event);
    }

    /* All callbacks should have fired */
    EXPECT_EQ(g_callback_count.load(), NUM_EVENTS)
        << "Expected all callbacks to fire";
    EXPECT_EQ(g_drive_activated_count.load(), NUM_EVENTS)
        << "Expected all drive activation events";
}

/* ============================================================================
 * REG-005: Performance - Event publish time bounds
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG005_PublishPerformanceBounds) {
    int handle = 42;
    uint32_t bridge_id;
    hypo_orch_register_bridge(orchestrator, HYPO_BRIDGE_EMOTION, "test_bridge",
                              &handle, nullptr, &bridge_id);

    /* Add a few subscribers to make it realistic */
    for (int i = 0; i < 5; i++) {
        int sub_handle = 100 + i;
        uint32_t sub_id;
        hypo_orch_register_bridge(orchestrator, (hypo_bridge_type_t)(i + 1), "subscriber",
                                  &sub_handle, nullptr, &sub_id);
        hypo_orch_subscribe(orchestrator, sub_id, HYPO_EVENT_DRIVE_ACTIVATED,
                           regression_event_callback, nullptr);
    }

    /* Warm up */
    hypo_event_data_t event;
    create_test_event(&event, HYPO_EVENT_DRIVE_ACTIVATED, HYPO_BRIDGE_EMOTION, 0.5f);

    for (int i = 0; i < 10; i++) {
        hypo_orch_publish(orchestrator, bridge_id, &event);
    }

    /* Collect timing samples */
    const int NUM_SAMPLES = 100;
    std::vector<uint64_t> publish_times;
    publish_times.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        event.drive.drive_level = 0.5f + 0.3f * sinf((float)i * 0.1f);
        publish_times.push_back(measure_publish_time_us(&event, bridge_id));
    }

    /* Calculate statistics */
    uint64_t max_time = *std::max_element(publish_times.begin(), publish_times.end());
    uint64_t total_time = std::accumulate(publish_times.begin(), publish_times.end(), 0ULL);
    double avg_time = (double)total_time / NUM_SAMPLES;

    /* Verify performance bounds */
    EXPECT_LT(max_time, REGRESSION_MAX_PUBLISH_TIME_US)
        << "Maximum publish time exceeded: " << max_time << "us";
    EXPECT_LT(avg_time, REGRESSION_AVG_PUBLISH_TIME_US)
        << "Average publish time exceeded: " << avg_time << "us";
}

/* ============================================================================
 * REG-006: Memory stability over many registration cycles
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG006_MemoryStabilityRegisterCycles) {
    const int TOTAL_ITERATIONS = REGRESSION_MAX_ITERATIONS;

    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        int handle = i;
        uint32_t bridge_id;

        int ret = hypo_orch_register_bridge(
            orchestrator, HYPO_BRIDGE_EMOTION, "test_bridge",
            &handle, nullptr, &bridge_id
        );
        ASSERT_EQ(ret, 0) << "Registration failed at iteration " << i;

        /* Do some operations */
        hypo_orch_report_drive(orchestrator, bridge_id, 0, 0.5f,
                              HYPO_URGENCY_LOW, "Test");

        hypo_orch_unregister_bridge(orchestrator, bridge_id);

        /* Periodically verify state */
        if (i % REGRESSION_CYCLES_PER_CHECK == 0) {
            hypo_orch_stats_t stats;
            hypo_orch_get_stats(orchestrator, &stats);
            ASSERT_EQ(stats.registered_bridges, 0u)
                << "Bridge count should be 0 after " << i << " iterations";

            hypo_orch_state_t state;
            hypo_orch_get_state(orchestrator, &state);
            ASSERT_NE(state, HYPO_ORCH_STATE_ERROR)
                << "Orchestrator entered error state after " << i << " iterations";
        }
    }

    /* Final verification */
    hypo_orch_stats_t final_stats;
    hypo_orch_get_stats(orchestrator, &final_stats);
    EXPECT_EQ(final_stats.registered_bridges, 0u);
}

/* ============================================================================
 * REG-007: Bridge IDs are unique across registration cycles
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG007_UniqueIDs) {
    std::set<uint32_t> seen_ids;
    /* Only one bridge per type allowed, so register/unregister one type per cycle */
    const int NUM_PER_CYCLE = 10;  /* Use 10 different types per cycle */

    for (int cycle = 0; cycle < 10; cycle++) {
        int handles[10];
        uint32_t ids[10];

        for (int i = 0; i < NUM_PER_CYCLE; i++) {
            handles[i] = i + cycle * 100;
            hypo_bridge_type_t btype = (hypo_bridge_type_t)(i + 1); /* Skip UNKNOWN=0 */
            int ret = hypo_orch_register_bridge(
                orchestrator, btype, "test_bridge",
                &handles[i], nullptr, &ids[i]
            );
            if (ret != 0) continue;  /* Skip if type already registered */

            EXPECT_EQ(seen_ids.count(ids[i]), 0u) << "Duplicate ID: " << ids[i];
            seen_ids.insert(ids[i]);
        }

        for (int i = 0; i < NUM_PER_CYCLE; i++) {
            hypo_orch_unregister_bridge(orchestrator, ids[i]);
        }
    }
}

/* ============================================================================
 * REG-008: State machine transitions work correctly
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG008_StateTransitions) {
    /* Initial state should be IDLE or MONITORING */
    hypo_orch_state_t initial_state;
    hypo_orch_get_state(orchestrator, &initial_state);
    EXPECT_TRUE(initial_state == HYPO_ORCH_STATE_IDLE ||
                initial_state == HYPO_ORCH_STATE_MONITORING)
        << "Initial state should be IDLE or MONITORING";

    /* Trigger stress and verify state */
    hypo_orch_trigger_stress(orchestrator, "Test stress trigger");
    hypo_orch_state_t stress_state;
    hypo_orch_get_state(orchestrator, &stress_state);
    EXPECT_EQ(stress_state, HYPO_ORCH_STATE_STRESS)
        << "State should be STRESS after trigger";

    bool in_stress = false;
    hypo_orch_is_stressed(orchestrator, &in_stress);
    EXPECT_TRUE(in_stress);

    /* Release stress and verify state */
    hypo_orch_release_stress(orchestrator);
    hypo_orch_state_t after_release;
    hypo_orch_get_state(orchestrator, &after_release);
    EXPECT_TRUE(after_release == HYPO_ORCH_STATE_RECOVERY ||
                after_release == HYPO_ORCH_STATE_IDLE)
        << "State should be RECOVERY or IDLE after release";

    /* Reset and verify state */
    hypo_orch_reset(orchestrator);
    hypo_orch_state_t reset_state;
    hypo_orch_get_state(orchestrator, &reset_state);
    EXPECT_TRUE(reset_state == HYPO_ORCH_STATE_IDLE ||
                reset_state == HYPO_ORCH_STATE_UNINITIALIZED)
        << "State should be IDLE after reset";
}

/* ============================================================================
 * REG-009: Thread safety - concurrent operations
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG009_ThreadSafety) {
    std::atomic<int> completed_threads{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    /* Create threads that concurrently access the orchestrator */
    for (int t = 0; t < REGRESSION_NUM_THREADS; t++) {
        threads.emplace_back([this, &completed_threads, &errors, t]() {
            try {
                for (int i = 0; i < REGRESSION_THREAD_ITERATIONS; i++) {
                    int handle = t * 1000 + i;
                    uint32_t bridge_id;

                    /* Register */
                    int ret = hypo_orch_register_bridge(
                        orchestrator, (hypo_bridge_type_t)(t % HYPO_BRIDGE_COUNT),
                        "thread_bridge", &handle, nullptr, &bridge_id
                    );

                    if (ret == 0) {
                        /* Report drive */
                        float level = (float)(i % 100) / 100.0f;
                        hypo_orch_report_drive(orchestrator, bridge_id, 0, level,
                                              HYPO_URGENCY_LOW, "Thread test");

                        /* Query state */
                        hypo_unified_drive_state_t state;
                        hypo_orch_get_drive_state(orchestrator, &state);

                        if (!verify_drive_level_bounds(state.unified_drive_level)) {
                            errors++;
                        }

                        /* Unregister */
                        hypo_orch_unregister_bridge(orchestrator, bridge_id);
                    }
                }
                completed_threads++;
            } catch (...) {
                errors++;
            }
        });
    }

    /* Wait for all threads to complete */
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed_threads.load(), REGRESSION_NUM_THREADS)
        << "All threads should complete";
    EXPECT_EQ(errors.load(), 0)
        << "No errors should occur during concurrent access";

    /* Verify orchestrator is still functional */
    hypo_orch_state_t final_state;
    hypo_orch_get_state(orchestrator, &final_state);
    EXPECT_NE(final_state, HYPO_ORCH_STATE_ERROR)
        << "Orchestrator should not be in error state after concurrent access";
}

/* ============================================================================
 * REG-010: Drive level query performance
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG010_DriveStateQueryPerformance) {
    int handle = 42;
    uint32_t bridge_id;
    hypo_orch_register_bridge(orchestrator, HYPO_BRIDGE_EMOTION, "test_bridge",
                              &handle, nullptr, &bridge_id);

    hypo_orch_report_drive(orchestrator, bridge_id, 0, 0.5f,
                          HYPO_URGENCY_MODERATE, "Test drive");

    /* Warm up */
    for (int i = 0; i < 10; i++) {
        float level;
        hypo_orch_get_drive_level(orchestrator, &level);
    }

    /* Measure query times */
    const int NUM_SAMPLES = 100;
    std::vector<uint64_t> query_times;
    query_times.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        float level;
        hypo_orch_get_drive_level(orchestrator, &level);
        auto end = std::chrono::high_resolution_clock::now();
        query_times.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
        );
    }

    uint64_t max_time = *std::max_element(query_times.begin(), query_times.end());

    EXPECT_LT(max_time, REGRESSION_MAX_DRIVE_STATE_TIME_US)
        << "Drive state query too slow: " << max_time << "us";
}

/* ============================================================================
 * REG-011: Utility functions return valid strings
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG011_UtilityStringsNotNull) {
    /* Bridge type names */
    for (int i = 0; i < HYPO_BRIDGE_COUNT; i++) {
        const char* name = hypo_bridge_type_name((hypo_bridge_type_t)i);
        EXPECT_NE(name, nullptr) << "Bridge type " << i << " has null name";
    }

    /* Event type names */
    for (int i = 0; i < HYPO_EVENT_COUNT; i++) {
        const char* name = hypo_event_type_name((hypo_event_type_t)i);
        EXPECT_NE(name, nullptr) << "Event type " << i << " has null name";
    }

    /* Urgency names */
    const char* none = hypo_urgency_name(HYPO_URGENCY_NONE);
    const char* low = hypo_urgency_name(HYPO_URGENCY_LOW);
    const char* moderate = hypo_urgency_name(HYPO_URGENCY_MODERATE);
    const char* elevated = hypo_urgency_name(HYPO_URGENCY_ELEVATED);
    const char* urgent = hypo_urgency_name(HYPO_URGENCY_URGENT);

    EXPECT_NE(none, nullptr);
    EXPECT_NE(low, nullptr);
    EXPECT_NE(moderate, nullptr);
    EXPECT_NE(elevated, nullptr);
    EXPECT_NE(urgent, nullptr);

    /* State names */
    for (int i = 0; i <= HYPO_ORCH_STATE_ERROR; i++) {
        const char* name = hypo_orch_state_name((hypo_orch_state_t)i);
        EXPECT_NE(name, nullptr) << "State " << i << " has null name";
    }
}

/* ============================================================================
 * REG-012: NULL safety - functions handle NULL gracefully
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG012_NullSafety) {
    /* hypo_orch_destroy should be safe with NULL */
    hypo_orch_destroy(nullptr);  /* Should not crash */

    /* Registration with NULL orchestrator should fail */
    int handle = 42;
    uint32_t bridge_id;
    EXPECT_NE(0, hypo_orch_register_bridge(nullptr, HYPO_BRIDGE_EMOTION, "test",
                                           &handle, nullptr, &bridge_id));

    /* Get state with NULL should fail */
    hypo_orch_state_t state;
    EXPECT_NE(0, hypo_orch_get_state(nullptr, &state));
    EXPECT_NE(0, hypo_orch_get_state(orchestrator, nullptr));

    /* Get stats with NULL should fail */
    hypo_orch_stats_t stats;
    EXPECT_NE(0, hypo_orch_get_stats(nullptr, &stats));
    EXPECT_NE(0, hypo_orch_get_stats(orchestrator, nullptr));

    /* Get drive state with NULL should fail */
    hypo_unified_drive_state_t drive_state;
    EXPECT_NE(0, hypo_orch_get_drive_state(nullptr, &drive_state));
    EXPECT_NE(0, hypo_orch_get_drive_state(orchestrator, nullptr));

    /* Get drive level with NULL should fail */
    float level;
    EXPECT_NE(0, hypo_orch_get_drive_level(nullptr, &level));
    EXPECT_NE(0, hypo_orch_get_drive_level(orchestrator, nullptr));
}

/* ============================================================================
 * REG-013: Default configuration validation
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG013_DefaultConfigValid) {
    hypo_orch_config_t default_config;
    int ret = hypo_orch_default_config(&default_config);
    EXPECT_EQ(ret, 0);

    /* Validate thresholds are in valid ranges */
    EXPECT_GT(default_config.urgent_threshold, 0.0f);
    EXPECT_LE(default_config.urgent_threshold, 1.0f);

    EXPECT_GT(default_config.elevated_threshold, 0.0f);
    EXPECT_LE(default_config.elevated_threshold, 1.0f);

    EXPECT_GT(default_config.moderate_threshold, 0.0f);
    EXPECT_LE(default_config.moderate_threshold, 1.0f);

    /* Thresholds should be ordered correctly */
    EXPECT_GT(default_config.urgent_threshold, default_config.elevated_threshold);
    EXPECT_GT(default_config.elevated_threshold, default_config.moderate_threshold);

    /* Decay rate should be positive */
    EXPECT_GT(default_config.drive_decay_rate, 0.0f);
    EXPECT_LT(default_config.drive_decay_rate, 1.0f);

    /* Limits should be positive */
    EXPECT_GT(default_config.max_bridges, 0u);
    EXPECT_GT(default_config.max_subscriptions, 0u);
}

/* ============================================================================
 * REG-014: Statistics integrity after many operations
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG014_StatisticsIntegrity) {
    hypo_orch_reset_stats(orchestrator);

    int handle = 42;
    uint32_t bridge_id;
    hypo_orch_register_bridge(orchestrator, HYPO_BRIDGE_EMOTION, "test_bridge",
                              &handle, nullptr, &bridge_id);

    int subscriber_handle = 43;
    uint32_t subscriber_id;
    hypo_orch_register_bridge(orchestrator, HYPO_BRIDGE_ATTENTION, "subscriber",
                              &subscriber_handle, nullptr, &subscriber_id);

    hypo_orch_subscribe(orchestrator, subscriber_id, HYPO_EVENT_DRIVE_ACTIVATED,
                       regression_event_callback, nullptr);

    const int NUM_EVENTS = 200;
    const int NUM_DRIVES = 50;

    for (int i = 0; i < NUM_EVENTS; i++) {
        hypo_event_data_t event;
        create_test_event(&event, HYPO_EVENT_DRIVE_ACTIVATED,
                         HYPO_BRIDGE_EMOTION, (float)i / NUM_EVENTS);
        hypo_orch_publish(orchestrator, bridge_id, &event);
    }

    for (int i = 0; i < NUM_DRIVES; i++) {
        hypo_orch_report_drive(orchestrator, bridge_id, 0,
                              (float)i / NUM_DRIVES, HYPO_URGENCY_LOW, "Test");
    }

    hypo_orch_stats_t stats;
    hypo_orch_get_stats(orchestrator, &stats);

    /* Verify counts - events_published may include drive reports as events */
    EXPECT_GE(stats.events_published, (uint64_t)NUM_EVENTS)
        << "Events published should be at least NUM_EVENTS";
    EXPECT_GE(stats.events_delivered, (uint64_t)NUM_EVENTS)
        << "Events delivered should be at least NUM_EVENTS (one subscriber)";
    EXPECT_GE(stats.drives_activated, (uint64_t)NUM_DRIVES)
        << "Drives activated should be at least NUM_DRIVES";

    /* Subscription count */
    EXPECT_EQ(stats.active_subscriptions, 1u)
        << "Should have 1 active subscription";

    /* Registered bridges */
    EXPECT_EQ(stats.registered_bridges, 2u)
        << "Should have 2 registered bridges";
}

/* ============================================================================
 * REG-015: Rapid start/stop cycles don't corrupt state
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorRegressionTest, REG015_RapidResetCycles) {
    int handle = 42;
    uint32_t bridge_id;

    for (int i = 0; i < 50; i++) {
        /* Register a bridge */
        hypo_orch_register_bridge(orchestrator, HYPO_BRIDGE_EMOTION, "test",
                                  &handle, nullptr, &bridge_id);

        /* Do some operations */
        hypo_orch_report_drive(orchestrator, bridge_id, 0, 0.5f,
                              HYPO_URGENCY_MODERATE, "Test");
        hypo_orch_trigger_stress(orchestrator, "Rapid test");
        hypo_orch_release_stress(orchestrator);

        /* Unregister bridge before reset - reset() resets state but preserves bridges */
        hypo_orch_unregister_bridge(orchestrator, bridge_id);

        /* Reset */
        hypo_orch_reset(orchestrator);

        /* Verify state */
        hypo_orch_state_t state;
        hypo_orch_get_state(orchestrator, &state);
        EXPECT_NE(state, HYPO_ORCH_STATE_ERROR)
            << "State should not be ERROR after cycle " << i;

        hypo_orch_stats_t stats;
        hypo_orch_get_stats(orchestrator, &stats);
        EXPECT_EQ(stats.registered_bridges, 0u)
            << "Should have 0 bridges after unregister+reset in cycle " << i;
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
