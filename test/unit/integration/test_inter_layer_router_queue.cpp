/**
 * @file test_inter_layer_router_queue.cpp
 * @brief Unit tests for inter-layer router configured queue depth (Tier 4)
 *
 * WHAT: Verify router uses configured queue depth instead of hardcoded 256
 * WHY: Hardcoded array wastes memory or limits capacity depending on use case
 * HOW: Create routers with different queue depths, test capacity and overflow
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "integration/core/nimcp_inter_layer_router.h"
}

#include "utils/nimcp_test_base.h"

//=============================================================================
// Test Fixture
//=============================================================================

class InterLayerRouterQueueTest : public NimcpTestBase {
protected:
    nimcp_inter_layer_router_t router = nullptr;

    void TearDown() override {
        if (router) {
            nimcp_inter_layer_router_destroy(router);
            router = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Test 1: Default queue depth
//=============================================================================

TEST_F(InterLayerRouterQueueTest, DefaultQueueDepth) {
    // WHAT: Create router with default config
    // WHY: Default should still work (256 queue depth)

    router = nimcp_inter_layer_router_create(nullptr, nullptr);
    ASSERT_NE(router, nullptr);

    // Should be able to route messages
    nimcp_inter_layer_router_stats_t stats;
    nimcp_layer_error_t err = nimcp_inter_layer_router_get_stats(router, &stats);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(stats.messages_routed, 0u);
}

//=============================================================================
// Test 2: Custom small queue depth
//=============================================================================

TEST_F(InterLayerRouterQueueTest, SmallQueueDepth) {
    // WHAT: Create router with small queue (8 messages)
    // WHY: Verify configured depth is respected

    nimcp_inter_layer_router_config_t config = nimcp_inter_layer_router_default_config();
    config.default_queue_depth = 8;

    router = nimcp_inter_layer_router_create(&config, nullptr);
    ASSERT_NE(router, nullptr);

    // Fill queue for layer 0 with 8 messages
    for (int i = 0; i < 8; i++) {
        nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
            NIMCP_LAYER_MSG_DATA_PUSH, NIMCP_LAYER_SENSORY,
            (nimcp_layer_id_t)0, nullptr, 0);
        if (!msg) continue;

        nimcp_layer_error_t err = nimcp_inter_layer_router_route(router, msg);
        EXPECT_EQ(err, NIMCP_LAYER_OK) << "Should accept message " << i;
    }

    // 9th message should fail — queue full
    nimcp_layer_msg_t* overflow_msg = nimcp_layer_msg_create(
        NIMCP_LAYER_MSG_DATA_PUSH, NIMCP_LAYER_SENSORY,
        (nimcp_layer_id_t)0, nullptr, 0);
    if (overflow_msg) {
        nimcp_layer_error_t err = nimcp_inter_layer_router_route(router, overflow_msg);
        EXPECT_EQ(err, NIMCP_LAYER_ERR_QUEUE_FULL);
        nimcp_layer_msg_destroy(overflow_msg);
    }

    // Verify queue depth
    int depth = nimcp_inter_layer_router_get_queue_depth(router, (nimcp_layer_id_t)0);
    EXPECT_EQ(depth, 8);
}

//=============================================================================
// Test 3: Large queue depth
//=============================================================================

TEST_F(InterLayerRouterQueueTest, LargeQueueDepth) {
    // WHAT: Create router with large queue (1024 messages)
    // WHY: Verify queue can hold more than old hardcoded 256

    nimcp_inter_layer_router_config_t config = nimcp_inter_layer_router_default_config();
    config.default_queue_depth = 1024;

    router = nimcp_inter_layer_router_create(&config, nullptr);
    ASSERT_NE(router, nullptr);

    // Fill queue with 512 messages (exceeds old 256 limit)
    int routed = 0;
    for (int i = 0; i < 512; i++) {
        nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
            NIMCP_LAYER_MSG_DATA_PUSH, NIMCP_LAYER_SENSORY,
            (nimcp_layer_id_t)0, nullptr, 0);
        if (!msg) continue;

        nimcp_layer_error_t err = nimcp_inter_layer_router_route(router, msg);
        if (err == NIMCP_LAYER_OK) routed++;
    }

    // Should have routed all 512 (was capped at 256 before fix)
    EXPECT_EQ(routed, 512);

    int depth = nimcp_inter_layer_router_get_queue_depth(router, (nimcp_layer_id_t)0);
    EXPECT_EQ(depth, 512);
}

//=============================================================================
// Test 4: Process and re-fill with configured depth
//=============================================================================

TEST_F(InterLayerRouterQueueTest, ProcessAndRefillWithConfiguredDepth) {
    // WHAT: Fill queue, process messages, refill
    // WHY: Verify circular buffer works with dynamic capacity

    nimcp_inter_layer_router_config_t config = nimcp_inter_layer_router_default_config();
    config.default_queue_depth = 16;

    router = nimcp_inter_layer_router_create(&config, nullptr);
    ASSERT_NE(router, nullptr);

    // Fill with 16 messages
    for (int i = 0; i < 16; i++) {
        nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
            NIMCP_LAYER_MSG_DATA_PUSH, NIMCP_LAYER_SENSORY,
            (nimcp_layer_id_t)0, nullptr, 0);
        if (msg) nimcp_inter_layer_router_route(router, msg);
    }

    EXPECT_EQ(nimcp_inter_layer_router_get_queue_depth(router, (nimcp_layer_id_t)0), 16);

    // Process 8 messages
    uint32_t processed = 0;
    nimcp_inter_layer_router_process_layer(router, (nimcp_layer_id_t)0, 8, &processed);
    EXPECT_EQ(processed, 8u);
    EXPECT_EQ(nimcp_inter_layer_router_get_queue_depth(router, (nimcp_layer_id_t)0), 8);

    // Refill the 8 freed slots (tests wrap-around in circular buffer)
    int refilled = 0;
    for (int i = 0; i < 8; i++) {
        nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
            NIMCP_LAYER_MSG_DATA_PUSH, NIMCP_LAYER_SENSORY,
            (nimcp_layer_id_t)0, nullptr, 0);
        if (!msg) continue;
        nimcp_layer_error_t err = nimcp_inter_layer_router_route(router, msg);
        if (err == NIMCP_LAYER_OK) refilled++;
    }

    EXPECT_EQ(refilled, 8);
    EXPECT_EQ(nimcp_inter_layer_router_get_queue_depth(router, (nimcp_layer_id_t)0), 16);

    // Should be full now
    nimcp_layer_msg_t* overflow = nimcp_layer_msg_create(
        NIMCP_LAYER_MSG_DATA_PUSH, NIMCP_LAYER_SENSORY,
        (nimcp_layer_id_t)0, nullptr, 0);
    if (overflow) {
        nimcp_layer_error_t err = nimcp_inter_layer_router_route(router, overflow);
        EXPECT_EQ(err, NIMCP_LAYER_ERR_QUEUE_FULL);
        nimcp_layer_msg_destroy(overflow);
    }
}

//=============================================================================
// Test 5: Reset clears all queues with configured depth
//=============================================================================

TEST_F(InterLayerRouterQueueTest, ResetClearsConfiguredQueues) {
    // WHAT: Reset router with messages in dynamic queues
    // WHY: Reset must work with new dynamic allocation

    nimcp_inter_layer_router_config_t config = nimcp_inter_layer_router_default_config();
    config.default_queue_depth = 32;

    router = nimcp_inter_layer_router_create(&config, nullptr);
    ASSERT_NE(router, nullptr);

    // Add some messages
    for (int i = 0; i < 10; i++) {
        nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
            NIMCP_LAYER_MSG_DATA_PUSH, NIMCP_LAYER_SENSORY,
            (nimcp_layer_id_t)0, nullptr, 0);
        if (msg) nimcp_inter_layer_router_route(router, msg);
    }

    EXPECT_EQ(nimcp_inter_layer_router_get_queue_depth(router, (nimcp_layer_id_t)0), 10);

    // Reset should clear everything
    nimcp_layer_error_t err = nimcp_inter_layer_router_reset(router);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(nimcp_inter_layer_router_get_queue_depth(router, (nimcp_layer_id_t)0), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
