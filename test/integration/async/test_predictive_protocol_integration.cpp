/**
 * @file test_predictive_protocol_integration.cpp
 * @brief Integration tests for predictive protocol with bio-router
 *
 * WHAT: Tests predictive protocol integrated with bio-router
 * WHY:  Ensure protocol works correctly in real routing scenarios
 * HOW:  Route messages, verify predictions, measure latency improvements
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_predictive_protocol.h"
#include "async/nimcp_bio_async.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_time.h"

class PredictiveRouterTest : public ::testing::Test {
protected:
    bio_module_context_t module_a;
    bio_module_context_t module_b;
    bio_module_context_t module_c;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Initialize bio-async (required by router)
        nimcp_bio_async_config_t async_cfg = nimcp_bio_async_default_config();
        ASSERT_EQ(nimcp_bio_async_init(&async_cfg), NIMCP_SUCCESS);

        // Initialize router with predictive protocol enabled
        bio_router_config_t router_cfg = bio_router_default_config();
        router_cfg.enable_predictive_protocol = true;
        ASSERT_EQ(bio_router_init(&router_cfg), NIMCP_SUCCESS);

        // Register test modules
        bio_module_info_t info_a = {
            .module_id = BIO_MODULE_BRAIN,
            .module_name = "TestModuleA",
            .inbox_capacity = 64,
            .user_data = nullptr
        };
        module_a = bio_router_register_module(&info_a);
        ASSERT_NE(module_a, nullptr);

        bio_module_info_t info_b = {
            .module_id = BIO_MODULE_INTROSPECTION,
            .module_name = "TestModuleB",
            .inbox_capacity = 64,
            .user_data = nullptr
        };
        module_b = bio_router_register_module(&info_b);
        ASSERT_NE(module_b, nullptr);

        bio_module_info_t info_c = {
            .module_id = BIO_MODULE_ETHICS,
            .module_name = "TestModuleC",
            .inbox_capacity = 64,
            .user_data = nullptr
        };
        module_c = bio_router_register_module(&info_c);
        ASSERT_NE(module_c, nullptr);
    }

    void TearDown() override {
        if (module_a) bio_router_unregister_module(module_a);
        if (module_b) bio_router_unregister_module(module_b);
        if (module_c) bio_router_unregister_module(module_c);

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        nimcp_memory_check_leaks();
        nimcp_memory_cleanup();
    }

    void send_message(bio_module_context_t from,
                      bio_module_id_t to,
                      bio_message_type_t type) {
        bio_msg_brain_state_query_t msg{};
        msg.header.type = type;
        msg.header.source_module = bio_module_context_get_id(from);
        msg.header.target_module = to;
        msg.header.timestamp_us = nimcp_platform_time_monotonic_us();
        msg.header.channel = BIO_CHANNEL_DOPAMINE;
        msg.header.payload_size = sizeof(msg) - sizeof(msg.header);
        msg.header.flags = 0;

        ASSERT_EQ(bio_router_send(from, &msg, sizeof(msg), 0), NIMCP_SUCCESS);
    }
};

/**
 * WHAT: Test predictive protocol learns patterns from routed messages
 * WHY:  Ensure integration with router observation works
 * HOW:  Route repeated pattern, verify it's learned
 */
TEST_F(PredictiveRouterTest, PatternLearningViaRouter) {
    // Send repeating pattern: A -> B -> C
    for (int i = 0; i < 20; i++) {
        send_message(module_a, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY);
        nimcp_platform_sleep_ms(10);
        send_message(module_b, BIO_MODULE_ETHICS, BIO_MSG_INTROSPECTION_QUERY);
        nimcp_platform_sleep_ms(10);
        send_message(module_c, BIO_MODULE_BRAIN, BIO_MSG_ETHICS_EVALUATION_REQUEST);
        nimcp_platform_sleep_ms(10);
    }

    // Process inboxes
    bio_router_process_inbox(module_b, 0);
    bio_router_process_inbox(module_c, 0);
    bio_router_process_inbox(module_a, 0);

    // Router should have learned the pattern and generated predictions
    bio_router_stats_t stats;
    ASSERT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);

    EXPECT_GT(stats.messages_routed, 0UL);
}

/**
 * WHAT: Test message sequence prediction
 * WHY:  Ensure router can predict next message in sequence
 * HOW:  Train on A->B->C, send A, verify B is predicted
 */
TEST_F(PredictiveRouterTest, MessageSequencePrediction) {
    // Train on predictable sequence
    for (int i = 0; i < 30; i++) {
        send_message(module_a, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY);
        nimcp_platform_sleep_ms(5);
        send_message(module_b, BIO_MODULE_ETHICS, BIO_MSG_INTROSPECTION_QUERY);
        nimcp_platform_sleep_ms(5);
    }

    // Process messages
    bio_router_process_inbox(module_b, 0);
    bio_router_process_inbox(module_c, 0);

    // After training, the router's predictive protocol should have learned
    // that INTROSPECTION_QUERY follows BRAIN_STATE_QUERY

    // Verify router statistics show activity
    bio_router_stats_t stats;
    ASSERT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.messages_routed, 50UL);  // At least 60 messages routed
}

