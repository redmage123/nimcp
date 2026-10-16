//=============================================================================
// test_thalamic_router.cpp - Comprehensive Thalamic Router Tests
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/routing/nimcp_thalamic_router.h"

/**
 * WHAT: Comprehensive test suite for thalamic-style routing
 * WHY:  Ensure attention-gated signal routing works correctly
 * HOW:  Unit tests for all 13 functions, callbacks, priority routing
 */

// Test context for callbacks
struct CallbackContext {
    uint32_t dest_id_received;
    float* signal_received;
    uint32_t size_received;
    float attention_received;
    int call_count;

    CallbackContext() : dest_id_received(0), signal_received(nullptr),
                        size_received(0), attention_received(0.0f),
                        call_count(0) {}

    ~CallbackContext() {
        if (signal_received) delete[] signal_received;
    }
};

// Test callback function
static void test_callback(uint32_t dest_id, const float* signal,
                         uint32_t size, float attention, void* user_data) {
    CallbackContext* ctx = (CallbackContext*)user_data;
    ctx->dest_id_received = dest_id;
    ctx->size_received = size;
    ctx->attention_received = attention;
    ctx->call_count++;

    // Copy signal data
    if (ctx->signal_received) delete[] ctx->signal_received;
    ctx->signal_received = new float[size];
    for (uint32_t i = 0; i < size; i++) {
        ctx->signal_received[i] = signal[i];
    }
}

class ThalamicRouterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    bool FloatEquals(float a, float b, float eps = 0.001f) {
        return std::fabs(a - b) < eps;
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================
// WHAT: Test router creation and destruction
// WHY:  Verify resource management and parameter validation
// HOW:  Test config, creation, destruction

TEST_F(ThalamicRouterTest, DefaultConfig_Success) {
    thalamic_router_config_t config = thalamic_router_default_config();
    EXPECT_EQ(config.max_queue_size, THALAMIC_MAX_QUEUE_SIZE);
    EXPECT_EQ(config.max_destinations, THALAMIC_MAX_DESTINATIONS);
    EXPECT_TRUE(config.enable_attention_gating);
    EXPECT_TRUE(config.enable_priority_routing);
    EXPECT_TRUE(config.enable_statistics);
    EXPECT_FLOAT_EQ(config.min_attention_threshold, 0.01f);
    EXPECT_TRUE(config.enable_learning);
}

TEST_F(ThalamicRouterTest, Create_Success) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);
    ASSERT_NE(router, nullptr);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, Create_Success_CustomConfig) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.max_queue_size = 100;
    config.enable_attention_gating = false;
    config.enable_learning = false;

    thalamic_router_t* router = thalamic_router_create(&config);
    ASSERT_NE(router, nullptr);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, Create_Failure_NullConfig) {
    thalamic_router_t* router = thalamic_router_create(nullptr);
    EXPECT_EQ(router, nullptr);
}

TEST_F(ThalamicRouterTest, Destroy_NullSafe) {
    thalamic_router_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// SIGNAL HELPER TESTS
//=============================================================================
// WHAT: Test signal creation and destruction helpers
// WHY:  Verify signal packet allocation and cleanup
// HOW:  Create signals with various parameters

TEST_F(ThalamicRouterTest, CreateSignal_Success) {
    uint32_t dests[] = {100, 101};
    float data[] = {1.0f, 2.0f, 3.0f};

    routed_signal_t* signal = thalamic_router_create_signal(
        1, dests, 2, data, 3, SIGNAL_PRIORITY_NORMAL);

    ASSERT_NE(signal, nullptr);
    EXPECT_EQ(signal->source_id, 1);
    EXPECT_EQ(signal->num_dests, 2);
    EXPECT_EQ(signal->signal_size, 3);
    EXPECT_EQ(signal->priority, SIGNAL_PRIORITY_NORMAL);
    EXPECT_FLOAT_EQ(signal->attention_weight, 1.0f);
    EXPECT_FALSE(signal->bypass_queue);

    // Verify data copied
    EXPECT_EQ(signal->dest_ids[0], 100);
    EXPECT_EQ(signal->dest_ids[1], 101);
    EXPECT_FLOAT_EQ(signal->signal_data[0], 1.0f);
    EXPECT_FLOAT_EQ(signal->signal_data[1], 2.0f);
    EXPECT_FLOAT_EQ(signal->signal_data[2], 3.0f);

    thalamic_router_free_signal(signal);
}

TEST_F(ThalamicRouterTest, CreateSignal_Success_SingleDest) {
    uint32_t dest = 100;
    float data[] = {5.0f};

    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);

    ASSERT_NE(signal, nullptr);
    EXPECT_EQ(signal->num_dests, 1);
    EXPECT_EQ(signal->dest_ids[0], 100);

    thalamic_router_free_signal(signal);
}

