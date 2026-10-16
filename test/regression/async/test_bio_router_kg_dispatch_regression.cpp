/**
 * @file test_bio_router_kg_dispatch_regression.cpp
 * @brief Regression tests for Phase 7: KG-driven message dispatch
 *
 * Tests ensure KG dispatch behavior remains consistent across versions:
 * - API stability tests
 * - Error handling consistency
 * - Performance regression tests
 * - Edge case behavior preservation
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstring>

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Regression Test Fixture
//=============================================================================

class BioRouterKGDispatchRegression : public ::testing::Test {
protected:
    brain_kg_t* kg = nullptr;
    bio_module_context_t sender_ctx = nullptr;
    bio_module_context_t receiver_ctx = nullptr;

    static std::atomic<int> s_handler_count;

    void SetUp() override {
        s_handler_count = 0;

        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = false;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        router_config.enable_logging = false;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);

        // Create brain KG
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;
        kg = brain_kg_create(&kg_config);
        ASSERT_NE(kg, nullptr);

        // Register test modules
        bio_module_info_t sender_info = {
            .module_id = BIO_MODULE_BRAIN,
            .module_name = "regression_sender",
            .inbox_capacity = 32,
            .user_data = nullptr
        };
        sender_ctx = bio_router_register_module(&sender_info);
        ASSERT_NE(sender_ctx, nullptr);

        bio_module_info_t receiver_info = {
            .module_id = BIO_MODULE_ATTENTION,
            .module_name = "regression_receiver",
            .inbox_capacity = 32,
            .user_data = nullptr
        };
        receiver_ctx = bio_router_register_module(&receiver_info);
        ASSERT_NE(receiver_ctx, nullptr);

        bio_router_register_handler(receiver_ctx, BIO_MSG_BRAIN_STATE_QUERY, test_handler);
    }

    void TearDown() override {
        bio_router_set_brain_kg(nullptr);

        if (receiver_ctx) bio_router_unregister_module(receiver_ctx);
        if (sender_ctx) bio_router_unregister_module(sender_ctx);

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }

    static nimcp_error_t test_handler(const void* msg, size_t msg_size,
                                       nimcp_bio_promise_t promise, void* user_data) {
        (void)msg; (void)msg_size; (void)promise; (void)user_data;
        s_handler_count++;
        return NIMCP_SUCCESS;
    }

    bio_msg_brain_state_query_t CreateKGDispatchQuery() {
        bio_msg_brain_state_query_t msg;
        memset(&msg, 0, sizeof(msg));
        bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
                           BIO_MODULE_BRAIN, BIO_MODULE_KG_DISPATCH,
                           sizeof(msg));
        msg.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;
        return msg;
    }
};

std::atomic<int> BioRouterKGDispatchRegression::s_handler_count{0};

//=============================================================================
// API STABILITY REGRESSION TESTS
//=============================================================================

/**
 * @test Verify BIO_MODULE_KG_DISPATCH constant value is stable
 * @regression Ensures constant value does not change across versions
 */
TEST_F(BioRouterKGDispatchRegression, API_KGDispatchConstantValue) {
    // This value MUST remain 0xFFFE for backwards compatibility
    EXPECT_EQ(BIO_MODULE_KG_DISPATCH, 0xFFFE);

    // Ensure it doesn't collide with other special values
    EXPECT_NE(BIO_MODULE_KG_DISPATCH, BIO_MODULE_ALL);
    EXPECT_NE(BIO_MODULE_KG_DISPATCH, BIO_MODULE_UNKNOWN);
}

/**
 * @test Verify bio_router_set_brain_kg API signature and return values
 * @regression Ensures API behavior is consistent
 */
TEST_F(BioRouterKGDispatchRegression, API_SetBrainKGReturnValues) {
    // Setting KG should return success
    EXPECT_EQ(bio_router_set_brain_kg(kg), NIMCP_SUCCESS);

    // Setting same KG again should return success (idempotent)
    EXPECT_EQ(bio_router_set_brain_kg(kg), NIMCP_SUCCESS);

    // Setting nullptr should return success
    EXPECT_EQ(bio_router_set_brain_kg(nullptr), NIMCP_SUCCESS);
}

/**
 * @test Verify bio_router_get_brain_kg returns what was set
 * @regression Ensures getter/setter pair work correctly
 */
TEST_F(BioRouterKGDispatchRegression, API_GetBrainKGConsistency) {
    EXPECT_EQ(bio_router_get_brain_kg(), nullptr);

    bio_router_set_brain_kg(kg);
    EXPECT_EQ(bio_router_get_brain_kg(), kg);

    bio_router_set_brain_kg(nullptr);
    EXPECT_EQ(bio_router_get_brain_kg(), nullptr);
}

/**
 * @test Verify bio_router_kg_dispatch_available follows KG state
 * @regression Ensures availability check is accurate
 */
