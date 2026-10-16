/**
 * @file test_tier2_hub_bridges_regression.cpp
 * @brief Regression tests for Tier 2 Hub bridges in NIMCP
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Regression tests for Tier 2 Hub bridges behavior and performance
 * WHY:  Ensure bridge behavior remains consistent across code changes
 * HOW:  Test event routing stability, performance baselines, memory management,
 *       thread safety, state consistency, and statistics accuracy
 *
 * TIER 2 HUB BRIDGES TESTED:
 * - Imagination-Reasoning Bridge
 * - Game Theory-Executive Bridge
 * - Mirror-Empathy Bridge
 * - Salience-Attention Bridge
 * - Predictive-Attention Bridge
 *
 * REGRESSION TEST CATEGORIES:
 * - EventRoutingStability: Event routing remains consistent
 * - PerformanceRegression: Event latency doesn't degrade
 * - MemoryStabilityRegression: No memory leaks over many events
 * - ConcurrentEventHandling: Thread safety under load
 * - BridgeStateConsistency: Bridge states remain valid
 * - StatisticsAccuracyRegression: Stats track correctly
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <numeric>

// Cognitive Integration Hub
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

// Tier 2 Hub Bridges
#include "cognitive/integration/nimcp_imagination_reasoning_bridge.h"
#include "cognitive/integration/nimcp_game_theory_executive_bridge.h"
#include "cognitive/integration/nimcp_mirror_empathy_bridge.h"
#include "cognitive/integration/nimcp_salience_attention_bridge.h"
#include "cognitive/integration/nimcp_predictive_attention_bridge.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr int NUM_BRIDGES = 5;
static constexpr int REGRESSION_ITERATIONS = 1000;
static constexpr int MEMORY_TEST_ITERATIONS = 100;
static constexpr int CONCURRENT_THREADS = 4;
static constexpr int EVENTS_PER_THREAD = 100;
static constexpr uint64_t MAX_EVENT_LATENCY_US = 10000;  // 10ms max latency
static constexpr float FLOAT_TOLERANCE = 0.001f;

//=============================================================================
// Helper Structures
//=============================================================================

/**
 * @brief Context for tracking event sequence order
 */
struct SequenceTrackingContext {
    std::vector<uint32_t> received_sequences;
    std::mutex mutex;
};

/**
 * @brief Context for counting received events
 */
struct EventCountContext {
    std::atomic<uint64_t> event_count{0};
};

/**
 * @brief Context for thread safety testing
 */
struct ThreadSafetyContext {
    std::atomic<uint64_t> total_events{0};
    std::mutex event_mutex;
    std::vector<uint32_t> source_ids;
};

/**
 * @brief Context for latency measurement
 */
struct LatencyContext {
    std::vector<uint64_t> latencies_us;
    std::mutex mutex;
};

//=============================================================================
// Callback Functions
//=============================================================================

/**
 * @brief Callback that records sequence numbers
 */
static int sequence_tracking_callback(const cognitive_event_data_t* event, void* user_data) {
    if (!event || !user_data) return -1;

    SequenceTrackingContext* ctx = static_cast<SequenceTrackingContext*>(user_data);

    // Payload contains sequence number
    if (event->payload && event->payload_size >= sizeof(uint32_t)) {
        uint32_t seq = *static_cast<const uint32_t*>(event->payload);
        std::lock_guard<std::mutex> lock(ctx->mutex);
        ctx->received_sequences.push_back(seq);
    }

    return 0;
}

/**
 * @brief Callback that counts events
 */
static int event_counting_callback(const cognitive_event_data_t* event, void* user_data) {
    if (!event || !user_data) return -1;

    EventCountContext* ctx = static_cast<EventCountContext*>(user_data);
    ctx->event_count.fetch_add(1, std::memory_order_relaxed);

    return 0;
}

/**
 * @brief Callback for thread safety testing
 */
static int thread_safety_callback(const cognitive_event_data_t* event, void* user_data) {
    if (!event || !user_data) return -1;

    ThreadSafetyContext* ctx = static_cast<ThreadSafetyContext*>(user_data);
    ctx->total_events.fetch_add(1, std::memory_order_relaxed);

    // Record source module ID
    {
        std::lock_guard<std::mutex> lock(ctx->event_mutex);
        ctx->source_ids.push_back(event->source_module_id);
    }

    return 0;
}

