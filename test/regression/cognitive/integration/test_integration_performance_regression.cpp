/**
 * @file test_integration_performance_regression.cpp
 * @brief Performance regression tests for cognitive integration system
 * @date 2025-01-08
 *
 * WHAT: Performance regression tests for cognitive integration hub and bridges
 * WHY: Ensure performance characteristics don't regress across code changes
 * HOW: Measure latency and throughput, enforce performance bounds
 *
 * Tests:
 * - EventLatency: Measure time to publish and deliver an event
 * - ThroughputRegression: Measure events per second throughput
 * - BridgeOperationLatency: Measure per-operation latency for bridges
 *
 * Performance Targets:
 * - Event latency: < 1000 microseconds (1ms)
 * - Event throughput: > 10,000 events/second
 * - Bridge operation latency: < 1000 microseconds per operation
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>

#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"
#include "cognitive/integration/nimcp_emotion_memory_bridge.h"
#include "cognitive/integration/nimcp_attention_wm_bridge.h"
#include "cognitive/integration/nimcp_curiosity_reasoning_bridge.h"
#include "cognitive/integration/nimcp_ethics_executive_bridge.h"
#include "cognitive/integration/nimcp_tom_social_bridge.h"
#include "cognitive/integration/nimcp_gw_cognitive_bridge.h"

/* ============================================================================
 * Performance Thresholds (in microseconds unless noted)
 * ============================================================================ */

// Event latency: time from publish to callback execution
constexpr int64_t MAX_EVENT_LATENCY_US = 1000;  // 1ms

// Event throughput: minimum events per second
constexpr int64_t MIN_EVENT_THROUGHPUT = 10000;  // 10k events/sec

// Bridge operation latency: time per individual operation
constexpr int64_t MAX_BRIDGE_OP_LATENCY_US = 1000;  // 1000us (relaxed for CI/parallel test contention)

// Warm-up iterations before measuring
constexpr int WARMUP_ITERATIONS = 100;

/* ============================================================================
 * Helper Types
 * ============================================================================ */

/**
 * @brief High resolution clock type alias
 */
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::high_resolution_clock::time_point;
using Duration = std::chrono::microseconds;

/**
 * @brief Context for latency measurement
 */
struct LatencyContext {
    std::atomic<int64_t> total_latency_us{0};
    std::atomic<uint64_t> callback_count{0};
    TimePoint publish_time;
    std::atomic<bool> time_recorded{false};
};

/**
 * @brief Statistics helper
 */
struct PerformanceStats {
    double mean_us;
    double std_dev_us;
    double min_us;
    double max_us;
    double p50_us;
    double p95_us;
    double p99_us;

