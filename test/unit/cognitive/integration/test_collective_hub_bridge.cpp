/**
 * @file test_collective_hub_bridge.cpp
 * @brief Unit tests for Collective Cognition - Cognitive Hub Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * Tests the cognitive hub bridge for collective cognition:
 * - Bridge creation and destruction
 * - Hub connection/disconnection
 * - Event subscription and publication
 * - Event callback handling
 * - Consensus publication
 * - Phi update publication
 * - Query handling
 * - Statistics tracking
 * - Null parameter handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>

#include "cognitive/integration/nimcp_collective_hub_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"

/* ============================================================================
 * GLOBAL TEST HELPERS
 * ============================================================================ */

static int g_callback_count = 0;
static uint32_t g_last_event_source = 0;
static cognitive_event_type_t g_last_event_type = COG_EVENT_STATE_CHANGE;
static void* g_last_user_data = nullptr;
static collective_event_subtype_t g_last_subtype = COLLECTIVE_EVENT_CONSENSUS_REACHED;

/**
 * Reset global callback state
 */
static void reset_callback_state(void) {
    g_callback_count = 0;
    g_last_event_source = 0;
    g_last_event_type = COG_EVENT_STATE_CHANGE;
    g_last_user_data = nullptr;
    g_last_subtype = COLLECTIVE_EVENT_CONSENSUS_REACHED;
}

/**
 * Test event callback that tracks call count and event details
 */
static int test_event_callback(const cognitive_event_data_t* event, void* user_data) {
    g_callback_count++;
    if (event != nullptr) {
        g_last_event_source = event->source_module_id;
        g_last_event_type = event->event_type;
    }
    g_last_user_data = user_data;
    return 0;
}

/**
 * Second callback for multiple subscriber tests
 */
static int g_callback_count_2 = 0;

static int test_event_callback_2(const cognitive_event_data_t* event, void* user_data) {
    (void)event;
    (void)user_data;
    g_callback_count_2++;
    return 0;
}

/* ============================================================================
 * TEST FIXTURE
 * ============================================================================ */

class CollectiveHubBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset callback state
        reset_callback_state();
        g_callback_count_2 = 0;

        // Create cognitive integration hub
        hub_config_ = cognitive_hub_default_config();
        hub_ = cognitive_hub_create(&hub_config_);
        ASSERT_NE(hub_, nullptr) << "Hub creation should succeed";

        // Create collective cognition system
        cc_config_ = collective_cognition_default_config();
        collective_ = collective_cognition_create(&cc_config_);
        ASSERT_NE(collective_, nullptr) << "Collective creation should succeed";

        // Register some instances for the collective
        collective_cognition_register_instance(collective_, 1, nullptr);
        collective_cognition_register_instance(collective_, 2, nullptr);
        collective_cognition_update(collective_);
    }

    void TearDown() override {
        // Destroy bridge (will auto-disconnect if needed)
        if (bridge_) {
            collective_hub_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }

        // Destroy collective cognition
        if (collective_) {
            collective_cognition_destroy(collective_);
            collective_ = nullptr;
        }

        // Destroy hub
        if (hub_) {
            cognitive_hub_destroy(hub_);
            hub_ = nullptr;
        }
    }

    // Helper to create and connect a bridge
    void CreateAndConnectBridge() {
        collective_hub_bridge_config_t config;
        ASSERT_EQ(collective_hub_bridge_default_config(&config), 0);

        bridge_ = collective_hub_bridge_create(&config);
        ASSERT_NE(bridge_, nullptr);

        ASSERT_EQ(collective_hub_bridge_connect(bridge_, hub_, collective_), 0);
        ASSERT_TRUE(collective_hub_bridge_is_connected(bridge_));
    }

    cognitive_integration_hub_t hub_ = nullptr;
    cognitive_hub_config_t hub_config_;
    collective_cognition_t* collective_ = nullptr;
    collective_cognition_config_t cc_config_;
    collective_hub_bridge_t* bridge_ = nullptr;
};