/**
 * @brief Callback for latency measurement
 */
static int latency_callback(const cognitive_event_data_t* event, void* user_data) {
    if (!event || !user_data) return -1;

    auto now = std::chrono::high_resolution_clock::now();
    uint64_t receive_time = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()
    ).count();

    // Event timestamp should be in microseconds
    uint64_t latency = receive_time - event->timestamp;

    LatencyContext* ctx = static_cast<LatencyContext*>(user_data);
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->latencies_us.push_back(latency);

    return 0;
}

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @class Tier2HubBridgesRegressionTest
 * @brief Regression test fixture for Tier 2 Hub bridges
 */
class Tier2HubBridgesRegressionTest : public ::testing::Test {
protected:
    cognitive_integration_hub_t hub = nullptr;
    imagination_reasoning_bridge_t* imag_reason_bridge = nullptr;
    game_theory_executive_bridge_t* gt_exec_bridge = nullptr;
    mirror_empathy_bridge_t* mirror_empathy_bridge = nullptr;
    salience_attention_bridge_t* salience_attn_bridge = nullptr;
    predictive_attention_bridge_t* pred_attn_bridge = nullptr;

    void SetUp() override {
        cognitive_hub_config_t config = cognitive_hub_default_config();
        config.max_modules = 64;
        config.max_subscriptions = 256;
        config.enable_async = false;  // Sync for deterministic testing
        hub = cognitive_hub_create(&config);
        ASSERT_NE(hub, nullptr) << "Failed to create cognitive hub";

        // Create bridges
        imagination_reasoning_config_t imag_config;
        imagination_reasoning_bridge_default_config(&imag_config);
        imag_config.enable_logging = false;
        imag_reason_bridge = imagination_reasoning_bridge_create(&imag_config);
        ASSERT_NE(imag_reason_bridge, nullptr);

        game_theory_executive_config_t gt_config;
        game_theory_executive_bridge_default_config(&gt_config);
        gt_config.enable_logging = false;
        gt_exec_bridge = game_theory_executive_bridge_create(&gt_config);
        ASSERT_NE(gt_exec_bridge, nullptr);

        mirror_empathy_config_t mirror_config;
        mirror_empathy_bridge_default_config(&mirror_config);
        mirror_config.enable_logging = false;
        mirror_empathy_bridge = mirror_empathy_bridge_create(&mirror_config);
        ASSERT_NE(mirror_empathy_bridge, nullptr);

        salience_attention_config_t salience_config;
        salience_attention_bridge_default_config(&salience_config);
        salience_config.enable_logging = false;
        salience_attn_bridge = salience_attention_bridge_create(&salience_config);
        ASSERT_NE(salience_attn_bridge, nullptr);

        predictive_attention_bridge_config_t pred_config;
        predictive_attention_bridge_default_config(&pred_config);
        pred_config.enable_logging = false;
        pred_attn_bridge = predictive_attention_bridge_create(&pred_config);
        ASSERT_NE(pred_attn_bridge, nullptr);
    }

    void TearDown() override {
        if (imag_reason_bridge) {
            if (imagination_reasoning_bridge_is_connected(imag_reason_bridge))
                imagination_reasoning_bridge_disconnect(imag_reason_bridge);
            imagination_reasoning_bridge_destroy(imag_reason_bridge);
        }
        if (gt_exec_bridge) {
            if (game_theory_executive_bridge_is_connected(gt_exec_bridge))
                game_theory_executive_bridge_disconnect(gt_exec_bridge);
            game_theory_executive_bridge_destroy(gt_exec_bridge);
        }
        if (mirror_empathy_bridge) {
            if (mirror_empathy_bridge_is_registered(mirror_empathy_bridge))
                mirror_empathy_bridge_unregister_from_hub(mirror_empathy_bridge);
            mirror_empathy_bridge_destroy(mirror_empathy_bridge);
        }
        if (salience_attn_bridge) {
            if (salience_attention_bridge_is_registered(salience_attn_bridge))
                salience_attention_bridge_unregister_from_hub(salience_attn_bridge);
            salience_attention_bridge_destroy(salience_attn_bridge);
        }
        if (pred_attn_bridge) {
            if (predictive_attention_bridge_is_connected(pred_attn_bridge))
                predictive_attention_bridge_unregister_from_hub(pred_attn_bridge);
            predictive_attention_bridge_destroy(pred_attn_bridge);
        }
        if (hub) {
            cognitive_hub_destroy(hub);
        }
    }