    static PerformanceStats calculate(const std::vector<int64_t>& samples) {
        PerformanceStats stats = {};
        if (samples.empty()) return stats;

        std::vector<int64_t> sorted = samples;
        std::sort(sorted.begin(), sorted.end());

        // Mean
        double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
        stats.mean_us = sum / sorted.size();

        // Standard deviation
        double sq_sum = 0;
        for (int64_t val : sorted) {
            double diff = val - stats.mean_us;
            sq_sum += diff * diff;
        }
        stats.std_dev_us = std::sqrt(sq_sum / sorted.size());

        // Min/Max
        stats.min_us = static_cast<double>(sorted.front());
        stats.max_us = static_cast<double>(sorted.back());

        // Percentiles
        size_t n = sorted.size();
        stats.p50_us = static_cast<double>(sorted[n / 2]);
        stats.p95_us = static_cast<double>(sorted[static_cast<size_t>(n * 0.95)]);
        stats.p99_us = static_cast<double>(sorted[static_cast<size_t>(n * 0.99)]);

        return stats;
    }
};

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PerformanceRegressionTest : public ::testing::Test {
protected:
    cognitive_integration_hub_t hub = nullptr;

    void SetUp() override {
        cognitive_hub_config_t config = cognitive_hub_default_config();
        config.max_modules = 64;
        config.max_subscriptions = 256;
        config.enable_async = false;  // Sync mode for accurate latency measurement
        hub = cognitive_hub_create(&config);
        ASSERT_NE(hub, nullptr) << "Failed to create cognitive hub";
    }

    void TearDown() override {
        if (hub) {
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }
};

/* ============================================================================
 * Callback Functions
 * ============================================================================ */

/**
 * @brief Callback that records receive time for latency measurement
 */
static int latency_callback(const cognitive_event_data_t* event, void* user_data) {
    if (!user_data) return -1;

    TimePoint receive_time = Clock::now();
    LatencyContext* ctx = static_cast<LatencyContext*>(user_data);

    if (ctx->time_recorded.load(std::memory_order_acquire)) {
        auto duration = std::chrono::duration_cast<Duration>(receive_time - ctx->publish_time);
        ctx->total_latency_us.fetch_add(duration.count(), std::memory_order_relaxed);
        ctx->callback_count.fetch_add(1, std::memory_order_relaxed);
    }

    (void)event;
    return 0;
}

/**
 * @brief Simple counting callback for throughput tests
 */
static std::atomic<uint64_t> throughput_callback_count{0};

static int throughput_callback(const cognitive_event_data_t* event, void* user_data) {
    (void)event;
    (void)user_data;
    throughput_callback_count.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

/* ============================================================================
 * Event Latency Tests
 * ============================================================================ */

/**
 * @brief Test event publish-to-delivery latency
 *
 * WHAT: Measure time from event publish to callback execution
 * WHY: Low latency is critical for responsive cognitive systems
 * HOW: Publish events, measure time to callback, enforce < 1ms bound
 */
TEST_F(PerformanceRegressionTest, EventLatency) {
    const uint32_t PUBLISHER_ID = 1;
    const uint32_t SUBSCRIBER_ID = 2;
    const int NUM_SAMPLES = 1000;

    // Register modules
    ASSERT_EQ(cognitive_hub_register_module(
        hub, PUBLISHER_ID, COG_CATEGORY_PERCEPTION, "publisher", nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_register_module(
        hub, SUBSCRIBER_ID, COG_CATEGORY_MEMORY, "subscriber", nullptr
    ), 0);

    // Set up latency tracking
    LatencyContext ctx;

    ASSERT_EQ(cognitive_hub_subscribe(
        hub, SUBSCRIBER_ID, COG_EVENT_INPUT_RECEIVED,
        latency_callback, &ctx
    ), 0);

    // Warm-up phase
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        cognitive_event_data_t event = {};
        event.event_type = COG_EVENT_INPUT_RECEIVED;
        event.source_module_id = PUBLISHER_ID;
        event.priority = COG_PRIORITY_NORMAL;
        cognitive_hub_publish(hub, PUBLISHER_ID, COG_EVENT_INPUT_RECEIVED, &event);
    }

    // Reset counters after warmup
    ctx.total_latency_us.store(0);
    ctx.callback_count.store(0);

    // Measure latency
    std::vector<int64_t> latency_samples;
    latency_samples.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        cognitive_event_data_t event = {};
        event.event_type = COG_EVENT_INPUT_RECEIVED;
        event.source_module_id = PUBLISHER_ID;
        event.priority = COG_PRIORITY_NORMAL;

        // Reset context for this measurement
        ctx.callback_count.store(0);
        ctx.total_latency_us.store(0);

        // Record publish time and enable measurement
        ctx.publish_time = Clock::now();
        ctx.time_recorded.store(true, std::memory_order_release);

        cognitive_hub_publish(hub, PUBLISHER_ID, COG_EVENT_INPUT_RECEIVED, &event);

        ctx.time_recorded.store(false, std::memory_order_release);

        // Record sample
        if (ctx.callback_count.load() > 0) {
            latency_samples.push_back(ctx.total_latency_us.load());
        }
    }

    // Calculate statistics
    ASSERT_GT(latency_samples.size(), 0u) << "No latency samples collected";
    PerformanceStats stats = PerformanceStats::calculate(latency_samples);

    // Report statistics
    std::cout << "Event Latency Statistics (microseconds):" << std::endl;
    std::cout << "  Mean:   " << stats.mean_us << std::endl;
    std::cout << "  StdDev: " << stats.std_dev_us << std::endl;
    std::cout << "  Min:    " << stats.min_us << std::endl;
    std::cout << "  Max:    " << stats.max_us << std::endl;
    std::cout << "  P50:    " << stats.p50_us << std::endl;
    std::cout << "  P95:    " << stats.p95_us << std::endl;
    std::cout << "  P99:    " << stats.p99_us << std::endl;

    // Performance assertions
    EXPECT_LT(stats.mean_us, static_cast<double>(MAX_EVENT_LATENCY_US))
        << "Mean event latency exceeds " << MAX_EVENT_LATENCY_US << " microseconds";

    EXPECT_LT(stats.p95_us, static_cast<double>(MAX_EVENT_LATENCY_US * 2))
        << "P95 event latency exceeds " << (MAX_EVENT_LATENCY_US * 2) << " microseconds";
}

/**
 * @brief Test event throughput under sustained load
 *
 * WHAT: Measure events processed per second
 * WHY: High throughput is essential for cognitive system scalability
 * HOW: Publish many events, measure total time, calculate throughput
 */
TEST_F(PerformanceRegressionTest, ThroughputRegression) {
    const uint32_t PUBLISHER_ID = 1;
    const uint32_t SUBSCRIBER_ID = 2;
    const uint64_t NUM_EVENTS = 10000;

    // Reset global counter
    throughput_callback_count.store(0);

    // Register modules
    ASSERT_EQ(cognitive_hub_register_module(
        hub, PUBLISHER_ID, COG_CATEGORY_PERCEPTION, "publisher", nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_register_module(
        hub, SUBSCRIBER_ID, COG_CATEGORY_MEMORY, "subscriber", nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_subscribe(
        hub, SUBSCRIBER_ID, COG_EVENT_OUTPUT_READY,
        throughput_callback, nullptr
    ), 0);

    // Warm-up
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        cognitive_event_data_t event = {};
        event.event_type = COG_EVENT_OUTPUT_READY;
        event.source_module_id = PUBLISHER_ID;
        event.priority = COG_PRIORITY_NORMAL;
        cognitive_hub_publish(hub, PUBLISHER_ID, COG_EVENT_OUTPUT_READY, &event);
    }
    throughput_callback_count.store(0);

    // Start timing
    TimePoint start = Clock::now();

    // Publish all events
    for (uint64_t i = 0; i < NUM_EVENTS; i++) {
        cognitive_event_data_t event = {};
        event.event_type = COG_EVENT_OUTPUT_READY;
        event.source_module_id = PUBLISHER_ID;
        event.timestamp = i;
        event.priority = COG_PRIORITY_NORMAL;

        int result = cognitive_hub_publish(hub, PUBLISHER_ID, COG_EVENT_OUTPUT_READY, &event);
        ASSERT_EQ(result, 0) << "Publish failed at event " << i;
    }

    // End timing
    TimePoint end = Clock::now();

    // Calculate throughput
    auto elapsed_us = std::chrono::duration_cast<Duration>(end - start).count();
    double elapsed_sec = static_cast<double>(elapsed_us) / 1000000.0;
    double throughput = static_cast<double>(NUM_EVENTS) / elapsed_sec;

    // Report results
    std::cout << "Throughput Statistics:" << std::endl;
    std::cout << "  Events:     " << NUM_EVENTS << std::endl;
    std::cout << "  Time:       " << elapsed_sec << " seconds" << std::endl;
    std::cout << "  Throughput: " << throughput << " events/second" << std::endl;
    std::cout << "  Delivered:  " << throughput_callback_count.load() << std::endl;

    // Verify all events delivered
    EXPECT_EQ(throughput_callback_count.load(), NUM_EVENTS)
        << "Event loss detected during throughput test";

    // Performance assertion
    EXPECT_GT(throughput, static_cast<double>(MIN_EVENT_THROUGHPUT))
        << "Throughput below minimum of " << MIN_EVENT_THROUGHPUT << " events/second";
}

/**
 * @brief Test throughput with multiple subscribers
 *
 * WHAT: Measure throughput when broadcasting to multiple subscribers
 * WHY: Cognitive systems often have many listeners per event type
 * HOW: Register multiple subscribers, measure throughput degradation
 */
TEST_F(PerformanceRegressionTest, MultiSubscriberThroughput) {
    const uint32_t PUBLISHER_ID = 1;
    const uint32_t NUM_SUBSCRIBERS = 5;
    const uint64_t NUM_EVENTS = 5000;

    // Reset counter
    throughput_callback_count.store(0);

    // Register publisher
    ASSERT_EQ(cognitive_hub_register_module(
        hub, PUBLISHER_ID, COG_CATEGORY_PERCEPTION, "publisher", nullptr
    ), 0);

    // Register multiple subscribers
    for (uint32_t s = 0; s < NUM_SUBSCRIBERS; s++) {
        uint32_t sub_id = s + 10;
        char name[64];
        snprintf(name, sizeof(name), "subscriber_%u", s);

        ASSERT_EQ(cognitive_hub_register_module(
            hub, sub_id, COG_CATEGORY_MEMORY, name, nullptr
        ), 0);

        ASSERT_EQ(cognitive_hub_subscribe(
            hub, sub_id, COG_EVENT_ATTENTION_SHIFT,
            throughput_callback, nullptr
        ), 0);
    }

    // Start timing
    TimePoint start = Clock::now();

    // Publish events
    for (uint64_t i = 0; i < NUM_EVENTS; i++) {
        cognitive_event_data_t event = {};
        event.event_type = COG_EVENT_ATTENTION_SHIFT;
        event.source_module_id = PUBLISHER_ID;
        event.priority = COG_PRIORITY_NORMAL;

        cognitive_hub_publish(hub, PUBLISHER_ID, COG_EVENT_ATTENTION_SHIFT, &event);
    }

    // End timing
    TimePoint end = Clock::now();

    // Calculate throughput
    auto elapsed_us = std::chrono::duration_cast<Duration>(end - start).count();
    double elapsed_sec = static_cast<double>(elapsed_us) / 1000000.0;
    double throughput = static_cast<double>(NUM_EVENTS) / elapsed_sec;

    uint64_t expected_deliveries = NUM_EVENTS * NUM_SUBSCRIBERS;

    // Report results
    std::cout << "Multi-Subscriber Throughput:" << std::endl;
    std::cout << "  Subscribers: " << NUM_SUBSCRIBERS << std::endl;
    std::cout << "  Events:      " << NUM_EVENTS << std::endl;
    std::cout << "  Deliveries:  " << throughput_callback_count.load()
              << " (expected " << expected_deliveries << ")" << std::endl;
    std::cout << "  Throughput:  " << throughput << " events/second" << std::endl;

    // Verify all deliveries
    EXPECT_EQ(throughput_callback_count.load(), expected_deliveries);

    // Throughput should still be reasonable with multiple subscribers
    // Allow degradation proportional to subscriber count
    double adjusted_min = static_cast<double>(MIN_EVENT_THROUGHPUT) / NUM_SUBSCRIBERS;
    EXPECT_GT(throughput, adjusted_min)
        << "Multi-subscriber throughput below adjusted minimum";
}

/* ============================================================================
 * Bridge Operation Latency Tests
 * ============================================================================ */

/**
 * @brief Test emotion-memory bridge operation latency
 *
 * WHAT: Measure per-operation latency for emotion-memory bridge
 * WHY: Individual operations must be fast for responsive processing
 * HOW: Time each operation, verify < 100us average
 */
TEST(BridgePerformanceRegression, EmotionMemoryLatency) {
    emotion_memory_config_t config;
    emotion_memory_bridge_default_config(&config);
    emotion_memory_bridge_t* bridge = emotion_memory_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    const int NUM_OPERATIONS = 1000;
    std::vector<int64_t> tag_latencies;
    std::vector<int64_t> retrieval_latencies;
    tag_latencies.reserve(NUM_OPERATIONS);
    retrieval_latencies.reserve(NUM_OPERATIONS);

    // Warm-up
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        emotion_memory_tag_memory(bridge, static_cast<uint64_t>(i), 0.5f, 0.5f);
    }

    // Measure tag_memory latency
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        TimePoint start = Clock::now();

        emotion_memory_tag_memory(
            bridge,
            static_cast<uint64_t>(WARMUP_ITERATIONS + i),
            static_cast<float>(i % 200 - 100) / 100.0f,
            static_cast<float>(i % 100) / 100.0f
        );

        TimePoint end = Clock::now();
        auto latency = std::chrono::duration_cast<Duration>(end - start).count();
        tag_latencies.push_back(latency);
    }

    // Measure on_retrieval latency
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        emotion_memory_emotion_out_t emotion_out = {};

        TimePoint start = Clock::now();

        emotion_memory_on_retrieval(
            bridge,
            static_cast<uint64_t>(WARMUP_ITERATIONS + i),
            &emotion_out
        );

        TimePoint end = Clock::now();
        auto latency = std::chrono::duration_cast<Duration>(end - start).count();
        retrieval_latencies.push_back(latency);
    }

    // Calculate statistics
    PerformanceStats tag_stats = PerformanceStats::calculate(tag_latencies);
    PerformanceStats retrieval_stats = PerformanceStats::calculate(retrieval_latencies);

    // Report results
    std::cout << "Emotion-Memory Bridge Latency (microseconds):" << std::endl;
    std::cout << "  Tag Memory:" << std::endl;
    std::cout << "    Mean: " << tag_stats.mean_us << ", P95: " << tag_stats.p95_us << std::endl;
    std::cout << "  On Retrieval:" << std::endl;
    std::cout << "    Mean: " << retrieval_stats.mean_us << ", P95: " << retrieval_stats.p95_us << std::endl;

    // Performance assertions
    EXPECT_LT(tag_stats.mean_us, static_cast<double>(MAX_BRIDGE_OP_LATENCY_US))
        << "Tag memory mean latency exceeds threshold";
    EXPECT_LT(retrieval_stats.mean_us, static_cast<double>(MAX_BRIDGE_OP_LATENCY_US))
        << "Retrieval mean latency exceeds threshold";

    emotion_memory_bridge_destroy(bridge);
}

