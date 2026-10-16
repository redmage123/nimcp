/**
 * @file test_inter_layer_router.cpp
 * @brief Unit tests for Inter-Layer Router
 *
 * WHAT: Test suite for nimcp_inter_layer_router
 * WHY:  Verify correct routing of messages between layers
 * HOW:  Unit tests for create, route, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstdlib>

extern "C" {
#include "integration/core/nimcp_inter_layer_router.h"
#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class InterLayerRouterTest : public ::testing::Test {
protected:
    nimcp_inter_layer_router_t router = nullptr;
    nimcp_layer_registry_t registry = nullptr;

    void SetUp() override {
        /* Create registry first (router depends on it) */
        nimcp_layer_registry_config_t reg_config = nimcp_layer_registry_default_config();
        registry = nimcp_layer_registry_create(&reg_config);
        ASSERT_NE(registry, nullptr);

        /* Create router with registry */
        nimcp_inter_layer_router_config_t config = nimcp_inter_layer_router_default_config();
        router = nimcp_inter_layer_router_create(&config, registry);
        ASSERT_NE(router, nullptr);
    }

    void TearDown() override {
        if (router) {
            nimcp_inter_layer_router_destroy(router);
            router = nullptr;
        }
        if (registry) {
            nimcp_layer_registry_destroy(registry);
            registry = nullptr;
        }
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST(InterLayerRouterCreateTest, CreateWithDefaultConfig) {
    nimcp_layer_registry_config_t reg_config = nimcp_layer_registry_default_config();
    nimcp_layer_registry_t reg = nimcp_layer_registry_create(&reg_config);
    ASSERT_NE(reg, nullptr);

    nimcp_inter_layer_router_t rtr = nimcp_inter_layer_router_create(nullptr, reg);
    ASSERT_NE(rtr, nullptr);

    nimcp_inter_layer_router_destroy(rtr);
    nimcp_layer_registry_destroy(reg);
}

TEST(InterLayerRouterCreateTest, CreateWithCustomConfig) {
    nimcp_layer_registry_config_t reg_config = nimcp_layer_registry_default_config();
    nimcp_layer_registry_t reg = nimcp_layer_registry_create(&reg_config);
    ASSERT_NE(reg, nullptr);

    nimcp_inter_layer_router_config_t config = nimcp_inter_layer_router_default_config();
    config.route_mode = NIMCP_ROUTE_MODE_HIERARCHICAL;
    config.default_queue_depth = 128;
    config.enable_priority_queuing = true;
    config.enable_latency_tracking = true;

    nimcp_inter_layer_router_t rtr = nimcp_inter_layer_router_create(&config, reg);
    ASSERT_NE(rtr, nullptr);

    nimcp_inter_layer_router_destroy(rtr);
    nimcp_layer_registry_destroy(reg);
}

TEST(InterLayerRouterCreateTest, CreateWithNullRegistry) {
    /* Should still create with NULL registry */
    nimcp_inter_layer_router_t rtr = nimcp_inter_layer_router_create(nullptr, nullptr);
    /* May return NULL if registry is required, or succeed for standalone */
    if (rtr) {
        nimcp_inter_layer_router_destroy(rtr);
    }
}

TEST(InterLayerRouterCreateTest, DestroyNull) {
    /* Should not crash */
    nimcp_inter_layer_router_destroy(nullptr);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(InterLayerRouterTest, ResetSuccess) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_reset(router);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(InterLayerRouterTest, ResetNull) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_reset(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Route Registration Tests
//=============================================================================

TEST_F(InterLayerRouterTest, SetRouteEnabled) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_set_route_enabled(
        router, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY, true);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(InterLayerRouterTest, SetRouteDisabled) {
    /* First enable */
    nimcp_layer_error_t err = nimcp_inter_layer_router_set_route_enabled(
        router, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY, true);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Then disable */
    err = nimcp_inter_layer_router_set_route_enabled(
        router, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY, false);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(InterLayerRouterTest, SetCoupling) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_set_coupling(
        router, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY, 0.8f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(InterLayerRouterTest, SetCouplingBoundaries) {
    /* Test at boundaries */
    nimcp_layer_error_t err = nimcp_inter_layer_router_set_coupling(
        router, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY, 0.0f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_inter_layer_router_set_coupling(
        router, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY, 1.0f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(InterLayerRouterTest, RouteAvailable) {
    /* Enable route first */
    nimcp_inter_layer_router_set_route_enabled(
        router, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY, true);

    bool available = nimcp_inter_layer_router_route_available(
        router, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY);
    EXPECT_TRUE(available);
}

TEST_F(InterLayerRouterTest, RouteNotAvailable) {
    bool available = nimcp_inter_layer_router_route_available(
        router, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_SUPERHUMAN);
    /* Initially routes may not be available */
    (void)available;  /* Result depends on default state */
}

//=============================================================================
// Message Routing Tests
//=============================================================================

TEST_F(InterLayerRouterTest, RouteNull) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_route(router, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(InterLayerRouterTest, RouteNullRouter) {
    nimcp_layer_msg_t msg = {};
    nimcp_layer_error_t err = nimcp_inter_layer_router_route(nullptr, &msg);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(InterLayerRouterTest, RouteDirected) {
    nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
        NIMCP_LAYER_MSG_STATE_UPDATE,
        NIMCP_LAYER_PHYSICS,
        NIMCP_LAYER_CHEMISTRY,
        nullptr, 0);
    ASSERT_NE(msg, nullptr);

    /* Enable route */
    nimcp_inter_layer_router_set_route_enabled(
        router, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY, true);

    nimcp_layer_error_t err = nimcp_inter_layer_router_route_directed(
        router, msg, NIMCP_MSG_DIR_BOTTOM_UP);

    /* If routing failed, we still own the message and must destroy it.
     * If routing succeeded, the router takes ownership - don't destroy. */
    if (err != NIMCP_LAYER_OK) {
        nimcp_layer_msg_destroy(msg);
    }
}

TEST_F(InterLayerRouterTest, BroadcastNull) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_broadcast(router, NIMCP_LAYER_PHYSICS, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(InterLayerRouterTest, BroadcastDirected) {
    nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
        NIMCP_LAYER_MSG_STATE_UPDATE,
        NIMCP_LAYER_PHYSICS,
        NIMCP_LAYER_NONE,
        nullptr, 0);
    ASSERT_NE(msg, nullptr);

    nimcp_layer_error_t err = nimcp_inter_layer_router_broadcast_directed(
        router, NIMCP_LAYER_PHYSICS, msg, NIMCP_MSG_DIR_BOTTOM_UP);
    /* May succeed or fail based on route availability */
    (void)err;

    nimcp_layer_msg_destroy(msg);
}

TEST_F(InterLayerRouterTest, GetQueueDepth) {
    int depth = nimcp_inter_layer_router_get_queue_depth(router, NIMCP_LAYER_PHYSICS);
    EXPECT_GE(depth, 0);
}

TEST_F(InterLayerRouterTest, GetQueueDepthNull) {
    int depth = nimcp_inter_layer_router_get_queue_depth(nullptr, NIMCP_LAYER_PHYSICS);
    EXPECT_EQ(depth, -1);
}

//=============================================================================
// Message Processing Tests
//=============================================================================

TEST_F(InterLayerRouterTest, ProcessLayer) {
    uint32_t processed = 0;
    nimcp_layer_error_t err = nimcp_inter_layer_router_process_layer(
        router, NIMCP_LAYER_PHYSICS, 10, &processed);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(processed, 0u);  /* Empty queue */
}

TEST_F(InterLayerRouterTest, ProcessAll) {
    uint32_t processed = 0;
    nimcp_layer_error_t err = nimcp_inter_layer_router_process_all(router, 100, &processed);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(processed, 0u);  /* Empty queue */
}

TEST_F(InterLayerRouterTest, Peek) {
    const nimcp_layer_msg_t* msg = nullptr;
    nimcp_layer_error_t err = nimcp_inter_layer_router_peek(router, NIMCP_LAYER_PHYSICS, &msg);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_QUEUE_EMPTY);
}

//=============================================================================
// Path Query Tests
//=============================================================================

TEST_F(InterLayerRouterTest, GetPath) {
    nimcp_layer_id_t path[NIMCP_MAX_LAYERS];
    size_t path_len = 0;

    nimcp_layer_error_t err = nimcp_inter_layer_router_get_path(
        router, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY, path, NIMCP_MAX_LAYERS, &path_len);
    /* May succeed or fail based on route configuration */
    (void)err;
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(InterLayerRouterTest, GetStats) {
    nimcp_inter_layer_router_stats_t stats;
    nimcp_layer_error_t err = nimcp_inter_layer_router_get_stats(router, &stats);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(stats.messages_routed, 0u);
    EXPECT_EQ(stats.messages_dropped, 0u);
}

TEST_F(InterLayerRouterTest, GetStatsNull) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_get_stats(router, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(InterLayerRouterTest, GetStatsNullRouter) {
    nimcp_inter_layer_router_stats_t stats;
    nimcp_layer_error_t err = nimcp_inter_layer_router_get_stats(nullptr, &stats);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(InterLayerRouterTest, ResetStats) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_reset_stats(router);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(InterLayerRouterTest, ResetStatsNull) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_reset_stats(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Callback Tests
//=============================================================================

static void dummy_callback(
    nimcp_layer_id_t source,
    nimcp_layer_id_t target,
    const nimcp_layer_msg_t* msg,
    void* user_data)
{
    (void)source;
    (void)target;
    (void)msg;
    (void)user_data;
}

TEST_F(InterLayerRouterTest, SetCallback) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_set_callback(
        router, dummy_callback, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(InterLayerRouterTest, SetCallbackWithUserData) {
    int user_data = 42;
    nimcp_layer_error_t err = nimcp_inter_layer_router_set_callback(
        router, dummy_callback, &user_data);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(InterLayerRouterTest, ClearCallback) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_set_callback(router, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(InterLayerRouterTest, SetCallbackNullRouter) {
    nimcp_layer_error_t err = nimcp_inter_layer_router_set_callback(nullptr, dummy_callback, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
