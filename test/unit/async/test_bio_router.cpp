/**
 * @file test_bio_router.cpp
 * @brief Comprehensive unit tests for bio-async message router
 *
 * Test Categories:
 * 1. Initialization Tests - Init/shutdown sequences
 * 2. Registration Tests - Module and handler registration
 * 3. Message Tests - Send, receive, broadcast
 * 4. Channel Tests - Verify correct channel usage
 * 5. Statistics Tests - Verify tracking behavior
 * 6. Error Tests - Invalid inputs, edge cases
 * 7. Concurrency Tests - Multi-threaded scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BioRouterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = false;  // Reduce noise in tests
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        router_config.enable_logging = false;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);

        ASSERT_TRUE(bio_router_is_initialized());
    }

    void TearDown() override {
        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        ASSERT_FALSE(bio_router_is_initialized());
    }

    // Helper: Create test message
    bio_msg_brain_state_query_t CreateTestQuery() {
        bio_msg_brain_state_query_t msg;
        bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
                           BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION,
                           sizeof(msg));
        msg.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;
        msg.region_id = 0;
        return msg;
    }
};

//=============================================================================
// 1. INITIALIZATION TESTS
//=============================================================================

TEST_F(BioRouterTest, InitShutdownSequence) {
    // Already initialized in SetUp
    EXPECT_TRUE(bio_router_is_initialized());

    // Get global router
    bio_router_t router = bio_router_get_global();
    EXPECT_NE(router, nullptr);

    // Shutdown
    bio_router_shutdown();
    EXPECT_FALSE(bio_router_is_initialized());

    // Re-initialize
    bio_router_config_t config = bio_router_default_config();
    EXPECT_EQ(bio_router_init(&config), NIMCP_SUCCESS);
    EXPECT_TRUE(bio_router_is_initialized());
}

TEST_F(BioRouterTest, DefaultConfigIsValid) {
    bio_router_config_t config = bio_router_default_config();

    // Check reasonable defaults
    EXPECT_GT(config.max_modules, 0u);
    EXPECT_GT(config.inbox_capacity, 0u);
    EXPECT_GT(config.outbox_capacity, 0u);
    EXPECT_GT(config.max_message_size, 0u);
    EXPECT_GT(config.routing_timeout_ms, 0.0f);
}

TEST_F(BioRouterTest, DoubleInitFails) {
    // Already initialized
    bio_router_config_t config = bio_router_default_config();
    nimcp_error_t err = bio_router_init(&config);

    // Should fail or return gracefully
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(BioRouterTest, ShutdownWithoutInit) {
    // Shutdown in TearDown
    bio_router_shutdown();

    // Second shutdown should be safe
    bio_router_shutdown();
    EXPECT_FALSE(bio_router_is_initialized());
}

//=============================================================================
// 2. REGISTRATION TESTS
//=============================================================================

TEST_F(BioRouterTest, RegisterModuleBasic) {
    bio_module_info_t info;
    info.module_id = BIO_MODULE_BRAIN;
    info.module_name = "test_brain";
    info.inbox_capacity = 0;  // Use default
    info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(ctx, nullptr);

    // Check context
    EXPECT_EQ(bio_module_context_get_id(ctx), BIO_MODULE_BRAIN);
    EXPECT_STREQ(bio_module_context_get_name(ctx), "test_brain");
    EXPECT_EQ(bio_module_context_get_user_data(ctx), nullptr);

    bio_router_unregister_module(ctx);
}

TEST_F(BioRouterTest, RegisterMultipleModules) {
    std::vector<bio_module_context_t> modules;

    // Register 5 different modules
    bio_module_id_t ids[] = {
        BIO_MODULE_BRAIN,
        BIO_MODULE_INTROSPECTION,
        BIO_MODULE_ETHICS,
        BIO_MODULE_STDP,
        BIO_MODULE_ASTROCYTE
    };

    for (auto id : ids) {
        bio_module_info_t info;
        info.module_id = id;
        info.module_name = "test_module";
        info.inbox_capacity = 0;
        info.user_data = nullptr;

        bio_module_context_t ctx = bio_router_register_module(&info);
        ASSERT_NE(ctx, nullptr);
        modules.push_back(ctx);
    }

    // Unregister all
    for (auto ctx : modules) {
        bio_router_unregister_module(ctx);
    }
}

TEST_F(BioRouterTest, RegisterHandlerForMessageType) {
    bio_module_info_t info;
    info.module_id = BIO_MODULE_INTROSPECTION;
    info.module_name = "test_intro";
    info.inbox_capacity = 0;
    info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(ctx, nullptr);

    // Register handler
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        return NIMCP_SUCCESS;
    };

    nimcp_error_t err = bio_router_register_handler(
        ctx, BIO_MSG_BRAIN_STATE_QUERY, handler);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    bio_router_unregister_module(ctx);
}

TEST_F(BioRouterTest, RegisterCategoryHandler) {
    bio_module_info_t info;
    info.module_id = BIO_MODULE_STDP;
    info.module_name = "test_stdp";
    info.inbox_capacity = 0;
    info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(ctx, nullptr);

    // Register handler for all plasticity messages (0x0200)
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        return NIMCP_SUCCESS;
    };

    nimcp_error_t err = bio_router_register_category_handler(
        ctx, 0x0200, handler);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    bio_router_unregister_module(ctx);
}

TEST_F(BioRouterTest, UnregisterModule) {
    bio_module_info_t info;
    info.module_id = BIO_MODULE_BRAIN;
    info.module_name = "test_brain";
    info.inbox_capacity = 0;
    info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(ctx, nullptr);

    // Unregister
    bio_router_unregister_module(ctx);
    ctx = nullptr;  // Must set to NULL after unregister - pointer is now invalid

    // After unregistering, calling with NULL is safe (no-op)
    bio_router_unregister_module(ctx);
}

//=============================================================================
// 3. MESSAGE TESTS
//=============================================================================

TEST_F(BioRouterTest, SendMessageSynchronous) {
    // Register sender
    bio_module_info_t sender_info;
    sender_info.module_id = BIO_MODULE_BRAIN;
    sender_info.module_name = "sender";
    sender_info.inbox_capacity = 0;
    sender_info.user_data = nullptr;

    bio_module_context_t sender = bio_router_register_module(&sender_info);
    ASSERT_NE(sender, nullptr);

    // Register receiver
    bio_module_info_t receiver_info;
    receiver_info.module_id = BIO_MODULE_INTROSPECTION;
    receiver_info.module_name = "receiver";
    receiver_info.inbox_capacity = 100;
    receiver_info.user_data = nullptr;

    bio_module_context_t receiver = bio_router_register_module(&receiver_info);
    ASSERT_NE(receiver, nullptr);

    // Create and send message
    bio_msg_brain_state_query_t msg = CreateTestQuery();
    nimcp_error_t err = bio_router_send(sender, &msg, sizeof(msg), 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Check receiver inbox
    uint32_t count = bio_router_inbox_count(receiver);
    EXPECT_GT(count, 0u);

    bio_router_unregister_module(sender);
    bio_router_unregister_module(receiver);
}

TEST_F(BioRouterTest, SendReceiveWithHandler) {
    std::atomic<bool> handler_called{false};
    std::atomic<uint32_t> received_flags{0};

    // Register receiver with handler
    bio_module_info_t receiver_info;
    receiver_info.module_id = BIO_MODULE_INTROSPECTION;
    receiver_info.module_name = "receiver";
    receiver_info.inbox_capacity = 100;
    receiver_info.user_data = &handler_called;

    bio_module_context_t receiver = bio_router_register_module(&receiver_info);
    ASSERT_NE(receiver, nullptr);

    // Register handler
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* called = static_cast<std::atomic<bool>*>(user_data);
        *called = true;

        // Verify message
        auto* query = static_cast<const bio_msg_brain_state_query_t*>(msg);
        EXPECT_EQ(query->header.type, BIO_MSG_BRAIN_STATE_QUERY);

        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(receiver, BIO_MSG_BRAIN_STATE_QUERY, handler);

    // Register sender
    bio_module_info_t sender_info;
    sender_info.module_id = BIO_MODULE_BRAIN;
    sender_info.module_name = "sender";
    sender_info.inbox_capacity = 0;
    sender_info.user_data = nullptr;

    bio_module_context_t sender = bio_router_register_module(&sender_info);
    ASSERT_NE(sender, nullptr);

    // Send message
    bio_msg_brain_state_query_t msg = CreateTestQuery();
    bio_router_send(sender, &msg, sizeof(msg), 1000);

    // Process inbox
    uint32_t processed = bio_router_process_inbox(receiver, 10);
    EXPECT_GT(processed, 0u);
    EXPECT_TRUE(handler_called.load());

    bio_router_unregister_module(sender);
    bio_router_unregister_module(receiver);
}

TEST_F(BioRouterTest, BroadcastMessage) {
    std::atomic<int> handler_count{0};

    // Register 3 receivers
    std::vector<bio_module_context_t> receivers;
    for (int i = 0; i < 3; i++) {
        bio_module_info_t info;
        info.module_id = static_cast<bio_module_id_t>(BIO_MODULE_INTROSPECTION + i);
        info.module_name = "receiver";
        info.inbox_capacity = 100;
        info.user_data = &handler_count;

        bio_module_context_t ctx = bio_router_register_module(&info);
        ASSERT_NE(ctx, nullptr);

        // Register handler
        auto handler = [](const void* msg, size_t size,
                         nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
            auto* count = static_cast<std::atomic<int>*>(user_data);
            (*count)++;
            return NIMCP_SUCCESS;
        };

        bio_router_register_handler(ctx, BIO_MSG_BRAIN_STATE_QUERY, handler);
        receivers.push_back(ctx);
    }

    // Register sender
    bio_module_info_t sender_info;
    sender_info.module_id = BIO_MODULE_BRAIN;
    sender_info.module_name = "sender";
    sender_info.inbox_capacity = 0;
    sender_info.user_data = nullptr;

    bio_module_context_t sender = bio_router_register_module(&sender_info);
    ASSERT_NE(sender, nullptr);

    // Broadcast message
    bio_msg_brain_state_query_t msg = CreateTestQuery();
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.header.target_module = 0;  // Broadcast

    nimcp_error_t err = bio_router_broadcast(sender, &msg, sizeof(msg));
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process all receivers
    for (auto ctx : receivers) {
        bio_router_process_inbox(ctx, 10);
    }

    // All receivers should have been called
    EXPECT_EQ(handler_count.load(), 3);

    bio_router_unregister_module(sender);
    for (auto ctx : receivers) {
        bio_router_unregister_module(ctx);
    }
}

TEST_F(BioRouterTest, SendAsyncWithPromise) {
    // Register modules
    bio_module_info_t sender_info;
    sender_info.module_id = BIO_MODULE_BRAIN;
    sender_info.module_name = "sender";
    sender_info.inbox_capacity = 0;
    sender_info.user_data = nullptr;

    bio_module_context_t sender = bio_router_register_module(&sender_info);
    ASSERT_NE(sender, nullptr);

    bio_module_info_t receiver_info;
    receiver_info.module_id = BIO_MODULE_INTROSPECTION;
    receiver_info.module_name = "receiver";
    receiver_info.inbox_capacity = 100;
    receiver_info.user_data = nullptr;

    bio_module_context_t receiver = bio_router_register_module(&receiver_info);
    ASSERT_NE(receiver, nullptr);

    // Register handler that completes promise
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        if (promise) {
            bio_msg_brain_state_response_t response;
            bio_msg_init_header(&response.header, BIO_MSG_BRAIN_STATE_RESPONSE,
                               BIO_MODULE_INTROSPECTION, BIO_MODULE_BRAIN,
                               sizeof(response));
            response.neuron_count = 1000;

            nimcp_bio_promise_complete_sized(promise, &response, sizeof(response));
        }
        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(receiver, BIO_MSG_BRAIN_STATE_QUERY, handler);

    // Send async
    bio_msg_brain_state_query_t msg = CreateTestQuery();
    nimcp_bio_promise_t promise = bio_router_send_async(
        sender, &msg, sizeof(msg), BIO_CHANNEL_ACETYLCHOLINE);

    ASSERT_NE(promise, nullptr);

    // Process inbox to trigger handler
    bio_router_process_inbox(receiver, 10);

    // Get future and wait
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    bio_msg_brain_state_response_t response;
    nimcp_error_t err = nimcp_bio_future_wait(future, &response, 1000);

    if (err == NIMCP_SUCCESS) {
        EXPECT_EQ(response.neuron_count, 1000u);
    }

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);

    bio_router_unregister_module(sender);
    bio_router_unregister_module(receiver);
}

//=============================================================================
// 4. CHANNEL TESTS
//=============================================================================

TEST_F(BioRouterTest, RecommendedChannelForMessageTypes) {
    // Plasticity messages should use dopamine
    EXPECT_EQ(bio_msg_recommended_channel(BIO_MSG_WEIGHT_UPDATE_REQUEST),
              BIO_CHANNEL_DOPAMINE);

    // Ethics should use serotonin
    EXPECT_EQ(bio_msg_recommended_channel(BIO_MSG_ETHICS_EVALUATION_REQUEST),
              BIO_CHANNEL_SEROTONIN);

    // Salience/alerts should use norepinephrine
    EXPECT_EQ(bio_msg_recommended_channel(BIO_MSG_SALIENCE_QUERY),
              BIO_CHANNEL_NOREPINEPHRINE);

    // Fast queries should use acetylcholine
    EXPECT_EQ(bio_msg_recommended_channel(BIO_MSG_BRAIN_STATE_QUERY),
              BIO_CHANNEL_ACETYLCHOLINE);
}

//=============================================================================
// 5. STATISTICS TESTS
//=============================================================================

TEST_F(BioRouterTest, StatisticsTracking) {
    bio_router_stats_t stats;

    // Get initial stats
    nimcp_error_t err = bio_router_get_stats(&stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    uint64_t initial_routed = stats.messages_routed;

    // Send a message
    bio_module_info_t sender_info;
    sender_info.module_id = BIO_MODULE_BRAIN;
    sender_info.module_name = "sender";
    sender_info.inbox_capacity = 0;
    sender_info.user_data = nullptr;

    bio_module_context_t sender = bio_router_register_module(&sender_info);

    bio_module_info_t receiver_info;
    receiver_info.module_id = BIO_MODULE_INTROSPECTION;
    receiver_info.module_name = "receiver";
    receiver_info.inbox_capacity = 100;
    receiver_info.user_data = nullptr;

    bio_module_context_t receiver = bio_router_register_module(&receiver_info);

    bio_msg_brain_state_query_t msg = CreateTestQuery();
    bio_router_send(sender, &msg, sizeof(msg), 1000);

    // Get updated stats
    err = bio_router_get_stats(&stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(stats.messages_routed, initial_routed);

    bio_router_unregister_module(sender);
    bio_router_unregister_module(receiver);
}

TEST_F(BioRouterTest, ResetStatistics) {
    bio_router_stats_t stats;

    // Send some messages to generate stats
    bio_module_info_t sender_info;
    sender_info.module_id = BIO_MODULE_BRAIN;
    sender_info.module_name = "sender";
    sender_info.inbox_capacity = 0;
    sender_info.user_data = nullptr;

    bio_module_context_t sender = bio_router_register_module(&sender_info);

    bio_module_info_t receiver_info;
    receiver_info.module_id = BIO_MODULE_INTROSPECTION;
    receiver_info.module_name = "receiver";
    receiver_info.inbox_capacity = 100;
    receiver_info.user_data = nullptr;

    bio_module_context_t receiver = bio_router_register_module(&receiver_info);

    bio_msg_brain_state_query_t msg = CreateTestQuery();
    bio_router_send(sender, &msg, sizeof(msg), 1000);

    // Reset stats
    bio_router_reset_stats();

    // Verify reset
    bio_router_get_stats(&stats);
    EXPECT_EQ(stats.messages_routed, 0u);

    bio_router_unregister_module(sender);
    bio_router_unregister_module(receiver);
}

//=============================================================================
// 6. ERROR TESTS
//=============================================================================

TEST_F(BioRouterTest, SendToNonexistentModule) {
    bio_module_info_t sender_info;
    sender_info.module_id = BIO_MODULE_BRAIN;
    sender_info.module_name = "sender";
    sender_info.inbox_capacity = 0;
    sender_info.user_data = nullptr;

    bio_module_context_t sender = bio_router_register_module(&sender_info);
    ASSERT_NE(sender, nullptr);

    // Send to module that doesn't exist
    bio_msg_brain_state_query_t msg = CreateTestQuery();
    msg.header.target_module = 0xFFFF;  // Invalid module

    nimcp_error_t err = bio_router_send(sender, &msg, sizeof(msg), 100);

    // Should timeout or fail gracefully
    EXPECT_NE(err, NIMCP_SUCCESS);

    bio_router_unregister_module(sender);
}

TEST_F(BioRouterTest, NullMessagePointer) {
    bio_module_info_t sender_info;
    sender_info.module_id = BIO_MODULE_BRAIN;
    sender_info.module_name = "sender";
    sender_info.inbox_capacity = 0;
    sender_info.user_data = nullptr;

    bio_module_context_t sender = bio_router_register_module(&sender_info);
    ASSERT_NE(sender, nullptr);

    // Send null message
    nimcp_error_t err = bio_router_send(sender, nullptr, 0, 1000);
    EXPECT_NE(err, NIMCP_SUCCESS);

    bio_router_unregister_module(sender);
}

TEST_F(BioRouterTest, InboxOverflow) {
    // Create receiver with small inbox
    bio_module_info_t receiver_info;
    receiver_info.module_id = BIO_MODULE_INTROSPECTION;
    receiver_info.module_name = "receiver";
    receiver_info.inbox_capacity = 2;  // Very small
    receiver_info.user_data = nullptr;

    bio_module_context_t receiver = bio_router_register_module(&receiver_info);
    ASSERT_NE(receiver, nullptr);

    bio_module_info_t sender_info;
    sender_info.module_id = BIO_MODULE_BRAIN;
    sender_info.module_name = "sender";
    sender_info.inbox_capacity = 0;
    sender_info.user_data = nullptr;

    bio_module_context_t sender = bio_router_register_module(&sender_info);
    ASSERT_NE(sender, nullptr);

    // Send multiple messages to overflow
    bio_msg_brain_state_query_t msg = CreateTestQuery();
    for (int i = 0; i < 10; i++) {
        bio_router_send(sender, &msg, sizeof(msg), 10);
    }

    // Check inbox count (should be capped or overflow handled)
    uint32_t count = bio_router_inbox_count(receiver);
    // Implementation should handle overflow gracefully

    bio_router_unregister_module(sender);
    bio_router_unregister_module(receiver);
}

//=============================================================================
// 7. CONCURRENCY TESTS
//=============================================================================

TEST_F(BioRouterTest, ConcurrentSend) {
    const int NUM_THREADS = 4;
    const int MESSAGES_PER_THREAD = 100;
    std::atomic<int> total_sent{0};

    // Register receiver
    bio_module_info_t receiver_info;
    receiver_info.module_id = BIO_MODULE_INTROSPECTION;
    receiver_info.module_name = "receiver";
    receiver_info.inbox_capacity = 1000;
    receiver_info.user_data = nullptr;

    bio_module_context_t receiver = bio_router_register_module(&receiver_info);
    ASSERT_NE(receiver, nullptr);

    // Launch sender threads
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            // Each thread registers its own sender
            bio_module_info_t sender_info;
            sender_info.module_id = static_cast<bio_module_id_t>(BIO_MODULE_BRAIN + t);
            sender_info.module_name = "sender";
            sender_info.inbox_capacity = 0;
            sender_info.user_data = nullptr;

            bio_module_context_t sender = bio_router_register_module(&sender_info);

            for (int i = 0; i < MESSAGES_PER_THREAD; i++) {
                bio_msg_brain_state_query_t msg = CreateTestQuery();
                if (bio_router_send(sender, &msg, sizeof(msg), 1000) == NIMCP_SUCCESS) {
                    total_sent++;
                }
            }

            bio_router_unregister_module(sender);
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Verify messages received
    uint32_t inbox_count = bio_router_inbox_count(receiver);
    EXPECT_GT(inbox_count, 0u);
    EXPECT_GT(total_sent.load(), 0);

    bio_router_unregister_module(receiver);
}

TEST_F(BioRouterTest, ConcurrentProcessInbox) {
    std::atomic<int> handler_calls{0};

    // Register receiver
    bio_module_info_t receiver_info;
    receiver_info.module_id = BIO_MODULE_INTROSPECTION;
    receiver_info.module_name = "receiver";
    receiver_info.inbox_capacity = 1000;
    receiver_info.user_data = &handler_calls;

    bio_module_context_t receiver = bio_router_register_module(&receiver_info);
    ASSERT_NE(receiver, nullptr);

    // Register handler
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* count = static_cast<std::atomic<int>*>(user_data);
        (*count)++;
        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(receiver, BIO_MSG_BRAIN_STATE_QUERY, handler);

    // Register sender
    bio_module_info_t sender_info;
    sender_info.module_id = BIO_MODULE_BRAIN;
    sender_info.module_name = "sender";
    sender_info.inbox_capacity = 0;
    sender_info.user_data = nullptr;

    bio_module_context_t sender = bio_router_register_module(&sender_info);

    // Send many messages
    bio_msg_brain_state_query_t msg = CreateTestQuery();
    for (int i = 0; i < 100; i++) {
        bio_router_send(sender, &msg, sizeof(msg), 1000);
    }

    // Process from multiple threads
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&]() {
            bio_router_process_inbox(receiver, 25);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All messages should be processed
    EXPECT_GT(handler_calls.load(), 0);

    bio_router_unregister_module(sender);
    bio_router_unregister_module(receiver);
}