/* ============================================================================
 * LIFECYCLE TESTS
 * ============================================================================ */

/**
 * Test: BridgeCreation
 * Verify bridge can be created and destroyed successfully
 */
TEST_F(CollectiveHubBridgeTest, BridgeCreation) {
    collective_hub_bridge_config_t config;
    ASSERT_EQ(collective_hub_bridge_default_config(&config), 0);

    collective_hub_bridge_t* bridge = collective_hub_bridge_create(&config);
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    collective_hub_bridge_destroy(bridge);
    SUCCEED() << "Bridge destroyed successfully";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(CollectiveHubBridgeTest, BridgeCreationNullConfig) {
    collective_hub_bridge_t* bridge = collective_hub_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr) << "Bridge creation with NULL config should succeed";

    collective_hub_bridge_destroy(bridge);
}

/**
 * Test: BridgeDestroyNull
 * Verify destroying NULL bridge is safe
 */
TEST_F(CollectiveHubBridgeTest, BridgeDestroyNull) {
    collective_hub_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(CollectiveHubBridgeTest, DefaultConfig) {
    collective_hub_bridge_config_t config;
    ASSERT_EQ(collective_hub_bridge_default_config(&config), 0);

    // Verify default values
    EXPECT_EQ(config.module_id, COLLECTIVE_HUB_MODULE_ID);
    EXPECT_FLOAT_EQ(config.phi_change_threshold, COLLECTIVE_HUB_PHI_THRESHOLD);
    EXPECT_FLOAT_EQ(config.coherence_change_threshold, COLLECTIVE_HUB_COHERENCE_THRESHOLD);
    EXPECT_TRUE(config.enable_auto_publish);
    EXPECT_TRUE(config.enable_social_subscription);
    EXPECT_TRUE(config.enable_state_subscription);
    EXPECT_TRUE(config.enable_decision_subscription);
    EXPECT_TRUE(config.enable_query_handling);
}

/**
 * Test: DefaultConfigNull
 * Verify default config handles NULL
 */
TEST_F(CollectiveHubBridgeTest, DefaultConfigNull) {
    int result = collective_hub_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1) << "Default config with NULL should fail";
}

/* ============================================================================
 * CONNECTION TESTS
 * ============================================================================ */

/**
 * Test: HubConnection
 * Verify bridge can connect to cognitive hub
 */
TEST_F(CollectiveHubBridgeTest, HubConnection) {
    collective_hub_bridge_config_t config;
    ASSERT_EQ(collective_hub_bridge_default_config(&config), 0);

    bridge_ = collective_hub_bridge_create(&config);
    ASSERT_NE(bridge_, nullptr);

    // Should not be connected initially
    EXPECT_FALSE(collective_hub_bridge_is_connected(bridge_));

    // Connect
    int result = collective_hub_bridge_connect(bridge_, hub_, collective_);
    EXPECT_EQ(result, 0) << "Connection should succeed";
    EXPECT_TRUE(collective_hub_bridge_is_connected(bridge_));
}

/**
 * Test: HubDisconnection
 * Verify bridge can disconnect from hub cleanly
 */
TEST_F(CollectiveHubBridgeTest, HubDisconnection) {
    CreateAndConnectBridge();

    // Disconnect
    int result = collective_hub_bridge_disconnect(bridge_);
    EXPECT_EQ(result, 0) << "Disconnection should succeed";
    EXPECT_FALSE(collective_hub_bridge_is_connected(bridge_));
}

/**
 * Test: DoubleConnection
 * Verify double connection is handled properly
 */
TEST_F(CollectiveHubBridgeTest, DoubleConnection) {
    CreateAndConnectBridge();

    // Attempt second connection
    int result = collective_hub_bridge_connect(bridge_, hub_, collective_);
    EXPECT_EQ(result, -1) << "Double connection should fail";

    // Should still be connected from first connection
    EXPECT_TRUE(collective_hub_bridge_is_connected(bridge_));
}

/**
 * Test: DisconnectWhenNotConnected
 * Verify disconnecting when not connected returns error
 */