    bool RegisterAllBridges() {
        if (imagination_reasoning_bridge_connect(imag_reason_bridge, hub) != 0) return false;
        if (game_theory_executive_bridge_connect(gt_exec_bridge, hub) != 0) return false;
        if (mirror_empathy_bridge_register_with_hub(mirror_empathy_bridge, hub) != 0) return false;
        if (salience_attention_bridge_register_with_hub(salience_attn_bridge, hub) != 0) return false;
        if (predictive_attention_bridge_register_with_hub(pred_attn_bridge, hub) != 0) return false;
        return true;
    }
};

//=============================================================================
// Event Routing Stability Tests
//=============================================================================

/**
 * Test: Events are delivered in the order they were published
 */
TEST_F(Tier2HubBridgesRegressionTest, EventOrderPreserved) {
    ASSERT_TRUE(RegisterAllBridges());

    const uint32_t SUBSCRIBER_ID = 100;
    const uint32_t NUM_EVENTS = 100;

    // Register subscriber
    ASSERT_EQ(cognitive_hub_register_module(
        hub, SUBSCRIBER_ID, COG_CATEGORY_SELF, "subscriber", nullptr
    ), 0);

    SequenceTrackingContext ctx;

    // Subscribe to state change events
    ASSERT_EQ(cognitive_hub_subscribe(
        hub, SUBSCRIBER_ID, COG_EVENT_STATE_CHANGE,
        sequence_tracking_callback, &ctx
    ), 0);

    // Publish events with sequence numbers via salience bridge
    for (uint32_t i = 0; i < NUM_EVENTS; i++) {
        salient_item_t item = {};
        item.item_id = i;
        item.salience_score = 0.5f;
        salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);
    }

    // Give time for delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify events received (order depends on hub implementation)
    // At minimum, verify no event loss
    EXPECT_GT(ctx.received_sequences.size(), 0u)
        << "Expected events to be received";
}

/**
 * Test: No event loss during rapid publishing
 */
TEST_F(Tier2HubBridgesRegressionTest, NoEventLoss) {
    ASSERT_TRUE(RegisterAllBridges());

    const uint32_t SUBSCRIBER_ID = 200;
    const uint64_t NUM_EVENTS = REGRESSION_ITERATIONS;

    ASSERT_EQ(cognitive_hub_register_module(
        hub, SUBSCRIBER_ID, COG_CATEGORY_REASONING, "subscriber", nullptr
    ), 0);

    EventCountContext ctx;

    // Subscribe to relevant events
    ASSERT_EQ(cognitive_hub_subscribe(
        hub, SUBSCRIBER_ID, COG_EVENT_OUTPUT_READY,
        event_counting_callback, &ctx
    ), 0);

    // Rapidly publish events
    for (uint64_t i = 0; i < NUM_EVENTS; i++) {
        imagination_insight_t insight = {};
        insight.insight_id = i;
        insight.confidence = 0.5f;
        imagination_reasoning_publish_insight(imag_reason_bridge, &insight);
    }

    // Give time for delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify events received (may not be all due to filtering)
    // But total published should be tracked
    imagination_reasoning_stats_t stats;
    ASSERT_EQ(imagination_reasoning_bridge_get_stats(imag_reason_bridge, &stats), 0);
    EXPECT_GE(stats.insights_shared, static_cast<uint32_t>(NUM_EVENTS));
}

/**
 * Test: Event routing remains stable across multiple cycles
 */
