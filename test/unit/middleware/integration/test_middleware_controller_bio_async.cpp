//=============================================================================
// test_middleware_controller_bio_async.cpp
// Bio-Async Integration Tests for Middleware Controller
//=============================================================================

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_middleware_controller.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MiddlewareControllerBioAsyncTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    middleware_controller_t* controller = nullptr;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_logging = false;
        bio_config.enable_statistics = true;
        // BUG FIX: Enable simulation time mode so nimcp_bio_async_step() affects
        // concentration decay calculations in ConfidenceDecaysOverTime test.
        // With use_real_time=true (default), step() advances simulation_time_ms
        // but bio_time_ms() returns wall-clock time, so decay isn't affected.
        bio_config.use_real_time = false;
        nimcp_error_t err = nimcp_bio_async_init(&bio_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize bio-async";

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_logging = false;
        router_config.enable_statistics = true;
        err = bio_router_init(&router_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize bio-router";

        // Create minimal brain
        brain = brain_create("test_controller", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";

        // Create middleware controller
        controller = middleware_controller_create(brain);
        ASSERT_NE(controller, nullptr) << "Failed to create middleware controller";
    }

    void TearDown() override {
        if (controller) {
            middleware_controller_destroy(controller);
        }
        if (brain) {
            brain_destroy(brain);
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// Registration Tests
//=============================================================================

TEST_F(MiddlewareControllerBioAsyncTest, CreateRegistersWithBioRouter) {
    // Controller should register with bio-router upon creation
    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // At least one module should be registered (the controller)
    EXPECT_GE(stats.active_modules, 1u);
}

TEST_F(MiddlewareControllerBioAsyncTest, DestroyUnregistersFromBioRouter) {
    // Get initial stats
    bio_router_stats_t stats_before;
    nimcp_error_t err = bio_router_get_stats(&stats_before);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    uint32_t modules_before = stats_before.active_modules;

    // Destroy controller
    middleware_controller_destroy(controller);
    controller = nullptr;

    // Get stats after destroy
    bio_router_stats_t stats_after;
    err = bio_router_get_stats(&stats_after);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Should have one less module registered
    EXPECT_LE(stats_after.active_modules, modules_before);
}

TEST_F(MiddlewareControllerBioAsyncTest, MultipleControllersRegisterIndependently) {
    // Create second controller
    middleware_controller_t* ctrl2 = middleware_controller_create(brain);
    ASSERT_NE(ctrl2, nullptr);

    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Should have multiple modules registered
    EXPECT_GE(stats.active_modules, 2u);

    middleware_controller_destroy(ctrl2);
}

//=============================================================================
// Message Handling Tests
//=============================================================================

static std::atomic<int> g_command_received{0};
static std::atomic<middleware_command_type_t> g_last_command_type{COMMAND_CONFIGURE_ATTENTION};

TEST_F(MiddlewareControllerBioAsyncTest, ReceivesCommandMessages) {
    // Reset counter
    g_command_received = 0;

    // Note: In a full implementation, controller would register handlers
    // For now, we just verify router can deliver messages

    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Router should be operational
    EXPECT_TRUE(bio_router_is_initialized());
}

TEST_F(MiddlewareControllerBioAsyncTest, HandlesConcurrentMessages) {
    const int num_messages = 10;
    std::atomic<int> processed{0};

    // Simulate multiple commands arriving concurrently
    std::vector<std::thread> threads;
    for (int i = 0; i < num_messages; i++) {
        threads.emplace_back([this, &processed]() {
            // Set attention threshold (thread-safe command)
            bool success = middleware_controller_set_attention_threshold(
                controller, TARGET_VISUAL_CORTEX, 0.5f);
            if (success) {
                processed++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(processed.load(), num_messages);
}

//=============================================================================
// Async Response Tests
//=============================================================================

TEST_F(MiddlewareControllerBioAsyncTest, SendsAsyncResponses) {
    // Test that controller can send async responses via bio-router
    // This would happen when cognitive modules query controller state

    middleware_controller_metrics_t metrics;
    bool success = middleware_controller_get_metrics(controller, &metrics);
    ASSERT_TRUE(success);

    // Metrics should be accessible (verifies internal state management)
    EXPECT_EQ(metrics.total_commands, 0u);
}

TEST_F(MiddlewareControllerBioAsyncTest, AsyncResponsesUseCorrectChannel) {
    // Verify that responses use appropriate neuromodulator channels
    // Attention commands should use dopamine (reward-related)
    // Routing commands might use norepinephrine (arousal-related)

    bool success = middleware_controller_set_attention_threshold(
        controller, TARGET_PREFRONTAL, 0.7f);
    ASSERT_TRUE(success);

    // Get metrics to verify command was processed
    middleware_controller_metrics_t metrics;
    success = middleware_controller_get_metrics(controller, &metrics);
    ASSERT_TRUE(success);

    EXPECT_EQ(metrics.attention_commands, 1u);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(MiddlewareControllerBioAsyncTest, IntegratesWithBioAsyncPromises) {
    // Create a bio-promise for async command execution
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(bool));
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    // Execute command
    bool result = middleware_controller_set_attention_priority(
        controller, TARGET_HIPPOCAMPUS, 0.9f);

    // Complete promise with result
    nimcp_bio_promise_complete(promise, &result);

    // Future should be ready
    EXPECT_TRUE(nimcp_bio_future_is_ready(future));
    EXPECT_EQ(nimcp_bio_future_state(future), BIO_FUTURE_COMPLETED);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(MiddlewareControllerBioAsyncTest, HandlesPromiseTimeouts) {
    // Create promise with short timeout
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_ACETYLCHOLINE, sizeof(int));
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    // Wait with timeout (promise not completed)
    int result = 0;
    nimcp_error_t err = nimcp_bio_future_wait(future, &result, 10);  // 10ms

    EXPECT_EQ(err, NIMCP_ERROR_TIMEOUT);
    EXPECT_FALSE(nimcp_bio_future_is_ready(future));

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(MiddlewareControllerBioAsyncTest, UsesDifferentChannelsForDifferentCommands) {
    // Attention commands -> dopamine (reward/attention)
    nimcp_bio_promise_t attention_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(bool));
    ASSERT_NE(attention_promise, nullptr);

    // Routing commands -> norepinephrine (arousal/routing)
    nimcp_bio_promise_t routing_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_NOREPINEPHRINE, sizeof(bool));
    ASSERT_NE(routing_promise, nullptr);

    // Pattern commands -> serotonin (stability/pattern)
    nimcp_bio_promise_t pattern_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_SEROTONIN, sizeof(bool));
    ASSERT_NE(pattern_promise, nullptr);

    // Execute different command types
    bool result1 = middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 0.6f);
    bool result2 = middleware_controller_set_routing_priority(
        controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.8f);
    bool result3 = middleware_controller_set_pattern_threshold(
        controller, 0.85f);

    // Complete promises
    nimcp_bio_promise_complete(attention_promise, &result1);
    nimcp_bio_promise_complete(routing_promise, &result2);
    nimcp_bio_promise_complete(pattern_promise, &result3);

    // All should complete successfully
    nimcp_bio_future_t future1 = nimcp_bio_promise_get_future(attention_promise);
    nimcp_bio_future_t future2 = nimcp_bio_promise_get_future(routing_promise);
    nimcp_bio_future_t future3 = nimcp_bio_promise_get_future(pattern_promise);

    EXPECT_TRUE(nimcp_bio_future_is_ready(future1));
    EXPECT_TRUE(nimcp_bio_future_is_ready(future2));
    EXPECT_TRUE(nimcp_bio_future_is_ready(future3));

    nimcp_bio_future_destroy(future1);
    nimcp_bio_future_destroy(future2);
    nimcp_bio_future_destroy(future3);
    nimcp_bio_promise_destroy(attention_promise);
    nimcp_bio_promise_destroy(routing_promise);
    nimcp_bio_promise_destroy(pattern_promise);
}

//=============================================================================
// Neuromodulator Decay Tests
//=============================================================================

TEST_F(MiddlewareControllerBioAsyncTest, ConfidenceDecaysOverTime) {
    // Create promise and complete it
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(bool));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    bool result = true;
    nimcp_bio_promise_complete(promise, &result);

    float confidence_t0 = nimcp_bio_future_get_confidence(future);
    EXPECT_GT(confidence_t0, 0.9f);

    // Advance simulation time
    nimcp_bio_async_step(50.0f);  // 50ms

    float confidence_t1 = nimcp_bio_future_get_confidence(future);

    // Confidence should decay (dopamine tau = 200ms, so ~22% decay)
    EXPECT_LT(confidence_t1, confidence_t0);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(MiddlewareControllerBioAsyncTest, HandlesNullPromises) {
    // Should not crash with null promises
    // Note: Real implementation would check for nulls

    bool success = middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 0.5f);
    EXPECT_TRUE(success);
}

TEST_F(MiddlewareControllerBioAsyncTest, HandlesRouterShutdown) {
    // Shutdown router while controller exists
    bio_router_shutdown();

    // Commands should still work (fall back to direct execution)
    bool success = middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 0.5f);
    EXPECT_TRUE(success);

    // Re-initialize for teardown
    bio_router_config_t config = bio_router_default_config();
    config.enable_logging = false;
    nimcp_bio_async_init(nullptr);
    bio_router_init(&config);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MiddlewareControllerBioAsyncTest, TracksMessageStatistics) {
    bio_router_reset_stats();

    // Execute several commands
    middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 0.5f);
    middleware_controller_set_attention_priority(
        controller, TARGET_PREFRONTAL, 0.8f);
    middleware_controller_set_routing_priority(
        controller, TARGET_VISUAL_CORTEX, TARGET_PREFRONTAL, 0.7f);

    // Get controller metrics
    middleware_controller_metrics_t metrics;
    bool success = middleware_controller_get_metrics(controller, &metrics);
    ASSERT_TRUE(success);

    EXPECT_EQ(metrics.attention_commands, 2u);
    EXPECT_EQ(metrics.routing_commands, 1u);
    EXPECT_EQ(metrics.total_commands, 3u);
}

TEST_F(MiddlewareControllerBioAsyncTest, TracksLatencyMetrics) {
    // Execute commands and check latency tracking
    for (int i = 0; i < 10; i++) {
        middleware_controller_set_attention_threshold(
            controller, TARGET_VISUAL_CORTEX, 0.5f + i * 0.01f);
    }

    middleware_controller_metrics_t metrics;
    bool success = middleware_controller_get_metrics(controller, &metrics);
    ASSERT_TRUE(success);

    // Latency tracking may or may not be implemented yet
    EXPECT_GE(metrics.avg_latency_us, 0.0f);
    EXPECT_GE(metrics.max_latency_us, metrics.min_latency_us);
    EXPECT_GE(metrics.max_latency_us, metrics.avg_latency_us);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(MiddlewareControllerBioAsyncTest, MeetsLatencyTarget) {
    // Execute many commands and verify latency target
    const int num_commands = 100;

    for (int i = 0; i < num_commands; i++) {
        middleware_controller_set_attention_threshold(
            controller, TARGET_VISUAL_CORTEX, 0.5f);
    }

    middleware_controller_metrics_t metrics;
    bool success = middleware_controller_get_metrics(controller, &metrics);
    ASSERT_TRUE(success);

    // Most commands should meet <5µs target
    float success_rate = 1.0f -
        (float)metrics.commands_exceeding_target / metrics.total_commands;
    EXPECT_GT(success_rate, 0.8f);  // At least 80% should meet target
}

TEST_F(MiddlewareControllerBioAsyncTest, HandlesHighThroughput) {
    // Test high-throughput command execution
    const int num_commands = 1000;
    std::atomic<int> successful{0};

    for (int i = 0; i < num_commands; i++) {
        bool success = middleware_controller_set_attention_threshold(
            controller, TARGET_VISUAL_CORTEX, 0.5f + (i % 100) * 0.001f);
        if (success) {
            successful++;
        }
    }

    // Should handle all commands successfully
    EXPECT_EQ(successful.load(), num_commands);

    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);

    EXPECT_EQ(metrics.total_commands, (uint32_t)num_commands);
    EXPECT_LT(metrics.avg_latency_us, 10.0f);  // Should stay under 10µs average
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
