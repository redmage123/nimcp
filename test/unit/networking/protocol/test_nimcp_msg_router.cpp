/**
 * @file test_nimcp_msg_router.cpp
 * @brief Unit tests for message router
 *
 * Tests cover:
 * - Router lifecycle (create, destroy, reset)
 * - Handler registration and unregistration
 * - Message routing (fast path and protobuf)
 * - Queue operations
 * - Statistics tracking
 * - Edge cases and error handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "networking/protocol/nimcp_msg_router.h"

/*=============================================================================
 * Test Fixtures
 *===========================================================================*/

class MsgRouterTest : public ::testing::Test {
protected:
    nimcp_msg_router_t* router = nullptr;

    void SetUp() override {
        nimcp_msg_reset_stats();
    }

    void TearDown() override {
        if (router) {
            nimcp_msg_router_destroy(router);
            router = nullptr;
        }
    }

    // Create router with default config
    void createDefaultRouter() {
        router = nimcp_msg_router_create(nullptr);
        ASSERT_NE(router, nullptr);
    }

    // Create router with queue enabled
    void createQueuedRouter() {
        nimcp_msg_router_config_t config = nimcp_msg_router_default_config();
        config.enable_queue = true;
        config.queue_size = 16;
        router = nimcp_msg_router_create(&config);
        ASSERT_NE(router, nullptr);
    }
};

/* Global test state for handlers */
static int g_handler_call_count = 0;
static nimcp_msg_type_t g_last_msg_type = (nimcp_msg_type_t)0;
static uint8_t g_last_payload[256] = {0};
static size_t g_last_payload_len = 0;

static void reset_handler_state() {
    g_handler_call_count = 0;
    g_last_msg_type = (nimcp_msg_type_t)0;
    memset(g_last_payload, 0, sizeof(g_last_payload));
    g_last_payload_len = 0;
}

/* Test handler */
static int test_handler(
    const nimcp_msg_header_t* header,
    const uint8_t* payload,
    size_t payload_len,
    void* user_data
) {
    g_handler_call_count++;
    g_last_msg_type = (nimcp_msg_type_t)header->msg_type;
    if (payload && payload_len > 0 && payload_len <= sizeof(g_last_payload)) {
        memcpy(g_last_payload, payload, payload_len);
        g_last_payload_len = payload_len;
    }
    return 0;
}

/* Test fast handler */
static int test_fast_handler(
    const nimcp_fast_msg_t* msg,
    void* user_data
) {
    g_handler_call_count++;
    g_last_msg_type = (nimcp_msg_type_t)msg->header.msg_type;
    memcpy(g_last_payload, msg->payload, NIMCP_MSG_FAST_PAYLOAD);
    g_last_payload_len = NIMCP_MSG_FAST_PAYLOAD;
    return 0;
}

/* Error handler */
static int error_handler(
    const nimcp_msg_header_t* header,
    const uint8_t* payload,
    size_t payload_len,
    void* user_data
) {
    g_handler_call_count++;
    return -1;  // Always return error
}

/*=============================================================================
 * Lifecycle Tests
 *===========================================================================*/

TEST_F(MsgRouterTest, Create_DefaultConfig_Succeeds) {
    router = nimcp_msg_router_create(nullptr);
    EXPECT_NE(router, nullptr);
}

TEST_F(MsgRouterTest, Create_CustomConfig_Succeeds) {
    nimcp_msg_router_config_t config = nimcp_msg_router_default_config();
    config.enable_queue = true;
    config.queue_size = 64;

    router = nimcp_msg_router_create(&config);
    EXPECT_NE(router, nullptr);
}

TEST_F(MsgRouterTest, Destroy_NullRouter_DoesNotCrash) {
    nimcp_msg_router_destroy(nullptr);
    SUCCEED();
}