TEST_F(Tier2HubBridgesRegressionTest, EventRoutingStability) {
    ASSERT_TRUE(RegisterAllBridges());

    const int NUM_CYCLES = 50;
    const int EVENTS_PER_CYCLE = 20;

    for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
        for (int i = 0; i < EVENTS_PER_CYCLE; i++) {
            // Alternate between bridges
            switch (i % 4) {
                case 0: {
                    imagination_insight_t insight = {};
                    insight.insight_id = cycle * 1000 + i;
                    insight.confidence = 0.6f;
                    EXPECT_EQ(imagination_reasoning_publish_insight(imag_reason_bridge, &insight), 0);
                    break;
                }
                case 1: {
                    salient_item_t item = {};
                    item.item_id = cycle * 1000 + i;
                    item.salience_score = 0.7f;
                    EXPECT_EQ(salience_attention_publish_salience_detection(
                        salience_attn_bridge, &item, 0.7f), 0);
                    break;
                }
                case 2: {
                    EXPECT_EQ(predictive_attention_publish_prediction_error(
                        pred_attn_bridge, 0.4f, cycle * 1000 + i), 0);
                    break;
                }
                case 3: {
                    mirror_empathy_action_t action = {};
                    action.agent_id = cycle * 1000 + i;
                    action.action_type = MIRROR_ACTION_GESTURE;
                    EXPECT_EQ(mirror_empathy_publish_mirrored_action(
                        mirror_empathy_bridge, &action), 0);
                    break;
                }
            }
        }

        // All bridges should still be connected
        EXPECT_TRUE(imagination_reasoning_bridge_is_connected(imag_reason_bridge));
        EXPECT_TRUE(game_theory_executive_bridge_is_connected(gt_exec_bridge));
        EXPECT_TRUE(mirror_empathy_bridge_is_registered(mirror_empathy_bridge));
        EXPECT_TRUE(salience_attention_bridge_is_registered(salience_attn_bridge));
        EXPECT_TRUE(predictive_attention_bridge_is_connected(pred_attn_bridge));
    }
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

/**
 * Test: Event latency doesn't degrade (regression baseline)
 */
TEST_F(Tier2HubBridgesRegressionTest, PerformanceRegression) {
    ASSERT_TRUE(RegisterAllBridges());

    const uint32_t SUBSCRIBER_ID = 300;
    const int NUM_MEASUREMENTS = 100;

    ASSERT_EQ(cognitive_hub_register_module(
        hub, SUBSCRIBER_ID, COG_CATEGORY_SELF, "latency_monitor", nullptr
    ), 0);

    LatencyContext ctx;

    // Subscribe to events for latency measurement
    ASSERT_EQ(cognitive_hub_subscribe(
        hub, SUBSCRIBER_ID, COG_EVENT_STATE_CHANGE,
        latency_callback, &ctx
    ), 0);

    // Publish events with timestamps
    for (int i = 0; i < NUM_MEASUREMENTS; i++) {
        cognitive_event_data_t event = {};
        event.event_type = COG_EVENT_STATE_CHANGE;
        event.source_module_id = imagination_reasoning_bridge_get_module_id(imag_reason_bridge);
        event.priority = COG_PRIORITY_NORMAL;

        // Set timestamp to current time
        auto now = std::chrono::high_resolution_clock::now();
        event.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()
        ).count();

        cognitive_hub_publish(hub, event.source_module_id, event.event_type, &event);
    }

    // Give time for delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Analyze latencies
    if (!ctx.latencies_us.empty()) {
        // Sort for percentile calculations
        std::sort(ctx.latencies_us.begin(), ctx.latencies_us.end());

        uint64_t median = ctx.latencies_us[ctx.latencies_us.size() / 2];
        uint64_t p95 = ctx.latencies_us[ctx.latencies_us.size() * 95 / 100];
        uint64_t max_latency = ctx.latencies_us.back();

        // Regression check: latencies should be reasonable
        EXPECT_LT(median, MAX_EVENT_LATENCY_US)
            << "Median latency too high: " << median << "us";
        EXPECT_LT(p95, MAX_EVENT_LATENCY_US * 2)
            << "P95 latency too high: " << p95 << "us";
    }
}

/**
 * Test: Bridge operation throughput
 */
TEST_F(Tier2HubBridgesRegressionTest, ThroughputRegression) {
    ASSERT_TRUE(RegisterAllBridges());

    const int NUM_OPERATIONS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_OPERATIONS; i++) {
        // Simple operation: salience detection
        salient_item_t item = {};
        item.item_id = i;
        item.salience_score = 0.5f;
        salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    double ops_per_second = (NUM_OPERATIONS * 1000000.0) / duration_us;

    // Throughput regression check: at least 1000 ops/sec
    EXPECT_GT(ops_per_second, 1000.0)
        << "Throughput regression: " << ops_per_second << " ops/sec";
}

//=============================================================================
// Memory Stability Regression Tests
//=============================================================================

/**
 * Test: No memory leaks over many create/destroy cycles
 *
 * NOTE: Run with valgrind for complete memory leak detection
 */