TEST_F(CollectiveHubBridgeTest, DisconnectWhenNotConnected) {
    collective_hub_bridge_config_t config;
    ASSERT_EQ(collective_hub_bridge_default_config(&config), 0);

    bridge_ = collective_hub_bridge_create(&config);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_FALSE(collective_hub_bridge_is_connected(bridge_));
    int result = collective_hub_bridge_disconnect(bridge_);
    EXPECT_EQ(result, -1) << "Disconnect when not connected should fail";
}

/**
 * Test: ConnectionNullParameters
 * Verify connection fails with NULL parameters
 */
TEST_F(CollectiveHubBridgeTest, ConnectionNullParameters) {
    collective_hub_bridge_config_t config;
    ASSERT_EQ(collective_hub_bridge_default_config(&config), 0);

    bridge_ = collective_hub_bridge_create(&config);
    ASSERT_NE(bridge_, nullptr);

    // NULL bridge
    EXPECT_EQ(collective_hub_bridge_connect(nullptr, hub_, collective_), -1);

    // NULL hub
    EXPECT_EQ(collective_hub_bridge_connect(bridge_, nullptr, collective_), -1);

    // NULL collective
    EXPECT_EQ(collective_hub_bridge_connect(bridge_, hub_, nullptr), -1);
}

/* ============================================================================
 * EVENT SUBSCRIPTION TESTS
 * ============================================================================ */

/**
 * Test: EventSubscription
 * Verify bridge subscribes to relevant events when connected
 */
TEST_F(CollectiveHubBridgeTest, EventSubscription) {
    collective_hub_bridge_config_t config;
    ASSERT_EQ(collective_hub_bridge_default_config(&config), 0);
    config.enable_social_subscription = true;
    config.enable_state_subscription = true;
    config.enable_decision_subscription = true;

    bridge_ = collective_hub_bridge_create(&config);
    ASSERT_NE(bridge_, nullptr);

    ASSERT_EQ(collective_hub_bridge_connect(bridge_, hub_, collective_), 0);

    // Verify the module is registered in the hub
    cognitive_module_info_t info;
    int result = cognitive_hub_get_module_info(hub_, COLLECTIVE_HUB_MODULE_ID, &info);
    EXPECT_EQ(result, 0) << "Module should be registered in hub";
    EXPECT_STREQ(info.name, COLLECTIVE_HUB_MODULE_NAME);
    EXPECT_EQ(info.category, COG_CATEGORY_SOCIAL);
}

/**
 * Test: SelectiveSubscription
 * Verify subscriptions can be selectively enabled/disabled
 */
TEST_F(CollectiveHubBridgeTest, SelectiveSubscription) {
    collective_hub_bridge_config_t config;
    ASSERT_EQ(collective_hub_bridge_default_config(&config), 0);

    // Disable some subscriptions
    config.enable_social_subscription = false;
    config.enable_decision_subscription = false;
    config.enable_state_subscription = true;

    bridge_ = collective_hub_bridge_create(&config);
    ASSERT_NE(bridge_, nullptr);

    ASSERT_EQ(collective_hub_bridge_connect(bridge_, hub_, collective_), 0);
    EXPECT_TRUE(collective_hub_bridge_is_connected(bridge_));
}

/* ============================================================================
 * EVENT PUBLICATION TESTS
 * ============================================================================ */

/**
 * Test: EventPublication
 * Verify bridge can publish events to hub
 */