TEST_F(MsgRouterTest, Reset_ClearsStats) {
    createDefaultRouter();

    // Route some messages to generate stats
    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);
    nimcp_msg_router_route(router, buffer, 24);

    // Reset
    EXPECT_EQ(nimcp_msg_router_reset(router), 0);

    // Stats should be cleared
    nimcp_msg_router_stats_t stats;
    nimcp_msg_router_get_stats(router, &stats);
    EXPECT_EQ(stats.messages_routed, 0u);
}

/*=============================================================================
 * Handler Registration Tests
 *===========================================================================*/

TEST_F(MsgRouterTest, Register_ValidHandler_Succeeds) {
    createDefaultRouter();
    reset_handler_state();

    int result = nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, test_handler, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(nimcp_msg_router_has_handler(router, MSG_TYPE_HEARTBEAT));
}

TEST_F(MsgRouterTest, Register_MultipleHandlers_Succeeds) {
    createDefaultRouter();
    reset_handler_state();

    EXPECT_EQ(nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, test_handler, nullptr), 0);
    EXPECT_EQ(nimcp_msg_router_register(router, MSG_TYPE_SYNC, test_handler, nullptr), 0);
    EXPECT_EQ(nimcp_msg_router_register(router, MSG_TYPE_DANGER, test_handler, nullptr), 0);

    EXPECT_TRUE(nimcp_msg_router_has_handler(router, MSG_TYPE_HEARTBEAT));
    EXPECT_TRUE(nimcp_msg_router_has_handler(router, MSG_TYPE_SYNC));
    EXPECT_TRUE(nimcp_msg_router_has_handler(router, MSG_TYPE_DANGER));
}

TEST_F(MsgRouterTest, Register_DuplicateType_Updates) {
    createDefaultRouter();
    reset_handler_state();

    EXPECT_EQ(nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, test_handler, nullptr), 0);
    EXPECT_EQ(nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, error_handler, nullptr), 0);

    // Second registration should update, not add
    EXPECT_TRUE(nimcp_msg_router_has_handler(router, MSG_TYPE_HEARTBEAT));
}

TEST_F(MsgRouterTest, RegisterFast_ValidHandler_Succeeds) {
    createDefaultRouter();
    reset_handler_state();

    int result = nimcp_msg_router_register_fast(router, MSG_TYPE_HEARTBEAT, test_fast_handler, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(MsgRouterTest, RegisterFast_NonFastType_Fails) {
    createDefaultRouter();
    reset_handler_state();

    // NEURAL_STATE is not a fast path type
    int result = nimcp_msg_router_register_fast(router, MSG_TYPE_NEURAL_STATE, test_fast_handler, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MsgRouterTest, Unregister_ExistingHandler_Succeeds) {
    createDefaultRouter();
    reset_handler_state();

    nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, test_handler, nullptr);
    EXPECT_TRUE(nimcp_msg_router_has_handler(router, MSG_TYPE_HEARTBEAT));

    int result = nimcp_msg_router_unregister(router, MSG_TYPE_HEARTBEAT);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(nimcp_msg_router_has_handler(router, MSG_TYPE_HEARTBEAT));
}

TEST_F(MsgRouterTest, Unregister_NonexistentHandler_Fails) {
    createDefaultRouter();

    int result = nimcp_msg_router_unregister(router, MSG_TYPE_HEARTBEAT);
    EXPECT_EQ(result, -1);
}

TEST_F(MsgRouterTest, HasHandler_UnregisteredType_ReturnsFalse) {
    createDefaultRouter();

    EXPECT_FALSE(nimcp_msg_router_has_handler(router, MSG_TYPE_HEARTBEAT));
}

/*=============================================================================
 * Routing Tests
 *===========================================================================*/

TEST_F(MsgRouterTest, Route_RegisteredHandler_CallsHandler) {
    createDefaultRouter();
    reset_handler_state();

    nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, test_handler, nullptr);

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 0xDEADBEEF, 0.75f, 0.25f);
    nimcp_fast_msg_serialize(&msg, buffer);

    int result = nimcp_msg_router_route(router, buffer, 24);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_msg_type, MSG_TYPE_HEARTBEAT);
}