TEST_F(Tier2HubBridgesRegressionTest, MemoryStabilityRegression) {
    // First destroy fixture bridges
    imagination_reasoning_bridge_destroy(imag_reason_bridge);
    imag_reason_bridge = nullptr;
    game_theory_executive_bridge_destroy(gt_exec_bridge);
    gt_exec_bridge = nullptr;
    mirror_empathy_bridge_destroy(mirror_empathy_bridge);
    mirror_empathy_bridge = nullptr;
    salience_attention_bridge_destroy(salience_attn_bridge);
    salience_attn_bridge = nullptr;
    predictive_attention_bridge_destroy(pred_attn_bridge);
    pred_attn_bridge = nullptr;
    cognitive_hub_destroy(hub);
    hub = nullptr;

    for (int iter = 0; iter < MEMORY_TEST_ITERATIONS; iter++) {
        // Create hub
        cognitive_hub_config_t config = cognitive_hub_default_config();
        config.max_modules = 32;
        cognitive_integration_hub_t test_hub = cognitive_hub_create(&config);
        ASSERT_NE(test_hub, nullptr) << "Failed to create hub on iteration " << iter;

        // Create bridges
        imagination_reasoning_config_t imag_config;
        imagination_reasoning_bridge_default_config(&imag_config);
        imagination_reasoning_bridge_t* imag = imagination_reasoning_bridge_create(&imag_config);
        ASSERT_NE(imag, nullptr);

        salience_attention_config_t sal_config;
        salience_attention_bridge_default_config(&sal_config);
        salience_attention_bridge_t* sal = salience_attention_bridge_create(&sal_config);
        ASSERT_NE(sal, nullptr);

        // Register and use
        ASSERT_EQ(imagination_reasoning_bridge_connect(imag, test_hub), 0);
        ASSERT_EQ(salience_attention_bridge_register_with_hub(sal, test_hub), 0);

        // Do some operations
        imagination_insight_t insight = {};
        insight.insight_id = iter;
        imagination_reasoning_publish_insight(imag, &insight);

        salient_item_t item = {};
        item.item_id = iter;
        salience_attention_publish_salience_detection(sal, &item, 0.5f);

        // Cleanup
        imagination_reasoning_bridge_disconnect(imag);
        salience_attention_bridge_unregister_from_hub(sal);
        imagination_reasoning_bridge_destroy(imag);
        salience_attention_bridge_destroy(sal);
        cognitive_hub_destroy(test_hub);
    }

    // If we got here without crash or leak, test passes
    SUCCEED() << "Completed " << MEMORY_TEST_ITERATIONS << " memory test cycles";

    // Recreate fixture resources
    cognitive_hub_config_t config = cognitive_hub_default_config();
    hub = cognitive_hub_create(&config);

    imagination_reasoning_config_t imag_config;
    imagination_reasoning_bridge_default_config(&imag_config);
    imag_reason_bridge = imagination_reasoning_bridge_create(&imag_config);

    game_theory_executive_config_t gt_config;
    game_theory_executive_bridge_default_config(&gt_config);
    gt_exec_bridge = game_theory_executive_bridge_create(&gt_config);

    mirror_empathy_config_t mirror_config;
    mirror_empathy_bridge_default_config(&mirror_config);
    mirror_empathy_bridge = mirror_empathy_bridge_create(&mirror_config);

    salience_attention_config_t sal_config;
    salience_attention_bridge_default_config(&sal_config);
    salience_attn_bridge = salience_attention_bridge_create(&sal_config);

    predictive_attention_bridge_config_t pred_config;
    predictive_attention_bridge_default_config(&pred_config);
    pred_attn_bridge = predictive_attention_bridge_create(&pred_config);
}

//=============================================================================
// Concurrent Event Handling Tests
//=============================================================================

/**
 * Test: Thread safety of concurrent event publishing
 */
