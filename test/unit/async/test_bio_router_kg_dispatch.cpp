/**
 * @file test_bio_router_kg_dispatch.cpp
 * @brief Unit tests for Phase 7: KG-driven message dispatch
 *
 * Test Categories:
 * 1. KG Connection Tests - Set/get brain_kg reference
 * 2. KG Dispatch Availability - Check dispatch availability
 * 3. KG Dispatch Routing - BIO_MODULE_KG_DISPATCH routing mode
 * 4. Edge Cases - Error handling and boundary conditions
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <cstring>

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BioRouterKGDispatchTest : public ::testing::Test {
protected:
    brain_kg_t* kg = nullptr;
    bio_module_context_t sender_ctx = nullptr;
    bio_module_context_t receiver1_ctx = nullptr;
    bio_module_context_t receiver2_ctx = nullptr;

    // Track received messages
    static std::atomic<int> s_receiver1_count;
    static std::atomic<int> s_receiver2_count;
    static bio_message_type_t s_last_msg_type;

    void SetUp() override {
        s_receiver1_count = 0;
        s_receiver2_count = 0;
        s_last_msg_type = static_cast<bio_message_type_t>(0);

        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = false;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        router_config.enable_logging = false;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);

        // Create brain KG with security disabled for testing
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;
        kg = brain_kg_create(&kg_config);
        ASSERT_NE(kg, nullptr);

        // Register test modules
        bio_module_info_t sender_info = {
            .module_id = BIO_MODULE_BRAIN,
            .module_name = "test_sender",
            .inbox_capacity = 32,
            .user_data = nullptr
        };
        sender_ctx = bio_router_register_module(&sender_info);
        ASSERT_NE(sender_ctx, nullptr);

        bio_module_info_t receiver1_info = {
            .module_id = BIO_MODULE_ATTENTION,
            .module_name = "test_receiver1",
            .inbox_capacity = 32,
            .user_data = nullptr
        };
        receiver1_ctx = bio_router_register_module(&receiver1_info);
        ASSERT_NE(receiver1_ctx, nullptr);

        bio_module_info_t receiver2_info = {
            .module_id = BIO_MODULE_MEMORY,
            .module_name = "test_receiver2",
            .inbox_capacity = 32,
            .user_data = nullptr
        };
        receiver2_ctx = bio_router_register_module(&receiver2_info);
        ASSERT_NE(receiver2_ctx, nullptr);

        // Register handlers
        bio_router_register_handler(receiver1_ctx, BIO_MSG_BRAIN_STATE_QUERY, receiver1_handler);
        bio_router_register_handler(receiver2_ctx, BIO_MSG_BRAIN_STATE_QUERY, receiver2_handler);
    }

    void TearDown() override {
        // Disconnect KG from router
        bio_router_set_brain_kg(nullptr);

        // Unregister modules
        if (receiver2_ctx) bio_router_unregister_module(receiver2_ctx);
        if (receiver1_ctx) bio_router_unregister_module(receiver1_ctx);
        if (sender_ctx) bio_router_unregister_module(sender_ctx);

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }

    // Test message handlers
    static nimcp_error_t receiver1_handler(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t promise, void* user_data) {
        (void)msg_size; (void)promise; (void)user_data;
        const bio_message_header_t* header = (const bio_message_header_t*)msg;
        s_last_msg_type = header->type;
        s_receiver1_count++;
        return NIMCP_SUCCESS;
    }

    static nimcp_error_t receiver2_handler(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t promise, void* user_data) {
        (void)msg_size; (void)promise; (void)user_data;
        const bio_message_header_t* header = (const bio_message_header_t*)msg;
        s_last_msg_type = header->type;
        s_receiver2_count++;
        return NIMCP_SUCCESS;
    }

    // Helper: Create test message with KG dispatch target
    bio_msg_brain_state_query_t CreateKGDispatchQuery() {
        bio_msg_brain_state_query_t msg;
        memset(&msg, 0, sizeof(msg));
        bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
                           BIO_MODULE_BRAIN, BIO_MODULE_KG_DISPATCH,
                           sizeof(msg));
        msg.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;
        msg.region_id = 0;
        return msg;
    }

    // Helper: Add handlers to KG message index
    void SetupKGHandlers() {
        // Add KG nodes for the receiver modules
        brain_kg_node_id_t node1 = brain_kg_add_node(kg, "attention_module",
            BRAIN_KG_NODE_COGNITIVE, "Attention module");
        brain_kg_node_id_t node2 = brain_kg_add_node(kg, "memory_module",
            BRAIN_KG_NODE_COGNITIVE, "Memory module");

        // Register handlers in KG message index
        // Note: Using module_id as node_id for simplicity
        brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);
        brain_kg_add_message_handler(kg, BIO_MODULE_MEMORY, BIO_MSG_BRAIN_STATE_QUERY);

        (void)node1; (void)node2;
    }
};

std::atomic<int> BioRouterKGDispatchTest::s_receiver1_count{0};
std::atomic<int> BioRouterKGDispatchTest::s_receiver2_count{0};
bio_message_type_t BioRouterKGDispatchTest::s_last_msg_type{static_cast<bio_message_type_t>(0)};

//=============================================================================
// 1. KG CONNECTION TESTS
//=============================================================================

TEST_F(BioRouterKGDispatchTest, SetBrainKG) {
    // Initially no KG connected
    EXPECT_EQ(bio_router_get_brain_kg(), nullptr);
    EXPECT_FALSE(bio_router_kg_dispatch_available());

    // Connect KG
    EXPECT_EQ(bio_router_set_brain_kg(kg), NIMCP_SUCCESS);
    EXPECT_EQ(bio_router_get_brain_kg(), kg);
    EXPECT_TRUE(bio_router_kg_dispatch_available());

    // Disconnect KG
    EXPECT_EQ(bio_router_set_brain_kg(nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(bio_router_get_brain_kg(), nullptr);
    EXPECT_FALSE(bio_router_kg_dispatch_available());
}

TEST_F(BioRouterKGDispatchTest, SetBrainKGMultipleTimes) {
    // Connect first KG
    EXPECT_EQ(bio_router_set_brain_kg(kg), NIMCP_SUCCESS);
    EXPECT_EQ(bio_router_get_brain_kg(), kg);

    // Create another KG
    brain_kg_config_t kg2_config;
    brain_kg_default_config(&kg2_config);
    kg2_config.enable_security = false;
    brain_kg_t* kg2 = brain_kg_create(&kg2_config);
    ASSERT_NE(kg2, nullptr);

    // Replace with second KG
    EXPECT_EQ(bio_router_set_brain_kg(kg2), NIMCP_SUCCESS);
    EXPECT_EQ(bio_router_get_brain_kg(), kg2);

    // Cleanup
    bio_router_set_brain_kg(nullptr);
    brain_kg_destroy(kg2);
}

//=============================================================================
// 2. KG DISPATCH AVAILABILITY TESTS
//=============================================================================

TEST_F(BioRouterKGDispatchTest, KGDispatchAvailableWithKG) {
    EXPECT_FALSE(bio_router_kg_dispatch_available());

    bio_router_set_brain_kg(kg);
    EXPECT_TRUE(bio_router_kg_dispatch_available());

    bio_router_set_brain_kg(nullptr);
    EXPECT_FALSE(bio_router_kg_dispatch_available());
}

//=============================================================================
// 3. KG DISPATCH ROUTING TESTS
//=============================================================================

TEST_F(BioRouterKGDispatchTest, KGDispatchWithoutKGFails) {
    // Ensure no KG connected
    bio_router_set_brain_kg(nullptr);

    // Try KG dispatch - should fail
    bio_msg_brain_state_query_t msg = CreateKGDispatchQuery();
    nimcp_error_t result = bio_router_send(sender_ctx, &msg, sizeof(msg), 100);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_INITIALIZED);
}

TEST_F(BioRouterKGDispatchTest, KGDispatchWithNoHandlers) {
    // Connect KG but don't add any handlers
    bio_router_set_brain_kg(kg);

    // Send with KG dispatch
    bio_msg_brain_state_query_t msg = CreateKGDispatchQuery();
    nimcp_error_t result = bio_router_send(sender_ctx, &msg, sizeof(msg), 100);

    // Should succeed (no handlers found is not an error)
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Process inboxes - nothing should be received
    bio_router_process_inbox(receiver1_ctx, 10);
    bio_router_process_inbox(receiver2_ctx, 10);

    EXPECT_EQ(s_receiver1_count.load(), 0);
    EXPECT_EQ(s_receiver2_count.load(), 0);
}

TEST_F(BioRouterKGDispatchTest, KGDispatchToSingleHandler) {
    bio_router_set_brain_kg(kg);

    // Add only one handler to KG
    brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    // Send with KG dispatch
    bio_msg_brain_state_query_t msg = CreateKGDispatchQuery();
    nimcp_error_t result = bio_router_send(sender_ctx, &msg, sizeof(msg), 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Process inboxes
    uint32_t processed1 = bio_router_process_inbox(receiver1_ctx, 10);
    uint32_t processed2 = bio_router_process_inbox(receiver2_ctx, 10);

    EXPECT_EQ(processed1, 1u);
    EXPECT_EQ(processed2, 0u);
    EXPECT_EQ(s_receiver1_count.load(), 1);
    EXPECT_EQ(s_receiver2_count.load(), 0);
}

TEST_F(BioRouterKGDispatchTest, KGDispatchToMultipleHandlers) {
    bio_router_set_brain_kg(kg);
    SetupKGHandlers();  // Add both handlers to KG

    // Send with KG dispatch
    bio_msg_brain_state_query_t msg = CreateKGDispatchQuery();
    nimcp_error_t result = bio_router_send(sender_ctx, &msg, sizeof(msg), 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Process inboxes
    bio_router_process_inbox(receiver1_ctx, 10);
    bio_router_process_inbox(receiver2_ctx, 10);

    // Both receivers should get the message
    EXPECT_EQ(s_receiver1_count.load(), 1);
    EXPECT_EQ(s_receiver2_count.load(), 1);
}

TEST_F(BioRouterKGDispatchTest, KGDispatchMultipleMessages) {
    bio_router_set_brain_kg(kg);
    SetupKGHandlers();

    // Send multiple messages
    for (int i = 0; i < 5; i++) {
        bio_msg_brain_state_query_t msg = CreateKGDispatchQuery();
        nimcp_error_t result = bio_router_send(sender_ctx, &msg, sizeof(msg), 100);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Process all
    bio_router_process_inbox(receiver1_ctx, 10);
    bio_router_process_inbox(receiver2_ctx, 10);

    EXPECT_EQ(s_receiver1_count.load(), 5);
    EXPECT_EQ(s_receiver2_count.load(), 5);
}

TEST_F(BioRouterKGDispatchTest, KGDispatchDoesNotDeliverToSender) {
    bio_router_set_brain_kg(kg);

    // Add sender as a handler
    brain_kg_add_message_handler(kg, BIO_MODULE_BRAIN, BIO_MSG_BRAIN_STATE_QUERY);
    brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    // Register handler on sender
    static std::atomic<int> sender_received{0};
    sender_received = 0;
    bio_router_register_handler(sender_ctx, BIO_MSG_BRAIN_STATE_QUERY,
        [](const void*, size_t, nimcp_bio_promise_t, void*) -> nimcp_error_t {
            sender_received++;
            return NIMCP_SUCCESS;
        });

    // Send with KG dispatch
    bio_msg_brain_state_query_t msg = CreateKGDispatchQuery();
    bio_router_send(sender_ctx, &msg, sizeof(msg), 100);

    // Process all inboxes
    bio_router_process_inbox(sender_ctx, 10);
    bio_router_process_inbox(receiver1_ctx, 10);

    // Sender should NOT receive its own message
    EXPECT_EQ(sender_received.load(), 0);
    EXPECT_EQ(s_receiver1_count.load(), 1);
}

//=============================================================================
// 4. EDGE CASE TESTS
//=============================================================================

TEST_F(BioRouterKGDispatchTest, KGDispatchWithDynamicHandlerRegistration) {
    bio_router_set_brain_kg(kg);

    // Initially no handlers
    bio_msg_brain_state_query_t msg1 = CreateKGDispatchQuery();
    bio_router_send(sender_ctx, &msg1, sizeof(msg1), 100);
    bio_router_process_inbox(receiver1_ctx, 10);
    EXPECT_EQ(s_receiver1_count.load(), 0);

    // Add handler dynamically
    brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    // Now it should receive
    bio_msg_brain_state_query_t msg2 = CreateKGDispatchQuery();
    bio_router_send(sender_ctx, &msg2, sizeof(msg2), 100);
    bio_router_process_inbox(receiver1_ctx, 10);
    EXPECT_EQ(s_receiver1_count.load(), 1);

    // Remove handler
    brain_kg_remove_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);

    // Should not receive anymore
    bio_msg_brain_state_query_t msg3 = CreateKGDispatchQuery();
    bio_router_send(sender_ctx, &msg3, sizeof(msg3), 100);
    bio_router_process_inbox(receiver1_ctx, 10);
    EXPECT_EQ(s_receiver1_count.load(), 1);  // Still 1, not incremented
}

TEST_F(BioRouterKGDispatchTest, KGDispatchConstantValue) {
    // Verify the constant value is as expected
    EXPECT_EQ(BIO_MODULE_KG_DISPATCH, 0xFFFE);
    EXPECT_NE(BIO_MODULE_KG_DISPATCH, BIO_MODULE_ALL);
    EXPECT_NE(BIO_MODULE_KG_DISPATCH, BIO_MODULE_UNKNOWN);
}