TEST_F(ThalamicRouterTest, CreateSignal_Failure_NullDests) {
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, nullptr, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
    EXPECT_EQ(signal, nullptr);
}

TEST_F(ThalamicRouterTest, CreateSignal_Failure_ZeroDests) {
    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 0, data, 1, SIGNAL_PRIORITY_NORMAL);
    EXPECT_EQ(signal, nullptr);
}

TEST_F(ThalamicRouterTest, CreateSignal_Failure_NullData) {
    uint32_t dest = 100;
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, nullptr, 1, SIGNAL_PRIORITY_NORMAL);
    EXPECT_EQ(signal, nullptr);
}

TEST_F(ThalamicRouterTest, CreateSignal_Failure_ZeroSize) {
    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 0, SIGNAL_PRIORITY_NORMAL);
    EXPECT_EQ(signal, nullptr);
}

TEST_F(ThalamicRouterTest, FreeSignal_NullSafe) {
    thalamic_router_free_signal(nullptr);
    // Should not crash
}

//=============================================================================
// ROUTING OPERATION TESTS
//=============================================================================
// WHAT: Test signal routing functionality
// WHY:  Verify priority routing, queueing, and bypass behavior
// HOW:  Route signals with different priorities and configs

TEST_F(ThalamicRouterTest, RouteSignal_Success_HighPriority_Bypass) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.min_attention_threshold = 0.0f;  // Allow all signals for basic routing test
    thalamic_router_t* router = thalamic_router_create(&config);
    ASSERT_NE(router, nullptr);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);

    uint32_t dest = 100;
    float data[] = {1.0f, 2.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 2, SIGNAL_PRIORITY_HIGH);

    bool result = thalamic_router_route_signal(router, signal);
    EXPECT_TRUE(result);

    // High priority should bypass and deliver immediately
    EXPECT_EQ(ctx.call_count, 1);
    EXPECT_EQ(ctx.dest_id_received, 100);
    EXPECT_EQ(ctx.size_received, 2);

    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);
    EXPECT_EQ(stats.signals_routed, 1);
    EXPECT_EQ(stats.signals_bypassed, 1);
    EXPECT_EQ(stats.queue_depth, 0);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, RouteSignal_Success_NormalPriority_Enqueue) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);
    ASSERT_NE(router, nullptr);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);

    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);

    bool result = thalamic_router_route_signal(router, signal);
    EXPECT_TRUE(result);

    // Normal priority should enqueue (not deliver immediately)
    EXPECT_EQ(ctx.call_count, 0);

    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);
    EXPECT_EQ(stats.queue_depth, 1);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, RouteSignal_Success_BypassFlag) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.min_attention_threshold = 0.0f;  // Allow all signals for basic routing test
    thalamic_router_t* router = thalamic_router_create(&config);
    ASSERT_NE(router, nullptr);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);

    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_LOW);
    signal->bypass_queue = true;

    bool result = thalamic_router_route_signal(router, signal);
    EXPECT_TRUE(result);

    // bypass_queue flag should deliver immediately
    EXPECT_EQ(ctx.call_count, 1);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, RouteSignal_Failure_NullRouter) {
    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);

    bool result = thalamic_router_route_signal(nullptr, signal);
    EXPECT_FALSE(result);

    thalamic_router_free_signal(signal);
}