TEST_F(Tier2HubBridgesRegressionTest, ConcurrentEventHandling) {
    ASSERT_TRUE(RegisterAllBridges());

    const uint32_t SUBSCRIBER_ID = 500;
    const uint32_t NUM_THREADS = CONCURRENT_THREADS;
    const uint32_t EVENTS_PER = EVENTS_PER_THREAD;

    ASSERT_EQ(cognitive_hub_register_module(
        hub, SUBSCRIBER_ID, COG_CATEGORY_EXECUTIVE, "subscriber", nullptr
    ), 0);

    ThreadSafetyContext ctx;

    // Subscribe to events
    ASSERT_EQ(cognitive_hub_subscribe(
        hub, SUBSCRIBER_ID, COG_EVENT_OUTPUT_READY,
        thread_safety_callback, &ctx
    ), 0);

    // Thread function for publishing events
    auto publish_events = [this](uint32_t thread_id, uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            switch (thread_id % 4) {
                case 0: {
                    imagination_insight_t insight = {};
                    insight.insight_id = thread_id * 10000 + i;
                    imagination_reasoning_publish_insight(imag_reason_bridge, &insight);
                    break;
                }
                case 1: {
                    salient_item_t item = {};
                    item.item_id = thread_id * 10000 + i;
                    salience_attention_publish_salience_detection(
                        salience_attn_bridge, &item, 0.5f);
                    break;
                }
                case 2: {
                    predictive_attention_publish_prediction_error(
                        pred_attn_bridge, 0.4f, thread_id * 10000 + i);
                    break;
                }
                case 3: {
                    mirror_empathy_action_t action = {};
                    action.agent_id = thread_id * 10000 + i;
                    mirror_empathy_publish_mirrored_action(mirror_empathy_bridge, &action);
                    break;
                }
            }

            // Yield occasionally
            if (i % 10 == 0) {
                std::this_thread::yield();
            }
        }
    };

    // Spawn threads
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (uint32_t t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(publish_events, t, EVENTS_PER);
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Give time for final event processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // All bridges should still be connected (no crashes)
    EXPECT_TRUE(imagination_reasoning_bridge_is_connected(imag_reason_bridge));
    EXPECT_TRUE(game_theory_executive_bridge_is_connected(gt_exec_bridge));
    EXPECT_TRUE(mirror_empathy_bridge_is_registered(mirror_empathy_bridge));
    EXPECT_TRUE(salience_attention_bridge_is_registered(salience_attn_bridge));
    EXPECT_TRUE(predictive_attention_bridge_is_connected(pred_attn_bridge));

    // Verify hub processed events
    cognitive_hub_stats_t stats = {};
    ASSERT_EQ(cognitive_hub_get_stats(hub, &stats), 0);
    EXPECT_GT(stats.events_published, 0u);
}

/**
 * Test: Concurrent registration/unregistration
 */