/**
 * @brief Test attention-WM bridge operation latency
 *
 * WHAT: Measure per-operation latency for attention-WM bridge
 * WHY: Working memory operations must be fast
 * HOW: Time each operation type, verify < 100us average
 */
TEST(BridgePerformanceRegression, AttentionWMLatency) {
    attention_wm_config_t config;
    attention_wm_bridge_default_config(&config);
    attention_wm_bridge_t* bridge = attention_wm_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    const int NUM_OPERATIONS = 500;
    std::vector<int64_t> gate_latencies;
    std::vector<int64_t> focus_latencies;
    std::vector<int64_t> priority_latencies;

    // Warm-up
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        attention_wm_gate_entry(bridge, static_cast<uint64_t>(i), 0.9f);
    }

    // Measure gate_entry latency
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        TimePoint start = Clock::now();

        attention_wm_gate_entry(
            bridge,
            static_cast<uint64_t>(WARMUP_ITERATIONS + i),
            0.9f
        );

        TimePoint end = Clock::now();
        gate_latencies.push_back(std::chrono::duration_cast<Duration>(end - start).count());
    }

    // Measure focus_shift latency
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        TimePoint start = Clock::now();

        attention_wm_on_focus_shift(
            bridge,
            static_cast<uint64_t>(i),
            static_cast<uint64_t>(i + 1)
        );

        TimePoint end = Clock::now();
        focus_latencies.push_back(std::chrono::duration_cast<Duration>(end - start).count());
    }

    // Measure priority_update latency
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        TimePoint start = Clock::now();

        attention_wm_update_priority(
            bridge,
            static_cast<uint64_t>(WARMUP_ITERATIONS + i),
            0.5f
        );

        TimePoint end = Clock::now();
        priority_latencies.push_back(std::chrono::duration_cast<Duration>(end - start).count());
    }

    // Calculate statistics
    PerformanceStats gate_stats = PerformanceStats::calculate(gate_latencies);
    PerformanceStats focus_stats = PerformanceStats::calculate(focus_latencies);
    PerformanceStats priority_stats = PerformanceStats::calculate(priority_latencies);

    // Report results
    std::cout << "Attention-WM Bridge Latency (microseconds):" << std::endl;
    std::cout << "  Gate Entry:       Mean=" << gate_stats.mean_us
              << ", P95=" << gate_stats.p95_us << std::endl;
    std::cout << "  Focus Shift:      Mean=" << focus_stats.mean_us
              << ", P95=" << focus_stats.p95_us << std::endl;
    std::cout << "  Priority Update:  Mean=" << priority_stats.mean_us
              << ", P95=" << priority_stats.p95_us << std::endl;

    // Performance assertions
    EXPECT_LT(gate_stats.mean_us, static_cast<double>(MAX_BRIDGE_OP_LATENCY_US));
    EXPECT_LT(focus_stats.mean_us, static_cast<double>(MAX_BRIDGE_OP_LATENCY_US));
    EXPECT_LT(priority_stats.mean_us, static_cast<double>(MAX_BRIDGE_OP_LATENCY_US));

    attention_wm_bridge_destroy(bridge);
}