TEST_F(MsgRouterTest, RouteFast_RegisteredFastHandler_CallsHandler) {
    createDefaultRouter();
    reset_handler_state();

    nimcp_msg_router_register_fast(router, MSG_TYPE_SYNC, test_fast_handler, nullptr);

    nimcp_fast_msg_t msg;
    nimcp_fast_msg_sync(&msg, 42, 0.8f, 3.14f);

    int result = nimcp_msg_router_route_fast(router, &msg);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_msg_type, MSG_TYPE_SYNC);
}

TEST_F(MsgRouterTest, Route_UnregisteredType_ReturnsError) {
    createDefaultRouter();
    reset_handler_state();

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    int result = nimcp_msg_router_route(router, buffer, 24);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_handler_call_count, 0);
}

TEST_F(MsgRouterTest, Route_DefaultHandler_CalledForUnregistered) {
    reset_handler_state();

    nimcp_msg_router_config_t config = nimcp_msg_router_default_config();
    config.default_handler = test_handler;
    router = nimcp_msg_router_create(&config);

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    int result = nimcp_msg_router_route(router, buffer, 24);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 1);
}

TEST_F(MsgRouterTest, Route_HandlerReturnsError_TracksError) {
    createDefaultRouter();
    reset_handler_state();

    nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, error_handler, nullptr);

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    int result = nimcp_msg_router_route(router, buffer, 24);

    EXPECT_EQ(result, -1);

    nimcp_msg_router_stats_t stats;
    nimcp_msg_router_get_stats(router, &stats);
    EXPECT_EQ(stats.handler_errors, 1u);
}

TEST_F(MsgRouterTest, Route_InvalidMagic_ReturnsError) {
    createDefaultRouter();
    reset_handler_state();

    nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, test_handler, nullptr);

    uint8_t buffer[24] = {0xFF, 0xFF};  // Invalid magic

    int result = nimcp_msg_router_route(router, buffer, 24);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_handler_call_count, 0);
}

TEST_F(MsgRouterTest, Route_TruncatedMessage_ReturnsError) {
    createDefaultRouter();

    uint8_t buffer[4] = {0};  // Too short

    int result = nimcp_msg_router_route(router, buffer, 4);
    EXPECT_EQ(result, -1);
}

TEST_F(MsgRouterTest, RouteParsed_ValidMessage_Succeeds) {
    createDefaultRouter();
    reset_handler_state();

    nimcp_msg_router_register(router, MSG_TYPE_DANGER, test_handler, nullptr);

    nimcp_msg_header_t header;
    uint8_t payload[16] = {1, 2, 3, 4};
    nimcp_msg_header_init(&header, MSG_TYPE_DANGER, MSG_FLAG_BROADCAST, 16);

    int result = nimcp_msg_router_route_parsed(router, &header, payload, 16);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 1);
}

/*=============================================================================
 * Queue Tests
 *===========================================================================*/

TEST_F(MsgRouterTest, Queue_Enabled_AcceptsMessages) {
    createQueuedRouter();
    reset_handler_state();

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    int result = nimcp_msg_router_queue(router, buffer, 24);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_msg_router_queue_depth(router), 1u);
}

TEST_F(MsgRouterTest, Queue_Disabled_RejectsMessages) {
    createDefaultRouter();

    uint8_t buffer[24] = {0};

    int result = nimcp_msg_router_queue(router, buffer, 24);
    EXPECT_EQ(result, -1);
}

TEST_F(MsgRouterTest, Queue_Full_ReturnsError) {
    nimcp_msg_router_config_t config = nimcp_msg_router_default_config();
    config.enable_queue = true;
    config.queue_size = 2;
    router = nimcp_msg_router_create(&config);

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    // Fill queue
    EXPECT_EQ(nimcp_msg_router_queue(router, buffer, 24), 0);
    EXPECT_EQ(nimcp_msg_router_queue(router, buffer, 24), 0);

    // Should fail
    EXPECT_EQ(nimcp_msg_router_queue(router, buffer, 24), -1);

    nimcp_msg_router_stats_t stats;
    nimcp_msg_router_get_stats(router, &stats);
    EXPECT_EQ(stats.queue_overflows, 1u);
}