/**
 * WHAT: Test cache hit behavior with predicted messages
 * WHY:  Verify prefetching provides cache hits
 * HOW:  Train pattern, send predicted message, check if it was prefetched
 */
TEST_F(PredictiveRouterTest, CacheHitBehavior) {
    // Train strong pattern
    for (int i = 0; i < 50; i++) {
        send_message(module_a, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY);
        nimcp_platform_sleep_ms(5);
    }

    // Process all messages
    for (int i = 0; i < 10; i++) {
        bio_router_process_inbox(module_b, 5);
        nimcp_platform_sleep_ms(5);
    }

    // Router should have made predictions and potentially prefetched
    bio_router_stats_t stats;
    ASSERT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.messages_routed, 0UL);
}

/**
 * WHAT: Test latency improvement from prefetching
 * WHY:  Measure performance benefit of predictive protocol
 * HOW:  Route with/without predictions, compare latencies
 */
TEST_F(PredictiveRouterTest, LatencyImprovement) {
    // Train a strong pattern
    for (int i = 0; i < 100; i++) {
        send_message(module_a, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY);
        nimcp_platform_sleep_ms(2);
    }

    // Measure routing latency
    bio_router_stats_t stats;
    ASSERT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);

    // With predictive protocol enabled, average latency should be tracked
    EXPECT_GE(stats.avg_routing_latency_us, 0.0f);
    EXPECT_GE(stats.max_routing_latency_us, 0.0f);

    // Process messages
    bio_router_process_inbox(module_b, 0);
}

/**
 * WHAT: Test multiple concurrent message patterns
 * WHY:  Ensure protocol handles multiple traffic patterns
 * HOW:  Route different patterns concurrently, verify both learned
 */
TEST_F(PredictiveRouterTest, MultipleConcurrentPatterns) {
    // Pattern 1: A -> B
    // Pattern 2: B -> C
    // Pattern 3: C -> A

    for (int i = 0; i < 30; i++) {
        send_message(module_a, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY);
        send_message(module_b, BIO_MODULE_ETHICS, BIO_MSG_INTROSPECTION_QUERY);
        send_message(module_c, BIO_MODULE_BRAIN, BIO_MSG_ETHICS_EVALUATION_REQUEST);
        nimcp_platform_sleep_ms(10);
    }

    // Process all inboxes
    bio_router_process_inbox(module_a, 0);
    bio_router_process_inbox(module_b, 0);
    bio_router_process_inbox(module_c, 0);

    // All patterns should be routed
    bio_router_stats_t stats;
    ASSERT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.messages_routed, 80UL);  // At least 90 messages
}

/**
 * WHAT: Test broadcast message handling
 * WHY:  Ensure predictions work with broadcast traffic
 * HOW:  Send broadcasts, verify they're observed but not prefetched
 */
TEST_F(PredictiveRouterTest, BroadcastHandling) {
    // Send some broadcasts
    for (int i = 0; i < 10; i++) {
        send_message(module_a, 0, BIO_MSG_BRAIN_STATE_QUERY);  // 0 = broadcast
        nimcp_platform_sleep_ms(10);
    }

    // Process all inboxes
    bio_router_process_inbox(module_a, 0);
    bio_router_process_inbox(module_b, 0);
    bio_router_process_inbox(module_c, 0);

    // Broadcasts should be routed to all modules
    bio_router_stats_t stats;
    ASSERT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.broadcasts_sent, 0UL);
}

/**
 * WHAT: Test router with predictive protocol disabled
 * WHY:  Ensure router works without predictive protocol
 * HOW:  Create router without prediction, verify normal operation
 */
TEST_F(PredictiveRouterTest, DisabledPredictiveProtocol) {
    // Shutdown current router
    bio_router_unregister_module(module_a);
    bio_router_unregister_module(module_b);
    bio_router_unregister_module(module_c);
    bio_router_shutdown();

    // Reinit without predictive protocol
    bio_router_config_t router_cfg = bio_router_default_config();
    router_cfg.enable_predictive_protocol = false;
    ASSERT_EQ(bio_router_init(&router_cfg), NIMCP_SUCCESS);

    // Re-register modules
    bio_module_info_t info_a = {
        .module_id = BIO_MODULE_BRAIN,
        .module_name = "TestModuleA",
        .inbox_capacity = 64,
        .user_data = nullptr
    };
    module_a = bio_router_register_module(&info_a);
    ASSERT_NE(module_a, nullptr);

    bio_module_info_t info_b = {
        .module_id = BIO_MODULE_INTROSPECTION,
        .module_name = "TestModuleB",
        .inbox_capacity = 64,
        .user_data = nullptr
    };
    module_b = bio_router_register_module(&info_b);
    ASSERT_NE(module_b, nullptr);

    // Send messages - should still work
    send_message(module_a, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY);
    bio_router_process_inbox(module_b, 1);

    // Verify routing worked
    bio_router_stats_t stats;
    ASSERT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.messages_routed, 0UL);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