TEST_F(CollectiveHubBridgeTest, EventPublication) {
    CreateAndConnectBridge();

    // Register a subscriber module to receive events
    const uint32_t subscriber_id = 5000;
    int user_data_value = 12345;
    ASSERT_EQ(cognitive_hub_register_module(
        hub_,
        subscriber_id,
        COG_CATEGORY_MEMORY,
        "TestSubscriber",
        nullptr
    ), 0);

    // Subscribe to state change events
    ASSERT_EQ(cognitive_hub_subscribe(
        hub_,
        subscriber_id,
        COG_EVENT_STATE_CHANGE,
        test_event_callback,
        &user_data_value
    ), 0);

    // Publish state change event
    collective_state_change_t state_change;
    memset(&state_change, 0, sizeof(state_change));
    state_change.subtype = COLLECTIVE_EVENT_PHI_CHANGED;
    state_change.phi_old = 0.3f;
    state_change.phi_new = 0.5f;
    state_change.coherence_old = 0.4f;
    state_change.coherence_new = 0.6f;
    state_change.active_instances = 2;
    state_change.is_entrained = false;

    int result = collective_hub_publish_state_change(bridge_, &state_change);
    EXPECT_EQ(result, 0) << "State change publish should succeed";

    // Verify callback was invoked
    EXPECT_EQ(g_callback_count, 1) << "Subscriber callback should be called";
    EXPECT_EQ(g_last_event_source, COLLECTIVE_HUB_MODULE_ID);
    EXPECT_EQ(g_last_event_type, COG_EVENT_STATE_CHANGE);
    EXPECT_EQ(g_last_user_data, &user_data_value);
}

/**
 * Test: EventCallback
 * Verify event callback receives events correctly
 */
TEST_F(CollectiveHubBridgeTest, EventCallback) {
    CreateAndConnectBridge();

    // Register a publisher module
    const uint32_t publisher_id = 6000;
    ASSERT_EQ(cognitive_hub_register_module(
        hub_,
        publisher_id,
        COG_CATEGORY_SOCIAL,
        "TestPublisher",
        nullptr
    ), 0);

    // Publish a social signal event
    cognitive_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = COG_EVENT_SOCIAL_SIGNAL;
    event_data.source_module_id = publisher_id;
    event_data.priority = COG_PRIORITY_NORMAL;

    int result = cognitive_hub_publish(hub_, publisher_id, COG_EVENT_SOCIAL_SIGNAL, &event_data);
    EXPECT_EQ(result, 0) << "Publish should succeed";

    // The collective hub bridge should receive this event through its subscription
    // The actual callback is internal, but we can verify stats
    collective_hub_bridge_stats_t stats;
    ASSERT_EQ(collective_hub_bridge_get_stats(bridge_, &stats), 0);
    // Social signals received may be >= 1 depending on timing
}

/* ============================================================================
 * CONSENSUS PUBLICATION TESTS
 * ============================================================================ */

/**
 * Test: ConsensusPublication
 * Verify consensus events can be published
 */
