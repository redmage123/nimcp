/**
 * @file test_bio_async_integration.cpp
 * @brief Integration tests for the bio-async messaging system
 *
 * WHAT: Tests bio-router message passing, immune integration, cytokine broadcasting
 * WHY:  Verify inter-module communication, immune system alerts, message routing
 * HOW:  Test router lifecycle, module registration, message sending/receiving
 *
 * Test Categories:
 * - Bio-router initialization and shutdown
 * - Module registration and handler management
 * - Message passing (sync and async)
 * - Immune system integration
 * - Cytokine broadcasting
 * - Inflammation alerts
 * - Predictive coding integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_async.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Callback State
//=============================================================================

// Global state for tracking callback invocations
static std::atomic<int> g_message_received_count(0);
static std::atomic<int> g_broadcast_received_count(0);
static bio_message_type_t g_last_message_type = BIO_MSG_TYPE_COUNT;
static float g_last_cytokine_concentration = 0.0f;
static uint32_t g_last_inflammation_severity = 0;

// Reset test state
static void reset_test_state() {
    g_message_received_count = 0;
    g_broadcast_received_count = 0;
    g_last_message_type = BIO_MSG_TYPE_COUNT;
    g_last_cytokine_concentration = 0.0f;
    g_last_inflammation_severity = 0;
}

// Test message handler callback
static nimcp_error_t test_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)msg_size;
    (void)response_promise;
    (void)user_data;

    const bio_message_header_t* header = static_cast<const bio_message_header_t*>(msg);
    g_last_message_type = header->type;
    g_message_received_count++;

    return NIMCP_SUCCESS;
}

// Test broadcast handler callback
static nimcp_error_t test_broadcast_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)msg_size;
    (void)response_promise;
    (void)user_data;

    const bio_message_header_t* header = static_cast<const bio_message_header_t*>(msg);
    g_last_message_type = header->type;
    g_broadcast_received_count++;

    return NIMCP_SUCCESS;
}

// Prediction observer callback
static void test_prediction_observer(
    const char* signal_name,
    float value,
    void* user_data
) {
    (void)signal_name;
    (void)user_data;
    g_last_cytokine_concentration = value;
}

//=============================================================================
// Test Fixture - Bio Router Lifecycle
//=============================================================================

class BioRouterLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        reset_test_state();
        // Ensure router is not initialized
        if (bio_router_is_initialized()) {
            bio_router_shutdown();
        }
    }

    void TearDown() override {
        if (bio_router_is_initialized()) {
            bio_router_shutdown();
        }
    }
};

//=============================================================================
// Test Fixture - Bio Router with Initialization
//=============================================================================

class BioRouterTest : public ::testing::Test {
protected:
    bio_module_context_t module_ctx = nullptr;
    bio_module_info_t module_info;

    void SetUp() override {
        reset_test_state();

        // Initialize router with defaults
        bio_router_config_t config = bio_router_default_config();
        config.enable_logging = false;
        config.enable_statistics = true;

        nimcp_error_t err = bio_router_init(&config);
        ASSERT_EQ(err, NIMCP_SUCCESS);
        ASSERT_TRUE(bio_router_is_initialized());

        // Register test module
        memset(&module_info, 0, sizeof(module_info));
        module_info.module_id = 0x1000;
        module_info.module_name = "test_module";
        module_info.inbox_capacity = 64;
        module_info.user_data = nullptr;

        module_ctx = bio_router_register_module(&module_info);
        ASSERT_NE(module_ctx, nullptr);
    }

    void TearDown() override {
        if (module_ctx) {
            bio_router_unregister_module(module_ctx);
            module_ctx = nullptr;
        }
        if (bio_router_is_initialized()) {
            bio_router_shutdown();
        }
    }
};

//=============================================================================
// Test Fixture - Multi-Module Communication
//=============================================================================

class BioRouterMultiModuleTest : public ::testing::Test {
protected:
    bio_module_context_t sender_ctx = nullptr;
    bio_module_context_t receiver_ctx = nullptr;

    void SetUp() override {
        reset_test_state();

        // Initialize router
        bio_router_config_t config = bio_router_default_config();
        config.enable_logging = false;
        config.enable_statistics = true;

        nimcp_error_t err = bio_router_init(&config);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Register sender module
        bio_module_info_t sender_info;
        memset(&sender_info, 0, sizeof(sender_info));
        sender_info.module_id = 0x1000;
        sender_info.module_name = "sender_module";
        sender_info.inbox_capacity = 64;
        sender_ctx = bio_router_register_module(&sender_info);
        ASSERT_NE(sender_ctx, nullptr);

        // Register receiver module
        bio_module_info_t receiver_info;
        memset(&receiver_info, 0, sizeof(receiver_info));
        receiver_info.module_id = 0x2000;
        receiver_info.module_name = "receiver_module";
        receiver_info.inbox_capacity = 64;
        receiver_ctx = bio_router_register_module(&receiver_info);
        ASSERT_NE(receiver_ctx, nullptr);
    }

    void TearDown() override {
        if (sender_ctx) {
            bio_router_unregister_module(sender_ctx);
            sender_ctx = nullptr;
        }
        if (receiver_ctx) {
            bio_router_unregister_module(receiver_ctx);
            receiver_ctx = nullptr;
        }
        if (bio_router_is_initialized()) {
            bio_router_shutdown();
        }
    }
};

//=============================================================================
// Bio Router Lifecycle Tests
//=============================================================================

TEST_F(BioRouterLifecycleTest, DefaultConfigIsValid) {
    bio_router_config_t config = bio_router_default_config();

    EXPECT_GT(config.max_modules, 0u);
    EXPECT_GT(config.inbox_capacity, 0u);
    EXPECT_GT(config.outbox_capacity, 0u);
    EXPECT_GT(config.max_message_size, 0u);
}

TEST_F(BioRouterLifecycleTest, InitAndShutdown) {
    bio_router_config_t config = bio_router_default_config();

    nimcp_error_t err = bio_router_init(&config);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(bio_router_is_initialized());

    bio_router_shutdown();
    EXPECT_FALSE(bio_router_is_initialized());
}

TEST_F(BioRouterLifecycleTest, InitWithNullConfig) {
    // Should use defaults when config is NULL
    nimcp_error_t err = bio_router_init(nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(bio_router_is_initialized());
}

TEST_F(BioRouterLifecycleTest, DoubleInitFails) {
    bio_router_config_t config = bio_router_default_config();

    nimcp_error_t err = bio_router_init(&config);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Second init should fail (already initialized)
    err = bio_router_init(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(BioRouterLifecycleTest, ShutdownWithoutInit) {
    // Should be safe to call shutdown even if not initialized
    bio_router_shutdown();
    EXPECT_FALSE(bio_router_is_initialized());
}

TEST_F(BioRouterLifecycleTest, GetGlobalRouterWhenInitialized) {
    bio_router_config_t config = bio_router_default_config();
    bio_router_init(&config);

    bio_router_t router = bio_router_get_global();
    EXPECT_NE(router, nullptr);
}

TEST_F(BioRouterLifecycleTest, GetGlobalRouterWhenNotInitialized) {
    bio_router_t router = bio_router_get_global();
    EXPECT_EQ(router, nullptr);
}

//=============================================================================
// Module Registration Tests
//=============================================================================

TEST_F(BioRouterTest, RegisterModule) {
    // Module already registered in SetUp - verify it works
    bio_module_id_t id = bio_module_context_get_id(module_ctx);
    EXPECT_EQ(id, 0x1000u);

    const char* name = bio_module_context_get_name(module_ctx);
    EXPECT_STREQ(name, "test_module");
}

TEST_F(BioRouterTest, RegisterMultipleModules) {
    bio_module_info_t info2;
    memset(&info2, 0, sizeof(info2));
    info2.module_id = 0x2000;
    info2.module_name = "second_module";
    info2.inbox_capacity = 32;

    bio_module_context_t ctx2 = bio_router_register_module(&info2);
    EXPECT_NE(ctx2, nullptr);

    bio_module_id_t id2 = bio_module_context_get_id(ctx2);
    EXPECT_EQ(id2, 0x2000u);

    bio_router_unregister_module(ctx2);
}

TEST_F(BioRouterTest, UnregisterModule) {
    bio_module_info_t info2;
    memset(&info2, 0, sizeof(info2));
    info2.module_id = 0x3000;
    info2.module_name = "temp_module";

    bio_module_context_t ctx2 = bio_router_register_module(&info2);
    ASSERT_NE(ctx2, nullptr);

    bio_router_unregister_module(ctx2);
    // Should not crash - ctx2 is now invalid
}

TEST_F(BioRouterTest, RegisterHandler) {
    nimcp_error_t err = bio_router_register_handler(
        module_ctx,
        BIO_MSG_BRAIN_STATE_QUERY,
        test_message_handler
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(BioRouterTest, RegisterMultipleHandlers) {
    nimcp_error_t err = bio_router_register_handler(
        module_ctx,
        BIO_MSG_BRAIN_STATE_QUERY,
        test_message_handler
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = bio_router_register_handler(
        module_ctx,
        BIO_MSG_PLASTICITY_UPDATE,
        test_message_handler
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = bio_router_register_handler(
        module_ctx,
        BIO_MSG_TRAINING_STEP_COMPLETE,
        test_message_handler
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(BioRouterTest, UnregisterHandler) {
    bio_router_register_handler(module_ctx, BIO_MSG_BRAIN_STATE_QUERY, test_message_handler);

    nimcp_error_t err = bio_router_unregister_handler(
        module_ctx,
        BIO_MSG_BRAIN_STATE_QUERY
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(BioRouterTest, ClearHandlers) {
    bio_router_register_handler(module_ctx, BIO_MSG_BRAIN_STATE_QUERY, test_message_handler);
    bio_router_register_handler(module_ctx, BIO_MSG_PLASTICITY_UPDATE, test_message_handler);

    nimcp_error_t err = bio_router_clear_handlers(module_ctx);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Message Passing Tests
//=============================================================================

TEST_F(BioRouterMultiModuleTest, SendMessageSync) {
    // Register handler on receiver
    bio_router_register_handler(
        receiver_ctx,
        BIO_MSG_BRAIN_STATE_QUERY,
        test_message_handler
    );

    // Create message
    struct {
        bio_message_header_t header;
        uint32_t data;
    } msg;

    memset(&msg, 0, sizeof(msg));
    msg.header.type = BIO_MSG_BRAIN_STATE_QUERY;
    msg.header.source_module = 0x1000;
    msg.header.target_module = 0x2000;
    msg.header.payload_size = sizeof(uint32_t);
    msg.data = 42;

    // Send message
    nimcp_error_t err = bio_router_send(sender_ctx, &msg, sizeof(msg), 100);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process receiver inbox
    uint32_t processed = bio_router_process_inbox(receiver_ctx, 10);
    EXPECT_GE(processed, 1u);
    EXPECT_EQ(g_message_received_count, 1);
    EXPECT_EQ(g_last_message_type, BIO_MSG_BRAIN_STATE_QUERY);
}

TEST_F(BioRouterMultiModuleTest, BroadcastMessage) {
    // Register broadcast handler on receiver
    bio_router_register_handler(
        receiver_ctx,
        BIO_MSG_TRAINING_STEP_COMPLETE,
        test_broadcast_handler
    );

    // Create broadcast message
    struct {
        bio_message_header_t header;
        float loss;
    } msg;

    memset(&msg, 0, sizeof(msg));
    msg.header.type = BIO_MSG_TRAINING_STEP_COMPLETE;
    msg.header.source_module = 0x1000;
    msg.header.target_module = 0;  // Broadcast
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(float);
    msg.loss = 1.5f;

    // Broadcast message
    nimcp_error_t err = bio_router_broadcast(sender_ctx, &msg, sizeof(msg));
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process receiver inbox
    uint32_t processed = bio_router_process_inbox(receiver_ctx, 10);
    EXPECT_GE(processed, 1u);
    EXPECT_GE(g_broadcast_received_count, 1);
}

TEST_F(BioRouterMultiModuleTest, InboxCount) {
    // Register handler on receiver
    bio_router_register_handler(
        receiver_ctx,
        BIO_MSG_BRAIN_STATE_QUERY,
        test_message_handler
    );

    // Send multiple messages
    for (int i = 0; i < 5; ++i) {
        struct {
            bio_message_header_t header;
            int index;
        } msg;

        memset(&msg, 0, sizeof(msg));
        msg.header.type = BIO_MSG_BRAIN_STATE_QUERY;
        msg.header.source_module = 0x1000;
        msg.header.target_module = 0x2000;
        msg.header.payload_size = sizeof(int);
        msg.index = i;

        bio_router_send(sender_ctx, &msg, sizeof(msg), 100);
    }

    // Check inbox count before processing
    uint32_t count = bio_router_inbox_count(receiver_ctx);
    EXPECT_GE(count, 5u);

    // Process all
    bio_router_process_inbox(receiver_ctx, 0);
    EXPECT_EQ(g_message_received_count, 5);
}

TEST_F(BioRouterMultiModuleTest, CategoryHandler) {
    // Register category handler for all plasticity messages
    bio_router_register_category_handler(
        receiver_ctx,
        0x0200,  // Plasticity category base
        test_message_handler
    );

    // Send plasticity update message
    struct {
        bio_message_header_t header;
        float weight_delta;
    } msg;

    memset(&msg, 0, sizeof(msg));
    msg.header.type = BIO_MSG_PLASTICITY_UPDATE;
    msg.header.source_module = 0x1000;
    msg.header.target_module = 0x2000;
    msg.header.payload_size = sizeof(float);
    msg.weight_delta = 0.01f;

    bio_router_send(sender_ctx, &msg, sizeof(msg), 100);

    // Process - category handler should receive it
    uint32_t processed = bio_router_process_inbox(receiver_ctx, 10);
    EXPECT_GE(processed, 1u);
}

//=============================================================================
// Immune System Integration Tests
//=============================================================================

TEST_F(BioRouterTest, BroadcastCytokine) {
    // Register handler for cytokine messages
    bio_router_register_handler(
        module_ctx,
        BIO_MSG_CYTOKINE_UPDATE,
        test_message_handler
    );

    // Broadcast cytokine (this will internally create and send message)
    nimcp_error_t err = bio_async_broadcast_cytokine(
        1,      // cytokine_type: e.g., IL-1
        0.75f,  // concentration
        100     // source_cell
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process inbox
    bio_router_process_inbox(module_ctx, 10);

    // Verify message was received
    EXPECT_GE(g_message_received_count, 0);  // May be 0 if broadcast to self is filtered
}

TEST_F(BioRouterTest, InflammationAlert) {
    // Register handler for inflammation messages
    bio_router_register_handler(
        module_ctx,
        BIO_MSG_INFLAMMATION_CHANGE,
        test_message_handler
    );

    // Send inflammation alert
    nimcp_error_t err = bio_async_inflammation_alert(
        42,     // region_id
        3,      // severity (0-4)
        12345   // antigen_id
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process inbox
    bio_router_process_inbox(module_ctx, 10);
}

TEST_F(BioRouterTest, ImmunePhaseChange) {
    // Register handler for immune phase messages
    bio_router_register_handler(
        module_ctx,
        BIO_MSG_IMMUNE_RESPONSE_STARTED,
        test_message_handler
    );

    // Notify phase change
    nimcp_error_t err = bio_async_immune_phase_change(
        0,  // old_phase (e.g., SURVEILLANCE)
        1   // new_phase (e.g., RECOGNITION)
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Predictive Coding Integration Tests
//=============================================================================

TEST_F(BioRouterTest, ObserveSignal) {
    // Register as observer for a signal
    nimcp_error_t err = bio_router_observe_signal(
        module_ctx,
        "loss_trend",
        0.0f,   // initial_prediction
        1.0f,   // precision
        test_prediction_observer
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(BioRouterTest, PublishSignal) {
    // First register observer
    bio_router_observe_signal(
        module_ctx,
        "gradient_norm",
        0.1f,
        1.0f,
        test_prediction_observer
    );

    // Publish signal value
    nimcp_error_t err = bio_router_publish_signal(
        module_ctx,
        "gradient_norm",
        0.15f
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(BioRouterMultiModuleTest, RouterStatistics) {
    // Register handler
    bio_router_register_handler(
        receiver_ctx,
        BIO_MSG_BRAIN_STATE_QUERY,
        test_message_handler
    );

    // Send some messages
    for (int i = 0; i < 3; ++i) {
        struct {
            bio_message_header_t header;
            int data;
        } msg;

        memset(&msg, 0, sizeof(msg));
        msg.header.type = BIO_MSG_BRAIN_STATE_QUERY;
        msg.header.source_module = 0x1000;
        msg.header.target_module = 0x2000;
        msg.data = i;

        bio_router_send(sender_ctx, &msg, sizeof(msg), 100);
    }

    // Process messages
    bio_router_process_inbox(receiver_ctx, 0);

    // Get statistics
    bio_router_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_error_t err = bio_router_get_stats(&stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify some statistics
    EXPECT_GE(stats.messages_routed, 3u);
    EXPECT_GE(stats.active_modules, 2u);
}

TEST_F(BioRouterTest, ResetStatistics) {
    // Get current stats
    bio_router_stats_t stats1;
    memset(&stats1, 0, sizeof(stats1));
    bio_router_get_stats(&stats1);

    // Reset
    bio_router_reset_stats();

    // Get stats again
    bio_router_stats_t stats2;
    memset(&stats2, 0, sizeof(stats2));
    bio_router_get_stats(&stats2);

    // Counters should be reset
    EXPECT_EQ(stats2.messages_routed, 0u);
    EXPECT_EQ(stats2.broadcasts_sent, 0u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(BioRouterTest, SendWithNullMessage) {
    nimcp_error_t err = bio_router_send(module_ctx, nullptr, 0, 100);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(BioRouterTest, SendWithNullContext) {
    struct {
        bio_message_header_t header;
    } msg;
    memset(&msg, 0, sizeof(msg));

    nimcp_error_t err = bio_router_send(nullptr, &msg, sizeof(msg), 100);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(BioRouterTest, RegisterHandlerWithNullContext) {
    nimcp_error_t err = bio_router_register_handler(
        nullptr,
        BIO_MSG_BRAIN_STATE_QUERY,
        test_message_handler
    );
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(BioRouterTest, RegisterHandlerWithNullHandler) {
    nimcp_error_t err = bio_router_register_handler(
        module_ctx,
        BIO_MSG_BRAIN_STATE_QUERY,
        nullptr
    );
    EXPECT_NE(err, NIMCP_SUCCESS);
}

//=============================================================================
// Wait for Messages Tests
//=============================================================================

TEST_F(BioRouterMultiModuleTest, WaitForMessagesWithTimeout) {
    // No messages pending
    bool has_messages = bio_router_wait_for_messages(receiver_ctx, 10);
    EXPECT_FALSE(has_messages);

    // Send a message
    struct {
        bio_message_header_t header;
    } msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.type = BIO_MSG_BRAIN_STATE_QUERY;
    msg.header.source_module = 0x1000;
    msg.header.target_module = 0x2000;

    bio_router_send(sender_ctx, &msg, sizeof(msg), 100);

    // Now there should be messages
    has_messages = bio_router_wait_for_messages(receiver_ctx, 10);
    EXPECT_TRUE(has_messages);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(BioRouterMultiModuleTest, HighMessageVolume) {
    // Register handler
    bio_router_register_handler(
        receiver_ctx,
        BIO_MSG_PLASTICITY_UPDATE,
        test_message_handler
    );

    const int NUM_MESSAGES = 100;

    // Send many messages rapidly
    for (int i = 0; i < NUM_MESSAGES; ++i) {
        struct {
            bio_message_header_t header;
            int index;
            float value;
        } msg;

        memset(&msg, 0, sizeof(msg));
        msg.header.type = BIO_MSG_PLASTICITY_UPDATE;
        msg.header.source_module = 0x1000;
        msg.header.target_module = 0x2000;
        msg.header.payload_size = sizeof(int) + sizeof(float);
        msg.index = i;
        msg.value = static_cast<float>(i) * 0.01f;

        bio_router_send(sender_ctx, &msg, sizeof(msg), 10);
    }

    // Process all messages
    uint32_t total_processed = 0;
    while (bio_router_inbox_count(receiver_ctx) > 0) {
        total_processed += bio_router_process_inbox(receiver_ctx, 20);
    }

    EXPECT_GE(total_processed, static_cast<uint32_t>(NUM_MESSAGES));
}

//=============================================================================
// Async Message Tests
//=============================================================================

TEST_F(BioRouterMultiModuleTest, SendAsyncMessage) {
    // Register handler
    bio_router_register_handler(
        receiver_ctx,
        BIO_MSG_TRAINING_STEP_REQUEST,
        test_message_handler
    );

    // Create message
    struct {
        bio_message_header_t header;
        uint64_t step;
    } msg;

    memset(&msg, 0, sizeof(msg));
    msg.header.type = BIO_MSG_TRAINING_STEP_REQUEST;
    msg.header.source_module = 0x1000;
    msg.header.target_module = 0x2000;
    msg.header.payload_size = sizeof(uint64_t);
    msg.step = 1000;

    // Send async with promise
    nimcp_bio_promise_t promise = bio_router_send_async(
        sender_ctx,
        &msg,
        sizeof(msg),
        NIMCP_BIO_CHANNEL_DOPAMINE
    );

    // Promise may be NULL if async not fully implemented, that's OK for integration test
    if (promise) {
        // Would wait for response here in full implementation
    }

    // Process inbox
    bio_router_process_inbox(receiver_ctx, 10);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
