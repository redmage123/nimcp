//=============================================================================
// test_thalamic_router_integration.cpp - Comprehensive Thalamic Router Integration Tests
//=============================================================================
/**
 * @file test_thalamic_router_integration.cpp
 * @brief Integration tests for attention-gated neural routing system
 *
 * WHAT: Tests router creation, route registration, priority routing,
 *       message queuing, cross-module communication, and bio-async integration
 * WHY:  Verify thalamic-style attention-gated signal routing works correctly
 * HOW:  GTest framework testing routing infrastructure
 *
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "middleware/routing/nimcp_thalamic_router.h"
#include "middleware/routing/nimcp_thalamic_router_fep_bridge.h"

//=============================================================================
// Callback Infrastructure
//=============================================================================

struct TestCallbackContext {
    uint32_t dest_id_received;
    float* signal_received;
    uint32_t size_received;
    float attention_received;
    std::atomic<int> call_count;

    TestCallbackContext() : dest_id_received(0), signal_received(nullptr),
                            size_received(0), attention_received(0.0f),
                            call_count(0) {}

    ~TestCallbackContext() {
        if (signal_received) {
            delete[] signal_received;
            signal_received = nullptr;
        }
    }

    void reset() {
        dest_id_received = 0;
        if (signal_received) {
            delete[] signal_received;
            signal_received = nullptr;
        }
        size_received = 0;
        attention_received = 0.0f;
        call_count = 0;
    }
};

static void test_delivery_callback(uint32_t dest_id, const float* signal,
                                   uint32_t size, float attention, void* user_data) {
    TestCallbackContext* ctx = static_cast<TestCallbackContext*>(user_data);
    ctx->dest_id_received = dest_id;
    ctx->size_received = size;
    ctx->attention_received = attention;
    ctx->call_count++;

    // Copy signal data
    if (ctx->signal_received) {
        delete[] ctx->signal_received;
    }
    ctx->signal_received = new float[size];
    for (uint32_t i = 0; i < size; i++) {
        ctx->signal_received[i] = signal[i];
    }
}

//=============================================================================
// Test Fixtures
//=============================================================================

class ThalamicRouterIntegrationTest : public NimcpTestBase {
protected:
    thalamic_router_t* router = nullptr;
    thalamic_router_config_t config;
    TestCallbackContext ctx1, ctx2, ctx3;

    void SetUp() override {
        NimcpTestBase::SetUp();
        config = thalamic_router_default_config();
        config.enable_attention_gating = true;
        config.enable_priority_routing = true;
        config.enable_statistics = true;
        config.max_queue_size = 100;
        router = thalamic_router_create(&config);
    }

    void TearDown() override {
        ctx1.reset();
        ctx2.reset();
        ctx3.reset();
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    bool FloatNear(float a, float b, float eps = 0.001f) {
        return std::fabs(a - b) < eps;
    }
};

//=============================================================================
// Router Creation and Lifecycle Tests
//=============================================================================

TEST_F(ThalamicRouterIntegrationTest, CreateWithDefaultConfig) {
    ASSERT_NE(router, nullptr);
}

TEST_F(ThalamicRouterIntegrationTest, CreateWithCustomConfig) {
    thalamic_router_config_t custom_config = thalamic_router_default_config();
    custom_config.max_queue_size = 500;
    custom_config.max_destinations = 8;
    custom_config.enable_attention_gating = false;
    custom_config.enable_learning = true;

    thalamic_router_t* custom_router = thalamic_router_create(&custom_config);
    ASSERT_NE(custom_router, nullptr);
    thalamic_router_destroy(custom_router);
}

TEST_F(ThalamicRouterIntegrationTest, CreateWithNullConfigFails) {
    thalamic_router_t* null_router = thalamic_router_create(nullptr);
    EXPECT_EQ(null_router, nullptr);
}

TEST_F(ThalamicRouterIntegrationTest, DestroyNullSafe) {
    thalamic_router_destroy(nullptr);
    // Should not crash
}

TEST_F(ThalamicRouterIntegrationTest, DefaultConfigValues) {
    thalamic_router_config_t def_config = thalamic_router_default_config();
    EXPECT_EQ(def_config.max_queue_size, THALAMIC_MAX_QUEUE_SIZE);
    EXPECT_EQ(def_config.max_destinations, THALAMIC_MAX_DESTINATIONS);
    EXPECT_TRUE(def_config.enable_attention_gating);
    EXPECT_TRUE(def_config.enable_priority_routing);
    EXPECT_TRUE(def_config.enable_statistics);
    EXPECT_FLOAT_EQ(def_config.min_attention_threshold, 0.01f);
}

//=============================================================================
// Route Registration and Lookup Tests
//=============================================================================

TEST_F(ThalamicRouterIntegrationTest, SetCallbackForDestination) {
    ASSERT_NE(router, nullptr);

    bool result = thalamic_router_set_callback(router, 100, test_delivery_callback, &ctx1);
    EXPECT_TRUE(result);
}

TEST_F(ThalamicRouterIntegrationTest, SetMultipleCallbacks) {
    ASSERT_NE(router, nullptr);

    bool result1 = thalamic_router_set_callback(router, 100, test_delivery_callback, &ctx1);
    bool result2 = thalamic_router_set_callback(router, 101, test_delivery_callback, &ctx2);
    bool result3 = thalamic_router_set_callback(router, 102, test_delivery_callback, &ctx3);

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
    EXPECT_TRUE(result3);
}

TEST_F(ThalamicRouterIntegrationTest, SetCallbackNullRouterFails) {
    bool result = thalamic_router_set_callback(nullptr, 100, test_delivery_callback, &ctx1);
    EXPECT_FALSE(result);
}

TEST_F(ThalamicRouterIntegrationTest, SetAttentionForRoute) {
    ASSERT_NE(router, nullptr);

    bool result = thalamic_router_set_attention(router, 1, 100, 0.75f);
    EXPECT_TRUE(result);

    float attention = 0.0f;
    bool got = thalamic_router_get_attention(router, 1, 100, &attention);
    EXPECT_TRUE(got);
    // Note: Combined with topdown_weight, may not be exactly 0.75
}

TEST_F(ThalamicRouterIntegrationTest, GetAttentionForUnsetRouteReturnsDefault) {
    ASSERT_NE(router, nullptr);

    // Even for unset routes, the router may return a default attention value
    float attention = 0.0f;
    bool result = thalamic_router_get_attention(router, 999, 999, &attention);
    // The router may return true with a default value or false
    // Just verify the call completes without crashing
    if (result) {
        EXPECT_GE(attention, 0.0f);
        EXPECT_LE(attention, 1.0f);
    }
}

//=============================================================================
// Priority-Based Routing Tests
//=============================================================================

TEST_F(ThalamicRouterIntegrationTest, HighPriorityBypassesQueue) {
    ASSERT_NE(router, nullptr);
    thalamic_router_set_callback(router, 100, test_delivery_callback, &ctx1);

    // Disable attention gating for basic routing test
    thalamic_router_config_t no_gating_config = thalamic_router_default_config();
    no_gating_config.min_attention_threshold = 0.0f;
    thalamic_router_t* test_router = thalamic_router_create(&no_gating_config);
    ASSERT_NE(test_router, nullptr);

    TestCallbackContext local_ctx;
    thalamic_router_set_callback(test_router, 100, test_delivery_callback, &local_ctx);

    uint32_t dest = 100;
    float data[] = {1.0f, 2.0f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 2, SIGNAL_PRIORITY_HIGH);

    bool result = thalamic_router_route_signal(test_router, signal);
    EXPECT_TRUE(result);

    // High priority should deliver immediately
    EXPECT_EQ(local_ctx.call_count.load(), 1);

    routing_stats_t stats;
    thalamic_router_get_stats(test_router, &stats);
    EXPECT_EQ(stats.signals_bypassed, 1u);
    EXPECT_EQ(stats.queue_depth, 0u);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(test_router);
}

TEST_F(ThalamicRouterIntegrationTest, NormalPriorityEnqueues) {
    ASSERT_NE(router, nullptr);
    thalamic_router_set_callback(router, 100, test_delivery_callback, &ctx1);

    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);

    bool result = thalamic_router_route_signal(router, signal);
    EXPECT_TRUE(result);

    // Normal priority should enqueue, not deliver immediately
    EXPECT_EQ(ctx1.call_count.load(), 0);

    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);
    EXPECT_EQ(stats.queue_depth, 1u);

    thalamic_router_free_signal(signal);
}

TEST_F(ThalamicRouterIntegrationTest, PriorityOrderingInQueue) {
    ASSERT_NE(router, nullptr);

    thalamic_router_config_t no_gating_config = thalamic_router_default_config();
    no_gating_config.enable_attention_gating = false;
    thalamic_router_t* test_router = thalamic_router_create(&no_gating_config);
    ASSERT_NE(test_router, nullptr);

    TestCallbackContext local_ctx;
    thalamic_router_set_callback(test_router, 100, test_delivery_callback, &local_ctx);

    uint32_t dest = 100;

    // Enqueue low priority first
    float data_low[] = {1.0f};
    routed_signal_t* sig_low = thalamic_router_create_signal(1, &dest, 1, data_low, 1, SIGNAL_PRIORITY_LOW);
    thalamic_router_route_signal(test_router, sig_low);

    // Enqueue normal priority second (should be processed first)
    float data_normal[] = {2.0f};
    routed_signal_t* sig_normal = thalamic_router_create_signal(1, &dest, 1, data_normal, 1, SIGNAL_PRIORITY_NORMAL);
    thalamic_router_route_signal(test_router, sig_normal);

    // Process one signal
    uint32_t num_processed = 0;
    thalamic_router_process_queue(test_router, 1, &num_processed);

    // Should receive normal priority (2.0f) first
    ASSERT_EQ(local_ctx.call_count.load(), 1);
    ASSERT_NE(local_ctx.signal_received, nullptr);
    EXPECT_FLOAT_EQ(local_ctx.signal_received[0], 2.0f);

    thalamic_router_free_signal(sig_low);
    thalamic_router_free_signal(sig_normal);
    thalamic_router_destroy(test_router);
}

TEST_F(ThalamicRouterIntegrationTest, BypassFlagOverridesPriority) {
    ASSERT_NE(router, nullptr);

    thalamic_router_config_t no_gating_config = thalamic_router_default_config();
    no_gating_config.min_attention_threshold = 0.0f;
    thalamic_router_t* test_router = thalamic_router_create(&no_gating_config);
    ASSERT_NE(test_router, nullptr);

    TestCallbackContext local_ctx;
    thalamic_router_set_callback(test_router, 100, test_delivery_callback, &local_ctx);

    uint32_t dest = 100;
    float data[] = {5.0f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_LOW);
    signal->bypass_queue = true;

    bool result = thalamic_router_route_signal(test_router, signal);
    EXPECT_TRUE(result);

    // bypass_queue should deliver immediately even for low priority
    EXPECT_EQ(local_ctx.call_count.load(), 1);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(test_router);
}

//=============================================================================
// Message Queuing and Delivery Tests
//=============================================================================

TEST_F(ThalamicRouterIntegrationTest, ProcessQueueDeliversSignals) {
    ASSERT_NE(router, nullptr);

    thalamic_router_config_t no_gating_config = thalamic_router_default_config();
    no_gating_config.enable_attention_gating = false;
    thalamic_router_t* test_router = thalamic_router_create(&no_gating_config);
    ASSERT_NE(test_router, nullptr);

    TestCallbackContext local_ctx;
    thalamic_router_set_callback(test_router, 100, test_delivery_callback, &local_ctx);

    uint32_t dest = 100;
    float data[] = {3.14f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
    thalamic_router_route_signal(test_router, signal);

    EXPECT_EQ(local_ctx.call_count.load(), 0);

    uint32_t num_processed = 0;
    bool result = thalamic_router_process_queue(test_router, 10, &num_processed);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_processed, 1u);

    EXPECT_EQ(local_ctx.call_count.load(), 1);
    ASSERT_NE(local_ctx.signal_received, nullptr);
    EXPECT_FLOAT_EQ(local_ctx.signal_received[0], 3.14f);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(test_router);
}

TEST_F(ThalamicRouterIntegrationTest, ProcessQueueMaxLimit) {
    ASSERT_NE(router, nullptr);

    thalamic_router_config_t no_gating_config = thalamic_router_default_config();
    no_gating_config.min_attention_threshold = 0.0f;
    thalamic_router_t* test_router = thalamic_router_create(&no_gating_config);
    ASSERT_NE(test_router, nullptr);

    TestCallbackContext local_ctx;
    thalamic_router_set_callback(test_router, 100, test_delivery_callback, &local_ctx);

    uint32_t dest = 100;
    float data[] = {1.0f};

    // Enqueue 5 signals
    for (int i = 0; i < 5; i++) {
        routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
        thalamic_router_route_signal(test_router, signal);
        thalamic_router_free_signal(signal);
    }

    // Process only 2
    uint32_t num_processed = 0;
    thalamic_router_process_queue(test_router, 2, &num_processed);
    EXPECT_EQ(num_processed, 2u);
    EXPECT_EQ(local_ctx.call_count.load(), 2);

    routing_stats_t stats;
    thalamic_router_get_stats(test_router, &stats);
    EXPECT_EQ(stats.queue_depth, 3u);

    thalamic_router_destroy(test_router);
}

TEST_F(ThalamicRouterIntegrationTest, ProcessEmptyQueue) {
    ASSERT_NE(router, nullptr);

    uint32_t num_processed = 0;
    bool result = thalamic_router_process_queue(router, 10, &num_processed);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_processed, 0u);
}

TEST_F(ThalamicRouterIntegrationTest, ClearQueueRemovesAllSignals) {
    ASSERT_NE(router, nullptr);

    uint32_t dest = 100;
    float data[] = {1.0f};

    // Enqueue signals
    for (int i = 0; i < 5; i++) {
        routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
        thalamic_router_route_signal(router, signal);
        thalamic_router_free_signal(signal);
    }

    thalamic_router_clear_queue(router);

    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);
    EXPECT_EQ(stats.queue_depth, 0u);
}

TEST_F(ThalamicRouterIntegrationTest, QueueOverflowDropsSignals) {
    thalamic_router_config_t small_queue_config = thalamic_router_default_config();
    small_queue_config.max_queue_size = 2;
    thalamic_router_t* small_router = thalamic_router_create(&small_queue_config);
    ASSERT_NE(small_router, nullptr);

    uint32_t dest = 100;
    float data[] = {1.0f};

    // Fill queue
    routed_signal_t* sig1 = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
    routed_signal_t* sig2 = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
    thalamic_router_route_signal(small_router, sig1);
    thalamic_router_route_signal(small_router, sig2);

    // Overflow
    routed_signal_t* sig3 = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
    bool result = thalamic_router_route_signal(small_router, sig3);
    EXPECT_FALSE(result);

    routing_stats_t stats;
    thalamic_router_get_stats(small_router, &stats);
    EXPECT_EQ(stats.signals_dropped, 1u);

    thalamic_router_free_signal(sig1);
    thalamic_router_free_signal(sig2);
    thalamic_router_free_signal(sig3);
    thalamic_router_destroy(small_router);
}

//=============================================================================
// Cross-Module Communication Tests
//=============================================================================

TEST_F(ThalamicRouterIntegrationTest, MultipleDestinationsAllReceive) {
    ASSERT_NE(router, nullptr);

    thalamic_router_config_t no_gating_config = thalamic_router_default_config();
    no_gating_config.min_attention_threshold = 0.0f;
    thalamic_router_t* test_router = thalamic_router_create(&no_gating_config);
    ASSERT_NE(test_router, nullptr);

    TestCallbackContext local_ctx1, local_ctx2, local_ctx3;
    thalamic_router_set_callback(test_router, 100, test_delivery_callback, &local_ctx1);
    thalamic_router_set_callback(test_router, 101, test_delivery_callback, &local_ctx2);
    thalamic_router_set_callback(test_router, 102, test_delivery_callback, &local_ctx3);

    uint32_t dests[] = {100, 101, 102};
    float data[] = {7.77f};
    routed_signal_t* signal = thalamic_router_create_signal(1, dests, 3, data, 1, SIGNAL_PRIORITY_HIGH);

    thalamic_router_route_signal(test_router, signal);

    EXPECT_EQ(local_ctx1.call_count.load(), 1);
    EXPECT_EQ(local_ctx2.call_count.load(), 1);
    EXPECT_EQ(local_ctx3.call_count.load(), 1);

    EXPECT_EQ(local_ctx1.dest_id_received, 100u);
    EXPECT_EQ(local_ctx2.dest_id_received, 101u);
    EXPECT_EQ(local_ctx3.dest_id_received, 102u);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(test_router);
}

TEST_F(ThalamicRouterIntegrationTest, SignalDataCopiedCorrectly) {
    ASSERT_NE(router, nullptr);

    thalamic_router_config_t no_gating_config = thalamic_router_default_config();
    no_gating_config.enable_attention_gating = false;
    thalamic_router_t* test_router = thalamic_router_create(&no_gating_config);
    ASSERT_NE(test_router, nullptr);

    TestCallbackContext local_ctx;
    thalamic_router_set_callback(test_router, 100, test_delivery_callback, &local_ctx);

    uint32_t dest = 100;
    float data[] = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 5, SIGNAL_PRIORITY_HIGH);

    thalamic_router_route_signal(test_router, signal);

    ASSERT_EQ(local_ctx.call_count.load(), 1);
    ASSERT_NE(local_ctx.signal_received, nullptr);
    EXPECT_EQ(local_ctx.size_received, 5u);

    for (int i = 0; i < 5; i++) {
        EXPECT_FLOAT_EQ(local_ctx.signal_received[i], data[i]);
    }

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(test_router);
}

TEST_F(ThalamicRouterIntegrationTest, NoCallbackNoDelivery) {
    ASSERT_NE(router, nullptr);

    // Don't register callback for dest 200
    uint32_t dest = 200;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);

    bool result = thalamic_router_route_signal(router, signal);
    EXPECT_FALSE(result);  // No callback registered

    thalamic_router_free_signal(signal);
}

//=============================================================================
// Attention Gating Tests
//=============================================================================

TEST_F(ThalamicRouterIntegrationTest, AttentionModulatesSignalStrength) {
    ASSERT_NE(router, nullptr);

    thalamic_router_set_callback(router, 100, test_delivery_callback, &ctx1);
    thalamic_router_set_attention(router, 1, 100, 0.5f);

    uint32_t dest = 100;
    float data[] = {10.0f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);

    thalamic_router_route_signal(router, signal);

    ASSERT_EQ(ctx1.call_count.load(), 1);
    ASSERT_NE(ctx1.signal_received, nullptr);

    // Signal should be scaled by attention (combined with topdown_weight)
    EXPECT_LT(ctx1.signal_received[0], 10.0f);
    EXPECT_GT(ctx1.signal_received[0], 0.0f);

    thalamic_router_free_signal(signal);
}

TEST_F(ThalamicRouterIntegrationTest, BelowThresholdFiltersSignal) {
    thalamic_router_config_t threshold_config = thalamic_router_default_config();
    threshold_config.min_attention_threshold = 0.5f;
    thalamic_router_t* threshold_router = thalamic_router_create(&threshold_config);
    ASSERT_NE(threshold_router, nullptr);

    TestCallbackContext local_ctx;
    thalamic_router_set_callback(threshold_router, 100, test_delivery_callback, &local_ctx);
    thalamic_router_set_attention(threshold_router, 1, 100, 0.3f);  // Below threshold

    uint32_t dest = 100;
    float data[] = {10.0f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);

    thalamic_router_route_signal(threshold_router, signal);

    // Should be filtered
    EXPECT_EQ(local_ctx.call_count.load(), 0);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(threshold_router);
}

TEST_F(ThalamicRouterIntegrationTest, DisabledAttentionGatingPassesThrough) {
    thalamic_router_config_t no_gating_config = thalamic_router_default_config();
    no_gating_config.enable_attention_gating = false;
    thalamic_router_t* no_gating_router = thalamic_router_create(&no_gating_config);
    ASSERT_NE(no_gating_router, nullptr);

    TestCallbackContext local_ctx;
    thalamic_router_set_callback(no_gating_router, 100, test_delivery_callback, &local_ctx);

    uint32_t dest = 100;
    float data[] = {10.0f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);

    thalamic_router_route_signal(no_gating_router, signal);

    ASSERT_EQ(local_ctx.call_count.load(), 1);
    ASSERT_NE(local_ctx.signal_received, nullptr);
    EXPECT_FLOAT_EQ(local_ctx.signal_received[0], 10.0f);  // Unmodulated

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(no_gating_router);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ThalamicRouterIntegrationTest, StatsTrackSignalsRouted) {
    ASSERT_NE(router, nullptr);

    thalamic_router_config_t no_gating_config = thalamic_router_default_config();
    no_gating_config.min_attention_threshold = 0.0f;
    thalamic_router_t* test_router = thalamic_router_create(&no_gating_config);
    ASSERT_NE(test_router, nullptr);

    TestCallbackContext local_ctx;
    thalamic_router_set_callback(test_router, 100, test_delivery_callback, &local_ctx);

    uint32_t dest = 100;
    float data[] = {1.0f};

    // Route high priority (bypassed)
    routed_signal_t* sig1 = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);
    thalamic_router_route_signal(test_router, sig1);

    // Route normal priority (queued)
    routed_signal_t* sig2 = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
    thalamic_router_route_signal(test_router, sig2);

    routing_stats_t stats;
    thalamic_router_get_stats(test_router, &stats);

    EXPECT_EQ(stats.signals_routed, 1u);  // Only bypassed counts as routed
    EXPECT_EQ(stats.signals_bypassed, 1u);

    thalamic_router_free_signal(sig1);
    thalamic_router_free_signal(sig2);
    thalamic_router_destroy(test_router);
}

TEST_F(ThalamicRouterIntegrationTest, ResetStatsClearsCounters) {
    ASSERT_NE(router, nullptr);

    thalamic_router_config_t no_gating_config = thalamic_router_default_config();
    no_gating_config.min_attention_threshold = 0.0f;
    thalamic_router_t* test_router = thalamic_router_create(&no_gating_config);
    ASSERT_NE(test_router, nullptr);

    TestCallbackContext local_ctx;
    thalamic_router_set_callback(test_router, 100, test_delivery_callback, &local_ctx);

    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);
    thalamic_router_route_signal(test_router, signal);

    thalamic_router_reset_stats(test_router);

    routing_stats_t stats;
    thalamic_router_get_stats(test_router, &stats);
    EXPECT_EQ(stats.signals_routed, 0u);
    EXPECT_EQ(stats.signals_bypassed, 0u);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(test_router);
}

TEST_F(ThalamicRouterIntegrationTest, GetStatsNullRouterFails) {
    routing_stats_t stats;
    bool result = thalamic_router_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(ThalamicRouterIntegrationTest, GetStatsNullStatsFails) {
    ASSERT_NE(router, nullptr);
    bool result = thalamic_router_get_stats(router, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Signal Helper Tests
//=============================================================================

TEST_F(ThalamicRouterIntegrationTest, CreateSignalAllocatesCorrectly) {
    uint32_t dests[] = {100, 101};
    float data[] = {1.0f, 2.0f, 3.0f};

    routed_signal_t* signal = thalamic_router_create_signal(42, dests, 2, data, 3, SIGNAL_PRIORITY_NORMAL);
    ASSERT_NE(signal, nullptr);

    EXPECT_EQ(signal->source_id, 42u);
    EXPECT_EQ(signal->num_dests, 2u);
    EXPECT_EQ(signal->signal_size, 3u);
    EXPECT_EQ(signal->priority, SIGNAL_PRIORITY_NORMAL);
    EXPECT_FLOAT_EQ(signal->attention_weight, 1.0f);
    EXPECT_FALSE(signal->bypass_queue);

    EXPECT_EQ(signal->dest_ids[0], 100u);
    EXPECT_EQ(signal->dest_ids[1], 101u);
    EXPECT_FLOAT_EQ(signal->signal_data[0], 1.0f);
    EXPECT_FLOAT_EQ(signal->signal_data[1], 2.0f);
    EXPECT_FLOAT_EQ(signal->signal_data[2], 3.0f);

    thalamic_router_free_signal(signal);
}

TEST_F(ThalamicRouterIntegrationTest, CreateSignalNullDestsFails) {
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(1, nullptr, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
    EXPECT_EQ(signal, nullptr);
}

TEST_F(ThalamicRouterIntegrationTest, CreateSignalZeroDestsFails) {
    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 0, data, 1, SIGNAL_PRIORITY_NORMAL);
    EXPECT_EQ(signal, nullptr);
}

TEST_F(ThalamicRouterIntegrationTest, CreateSignalNullDataFails) {
    uint32_t dest = 100;
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, nullptr, 1, SIGNAL_PRIORITY_NORMAL);
    EXPECT_EQ(signal, nullptr);
}

TEST_F(ThalamicRouterIntegrationTest, CreateSignalZeroSizeFails) {
    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 0, SIGNAL_PRIORITY_NORMAL);
    EXPECT_EQ(signal, nullptr);
}

TEST_F(ThalamicRouterIntegrationTest, FreeSignalNullSafe) {
    thalamic_router_free_signal(nullptr);
    // Should not crash
}

//=============================================================================
// Imagination Routing Tests
//=============================================================================

TEST_F(ThalamicRouterIntegrationTest, SetImaginationAttention) {
    ASSERT_NE(router, nullptr);

    bool result = thalamic_router_set_imagination_attention(router, 0.8f);
    EXPECT_TRUE(result);

    float attention = thalamic_router_get_imagination_attention(router);
    EXPECT_FLOAT_EQ(attention, 0.8f);
}

TEST_F(ThalamicRouterIntegrationTest, ImaginationRoutingEnableDisable) {
    ASSERT_NE(router, nullptr);

    bool result = thalamic_router_set_imagination_routing_enabled(router, true);
    EXPECT_TRUE(result);

    bool enabled = thalamic_router_is_imagination_routing_enabled(router);
    EXPECT_TRUE(enabled);

    result = thalamic_router_set_imagination_routing_enabled(router, false);
    EXPECT_TRUE(result);

    enabled = thalamic_router_is_imagination_routing_enabled(router);
    EXPECT_FALSE(enabled);
}

//=============================================================================
// Stability Tests
//=============================================================================

TEST_F(ThalamicRouterIntegrationTest, ManySignalsStable) {
    ASSERT_NE(router, nullptr);

    thalamic_router_config_t stable_config = thalamic_router_default_config();
    stable_config.enable_attention_gating = false;
    stable_config.max_queue_size = 10000;
    thalamic_router_t* stable_router = thalamic_router_create(&stable_config);
    ASSERT_NE(stable_router, nullptr);

    TestCallbackContext local_ctx;
    thalamic_router_set_callback(stable_router, 100, test_delivery_callback, &local_ctx);

    uint32_t dest = 100;
    float data[] = {1.0f};

    // Route many signals
    for (int i = 0; i < 1000; i++) {
        routed_signal_t* signal = thalamic_router_create_signal(1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);
        bool result = thalamic_router_route_signal(stable_router, signal);
        EXPECT_TRUE(result);
        thalamic_router_free_signal(signal);
    }

    EXPECT_EQ(local_ctx.call_count.load(), 1000);

    routing_stats_t stats;
    thalamic_router_get_stats(stable_router, &stats);
    EXPECT_EQ(stats.signals_routed, 1000u);

    thalamic_router_destroy(stable_router);
}

TEST_F(ThalamicRouterIntegrationTest, ClearQueueNullSafe) {
    thalamic_router_clear_queue(nullptr);
    // Should not crash
}

TEST_F(ThalamicRouterIntegrationTest, ResetStatsNullSafe) {
    thalamic_router_reset_stats(nullptr);
    // Should not crash
}