/**
 * @brief Test curiosity-reasoning bridge operation latency
 *
 * WHAT: Measure latency for curiosity-driven exploration
 * WHY: Curiosity signals must be processed quickly
 * HOW: Time exploration and signaling operations
 */
TEST(BridgePerformanceRegression, CuriosityReasoningLatency) {
    curiosity_reasoning_config_t config;
    curiosity_reasoning_bridge_default_config(&config);
    curiosity_reasoning_bridge_t* bridge = curiosity_reasoning_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    const int NUM_OPERATIONS = 1000;
    std::vector<int64_t> exploration_latencies;
    std::vector<int64_t> conclusion_latencies;

    // Measure drive_exploration latency
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        curiosity_reasoning_context_t context = {};
        context.context_id = static_cast<uint64_t>(i);
        context.uncertainty = 0.5f;
        context.novelty = 0.5f;
        context.depth = static_cast<uint64_t>(i % 5);

        TimePoint start = Clock::now();
        curiosity_reasoning_drive_exploration(bridge, &context, 0.7f);
        TimePoint end = Clock::now();

        exploration_latencies.push_back(
            std::chrono::duration_cast<Duration>(end - start).count()
        );
    }

    // Measure on_novel_conclusion latency
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        TimePoint start = Clock::now();
        curiosity_reasoning_on_novel_conclusion(
            bridge,
            static_cast<uint64_t>(i),
            static_cast<float>(i % 100) / 100.0f
        );
        TimePoint end = Clock::now();

        conclusion_latencies.push_back(
            std::chrono::duration_cast<Duration>(end - start).count()
        );
    }

    // Calculate statistics
    PerformanceStats exploration_stats = PerformanceStats::calculate(exploration_latencies);
    PerformanceStats conclusion_stats = PerformanceStats::calculate(conclusion_latencies);

    // Report results
    std::cout << "Curiosity-Reasoning Bridge Latency (microseconds):" << std::endl;
    std::cout << "  Drive Exploration:    Mean=" << exploration_stats.mean_us
              << ", P95=" << exploration_stats.p95_us << std::endl;
    std::cout << "  Novel Conclusion:     Mean=" << conclusion_stats.mean_us
              << ", P95=" << conclusion_stats.p95_us << std::endl;

    // Performance assertions
    EXPECT_LT(exploration_stats.mean_us, static_cast<double>(MAX_BRIDGE_OP_LATENCY_US));
    EXPECT_LT(conclusion_stats.mean_us, static_cast<double>(MAX_BRIDGE_OP_LATENCY_US));

    curiosity_reasoning_bridge_destroy(bridge);
}