TEST_F(MsgRouterTest, ProcessQueue_RoutesMessages) {
    createQueuedRouter();
    reset_handler_state();

    nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, test_handler, nullptr);

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    nimcp_msg_router_queue(router, buffer, 24);
    nimcp_msg_router_queue(router, buffer, 24);
    nimcp_msg_router_queue(router, buffer, 24);

    EXPECT_EQ(nimcp_msg_router_queue_depth(router), 3u);

    int processed = nimcp_msg_router_process_queue(router, 0);

    EXPECT_EQ(processed, 3);
    EXPECT_EQ(nimcp_msg_router_queue_depth(router), 0u);
    EXPECT_EQ(g_handler_call_count, 3);
}

TEST_F(MsgRouterTest, ProcessQueue_LimitedProcessing) {
    createQueuedRouter();
    reset_handler_state();

    nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, test_handler, nullptr);

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    for (int i = 0; i < 5; i++) {
        nimcp_msg_router_queue(router, buffer, 24);
    }

    // Process only 2
    int processed = nimcp_msg_router_process_queue(router, 2);

    EXPECT_EQ(processed, 2);
    EXPECT_EQ(nimcp_msg_router_queue_depth(router), 3u);
    EXPECT_EQ(g_handler_call_count, 2);
}

TEST_F(MsgRouterTest, ClearQueue_EmptiesQueue) {
    createQueuedRouter();

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    nimcp_msg_router_queue(router, buffer, 24);
    nimcp_msg_router_queue(router, buffer, 24);

    EXPECT_EQ(nimcp_msg_router_queue_depth(router), 2u);

    nimcp_msg_router_clear_queue(router);

    EXPECT_EQ(nimcp_msg_router_queue_depth(router), 0u);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(MsgRouterTest, Stats_TracksMessagesRouted) {
    createDefaultRouter();
    reset_handler_state();

    nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, test_handler, nullptr);
    nimcp_msg_router_register(router, MSG_TYPE_SYNC, test_handler, nullptr);

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];

    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);
    nimcp_msg_router_route(router, buffer, 24);

    nimcp_fast_msg_sync(&msg, 2, 0.8f, 1.5f);
    nimcp_fast_msg_serialize(&msg, buffer);
    nimcp_msg_router_route(router, buffer, 24);

    nimcp_msg_router_stats_t stats;
    nimcp_msg_router_get_stats(router, &stats);

    EXPECT_EQ(stats.messages_routed, 2u);
    EXPECT_EQ(stats.fast_messages_routed, 2u);
    EXPECT_GT(stats.bytes_routed, 0u);
}

TEST_F(MsgRouterTest, Stats_TracksUnhandled) {
    createDefaultRouter();

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    nimcp_msg_router_route(router, buffer, 24);

    nimcp_msg_router_stats_t stats;
    nimcp_msg_router_get_stats(router, &stats);

    EXPECT_EQ(stats.unhandled_messages, 1u);
}

TEST_F(MsgRouterTest, ResetStats_ClearsAll) {
    createDefaultRouter();
    reset_handler_state();

    nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, test_handler, nullptr);

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);
    nimcp_msg_router_route(router, buffer, 24);

    nimcp_msg_router_reset_stats(router);

    nimcp_msg_router_stats_t stats;
    nimcp_msg_router_get_stats(router, &stats);

    EXPECT_EQ(stats.messages_routed, 0u);
    EXPECT_EQ(stats.bytes_routed, 0u);
}

/*=============================================================================
 * Edge Case Tests
 *===========================================================================*/