TEST_F(ThalamicRouterTest, RouteSignal_Failure_NullSignal) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    bool result = thalamic_router_route_signal(router, nullptr);
    EXPECT_FALSE(result);

    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, RouteSignal_Failure_ZeroDests) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    routed_signal_t signal = {};
    signal.num_dests = 0;

    bool result = thalamic_router_route_signal(router, &signal);
    EXPECT_FALSE(result);

    thalamic_router_destroy(router);
}

//=============================================================================
// QUEUE PROCESSING TESTS
//=============================================================================
// WHAT: Test queue processing and signal delivery
// WHY:  Verify asynchronous signal delivery
// HOW:  Enqueue signals, process queue, verify delivery

TEST_F(ThalamicRouterTest, ProcessQueue_Success) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.enable_attention_gating = false;  // Disable gating for basic routing test
    thalamic_router_t* router = thalamic_router_create(&config);
    ASSERT_NE(router, nullptr);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);

    // Enqueue normal priority signal
    uint32_t dest = 100;
    float data[] = {5.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
    thalamic_router_route_signal(router, signal);

    // Process queue
    uint32_t num_processed = 0;
    bool result = thalamic_router_process_queue(router, 10, &num_processed);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_processed, 1);

    // Signal should be delivered now
    ASSERT_EQ(ctx.call_count, 1);  // Use ASSERT to prevent segfault on null access
    EXPECT_EQ(ctx.dest_id_received, 100);
    ASSERT_NE(ctx.signal_received, nullptr);
    EXPECT_FLOAT_EQ(ctx.signal_received[0], 5.0f);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, ProcessQueue_Success_MaxSignalsLimit) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.min_attention_threshold = 0.0f;  // Allow all signals
    thalamic_router_t* router = thalamic_router_create(&config);
    ASSERT_NE(router, nullptr);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);

    // Enqueue 5 signals
    uint32_t dest = 100;
    float data[] = {1.0f};
    for (int i = 0; i < 5; i++) {
        routed_signal_t* signal = thalamic_router_create_signal(
            1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
        thalamic_router_route_signal(router, signal);
        thalamic_router_free_signal(signal);
    }

    // Process only 2 signals
    uint32_t num_processed = 0;
    thalamic_router_process_queue(router, 2, &num_processed);
    EXPECT_EQ(num_processed, 2);
    EXPECT_EQ(ctx.call_count, 2);

    // Queue should still have 3 signals
    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);
    EXPECT_EQ(stats.queue_depth, 3);

    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, ProcessQueue_Success_PriorityOrder) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.enable_attention_gating = false;  // Disable gating for routing test
    thalamic_router_t* router = thalamic_router_create(&config);
    ASSERT_NE(router, nullptr);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);

    uint32_t dest = 100;

    // Enqueue low priority
    float data_low[] = {1.0f};
    routed_signal_t* sig_low = thalamic_router_create_signal(
        1, &dest, 1, data_low, 1, SIGNAL_PRIORITY_LOW);
    thalamic_router_route_signal(router, sig_low);

    // Enqueue normal priority (should jump ahead)
    float data_normal[] = {2.0f};
    routed_signal_t* sig_normal = thalamic_router_create_signal(
        1, &dest, 1, data_normal, 1, SIGNAL_PRIORITY_NORMAL);
    thalamic_router_route_signal(router, sig_normal);

    // Process one signal
    uint32_t num_processed = 0;
    thalamic_router_process_queue(router, 1, &num_processed);

    // Should receive normal priority first (2.0f, not 1.0f)
    ASSERT_EQ(ctx.call_count, 1);
    ASSERT_NE(ctx.signal_received, nullptr);
    EXPECT_FLOAT_EQ(ctx.signal_received[0], 2.0f);

    thalamic_router_free_signal(sig_low);
    thalamic_router_free_signal(sig_normal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, ProcessQueue_Success_EmptyQueue) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    uint32_t num_processed = 0;
    bool result = thalamic_router_process_queue(router, 10, &num_processed);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_processed, 0);

    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, ProcessQueue_Failure_NullRouter) {
    uint32_t num_processed = 0;
    bool result = thalamic_router_process_queue(nullptr, 10, &num_processed);
    EXPECT_FALSE(result);
}

