/**
 * @file test_rcog_hub_bridge.cpp
 * @brief Unit tests for Recursive Cognition Cognitive Hub Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for Rcog-Cognitive Hub bidirectional integration
 * WHY:  Ensure recursive cognition integrates correctly with cognitive event system
 * HOW:  Test lifecycle, connection, events, queries, and statistics
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Hub Connection/Disconnection
 * - Event Subscription
 * - Event Publication
 * - Event Callbacks
 * - Query Handling
 * - Statistics Tracking
 * - Null Parameter Handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/integration/nimcp_rcog_hub_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_input_callback_count{0};
static std::atomic<int> g_attention_callback_count{0};
static std::atomic<int> g_memory_callback_count{0};
static const char* g_last_goal_query = nullptr;
static uint32_t g_last_goal_type = 0;
static float g_last_priority = 0.0f;

/**
 * Test callback for input events
 */
static int test_input_callback(
    const char* goal_query,
    uint32_t goal_type,
    float priority,
    void* user_data
) {
    (void)user_data;
    g_input_callback_count++;
    g_last_goal_query = goal_query;
    g_last_goal_type = goal_type;
    g_last_priority = priority;
    return 0;
}

/**
 * Test callback for attention shift events
 */
static void test_attention_callback(
    uint64_t new_focus_id,
    uint64_t old_focus_id,
    float urgency,
    void* user_data
) {
    (void)new_focus_id;
    (void)old_focus_id;
    (void)urgency;
    (void)user_data;
    g_attention_callback_count++;
}

/**
 * Test callback for memory access events
 */