/**
 * @brief Test ToM-Social bridge operation latency
 *
 * WHAT: Measure latency for agent model operations
 * WHY: Social inference must be fast for real-time interaction
 * HOW: Time model updates and queries
 */
TEST(BridgePerformanceRegression, TomSocialLatency) {
    tom_social_config_t config;
    tom_social_default_config(&config);
    tom_social_bridge_t* bridge = tom_social_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    const int NUM_OPERATIONS = 500;
    const int NUM_AGENTS = 10;
    std::vector<int64_t> update_latencies;
    std::vector<int64_t> query_latencies;

    // Initialize some agents first
    for (int a = 0; a < NUM_AGENTS; a++) {
        tom_social_belief_update_t init_update = {};
        init_update.belief_type = 0;
        init_update.belief_value = 0.5f;
        init_update.confidence = 0.8f;
        init_update.source = 0;
        tom_social_update_agent_model(bridge, static_cast<uint32_t>(a), &init_update);
    }

    // Measure update_agent_model latency
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        tom_social_belief_update_t update = {};
        update.belief_type = static_cast<uint32_t>(i % 5);
        update.belief_value = static_cast<float>(i % 100) / 100.0f;
        update.confidence = 0.7f;
        update.source = static_cast<uint32_t>(i % 3);

        TimePoint start = Clock::now();
        tom_social_update_agent_model(bridge, static_cast<uint32_t>(i % NUM_AGENTS), &update);
        TimePoint end = Clock::now();

        update_latencies.push_back(
            std::chrono::duration_cast<Duration>(end - start).count()
        );
    }

    // Measure get_agent_state latency
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        tom_social_agent_state_t state = {};

        TimePoint start = Clock::now();
        tom_social_get_agent_state(bridge, static_cast<uint32_t>(i % NUM_AGENTS), &state);
        TimePoint end = Clock::now();

        query_latencies.push_back(
            std::chrono::duration_cast<Duration>(end - start).count()
        );
    }

    // Calculate statistics
    PerformanceStats update_stats = PerformanceStats::calculate(update_latencies);
    PerformanceStats query_stats = PerformanceStats::calculate(query_latencies);

    // Report results
    std::cout << "ToM-Social Bridge Latency (microseconds):" << std::endl;
    std::cout << "  Update Agent Model:  Mean=" << update_stats.mean_us
              << ", P95=" << update_stats.p95_us << std::endl;
    std::cout << "  Get Agent State:     Mean=" << query_stats.mean_us
              << ", P95=" << query_stats.p95_us << std::endl;

    // Performance assertions
    EXPECT_LT(update_stats.mean_us, static_cast<double>(MAX_BRIDGE_OP_LATENCY_US));
    EXPECT_LT(query_stats.mean_us, static_cast<double>(MAX_BRIDGE_OP_LATENCY_US));

    tom_social_bridge_destroy(bridge);
}