//=============================================================================
// CALLBACK MANAGEMENT TESTS
//=============================================================================
// WHAT: Test callback registration and invocation
// WHY:  Verify signal delivery mechanism
// HOW:  Set callbacks, route signals, verify invocation

TEST_F(ThalamicRouterTest, SetCallback_Success) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    CallbackContext ctx;
    bool result = thalamic_router_set_callback(router, 100, test_callback, &ctx);
    EXPECT_TRUE(result);

    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, SetCallback_Failure_NullRouter) {
    CallbackContext ctx;
    bool result = thalamic_router_set_callback(nullptr, 100, test_callback, &ctx);
    EXPECT_FALSE(result);
}

TEST_F(ThalamicRouterTest, SetCallback_Failure_InvalidDestId) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    CallbackContext ctx;
    bool result = thalamic_router_set_callback(router, 999999, test_callback, &ctx);
    EXPECT_FALSE(result);

    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, Callback_Invoked_OnSignalDelivery) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.enable_attention_gating = false;  // Disable gating for basic callback test
    thalamic_router_t* router = thalamic_router_create(&config);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);

    uint32_t dest = 100;
    float data[] = {3.14f, 2.71f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 2, SIGNAL_PRIORITY_HIGH);

    thalamic_router_route_signal(router, signal);

    ASSERT_EQ(ctx.call_count, 1);
    EXPECT_EQ(ctx.dest_id_received, 100);
    EXPECT_EQ(ctx.size_received, 2);
    ASSERT_NE(ctx.signal_received, nullptr);
    EXPECT_FLOAT_EQ(ctx.signal_received[0], 3.14f);
    EXPECT_FLOAT_EQ(ctx.signal_received[1], 2.71f);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, Callback_MultipleDestinations) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.min_attention_threshold = 0.0f;  // Allow all signals
    thalamic_router_t* router = thalamic_router_create(&config);

    CallbackContext ctx1, ctx2;
    thalamic_router_set_callback(router, 100, test_callback, &ctx1);
    thalamic_router_set_callback(router, 101, test_callback, &ctx2);

    uint32_t dests[] = {100, 101};
    float data[] = {5.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, dests, 2, data, 1, SIGNAL_PRIORITY_HIGH);

    thalamic_router_route_signal(router, signal);

    // Both callbacks should be invoked
    EXPECT_EQ(ctx1.call_count, 1);
    EXPECT_EQ(ctx2.call_count, 1);
    EXPECT_EQ(ctx1.dest_id_received, 100);
    EXPECT_EQ(ctx2.dest_id_received, 101);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

//=============================================================================
// ATTENTION GATING TESTS
//=============================================================================
// WHAT: Test attention-based signal modulation
// WHY:  Verify top-down attention control
// HOW:  Set attention weights, verify signal scaling

TEST_F(ThalamicRouterTest, SetAttention_Success) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    bool result = thalamic_router_set_attention(router, 1, 100, 0.5f);
    EXPECT_TRUE(result);

    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, GetAttention_Success) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    thalamic_router_set_attention(router, 1, 100, 0.7f);

    float attention = 0.0f;
    bool result = thalamic_router_get_attention(router, 1, 100, &attention);
    EXPECT_TRUE(result);
    // Note: Returns combined weight in MIXED mode (0.7 * topdown_weight)
    EXPECT_FLOAT_EQ(attention, 0.49f);  // 0.7 * 0.7 (default topdown_weight)

    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, SetAttention_Failure_NullRouter) {
    bool result = thalamic_router_set_attention(nullptr, 1, 100, 0.5f);
    EXPECT_FALSE(result);
}

TEST_F(ThalamicRouterTest, GetAttention_Failure_NullRouter) {
    float attention = 0.0f;
    bool result = thalamic_router_get_attention(nullptr, 1, 100, &attention);
    EXPECT_FALSE(result);
}