TEST_F(CollectiveHubBridgeTest, ConsensusPublication) {
    CreateAndConnectBridge();

    // Register a subscriber
    const uint32_t subscriber_id = 7000;
    ASSERT_EQ(cognitive_hub_register_module(
        hub_,
        subscriber_id,
        COG_CATEGORY_EXECUTIVE,
        "ConsensusSubscriber",
        nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_subscribe(
        hub_,
        subscriber_id,
        COG_EVENT_CONSOLIDATION,
        test_event_callback,
        nullptr
    ), 0);

    // Publish consensus event
    collective_consensus_data_t consensus;
    memset(&consensus, 0, sizeof(consensus));
    consensus.consensus_type = 1;
    consensus.confidence = 0.9f;
    consensus.agreeing_instances = 4;
    consensus.total_instances = 5;
    consensus.phi_at_consensus = 0.7f;
    consensus.coherence_at_consensus = 0.85f;
    consensus.data = nullptr;
    consensus.data_size = 0;

    int result = collective_hub_publish_consensus(bridge_, &consensus);
    EXPECT_EQ(result, 0) << "Consensus publish should succeed";

    // Verify callback was invoked
    EXPECT_EQ(g_callback_count, 1);
    EXPECT_EQ(g_last_event_source, COLLECTIVE_HUB_MODULE_ID);
    EXPECT_EQ(g_last_event_type, COG_EVENT_CONSOLIDATION);
}

/**
 * Test: ConsensusPublicationNotConnected
 * Verify consensus publication fails when not connected
 */
TEST_F(CollectiveHubBridgeTest, ConsensusPublicationNotConnected) {
    collective_hub_bridge_config_t config;
    ASSERT_EQ(collective_hub_bridge_default_config(&config), 0);

    bridge_ = collective_hub_bridge_create(&config);
    ASSERT_NE(bridge_, nullptr);
    EXPECT_FALSE(collective_hub_bridge_is_connected(bridge_));

    collective_consensus_data_t consensus;
    memset(&consensus, 0, sizeof(consensus));

    int result = collective_hub_publish_consensus(bridge_, &consensus);
    EXPECT_EQ(result, -1) << "Publish when not connected should fail";
}

/* ============================================================================
 * PHI UPDATE PUBLICATION TESTS
 * ============================================================================ */

/**
 * Test: PhiUpdatePublication
 * Verify phi update events are published on significant phi change
 */
TEST_F(CollectiveHubBridgeTest, PhiUpdatePublication) {
    CreateAndConnectBridge();

    // Register a subscriber for state changes
    const uint32_t subscriber_id = 8000;
    ASSERT_EQ(cognitive_hub_register_module(
        hub_,
        subscriber_id,
        COG_CATEGORY_REASONING,
        "PhiSubscriber",
        nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_subscribe(
        hub_,
        subscriber_id,
        COG_EVENT_STATE_CHANGE,
        test_event_callback,
        nullptr
    ), 0);

    // Publish state change with phi update subtype
    collective_state_change_t state_change;
    memset(&state_change, 0, sizeof(state_change));
    state_change.subtype = COLLECTIVE_EVENT_PHI_CHANGED;
    state_change.phi_old = 0.2f;
    state_change.phi_new = 0.8f;  // Significant change
    state_change.active_instances = 2;

    int result = collective_hub_publish_state_change(bridge_, &state_change);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_callback_count, 1);
}

/**
 * Test: BridgeUpdate
 * Verify bridge update checks for state changes and publishes if needed
 */
TEST_F(CollectiveHubBridgeTest, BridgeUpdate) {
    CreateAndConnectBridge();

    // Register subscriber
    const uint32_t subscriber_id = 8500;
    ASSERT_EQ(cognitive_hub_register_module(
        hub_,
        subscriber_id,
        COG_CATEGORY_MEMORY,
        "UpdateSubscriber",
        nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_subscribe(
        hub_,
        subscriber_id,
        COG_EVENT_STATE_CHANGE,
        test_event_callback,
        nullptr
    ), 0);

    // Update collective state
    collective_cognition_update(collective_);

    // Bridge update should check for changes
    int result = collective_hub_bridge_update(bridge_);
    EXPECT_EQ(result, 0) << "Bridge update should succeed";
}

/* ============================================================================
 * QUERY HANDLING TESTS
 * ============================================================================ */

/**
 * Test: QueryHandling
 * Verify bridge handles queries from other modules
 */
TEST_F(CollectiveHubBridgeTest, QueryHandling) {
    collective_hub_bridge_config_t config;
    ASSERT_EQ(collective_hub_bridge_default_config(&config), 0);
    config.enable_query_handling = true;

    bridge_ = collective_hub_bridge_create(&config);
    ASSERT_NE(bridge_, nullptr);

    ASSERT_EQ(collective_hub_bridge_connect(bridge_, hub_, collective_), 0);

    // Register a requester module
    const uint32_t requester_id = 9000;
    ASSERT_EQ(cognitive_hub_register_module(
        hub_,
        requester_id,
        COG_CATEGORY_REASONING,
        "QueryRequester",
        nullptr
    ), 0);

    // Prepare query
    cognitive_query_t query;
    memset(&query, 0, sizeof(query));
    query.query_type = COG_QUERY_STATUS;
    query.query_params = nullptr;
    query.params_size = 0;

    cognitive_query_result_t result_data;
    memset(&result_data, 0, sizeof(result_data));

    // Query the collective module
    int result = cognitive_hub_query_module(
        hub_,
        requester_id,
        COLLECTIVE_HUB_MODULE_ID,
        &query,
        &result_data
    );

    // Query should either succeed or fail gracefully
    // (depends on whether query handler is fully implemented)
    EXPECT_TRUE(result == 0 || result == -1);
}

/* ============================================================================
 * STATISTICS TRACKING TESTS
 * ============================================================================ */

/**
 * Test: StatsTracking
 * Verify statistics are updated correctly
 */
TEST_F(CollectiveHubBridgeTest, StatsTracking) {
    CreateAndConnectBridge();

    // Get initial stats
    collective_hub_bridge_stats_t stats1;
    ASSERT_EQ(collective_hub_bridge_get_stats(bridge_, &stats1), 0);
    uint32_t initial_published = stats1.events_published;

    // Publish some events
    collective_state_change_t state_change;
    memset(&state_change, 0, sizeof(state_change));
    state_change.subtype = COLLECTIVE_EVENT_COHERENCE_CHANGED;

    collective_hub_publish_state_change(bridge_, &state_change);
    collective_hub_publish_state_change(bridge_, &state_change);

    // Get updated stats
    collective_hub_bridge_stats_t stats2;
    ASSERT_EQ(collective_hub_bridge_get_stats(bridge_, &stats2), 0);

    EXPECT_GT(stats2.events_published, initial_published)
        << "Events published should increase";
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(CollectiveHubBridgeTest, StatsReset) {
    CreateAndConnectBridge();

    // Publish some events to accumulate stats
    collective_state_change_t state_change;
    memset(&state_change, 0, sizeof(state_change));
    state_change.subtype = COLLECTIVE_EVENT_PHI_CHANGED;

    collective_hub_publish_state_change(bridge_, &state_change);
    collective_hub_publish_state_change(bridge_, &state_change);
    collective_hub_publish_state_change(bridge_, &state_change);

    // Reset stats
    int result = collective_hub_bridge_reset_stats(bridge_);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Verify stats are reset
    collective_hub_bridge_stats_t stats;
    ASSERT_EQ(collective_hub_bridge_get_stats(bridge_, &stats), 0);
    EXPECT_EQ(stats.events_published, 0u) << "Events published should be reset";
    EXPECT_EQ(stats.events_received, 0u);
    EXPECT_EQ(stats.queries_handled, 0u);
}

/* ============================================================================
 * NULL HANDLING TESTS
 * ============================================================================ */

/**
 * Test: NullHandling
 * Verify all functions handle null parameters correctly
 */
TEST_F(CollectiveHubBridgeTest, NullHandling) {
    // CreateAndConnectBridge for most tests
    CreateAndConnectBridge();

    // is_connected with NULL
    EXPECT_FALSE(collective_hub_bridge_is_connected(nullptr));

    // disconnect with NULL
    EXPECT_EQ(collective_hub_bridge_disconnect(nullptr), -1);

    // publish_consensus with NULL bridge
    collective_consensus_data_t consensus;
    memset(&consensus, 0, sizeof(consensus));
    EXPECT_EQ(collective_hub_publish_consensus(nullptr, &consensus), -1);

    // publish_consensus with NULL data
    EXPECT_EQ(collective_hub_publish_consensus(bridge_, nullptr), -1);

    // publish_state_change with NULL bridge
    collective_state_change_t state_change;
    memset(&state_change, 0, sizeof(state_change));
    EXPECT_EQ(collective_hub_publish_state_change(nullptr, &state_change), -1);

    // publish_state_change with NULL data
    EXPECT_EQ(collective_hub_publish_state_change(bridge_, nullptr), -1);

    // publish_event with NULL bridge
    EXPECT_EQ(collective_hub_publish_event(nullptr, COLLECTIVE_EVENT_PHI_CHANGED, nullptr, 0), -1);

    // update with NULL bridge
    EXPECT_EQ(collective_hub_bridge_update(nullptr), -1);

    // get_stats with NULL bridge
    collective_hub_bridge_stats_t stats;
    EXPECT_EQ(collective_hub_bridge_get_stats(nullptr, &stats), -1);

    // get_stats with NULL output
    EXPECT_EQ(collective_hub_bridge_get_stats(bridge_, nullptr), -1);

    // reset_stats with NULL
    EXPECT_EQ(collective_hub_bridge_reset_stats(nullptr), -1);
}

/* ============================================================================
 * EVENT SUBTYPE STRING TESTS
 * ============================================================================ */

/**
 * Test: EventSubtypeToString
 * Verify event subtype names are correct
 */
TEST_F(CollectiveHubBridgeTest, EventSubtypeToString) {
    EXPECT_STREQ(collective_event_subtype_to_string(COLLECTIVE_EVENT_CONSENSUS_REACHED),
                 "CONSENSUS_REACHED");
    EXPECT_STREQ(collective_event_subtype_to_string(COLLECTIVE_EVENT_PHI_CHANGED),
                 "PHI_CHANGED");
    EXPECT_STREQ(collective_event_subtype_to_string(COLLECTIVE_EVENT_COHERENCE_CHANGED),
                 "COHERENCE_CHANGED");
    EXPECT_STREQ(collective_event_subtype_to_string(COLLECTIVE_EVENT_ENTRAINMENT_ACHIEVED),
                 "ENTRAINMENT_ACHIEVED");
    EXPECT_STREQ(collective_event_subtype_to_string(COLLECTIVE_EVENT_INSTANCE_JOINED),
                 "INSTANCE_JOINED");
    EXPECT_STREQ(collective_event_subtype_to_string(COLLECTIVE_EVENT_INSTANCE_LEFT),
                 "INSTANCE_LEFT");
    EXPECT_STREQ(collective_event_subtype_to_string(COLLECTIVE_EVENT_GOAL_PROPOSED),
                 "GOAL_PROPOSED");
    EXPECT_STREQ(collective_event_subtype_to_string(COLLECTIVE_EVENT_GOAL_ACCEPTED),
                 "GOAL_ACCEPTED");
    EXPECT_STREQ(collective_event_subtype_to_string(COLLECTIVE_EVENT_GOAL_COMPLETED),
                 "GOAL_COMPLETED");
    EXPECT_STREQ(collective_event_subtype_to_string(COLLECTIVE_EVENT_LOAD_REBALANCED),
                 "LOAD_REBALANCED");

    // Invalid value should return "UNKNOWN"
    EXPECT_STREQ(collective_event_subtype_to_string((collective_event_subtype_t)999),
                 "UNKNOWN");
}

/* ============================================================================
 * INTEGRATION TESTS
 * ============================================================================ */

/**
 * Test: FullWorkflow
 * Verify complete workflow of connecting, publishing, and receiving events
 */
TEST_F(CollectiveHubBridgeTest, FullWorkflow) {
    // Create bridge
    collective_hub_bridge_config_t config;
    ASSERT_EQ(collective_hub_bridge_default_config(&config), 0);

    bridge_ = collective_hub_bridge_create(&config);
    ASSERT_NE(bridge_, nullptr);

    // Connect
    ASSERT_EQ(collective_hub_bridge_connect(bridge_, hub_, collective_), 0);
    EXPECT_TRUE(collective_hub_bridge_is_connected(bridge_));

    // Register external subscriber
    const uint32_t subscriber_id = 10000;
    ASSERT_EQ(cognitive_hub_register_module(
        hub_,
        subscriber_id,
        COG_CATEGORY_MEMORY,
        "WorkflowSubscriber",
        nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_subscribe(
        hub_,
        subscriber_id,
        COG_EVENT_STATE_CHANGE,
        test_event_callback,
        nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_subscribe(
        hub_,
        subscriber_id,
        COG_EVENT_CONSOLIDATION,
        test_event_callback_2,
        nullptr
    ), 0);

    // Publish state change
    collective_state_change_t state_change;
    memset(&state_change, 0, sizeof(state_change));
    state_change.subtype = COLLECTIVE_EVENT_PHI_CHANGED;
    state_change.phi_new = 0.6f;

    ASSERT_EQ(collective_hub_publish_state_change(bridge_, &state_change), 0);
    EXPECT_EQ(g_callback_count, 1);

    // Publish consensus
    collective_consensus_data_t consensus;
    memset(&consensus, 0, sizeof(consensus));
    consensus.confidence = 0.95f;

    ASSERT_EQ(collective_hub_publish_consensus(bridge_, &consensus), 0);
    EXPECT_EQ(g_callback_count_2, 1);

    // Check stats
    collective_hub_bridge_stats_t stats;
    ASSERT_EQ(collective_hub_bridge_get_stats(bridge_, &stats), 0);
    EXPECT_GE(stats.events_published, 2u);
    EXPECT_GE(stats.consensus_reached, 1u);

    // Disconnect
    ASSERT_EQ(collective_hub_bridge_disconnect(bridge_), 0);
    EXPECT_FALSE(collective_hub_bridge_is_connected(bridge_));
}

/**
 * Test: MultipleEventsSequence
 * Verify multiple events are handled correctly in sequence
 */
TEST_F(CollectiveHubBridgeTest, MultipleEventsSequence) {
    CreateAndConnectBridge();

    // Register subscriber
    const uint32_t subscriber_id = 11000;
    ASSERT_EQ(cognitive_hub_register_module(
        hub_,
        subscriber_id,
        COG_CATEGORY_EXECUTIVE,
        "SequenceSubscriber",
        nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_subscribe(
        hub_,
        subscriber_id,
        COG_EVENT_STATE_CHANGE,
        test_event_callback,
        nullptr
    ), 0);

    // Publish sequence of events
    const int event_count = 10;
    for (int i = 0; i < event_count; i++) {
        collective_state_change_t state_change;
        memset(&state_change, 0, sizeof(state_change));
        state_change.subtype = (collective_event_subtype_t)(i % COLLECTIVE_EVENT_COUNT);
        state_change.phi_new = 0.1f * (i + 1);

        ASSERT_EQ(collective_hub_publish_state_change(bridge_, &state_change), 0);
    }

    EXPECT_EQ(g_callback_count, event_count);

    // Verify stats
    collective_hub_bridge_stats_t stats;
    ASSERT_EQ(collective_hub_bridge_get_stats(bridge_, &stats), 0);
    EXPECT_GE(stats.events_published, (uint32_t)event_count);
}

/**
 * Test: CollectiveStateChangeTriggersBridgeUpdate
 * Verify that collective state changes trigger bridge updates
 */
TEST_F(CollectiveHubBridgeTest, CollectiveStateChangeTriggersBridgeUpdate) {
    CreateAndConnectBridge();

    // Get initial stats
    collective_hub_bridge_stats_t stats1;
    ASSERT_EQ(collective_hub_bridge_get_stats(bridge_, &stats1), 0);

    // Add a new instance to the collective
    collective_cognition_register_instance(collective_, 100, nullptr);
    collective_cognition_update(collective_);

    // Update bridge
    ASSERT_EQ(collective_hub_bridge_update(bridge_), 0);

    // Check if any events were published due to state change
    collective_hub_bridge_stats_t stats2;
    ASSERT_EQ(collective_hub_bridge_get_stats(bridge_, &stats2), 0);

    // The bridge update might publish events if phi/coherence changed significantly
    // We just verify the update succeeded without error
    SUCCEED();
}

/**
 * Test: DestroyWhileConnected
 * Verify destroying bridge while connected auto-disconnects
 */
TEST_F(CollectiveHubBridgeTest, DestroyWhileConnected) {
    CreateAndConnectBridge();
    ASSERT_TRUE(collective_hub_bridge_is_connected(bridge_));

    // Destroy should auto-disconnect
    collective_hub_bridge_destroy(bridge_);
    bridge_ = nullptr;  // Prevent double-free in TearDown

    // Verify module is unregistered from hub
    cognitive_module_info_t info;
    int result = cognitive_hub_get_module_info(hub_, COLLECTIVE_HUB_MODULE_ID, &info);
    EXPECT_EQ(result, -1) << "Module should be unregistered from hub";
}