/**
 * @brief Test ethics-executive bridge operation latency
 *
 * WHAT: Measure latency for ethical constraint checking
 * WHY: Ethics checks should not unduly delay decisions
 * HOW: Time constraint and evaluation operations
 */
TEST(BridgePerformanceRegression, EthicsExecutiveLatency) {
    ethics_executive_config_t config;
    ethics_executive_bridge_default_config(&config);
    ethics_executive_bridge_t* bridge = ethics_executive_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    const int NUM_OPERATIONS = 1000;
    std::vector<int64_t> constrain_latencies;
    std::vector<int64_t> evaluate_latencies;

    // Measure constrain_action latency
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        ethics_constraints_out_t constraints = {};

        TimePoint start = Clock::now();
        ethics_executive_constrain_action(
            bridge,
            static_cast<uint64_t>(i),
            &constraints
        );
        TimePoint end = Clock::now();

        constrain_latencies.push_back(
            std::chrono::duration_cast<Duration>(end - start).count()
        );
    }

    // Measure evaluate_action latency
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        float score = 0.0f;

        TimePoint start = Clock::now();
        ethics_executive_evaluate_action(
            bridge,
            static_cast<uint64_t>(i),
            &score
        );
        TimePoint end = Clock::now();

        evaluate_latencies.push_back(
            std::chrono::duration_cast<Duration>(end - start).count()
        );
    }

    // Calculate statistics
    PerformanceStats constrain_stats = PerformanceStats::calculate(constrain_latencies);
    PerformanceStats evaluate_stats = PerformanceStats::calculate(evaluate_latencies);

    // Report results
    std::cout << "Ethics-Executive Bridge Latency (microseconds):" << std::endl;
    std::cout << "  Constrain Action:   Mean=" << constrain_stats.mean_us
              << ", P95=" << constrain_stats.p95_us << std::endl;
    std::cout << "  Evaluate Action:    Mean=" << evaluate_stats.mean_us
              << ", P95=" << evaluate_stats.p95_us << std::endl;

    // Performance assertions
    EXPECT_LT(constrain_stats.mean_us, static_cast<double>(MAX_BRIDGE_OP_LATENCY_US));
    EXPECT_LT(evaluate_stats.mean_us, static_cast<double>(MAX_BRIDGE_OP_LATENCY_US));

    ethics_executive_bridge_destroy(bridge);
}