TEST_F(ThalamicRouterTest, Attention_ModulatesSignal) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);
    thalamic_router_set_attention(router, 1, 100, 0.5f);

    uint32_t dest = 100;
    float data[] = {10.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);

    thalamic_router_route_signal(router, signal);

    // Signal scaled by attention (MIXED mode: 0.5 * topdown_weight=0.7 = 0.35)
    ASSERT_EQ(ctx.call_count, 1);
    ASSERT_NE(ctx.signal_received, nullptr);
    EXPECT_FLOAT_EQ(ctx.signal_received[0], 3.5f);  // 10.0 * 0.35
    EXPECT_FLOAT_EQ(ctx.attention_received, 0.35f);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, Attention_ThresholdFiltering) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.min_attention_threshold = 0.5f;
    thalamic_router_t* router = thalamic_router_create(&config);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);

    // Set attention below threshold
    thalamic_router_set_attention(router, 1, 100, 0.3f);

    uint32_t dest = 100;
    float data[] = {10.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);

    thalamic_router_route_signal(router, signal);

    // Signal should be filtered (callback not invoked)
    EXPECT_EQ(ctx.call_count, 0);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, Attention_DisabledConfig) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.enable_attention_gating = false;
    thalamic_router_t* router = thalamic_router_create(&config);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);

    uint32_t dest = 100;
    float data[] = {10.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);

    thalamic_router_route_signal(router, signal);

    // Without gating, signal should be unmodulated
    ASSERT_EQ(ctx.call_count, 1);
    ASSERT_NE(ctx.signal_received, nullptr);
    EXPECT_FLOAT_EQ(ctx.signal_received[0], 10.0f);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================
// WHAT: Test statistics tracking
// WHY:  Verify routing metrics
// HOW:  Route signals, check stats

TEST_F(ThalamicRouterTest, GetStats_Success) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    routing_stats_t stats;
    bool result = thalamic_router_get_stats(router, &stats);
    EXPECT_TRUE(result);

    // Initial stats should be zero
    EXPECT_EQ(stats.signals_routed, 0);
    EXPECT_EQ(stats.signals_dropped, 0);
    EXPECT_EQ(stats.signals_bypassed, 0);
    EXPECT_EQ(stats.queue_depth, 0);

    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, GetStats_Failure_NullRouter) {
    routing_stats_t stats;
    bool result = thalamic_router_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(ThalamicRouterTest, GetStats_Failure_NullStats) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    bool result = thalamic_router_get_stats(router, nullptr);
    EXPECT_FALSE(result);

    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, Stats_TracksRouted) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.min_attention_threshold = 0.0f;  // Allow all signals
    thalamic_router_t* router = thalamic_router_create(&config);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);

    // Route high priority signal (bypassed)
    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);
    thalamic_router_route_signal(router, signal);

    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);

    EXPECT_EQ(stats.signals_routed, 1);
    EXPECT_EQ(stats.signals_bypassed, 1);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, Stats_TracksQueueDepth) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    uint32_t dest = 100;
    float data[] = {1.0f};

    // Enqueue 3 normal priority signals
    for (int i = 0; i < 3; i++) {
        routed_signal_t* signal = thalamic_router_create_signal(
            1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
        thalamic_router_route_signal(router, signal);
        thalamic_router_free_signal(signal);
    }

    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);
    EXPECT_EQ(stats.queue_depth, 3);

    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, ResetStats_Success) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.min_attention_threshold = 0.0f;  // Allow all signals
    thalamic_router_t* router = thalamic_router_create(&config);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);

    // Generate some stats
    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);
    thalamic_router_route_signal(router, signal);

    thalamic_router_reset_stats(router);

    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);
    EXPECT_EQ(stats.signals_routed, 0);
    EXPECT_EQ(stats.signals_bypassed, 0);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, ResetStats_NullSafe) {
    thalamic_router_reset_stats(nullptr);
    // Should not crash
}

//=============================================================================
// QUEUE MANAGEMENT TESTS
//=============================================================================
// WHAT: Test queue clearing
// WHY:  Verify state reset capability
// HOW:  Enqueue signals, clear, verify empty