TEST_F(BioRouterKGDispatchRegression, API_KGDispatchAvailabilityTracking) {
    // No KG = not available
    EXPECT_FALSE(bio_router_kg_dispatch_available());

    // With KG = available
    bio_router_set_brain_kg(kg);
    EXPECT_TRUE(bio_router_kg_dispatch_available());

    // After disconnection = not available
    bio_router_set_brain_kg(nullptr);
    EXPECT_FALSE(bio_router_kg_dispatch_available());
}

//=============================================================================
// ERROR HANDLING REGRESSION TESTS
//=============================================================================

/**
 * @test KG dispatch without KG returns NOT_INITIALIZED
 * @regression Error code must remain consistent for clients depending on it
 */
TEST_F(BioRouterKGDispatchRegression, Error_NoKGReturnsNotInitialized) {
    bio_router_set_brain_kg(nullptr);

    auto msg = CreateKGDispatchQuery();
    nimcp_error_t result = bio_router_send(sender_ctx, &msg, sizeof(msg), 100);

    // MUST return NIMCP_ERROR_NOT_INITIALIZED (not any other error)
    EXPECT_EQ(result, NIMCP_ERROR_NOT_INITIALIZED);
}

/**
 * @test KG dispatch with no handlers returns success (silent success)
 * @regression No handlers is not an error - clients depend on this
 */
TEST_F(BioRouterKGDispatchRegression, Error_NoHandlersIsNotError) {
    bio_router_set_brain_kg(kg);
    // Note: no handlers registered in KG

    auto msg = CreateKGDispatchQuery();
    nimcp_error_t result = bio_router_send(sender_ctx, &msg, sizeof(msg), 100);

    // MUST return SUCCESS even with no handlers
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// BEHAVIOR CONSISTENCY REGRESSION TESTS
//=============================================================================

/**
 * @test Messages are delivered to KG-registered handlers
 * @regression Core dispatch functionality must work
 */
TEST_F(BioRouterKGDispatchRegression, Behavior_MessagesDeliveredToHandlers) {
    bio_router_set_brain_kg(kg);
    brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    auto msg = CreateKGDispatchQuery();
    bio_router_send(sender_ctx, &msg, sizeof(msg), 100);

    bio_router_process_inbox(receiver_ctx, 10);

    EXPECT_EQ(s_handler_count.load(), 1);
}

/**
 * @test Handler removal stops delivery
 * @regression Dynamic reconfiguration must work
 */
TEST_F(BioRouterKGDispatchRegression, Behavior_HandlerRemovalStopsDelivery) {
    bio_router_set_brain_kg(kg);
    brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    // First message delivered
    auto msg1 = CreateKGDispatchQuery();
    bio_router_send(sender_ctx, &msg1, sizeof(msg1), 100);
    bio_router_process_inbox(receiver_ctx, 10);
    EXPECT_EQ(s_handler_count.load(), 1);

    // Remove handler
    brain_kg_remove_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    // Second message NOT delivered
    auto msg2 = CreateKGDispatchQuery();
    bio_router_send(sender_ctx, &msg2, sizeof(msg2), 100);
    bio_router_process_inbox(receiver_ctx, 10);
    EXPECT_EQ(s_handler_count.load(), 1);  // Still 1, not 2
}

/**
 * @test KG disconnect mid-operation is safe
 * @regression System must not crash on rapid reconfiguration
 */
TEST_F(BioRouterKGDispatchRegression, Behavior_KGDisconnectIsSafe) {
    bio_router_set_brain_kg(kg);
    brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    // Send first message
    auto msg1 = CreateKGDispatchQuery();
    EXPECT_EQ(bio_router_send(sender_ctx, &msg1, sizeof(msg1), 100), NIMCP_SUCCESS);

    // Disconnect
    bio_router_set_brain_kg(nullptr);

    // Second message should fail gracefully
    auto msg2 = CreateKGDispatchQuery();
    EXPECT_EQ(bio_router_send(sender_ctx, &msg2, sizeof(msg2), 100),
              NIMCP_ERROR_NOT_INITIALIZED);

    // Reconnect and verify recovery
    bio_router_set_brain_kg(kg);
    auto msg3 = CreateKGDispatchQuery();
    EXPECT_EQ(bio_router_send(sender_ctx, &msg3, sizeof(msg3), 100), NIMCP_SUCCESS);
}

//=============================================================================
// PERFORMANCE REGRESSION TESTS
//=============================================================================

/**
 * @test KG dispatch throughput baseline
 * @regression Performance must not degrade significantly
 */
TEST_F(BioRouterKGDispatchRegression, Performance_ThroughputBaseline) {
    bio_router_set_brain_kg(kg);
    brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    const int MSG_COUNT = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < MSG_COUNT; i++) {
        auto msg = CreateKGDispatchQuery();
        bio_router_send(sender_ctx, &msg, sizeof(msg), 100);
    }

    auto send_end = std::chrono::high_resolution_clock::now();

    // Process all
    bio_router_process_inbox(receiver_ctx, MSG_COUNT + 100);

    auto process_end = std::chrono::high_resolution_clock::now();

    // Verify all messages received
    EXPECT_EQ(s_handler_count.load(), MSG_COUNT);

    // Calculate timings
    auto send_time = std::chrono::duration_cast<std::chrono::microseconds>(
        send_end - start).count();
    auto process_time = std::chrono::duration_cast<std::chrono::microseconds>(
        process_end - send_end).count();

    // Log for regression tracking (these can be compared across versions)
    printf("  [Performance] Send %d msgs: %ld us (%.2f msg/us)\n",
           MSG_COUNT, send_time, (float)MSG_COUNT / send_time);
    printf("  [Performance] Process %d msgs: %ld us (%.2f msg/us)\n",
           MSG_COUNT, process_time, (float)MSG_COUNT / process_time);

    // Sanity check - should complete in reasonable time
    EXPECT_LT(send_time + process_time, 1000000);  // < 1 second
}