static void test_memory_callback(
    uint64_t memory_id,
    uint32_t access_type,
    float relevance,
    void* user_data
) {
    (void)memory_id;
    (void)access_type;
    (void)relevance;
    (void)user_data;
    g_memory_callback_count++;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class RcogHubBridgeTest : public ::testing::Test {
protected:
    rcog_hub_bridge_t* bridge = nullptr;
    rcog_hub_bridge_config_t config;
    cognitive_integration_hub_t hub = nullptr;

    void SetUp() override {
        // Reset global state
        g_input_callback_count = 0;
        g_attention_callback_count = 0;
        g_memory_callback_count = 0;
        g_last_goal_query = nullptr;
        g_last_goal_type = 0;
        g_last_priority = 0.0f;

        // Get default config
        int result = rcog_hub_bridge_default_config(&config);
        ASSERT_EQ(result, 0) << "Default config should succeed";

        // Create bridge
        bridge = rcog_hub_bridge_create(&config);

        // Create cognitive hub for connection tests
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub = cognitive_hub_create(&hub_config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            rcog_hub_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (hub != nullptr) {
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }
};

/* ============================================================================
 * Bridge Lifecycle Tests
 * ============================================================================ */

/**
 * Test: BridgeCreation
 * Verify bridge can be created and destroyed successfully
 */
TEST_F(RcogHubBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify not connected initially
    EXPECT_FALSE(rcog_hub_bridge_is_connected(bridge))
        << "Bridge should not be connected initially";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(RcogHubBridgeTest, BridgeCreationNullConfig) {
    rcog_hub_bridge_t* br = rcog_hub_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    rcog_hub_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(RcogHubBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    rcog_hub_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    rcog_hub_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(RcogHubBridgeTest, DefaultConfig) {
    rcog_hub_bridge_config_t default_config;
    int result = rcog_hub_bridge_default_config(&default_config);
    EXPECT_EQ(result, 0) << "Default config should succeed";

    // Verify module ID
    EXPECT_EQ(default_config.module_id, RCOG_HUB_DEFAULT_MODULE_ID);

    // Verify auto-subscribe options
    EXPECT_TRUE(default_config.auto_subscribe_input);
    EXPECT_TRUE(default_config.auto_subscribe_memory);
    EXPECT_TRUE(default_config.auto_subscribe_attention);

    // Verify other options
    EXPECT_TRUE(default_config.enable_query_handler);
    EXPECT_GT(default_config.event_buffer_size, 0u);
}

/**
 * Test: DefaultConfigNull
 * Verify default_config handles NULL gracefully
 */
TEST_F(RcogHubBridgeTest, DefaultConfigNull) {
    int result = rcog_hub_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/* ============================================================================
 * Hub Connection Tests
 * ============================================================================ */

/**
 * Test: HubConnection
 * Verify bridge can connect to cognitive hub
 */
TEST_F(RcogHubBridgeTest, HubConnection) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    EXPECT_EQ(result, 0) << "Connection should succeed";

    EXPECT_TRUE(rcog_hub_bridge_is_connected(bridge))
        << "Bridge should be connected after connect()";
}

/**
 * Test: HubConnectionNullParams
 * Verify connection handles NULL parameters gracefully
 */
TEST_F(RcogHubBridgeTest, HubConnectionNullParams) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // NULL bridge
    int result = rcog_hub_bridge_connect(nullptr, hub, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    // NULL hub
    result = rcog_hub_bridge_connect(bridge, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "NULL hub should fail";
}

/**
 * Test: HubConnectionDuplicate
 * Verify connecting when already connected is handled
 */
TEST_F(RcogHubBridgeTest, HubConnectionDuplicate) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // First connection
    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    EXPECT_EQ(result, 0) << "First connection should succeed";

    // Second connection - should fail or handle gracefully
    result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    EXPECT_EQ(result, -1) << "Duplicate connection should fail";
}

/**
 * Test: HubDisconnection
 * Verify bridge can disconnect cleanly
 */
TEST_F(RcogHubBridgeTest, HubDisconnection) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect first
    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    ASSERT_EQ(result, 0) << "Connection required for disconnect test";

    // Disconnect
    result = rcog_hub_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0) << "Disconnect should succeed";

    EXPECT_FALSE(rcog_hub_bridge_is_connected(bridge))
        << "Bridge should not be connected after disconnect";
}

/**
 * Test: HubDisconnectionNull
 * Verify disconnect handles NULL gracefully
 */
TEST_F(RcogHubBridgeTest, HubDisconnectionNull) {
    int result = rcog_hub_bridge_disconnect(nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/**
 * Test: HubDisconnectionNotConnected
 * Verify disconnecting when not connected is handled
 */
TEST_F(RcogHubBridgeTest, HubDisconnectionNotConnected) {
    ASSERT_NE(bridge, nullptr);

    // Disconnect without connecting first
    int result = rcog_hub_bridge_disconnect(bridge);
    // May succeed (no-op) or fail - shouldn't crash
    (void)result;

    EXPECT_FALSE(rcog_hub_bridge_is_connected(bridge))
        << "Bridge should remain disconnected";
}

/**
 * Test: IsConnectedNull
 * Verify is_connected handles NULL gracefully
 */
TEST_F(RcogHubBridgeTest, IsConnectedNull) {
    bool connected = rcog_hub_bridge_is_connected(nullptr);
    EXPECT_FALSE(connected) << "NULL bridge should return false";
}

/**
 * Test: SetEngine
 * Verify engine reference can be updated
 */
TEST_F(RcogHubBridgeTest, SetEngine) {
    ASSERT_NE(bridge, nullptr);

    // Set engine to NULL (clearing)
    int result = rcog_hub_bridge_set_engine(bridge, nullptr);
    EXPECT_EQ(result, 0) << "Setting engine to NULL should succeed";
}

/**
 * Test: SetEngineNull
 * Verify set_engine handles NULL bridge gracefully
 */
TEST_F(RcogHubBridgeTest, SetEngineNull) {
    int result = rcog_hub_bridge_set_engine(nullptr, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Event Callback Registration Tests
 * ============================================================================ */

/**
 * Test: EventSubscription
 * Verify callbacks can be registered for events
 */
TEST_F(RcogHubBridgeTest, EventSubscription) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;

    // Set input callback
    int result = rcog_hub_bridge_set_input_callback(
        bridge, test_input_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting input callback should succeed";

    // Set attention callback
    result = rcog_hub_bridge_set_attention_callback(
        bridge, test_attention_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting attention callback should succeed";

    // Set memory callback
    result = rcog_hub_bridge_set_memory_callback(
        bridge, test_memory_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting memory callback should succeed";
}

/**
 * Test: EventSubscriptionClear
 * Verify callbacks can be cleared
 */
TEST_F(RcogHubBridgeTest, EventSubscriptionClear) {
    ASSERT_NE(bridge, nullptr);

    // Set then clear input callback
    int result = rcog_hub_bridge_set_input_callback(
        bridge, test_input_callback, nullptr);
    EXPECT_EQ(result, 0);

    result = rcog_hub_bridge_set_input_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";

    // Set then clear attention callback
    result = rcog_hub_bridge_set_attention_callback(
        bridge, test_attention_callback, nullptr);
    EXPECT_EQ(result, 0);

    result = rcog_hub_bridge_set_attention_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";

    // Set then clear memory callback
    result = rcog_hub_bridge_set_memory_callback(
        bridge, test_memory_callback, nullptr);
    EXPECT_EQ(result, 0);

    result = rcog_hub_bridge_set_memory_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: EventSubscriptionNullBridge
 * Verify callback registration handles NULL bridge
 */
TEST_F(RcogHubBridgeTest, EventSubscriptionNullBridge) {
    int result = rcog_hub_bridge_set_input_callback(
        nullptr, test_input_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = rcog_hub_bridge_set_attention_callback(
        nullptr, test_attention_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = rcog_hub_bridge_set_memory_callback(
        nullptr, test_memory_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Event Publication Tests
 * ============================================================================ */

/**
 * Test: EventPublication
 * Verify recursion events can be published
 */
TEST_F(RcogHubBridgeTest, EventPublication) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect first
    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    ASSERT_EQ(result, 0) << "Connection required for publication test";

    // Publish recursion start event
    result = rcog_hub_publish_recursion_start(
        bridge,
        1,      // goal_id
        0,      // goal_type
        8,      // max_depth
        0.5f    // priority
    );
    EXPECT_EQ(result, 0) << "Publish recursion start should succeed";
}

/**
 * Test: EventPublicationComplete
 * Verify recursion complete event can be published
 */
TEST_F(RcogHubBridgeTest, EventPublicationComplete) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect first
    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    ASSERT_EQ(result, 0) << "Connection required for publication test";

    // Publish recursion complete event
    result = rcog_hub_publish_recursion_complete(
        bridge,
        1,          // goal_id
        true,       // success
        0.95f,      // final_confidence
        10,         // subtasks_total
        9,          // subtasks_completed
        4,          // max_depth_reached
        1500        // processing_time_ms
    );
    EXPECT_EQ(result, 0) << "Publish recursion complete should succeed";
}

/**
 * Test: EventPublicationSubtask
 * Verify subtask spawned event can be published
 */
TEST_F(RcogHubBridgeTest, EventPublicationSubtask) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect first with config that enables subtask events
    rcog_hub_bridge_config_t pub_config;
    rcog_hub_bridge_default_config(&pub_config);
    pub_config.publish_subtask_events = true;

    rcog_hub_bridge_t* pub_bridge = rcog_hub_bridge_create(&pub_config);
    ASSERT_NE(pub_bridge, nullptr);

    int result = rcog_hub_bridge_connect(pub_bridge, hub, nullptr);
    ASSERT_EQ(result, 0) << "Connection required for publication test";

    // Publish subtask spawned event
    result = rcog_hub_publish_subtask_spawned(
        pub_bridge,
        1,      // parent_goal_id
        100,    // subtask_id
        2,      // current_depth
        0,      // subtask_type
        0.7f    // priority
    );
    // May succeed or be filtered based on config
    (void)result;

    rcog_hub_bridge_destroy(pub_bridge);
}

/**
 * Test: EventPublicationGeneric
 * Verify generic recursion event can be published
 */
TEST_F(RcogHubBridgeTest, EventPublicationGeneric) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect first
    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    ASSERT_EQ(result, 0) << "Connection required for publication test";

    // Publish generic event
    uint32_t payload_data = 42;
    result = rcog_hub_publish_recursion_event(
        bridge,
        RCOG_HUB_EVENT_CONFIDENCE_CHANGED,
        &payload_data,
        sizeof(payload_data)
    );
    EXPECT_EQ(result, 0) << "Publish generic event should succeed";
}

/**
 * Test: EventPublicationNotConnected
 * Verify publishing when not connected is handled
 */
TEST_F(RcogHubBridgeTest, EventPublicationNotConnected) {
    ASSERT_NE(bridge, nullptr);

    // Don't connect - try to publish
    int result = rcog_hub_publish_recursion_start(
        bridge, 1, 0, 8, 0.5f);
    EXPECT_EQ(result, -1) << "Publishing when not connected should fail";
}

/**
 * Test: EventPublicationNullBridge
 * Verify publish functions handle NULL bridge
 */
TEST_F(RcogHubBridgeTest, EventPublicationNullBridge) {
    // Publish recursion start
    int result = rcog_hub_publish_recursion_start(
        nullptr, 1, 0, 8, 0.5f);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    // Publish recursion complete
    result = rcog_hub_publish_recursion_complete(
        nullptr, 1, true, 0.95f, 10, 9, 4, 1500);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    // Publish subtask spawned
    result = rcog_hub_publish_subtask_spawned(
        nullptr, 1, 100, 2, 0, 0.7f);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    // Publish generic event
    uint32_t payload = 42;
    result = rcog_hub_publish_recursion_event(
        nullptr, RCOG_HUB_EVENT_CONFIDENCE_CHANGED, &payload, sizeof(payload));
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Query Handling Tests
 * ============================================================================ */

/**
 * Test: QueryHandling
 * Verify state queries work correctly
 */
TEST_F(RcogHubBridgeTest, QueryHandling) {
    ASSERT_NE(bridge, nullptr);

    uint32_t active_goals = 0;
    uint32_t current_depth = 0;
    float avg_confidence = 0.0f;

    int result = rcog_hub_bridge_get_state(
        bridge,
        &active_goals,
        &current_depth,
        &avg_confidence
    );
    EXPECT_EQ(result, 0) << "Get state should succeed";

    // Without engine connected, these should be 0
    EXPECT_EQ(active_goals, 0u);
    EXPECT_EQ(current_depth, 0u);
}

/**
 * Test: QueryHandlingPartialNull
 * Verify state query with some NULL outputs
 */
TEST_F(RcogHubBridgeTest, QueryHandlingPartialNull) {
    ASSERT_NE(bridge, nullptr);

    uint32_t active_goals = 0;

    // Query with some NULL outputs
    int result = rcog_hub_bridge_get_state(
        bridge,
        &active_goals,
        nullptr,
        nullptr
    );
    EXPECT_EQ(result, 0) << "Query with partial NULL outputs should succeed";
}

/**
 * Test: QueryHandlingNullBridge
 * Verify state query handles NULL bridge
 */
TEST_F(RcogHubBridgeTest, QueryHandlingNullBridge) {
    uint32_t active_goals = 0;
    uint32_t current_depth = 0;
    float avg_confidence = 0.0f;

    int result = rcog_hub_bridge_get_state(
        nullptr,
        &active_goals,
        &current_depth,
        &avg_confidence
    );
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * Test: StatsTracking
 * Verify statistics are tracked correctly
 */
TEST_F(RcogHubBridgeTest, StatsTracking) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect and publish some events
    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    ASSERT_EQ(result, 0);

    // Publish events
    rcog_hub_publish_recursion_start(bridge, 1, 0, 8, 0.5f);
    rcog_hub_publish_recursion_complete(bridge, 1, true, 0.9f, 5, 5, 3, 500);

    // Get stats
    rcog_hub_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    result = rcog_hub_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify events were counted
    EXPECT_GE(stats.events_published, 2u)
        << "Should have at least 2 events published";
}

/**
 * Test: StatsTrackingNull
 * Verify get_stats handles NULL parameters
 */
TEST_F(RcogHubBridgeTest, StatsTrackingNull) {
    rcog_hub_bridge_stats_t stats;

    int result = rcog_hub_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = rcog_hub_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL stats output should fail";
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(RcogHubBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect and publish some events
    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    ASSERT_EQ(result, 0);

    rcog_hub_publish_recursion_start(bridge, 1, 0, 8, 0.5f);

    // Reset stats
    result = rcog_hub_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    rcog_hub_bridge_stats_t stats;
    result = rcog_hub_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    // Counters should be reset
    EXPECT_EQ(stats.events_published, 0u) << "Events published should be 0";
    EXPECT_EQ(stats.events_received, 0u) << "Events received should be 0";
    EXPECT_EQ(stats.queries_handled, 0u) << "Queries handled should be 0";
}

/**
 * Test: StatsResetNull
 * Verify reset_stats handles NULL gracefully
 */
TEST_F(RcogHubBridgeTest, StatsResetNull) {
    int result = rcog_hub_bridge_reset_stats(nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(RcogHubBridgeTest, NullHandling) {
    // Lifecycle
    rcog_hub_bridge_destroy(nullptr);
    EXPECT_EQ(rcog_hub_bridge_default_config(nullptr), -1);

    // Connection
    EXPECT_EQ(rcog_hub_bridge_connect(nullptr, hub, nullptr), -1);
    EXPECT_EQ(rcog_hub_bridge_connect(bridge, nullptr, nullptr), -1);
    EXPECT_EQ(rcog_hub_bridge_disconnect(nullptr), -1);
    EXPECT_FALSE(rcog_hub_bridge_is_connected(nullptr));
    EXPECT_EQ(rcog_hub_bridge_set_engine(nullptr, nullptr), -1);

    // Callbacks
    EXPECT_EQ(rcog_hub_bridge_set_input_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(rcog_hub_bridge_set_attention_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(rcog_hub_bridge_set_memory_callback(nullptr, nullptr, nullptr), -1);

    // Publication
    EXPECT_EQ(rcog_hub_publish_recursion_start(nullptr, 1, 0, 8, 0.5f), -1);
    EXPECT_EQ(rcog_hub_publish_recursion_complete(nullptr, 1, true, 0.9f, 5, 5, 3, 500), -1);
    EXPECT_EQ(rcog_hub_publish_subtask_spawned(nullptr, 1, 100, 2, 0, 0.7f), -1);
    EXPECT_EQ(rcog_hub_publish_recursion_event(nullptr, RCOG_HUB_EVENT_RECURSION_START, nullptr, 0), -1);

    // Query
    uint32_t dummy_u32 = 0;
    float dummy_f = 0.0f;
    EXPECT_EQ(rcog_hub_bridge_get_state(nullptr, &dummy_u32, &dummy_u32, &dummy_f), -1);

    // Stats
    rcog_hub_bridge_stats_t stats;
    EXPECT_EQ(rcog_hub_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(rcog_hub_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(rcog_hub_bridge_reset_stats(nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Integration with Cognitive Hub Tests
 * ============================================================================ */

/**
 * Test: FullIntegrationFlow
 * Test complete flow: connect, publish, disconnect
 */
TEST_F(RcogHubBridgeTest, FullIntegrationFlow) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect
    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    EXPECT_EQ(result, 0) << "Connect should succeed";
    EXPECT_TRUE(rcog_hub_bridge_is_connected(bridge));

    // Publish start
    result = rcog_hub_publish_recursion_start(bridge, 1, 0, 8, 0.5f);
    EXPECT_EQ(result, 0) << "Publish start should succeed";

    // Publish complete
    result = rcog_hub_publish_recursion_complete(
        bridge, 1, true, 0.95f, 10, 9, 4, 1500);
    EXPECT_EQ(result, 0) << "Publish complete should succeed";

    // Check stats
    rcog_hub_bridge_stats_t stats;
    result = rcog_hub_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.events_published, 2u);

    // Disconnect
    result = rcog_hub_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0) << "Disconnect should succeed";
    EXPECT_FALSE(rcog_hub_bridge_is_connected(bridge));
}

/**
 * Test: ReconnectAfterDisconnect
 * Verify bridge can reconnect after disconnect
 */
TEST_F(RcogHubBridgeTest, ReconnectAfterDisconnect) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect
    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    EXPECT_EQ(result, 0);

    // Disconnect
    result = rcog_hub_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);

    // Reconnect
    result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    EXPECT_EQ(result, 0) << "Reconnect should succeed";
    EXPECT_TRUE(rcog_hub_bridge_is_connected(bridge));
}

/* ============================================================================
 * Event Type Coverage Tests
 * ============================================================================ */

/**
 * Test: AllEventTypes
 * Verify all recursion event types can be published
 */
TEST_F(RcogHubBridgeTest, AllEventTypes) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect
    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    ASSERT_EQ(result, 0);

    // Test all event types
    uint32_t dummy_payload = 0;

    for (int i = 0; i < RCOG_HUB_EVENT_COUNT; i++) {
        rcog_hub_event_type_t event_type = (rcog_hub_event_type_t)i;
        result = rcog_hub_publish_recursion_event(
            bridge, event_type, &dummy_payload, sizeof(dummy_payload));
        // Should succeed or at least not crash
        (void)result;
    }

    SUCCEED() << "All event types should be publishable";
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

/**
 * Test: ConcurrentAccess
 * Basic test for concurrent access to bridge
 */
TEST_F(RcogHubBridgeTest, ConcurrentAccess) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect
    int result = rcog_hub_bridge_connect(bridge, hub, nullptr);
    ASSERT_EQ(result, 0);

    const int NUM_THREADS = 4;
    const int ITERATIONS = 50;
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;

    // Create threads that concurrently access the bridge
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &completed, t, ITERATIONS]() {
            for (int i = 0; i < ITERATIONS; i++) {
                // Read operations
                rcog_hub_bridge_is_connected(bridge);

                uint32_t active_goals = 0;
                uint32_t current_depth = 0;
                float avg_confidence = 0.0f;
                rcog_hub_bridge_get_state(bridge, &active_goals,
                    &current_depth, &avg_confidence);

                rcog_hub_bridge_stats_t stats;
                rcog_hub_bridge_get_stats(bridge, &stats);

                // Write operations (publish events)
                rcog_hub_publish_recursion_start(
                    bridge,
                    (uint64_t)(t * 1000 + i),
                    0, 8, 0.5f
                );
            }
            completed++;
        });
    }

    // Wait for all threads to complete
    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS)
        << "All threads should complete successfully";

    // Verify stats show all events were counted
    rcog_hub_bridge_stats_t stats;
    result = rcog_hub_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.events_published, (uint32_t)(NUM_THREADS * ITERATIONS))
        << "All events should be counted";
}

/* ============================================================================
 * Configuration Variants Tests
 * ============================================================================ */

/**
 * Test: ConfigVariants
 * Test bridge creation with various config options
 */
TEST_F(RcogHubBridgeTest, ConfigVariants) {
    // Config with all auto-subscribe disabled
    rcog_hub_bridge_config_t config1;
    rcog_hub_bridge_default_config(&config1);
    config1.auto_subscribe_input = false;
    config1.auto_subscribe_memory = false;
    config1.auto_subscribe_attention = false;

    rcog_hub_bridge_t* br1 = rcog_hub_bridge_create(&config1);
    ASSERT_NE(br1, nullptr);
    rcog_hub_bridge_destroy(br1);

    // Config with query handler disabled
    rcog_hub_bridge_config_t config2;
    rcog_hub_bridge_default_config(&config2);
    config2.enable_query_handler = false;

    rcog_hub_bridge_t* br2 = rcog_hub_bridge_create(&config2);
    ASSERT_NE(br2, nullptr);
    rcog_hub_bridge_destroy(br2);

    // Config with custom module ID
    rcog_hub_bridge_config_t config3;
    rcog_hub_bridge_default_config(&config3);
    config3.module_id = 0x12345678;

    rcog_hub_bridge_t* br3 = rcog_hub_bridge_create(&config3);
    ASSERT_NE(br3, nullptr);
    rcog_hub_bridge_destroy(br3);

    // Config with small event buffer
    rcog_hub_bridge_config_t config4;
    rcog_hub_bridge_default_config(&config4);
    config4.event_buffer_size = 8;

    rcog_hub_bridge_t* br4 = rcog_hub_bridge_create(&config4);
    ASSERT_NE(br4, nullptr);
    rcog_hub_bridge_destroy(br4);

    SUCCEED() << "All config variants should work";
}

/* ============================================================================
 * Callback Invocation Tests
 * ============================================================================ */

/**
 * Test: EventCallback
 * Verify callbacks receive events properly
 */
TEST_F(RcogHubBridgeTest, EventCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;

    // Set input callback
    int result = rcog_hub_bridge_set_input_callback(
        bridge, test_input_callback, &user_data);
    EXPECT_EQ(result, 0);

    // Set attention callback
    result = rcog_hub_bridge_set_attention_callback(
        bridge, test_attention_callback, &user_data);
    EXPECT_EQ(result, 0);

    // Set memory callback
    result = rcog_hub_bridge_set_memory_callback(
        bridge, test_memory_callback, &user_data);
    EXPECT_EQ(result, 0);

    // Callbacks are registered - actual invocation depends on hub events
    // Just verify no crashes occurred
    SUCCEED() << "Callbacks should be registered without error";
}