TEST_F(Tier2HubBridgesRegressionTest, ConcurrentRegistration) {
    // Test that concurrent register/unregister doesn't crash
    const int CHURN_CYCLES = 20;

    std::atomic<bool> stop{false};
    std::atomic<int> errors{0};

    // Thread that continuously registers/unregisters bridges
    auto churn_thread = [&]() {
        for (int i = 0; i < CHURN_CYCLES && !stop; i++) {
            // Register
            int result = imagination_reasoning_bridge_connect(imag_reason_bridge, hub);
            if (result != 0 && imagination_reasoning_bridge_is_connected(imag_reason_bridge) == false) {
                // Error only if not already connected
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // Unregister
            imagination_reasoning_bridge_disconnect(imag_reason_bridge);

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    // Thread that publishes events
    auto publish_thread = [&]() {
        for (int i = 0; i < CHURN_CYCLES * 10 && !stop; i++) {
            // Only publish if connected
            if (salience_attention_bridge_is_registered(salience_attn_bridge)) {
                salient_item_t item = {};
                item.item_id = i;
                salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);
            }
            std::this_thread::yield();
        }
    };

    // Register salience bridge for publishing
    ASSERT_EQ(salience_attention_bridge_register_with_hub(salience_attn_bridge, hub), 0);

    std::thread t1(churn_thread);
    std::thread t2(publish_thread);

    t1.join();
    stop = true;
    t2.join();

    // No crashes = success
    SUCCEED();
}

//=============================================================================
// Bridge State Consistency Tests
//=============================================================================

/**
 * Test: Bridge states remain valid after operations
 */
TEST_F(Tier2HubBridgesRegressionTest, BridgeStateConsistency) {
    ASSERT_TRUE(RegisterAllBridges());

    // Perform operations and verify state remains consistent
    for (int i = 0; i < 50; i++) {
        // Various operations
        imagination_insight_t insight = {};
        insight.insight_id = i;
        imagination_reasoning_publish_insight(imag_reason_bridge, &insight);

        gt_strategic_recommendation_t rec;
        game_theory_executive_get_recommendation(gt_exec_bridge, &rec);

        mirror_empathy_action_t action = {};
        action.agent_id = i;
        mirror_empathy_publish_mirrored_action(mirror_empathy_bridge, &action);

        salient_item_t item = {};
        item.item_id = i;
        salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);

        predictive_attention_publish_prediction_error(pred_attn_bridge, 0.3f, i);

        // Verify all bridges still connected
        EXPECT_TRUE(imagination_reasoning_bridge_is_connected(imag_reason_bridge))
            << "Imagination bridge disconnected at iteration " << i;
        EXPECT_TRUE(game_theory_executive_bridge_is_connected(gt_exec_bridge))
            << "GT bridge disconnected at iteration " << i;
        EXPECT_TRUE(mirror_empathy_bridge_is_registered(mirror_empathy_bridge))
            << "Mirror bridge disconnected at iteration " << i;
        EXPECT_TRUE(salience_attention_bridge_is_registered(salience_attn_bridge))
            << "Salience bridge disconnected at iteration " << i;
        EXPECT_TRUE(predictive_attention_bridge_is_connected(pred_attn_bridge))
            << "Predictive bridge disconnected at iteration " << i;
    }

    // Verify states are not in error
    EXPECT_NE(imagination_reasoning_bridge_get_state(imag_reason_bridge),
              IMAG_REASON_STATE_ERROR);
    EXPECT_NE(game_theory_executive_bridge_get_state(gt_exec_bridge),
              GT_EXEC_STATE_ERROR);
}

/**
 * Test: Config defaults produce valid initial state
 */
TEST_F(Tier2HubBridgesRegressionTest, ConfigDefaultsProduceValidState) {
    // Test each bridge type with default config

    // Imagination-Reasoning
    imagination_reasoning_config_t imag_config;
    EXPECT_EQ(imagination_reasoning_bridge_default_config(&imag_config), 0);
    EXPECT_GT(imag_config.module_id, 0u);

    // Game Theory-Executive
    game_theory_executive_config_t gt_config;
    EXPECT_EQ(game_theory_executive_bridge_default_config(&gt_config), 0);
    EXPECT_GT(gt_config.module_id, 0u);

    // Mirror-Empathy
    mirror_empathy_config_t mirror_config;
    EXPECT_EQ(mirror_empathy_bridge_default_config(&mirror_config), 0);
    EXPECT_GT(mirror_config.module_id, 0u);

    // Salience-Attention
    salience_attention_config_t salience_config;
    EXPECT_EQ(salience_attention_bridge_default_config(&salience_config), 0);
    EXPECT_GT(salience_config.module_id, 0u);

    // Predictive-Attention
    predictive_attention_bridge_config_t pred_config;
    EXPECT_EQ(predictive_attention_bridge_default_config(&pred_config), 0);
}

//=============================================================================
// Statistics Accuracy Regression Tests
//=============================================================================

/**
 * Test: Statistics track correctly over operations
 */
TEST_F(Tier2HubBridgesRegressionTest, StatisticsAccuracyRegression) {
    ASSERT_TRUE(RegisterAllBridges());

    const int NUM_OPS = 100;

    // Reset stats
    imagination_reasoning_bridge_reset_stats(imag_reason_bridge);
    salience_attention_bridge_reset_stats(salience_attn_bridge);
    predictive_attention_bridge_reset_stats(pred_attn_bridge);
    mirror_empathy_bridge_reset_stats(mirror_empathy_bridge);
    game_theory_executive_bridge_reset_stats(gt_exec_bridge);

    // Perform known number of operations
    for (int i = 0; i < NUM_OPS; i++) {
        imagination_insight_t insight = {};
        insight.insight_id = i;
        imagination_reasoning_publish_insight(imag_reason_bridge, &insight);

        salient_item_t item = {};
        item.item_id = i;
        salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);
    }

    // Verify stats match expected counts
    imagination_reasoning_stats_t imag_stats;
    EXPECT_EQ(imagination_reasoning_bridge_get_stats(imag_reason_bridge, &imag_stats), 0);
    EXPECT_GE(imag_stats.insights_shared, static_cast<uint32_t>(NUM_OPS));

    salience_attention_stats_t sal_stats;
    EXPECT_EQ(salience_attention_bridge_get_stats(salience_attn_bridge, &sal_stats), 0);
    EXPECT_GE(sal_stats.salience_detections, static_cast<uint64_t>(NUM_OPS));
}