TEST_F(MsgRouterTest, Route_NullParams_ReturnsError) {
    createDefaultRouter();

    uint8_t buffer[24] = {0};

    EXPECT_EQ(nimcp_msg_router_route(nullptr, buffer, 24), -1);
    EXPECT_EQ(nimcp_msg_router_route(router, nullptr, 24), -1);
}

TEST_F(MsgRouterTest, RouteFast_NullParams_ReturnsError) {
    createDefaultRouter();

    nimcp_fast_msg_t msg;

    EXPECT_EQ(nimcp_msg_router_route_fast(nullptr, &msg), -1);
    EXPECT_EQ(nimcp_msg_router_route_fast(router, nullptr), -1);
}

TEST_F(MsgRouterTest, Register_NullParams_ReturnsError) {
    createDefaultRouter();

    EXPECT_EQ(nimcp_msg_router_register(nullptr, MSG_TYPE_HEARTBEAT, test_handler, nullptr), -1);
    EXPECT_EQ(nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, nullptr, nullptr), -1);
}

TEST_F(MsgRouterTest, QueueDepth_NullRouter_ReturnsZero) {
    EXPECT_EQ(nimcp_msg_router_queue_depth(nullptr), 0u);
}

TEST_F(MsgRouterTest, GetStats_NullParams_ReturnsError) {
    createDefaultRouter();
    nimcp_msg_router_stats_t stats;

    EXPECT_EQ(nimcp_msg_router_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(nimcp_msg_router_get_stats(router, nullptr), -1);
}

/*=============================================================================
 * Integration-style Tests
 *===========================================================================*/

TEST_F(MsgRouterTest, MultipleMessageTypes_AllRouted) {
    createDefaultRouter();
    reset_handler_state();

    // Register handlers for multiple types
    nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT, test_handler, nullptr);
    nimcp_msg_router_register(router, MSG_TYPE_SYNC, test_handler, nullptr);
    nimcp_msg_router_register(router, MSG_TYPE_DANGER, test_handler, nullptr);
    nimcp_msg_router_register(router, MSG_TYPE_ACK, test_handler, nullptr);

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];

    // Send different message types
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);
    EXPECT_EQ(nimcp_msg_router_route(router, buffer, 24), 0);

    nimcp_fast_msg_sync(&msg, 2, 0.8f, 1.5f);
    nimcp_fast_msg_serialize(&msg, buffer);
    EXPECT_EQ(nimcp_msg_router_route(router, buffer, 24), 0);

    nimcp_fast_msg_danger(&msg, 3, 99, 0.9f);
    nimcp_fast_msg_serialize(&msg, buffer);
    EXPECT_EQ(nimcp_msg_router_route(router, buffer, 24), 0);

    nimcp_fast_msg_ack(&msg, 4, 1000);
    nimcp_fast_msg_serialize(&msg, buffer);
    EXPECT_EQ(nimcp_msg_router_route(router, buffer, 24), 0);

    EXPECT_EQ(g_handler_call_count, 4);

    nimcp_msg_router_stats_t stats;
    nimcp_msg_router_get_stats(router, &stats);
    EXPECT_EQ(stats.messages_routed, 4u);
}

TEST_F(MsgRouterTest, UserData_PassedToHandler) {
    createDefaultRouter();

    int user_value = 42;
    bool handler_called = false;

    auto handler_with_data = [](
        const nimcp_msg_header_t* header,
        const uint8_t* payload,
        size_t payload_len,
        void* user_data
    ) -> int {
        int* value = static_cast<int*>(user_data);
        EXPECT_EQ(*value, 42);
        return 0;
    };

    nimcp_msg_router_register(router, MSG_TYPE_HEARTBEAT,
        +[](const nimcp_msg_header_t* h, const uint8_t* p, size_t l, void* u) -> int {
            int* val = static_cast<int*>(u);
            return (*val == 42) ? 0 : -1;
        },
        &user_value);

    nimcp_fast_msg_t msg;
    uint8_t buffer[24];
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    int result = nimcp_msg_router_route(router, buffer, 24);
    EXPECT_EQ(result, 0);
}