/**
 * @test KG handler lookup performance
 * @regression Handler lookup must be fast (O(1) expected)
 */
TEST_F(BioRouterKGDispatchRegression, Performance_HandlerLookupScaling) {
    bio_router_set_brain_kg(kg);

    // Add the handler we'll query
    brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    // Add many other handlers (different message types) to stress the index
    for (uint32_t i = 0; i < 100; i++) {
        brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, 0x9000 + i);
    }

    const int LOOKUP_COUNT = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < LOOKUP_COUNT; i++) {
        brain_kg_handler_list_t* handlers =
            brain_kg_get_handlers_for_message_type(kg, BIO_MSG_BRAIN_STATE_QUERY);
        ASSERT_NE(handlers, nullptr);
        brain_kg_handler_list_destroy(handlers);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    printf("  [Performance] %d lookups: %ld us (%.2f us/lookup)\n",
           LOOKUP_COUNT, elapsed, (float)elapsed / LOOKUP_COUNT);

    // Should be fast - target < 1us per lookup average
    EXPECT_LT(elapsed, LOOKUP_COUNT * 10);  // < 10us per lookup max
}

//=============================================================================
// EDGE CASE REGRESSION TESTS
//=============================================================================

/**
 * @test Sender module is excluded from KG dispatch
 * @regression Sender must not receive its own broadcast
 */
TEST_F(BioRouterKGDispatchRegression, Edge_SenderExcludedFromDispatch) {
    bio_router_set_brain_kg(kg);

    // Register sender module as a handler
    brain_kg_add_message_handler(kg, BIO_MODULE_BRAIN, BIO_MSG_BRAIN_STATE_QUERY);
    brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    static std::atomic<int> sender_received{0};
    sender_received = 0;
    bio_router_register_handler(sender_ctx, BIO_MSG_BRAIN_STATE_QUERY,
        [](const void*, size_t, nimcp_bio_promise_t, void*) -> nimcp_error_t {
            sender_received++;
            return NIMCP_SUCCESS;
        });

    auto msg = CreateKGDispatchQuery();
    bio_router_send(sender_ctx, &msg, sizeof(msg), 100);

    bio_router_process_inbox(sender_ctx, 10);
    bio_router_process_inbox(receiver_ctx, 10);

    // Sender MUST NOT receive its own message
    EXPECT_EQ(sender_received.load(), 0);
    // But regular receiver should
    EXPECT_EQ(s_handler_count.load(), 1);
}

/**
 * @test Duplicate handler registration is idempotent
 * @regression Must not cause double delivery
 */
TEST_F(BioRouterKGDispatchRegression, Edge_DuplicateHandlerIdempotent) {
    bio_router_set_brain_kg(kg);

    // Register same handler twice
    brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);
    brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    auto msg = CreateKGDispatchQuery();
    bio_router_send(sender_ctx, &msg, sizeof(msg), 100);
    bio_router_process_inbox(receiver_ctx, 10);

    // Must receive exactly once, not twice
    EXPECT_EQ(s_handler_count.load(), 1);
}

/**
 * @test Multiple rapid KG switches
 * @regression System must handle rapid reconfiguration
 */
TEST_F(BioRouterKGDispatchRegression, Edge_RapidKGSwitching) {
    // Create second KG
    brain_kg_config_t kg2_config;
    brain_kg_default_config(&kg2_config);
    kg2_config.enable_security = false;
    brain_kg_t* kg2 = brain_kg_create(&kg2_config);
    ASSERT_NE(kg2, nullptr);

    // Rapidly switch between KGs
    for (int i = 0; i < 100; i++) {
        bio_router_set_brain_kg((i % 2 == 0) ? kg : kg2);
        EXPECT_NE(bio_router_get_brain_kg(), nullptr);
    }

    // Cleanup
    bio_router_set_brain_kg(nullptr);
    brain_kg_destroy(kg2);
}