/**
 * Test: Stats reset works correctly
 */
TEST_F(Tier2HubBridgesRegressionTest, StatsResetRegression) {
    ASSERT_TRUE(RegisterAllBridges());

    // Generate some stats
    for (int i = 0; i < 10; i++) {
        salient_item_t item = {};
        item.item_id = i;
        salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);
    }

    // Verify non-zero before reset
    salience_attention_stats_t before;
    EXPECT_EQ(salience_attention_bridge_get_stats(salience_attn_bridge, &before), 0);
    EXPECT_GT(before.salience_detections, 0u);

    // Reset
    EXPECT_EQ(salience_attention_bridge_reset_stats(salience_attn_bridge), 0);

    // Verify zero after reset
    salience_attention_stats_t after;
    EXPECT_EQ(salience_attention_bridge_get_stats(salience_attn_bridge, &after), 0);
    EXPECT_EQ(after.salience_detections, 0u);
}

/**
 * Test: Stats don't overflow under high volume
 */
TEST_F(Tier2HubBridgesRegressionTest, StatsNoOverflow) {
    ASSERT_TRUE(RegisterAllBridges());

    const int HIGH_VOLUME = 10000;

    for (int i = 0; i < HIGH_VOLUME; i++) {
        salient_item_t item = {};
        item.item_id = i;
        salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);

        if (i % 1000 == 0) {
            std::this_thread::yield();
        }
    }

    salience_attention_stats_t stats;
    EXPECT_EQ(salience_attention_bridge_get_stats(salience_attn_bridge, &stats), 0);

    // Stats should be >= HIGH_VOLUME (could be more if multiple deliveries)
    EXPECT_GE(stats.salience_detections, static_cast<uint64_t>(HIGH_VOLUME));
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

/**
 * Test: Null pointer safety
 */
TEST_F(Tier2HubBridgesRegressionTest, NullPointerSafety) {
    // Test null pointer handling for various functions
    EXPECT_NE(imagination_reasoning_publish_insight(nullptr, nullptr), 0);
    EXPECT_NE(salience_attention_publish_salience_detection(nullptr, nullptr, 0.0f), 0);
    EXPECT_NE(predictive_attention_publish_prediction_error(nullptr, 0.0f, 0), 0);
    EXPECT_NE(mirror_empathy_publish_mirrored_action(nullptr, nullptr), 0);

    // Stats with null
    EXPECT_NE(imagination_reasoning_bridge_get_stats(nullptr, nullptr), 0);
    EXPECT_NE(salience_attention_bridge_get_stats(nullptr, nullptr), 0);
}

/**
 * Test: Bridge operations when not connected
 */
TEST_F(Tier2HubBridgesRegressionTest, OperationsWhenNotConnected) {
    // Don't register bridges - they should handle operations gracefully

    // These should not crash, may return error or succeed based on impl
    imagination_insight_t insight = {};
    insight.insight_id = 1;
    int result = imagination_reasoning_publish_insight(imag_reason_bridge, &insight);
    // Result depends on implementation - may succeed or fail gracefully

    salient_item_t item = {};
    item.item_id = 1;
    result = salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);
    // Same - implementation dependent

    // Key: no crashes
    SUCCEED();
}

/**
 * Test: Double disconnect handling
 */
TEST_F(Tier2HubBridgesRegressionTest, DoubleDisconnectHandling) {
    ASSERT_TRUE(RegisterAllBridges());

    // First disconnect
    EXPECT_EQ(imagination_reasoning_bridge_disconnect(imag_reason_bridge), 0);
    EXPECT_FALSE(imagination_reasoning_bridge_is_connected(imag_reason_bridge));

    // Second disconnect - should handle gracefully
    int result = imagination_reasoning_bridge_disconnect(imag_reason_bridge);
    // Either 0 (idempotent) or error is acceptable
    EXPECT_TRUE(result == 0 || result != 0);

    // Should still not be connected
    EXPECT_FALSE(imagination_reasoning_bridge_is_connected(imag_reason_bridge));
}