TEST_F(ThalamicRouterTest, ClearQueue_Success) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    uint32_t dest = 100;
    float data[] = {1.0f};

    // Enqueue signals
    for (int i = 0; i < 5; i++) {
        routed_signal_t* signal = thalamic_router_create_signal(
            1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
        thalamic_router_route_signal(router, signal);
        thalamic_router_free_signal(signal);
    }

    thalamic_router_clear_queue(router);

    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);
    EXPECT_EQ(stats.queue_depth, 0);

    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, ClearQueue_NullSafe) {
    thalamic_router_clear_queue(nullptr);
    // Should not crash
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================
// WHAT: Test known edge cases and bugs
// WHY:  Prevent regressions
// HOW:  Test problematic scenarios

TEST_F(ThalamicRouterTest, Regression_QueueFull_SignalDropped) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.max_queue_size = 2;  // Small queue
    thalamic_router_t* router = thalamic_router_create(&config);

    uint32_t dest = 100;
    float data[] = {1.0f};

    // Fill queue
    routed_signal_t* sig1 = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
    routed_signal_t* sig2 = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
    thalamic_router_route_signal(router, sig1);
    thalamic_router_route_signal(router, sig2);

    // Overflow queue
    routed_signal_t* sig3 = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_NORMAL);
    bool result = thalamic_router_route_signal(router, sig3);
    EXPECT_FALSE(result);  // Should fail to enqueue

    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);
    EXPECT_EQ(stats.signals_dropped, 1);

    thalamic_router_free_signal(sig1);
    thalamic_router_free_signal(sig2);
    thalamic_router_free_signal(sig3);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, Regression_MultipleDests_AllDeliver) {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.min_attention_threshold = 0.0f;  // Allow all signals
    thalamic_router_t* router = thalamic_router_create(&config);

    CallbackContext ctx1, ctx2, ctx3;
    thalamic_router_set_callback(router, 100, test_callback, &ctx1);
    thalamic_router_set_callback(router, 101, test_callback, &ctx2);
    thalamic_router_set_callback(router, 102, test_callback, &ctx3);

    uint32_t dests[] = {100, 101, 102};
    float data[] = {7.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, dests, 3, data, 1, SIGNAL_PRIORITY_HIGH);

    thalamic_router_route_signal(router, signal);

    // All three destinations should receive
    EXPECT_EQ(ctx1.call_count, 1);
    EXPECT_EQ(ctx2.call_count, 1);
    EXPECT_EQ(ctx3.call_count, 1);

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, Regression_NoCallback_NoDelivery) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    // Don't register callback for dest 100

    uint32_t dest = 100;
    float data[] = {1.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);

    // Should not crash, but delivery fails when no callback registered
    bool result = thalamic_router_route_signal(router, signal);
    EXPECT_FALSE(result);  // Routing fails (no callback to deliver to)

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

TEST_F(ThalamicRouterTest, Regression_CombinedAttention_SignalAndGate) {
    thalamic_router_config_t config = thalamic_router_default_config();
    thalamic_router_t* router = thalamic_router_create(&config);

    CallbackContext ctx;
    thalamic_router_set_callback(router, 100, test_callback, &ctx);
    thalamic_router_set_attention(router, 1, 100, 0.5f);  // Gate attention

    uint32_t dest = 100;
    float data[] = {10.0f};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, &dest, 1, data, 1, SIGNAL_PRIORITY_HIGH);
    signal->attention_weight = 0.8f;  // Signal attention

    thalamic_router_route_signal(router, signal);

    // Combined: signal_weight * gate_weight (MIXED mode: 0.5 * 0.7 = 0.35)
    // Total: 0.8 * 0.35 = 0.28, Signal: 10.0 * 0.28 = 2.8
    ASSERT_EQ(ctx.call_count, 1);
    ASSERT_NE(ctx.signal_received, nullptr);
    EXPECT_TRUE(FloatEquals(ctx.signal_received[0], 2.8f, 0.01f));
    EXPECT_TRUE(FloatEquals(ctx.attention_received, 0.28f, 0.01f));

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(router);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