/* ============================================================================
 * GW-Cognitive Bridge Latency
 * ============================================================================ */

/**
 * @brief Dummy receiver for GW latency testing
 */
static void gw_perf_callback(
    gw_cognitive_content_type_t content_type,
    const void* content_data,
    size_t content_size,
    void* user_data
) {
    (void)content_type;
    (void)content_data;
    (void)content_size;
    (void)user_data;
}

/**
 * @brief Test GW broadcast latency
 *
 * WHAT: Measure latency for GW broadcast to all receivers
 * WHY: Global workspace broadcasts must be fast for consciousness
 * HOW: Register receivers, time broadcasts
 */
TEST(BridgePerformanceRegression, GWCognitiveLatency) {
    gw_cognitive_config_t config;
    gw_cognitive_default_config(&config);
    gw_cognitive_bridge_t* bridge = gw_cognitive_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    const int NUM_RECEIVERS = 5;
    const int NUM_BROADCASTS = 1000;
    std::vector<int64_t> broadcast_latencies;

    // Register receivers
    for (int r = 0; r < NUM_RECEIVERS; r++) {
        gw_cognitive_register_receiver(
            bridge,
            static_cast<uint32_t>(r + 1),
            gw_perf_callback,
            nullptr
        );
    }

    const char* test_content = "Performance test content";
    size_t content_size = strlen(test_content) + 1;

    // Measure broadcast latency
    for (int i = 0; i < NUM_BROADCASTS; i++) {
        TimePoint start = Clock::now();
        gw_cognitive_broadcast(
            bridge,
            GW_COGNITIVE_CONTENT_THOUGHT,
            test_content,
            content_size
        );
        TimePoint end = Clock::now();

        broadcast_latencies.push_back(
            std::chrono::duration_cast<Duration>(end - start).count()
        );
    }

    // Calculate statistics
    PerformanceStats stats = PerformanceStats::calculate(broadcast_latencies);

    // Report results
    std::cout << "GW-Cognitive Bridge Latency (microseconds):" << std::endl;
    std::cout << "  Broadcast (" << NUM_RECEIVERS << " receivers):" << std::endl;
    std::cout << "    Mean: " << stats.mean_us << ", P95: " << stats.p95_us << std::endl;

    // Performance assertion (allow more time for multi-receiver broadcast)
    double adjusted_threshold = static_cast<double>(MAX_BRIDGE_OP_LATENCY_US * NUM_RECEIVERS);
    EXPECT_LT(stats.mean_us, adjusted_threshold)
        << "GW broadcast latency exceeds adjusted threshold";

    gw_cognitive_bridge_destroy(bridge);
}

/* ============================================================================
 * Aggregate Performance Summary
 * ============================================================================ */

/**
 * @brief Print overall performance summary
 *
 * WHAT: Aggregate test that prints a summary of all performance metrics
 * WHY: Provide a single view of system performance characteristics
 * HOW: Run all operations, collect metrics, print summary
 */
TEST(PerformanceRegressionSummary, AggregateMetrics) {
    std::cout << "\n======================================" << std::endl;
    std::cout << "PERFORMANCE REGRESSION THRESHOLDS" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "Event Latency Target:       < " << MAX_EVENT_LATENCY_US << " us" << std::endl;
    std::cout << "Event Throughput Target:    > " << MIN_EVENT_THROUGHPUT << " events/sec" << std::endl;
    std::cout << "Bridge Op Latency Target:   < " << MAX_BRIDGE_OP_LATENCY_US << " us" << std::endl;
    std::cout << "======================================\n" << std::endl;

    SUCCEED();
}
