/**
 * @file test_cognitive_integration_hub.cpp
 * @brief Unit tests for cognitive integration hub
 * @version 1.0.0
 * @date 2025
 *
 * Tests the cognitive integration hub functionality including:
 * - Hub creation and destruction
 * - Module registration and unregistration
 * - Event subscription and publishing
 * - Inter-module queries
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cstring>

#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ========================================================================
 * GLOBAL TEST HELPERS
 * ======================================================================== */

static int g_callback_count = 0;
static uint32_t g_last_event_source = 0;
static cognitive_event_type_t g_last_event_type = COG_EVENT_STATE_CHANGE;
static void* g_last_user_data = nullptr;

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

/**
 * Test query handler that always succeeds
 */
static int test_query_handler(const cognitive_query_t* query,
                              cognitive_query_result_t* result,
                              void* context) {
    (void)query;
    (void)context;
    if (result != nullptr) {
        result->status = 0;
        result->result_data = nullptr;
        result->result_size = 0;
        memset(result->error_message, 0, sizeof(result->error_message));
    }
    return 0;
}

/* ========================================================================
 * TEST FIXTURE
 * ======================================================================== */

class CognitiveIntegrationHubTest : public ::testing::Test {
protected:
    cognitive_integration_hub_t hub;
    cognitive_hub_config_t config;

    void SetUp() override {
        // Reset global state
        g_callback_count = 0;
        g_callback_count_2 = 0;
        g_last_event_source = 0;
        g_last_event_type = COG_EVENT_STATE_CHANGE;
        g_last_user_data = nullptr;

        // Get default config and create hub
        config = cognitive_hub_default_config();
        hub = cognitive_hub_create(&config);
    }

    void TearDown() override {
        if (hub != nullptr) {
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }
};

/* ========================================================================
 * HUB LIFECYCLE TESTS
 * ======================================================================== */

/**
 * Test: HubCreation
 * Verify hub can be created and destroyed successfully
 */
TEST_F(CognitiveIntegrationHubTest, HubCreation) {
    // Hub should be created in SetUp
    ASSERT_NE(hub, nullptr) << "Hub creation should succeed";

    // Verify default config values are sensible
    EXPECT_GT(config.max_modules, 0u) << "max_modules should be positive";
    EXPECT_GT(config.max_subscriptions, 0u) << "max_subscriptions should be positive";
}

/**
 * Test: HubCreationNullConfig
 * Verify hub can be created with NULL config (uses defaults)
 */
TEST_F(CognitiveIntegrationHubTest, HubCreationNullConfig) {
    // Destroy the hub created in SetUp
    cognitive_hub_destroy(hub);

    // Create with NULL config - should use defaults
    hub = cognitive_hub_create(nullptr);
    ASSERT_NE(hub, nullptr) << "Hub creation with NULL config should succeed";
}

/* ========================================================================
 * MODULE REGISTRATION TESTS
 * ======================================================================== */

/**
 * Test: ModuleRegistration
 * Verify module can be registered and info can be retrieved
 */
TEST_F(CognitiveIntegrationHubTest, ModuleRegistration) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = 1;
    const char* module_name = "TestModule";
    int dummy_context = 42;

    // Register module
    int result = cognitive_hub_register_module(
        hub,
        module_id,
        COG_CATEGORY_MEMORY,
        module_name,
        &dummy_context
    );
    EXPECT_EQ(result, 0) << "Module registration should succeed";

    // Verify module info
    cognitive_module_info_t info;
    memset(&info, 0, sizeof(info));

    result = cognitive_hub_get_module_info(hub, module_id, &info);
    EXPECT_EQ(result, 0) << "Get module info should succeed";
    EXPECT_EQ(info.module_id, module_id) << "Module ID should match";
    EXPECT_EQ(info.category, COG_CATEGORY_MEMORY) << "Category should match";
    EXPECT_STREQ(info.name, module_name) << "Name should match";
    EXPECT_EQ(info.context, &dummy_context) << "Context should match";
    EXPECT_TRUE(info.is_active) << "Module should be active by default";
}

/**
 * Test: ModuleUnregistration
 * Verify module can be unregistered
 */
TEST_F(CognitiveIntegrationHubTest, ModuleUnregistration) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = 1;

    // Register module
    int result = cognitive_hub_register_module(
        hub,
        module_id,
        COG_CATEGORY_REASONING,
        "TempModule",
        nullptr
    );
    EXPECT_EQ(result, 0) << "Registration should succeed";

    // Unregister module
    result = cognitive_hub_unregister_module(hub, module_id);
    EXPECT_EQ(result, 0) << "Unregistration should succeed";

    // Verify module is no longer registered
    cognitive_module_info_t info;
    result = cognitive_hub_get_module_info(hub, module_id, &info);
    EXPECT_EQ(result, -1) << "Get info for unregistered module should fail";
}

/**
 * Test: DuplicateRegistration
 * Verify duplicate module ID registration is rejected
 */
TEST_F(CognitiveIntegrationHubTest, DuplicateRegistration) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = 1;

    // Register module first time
    int result = cognitive_hub_register_module(
        hub,
        module_id,
        COG_CATEGORY_PERCEPTION,
        "FirstModule",
        nullptr
    );
    EXPECT_EQ(result, 0) << "First registration should succeed";

    // Attempt duplicate registration
    result = cognitive_hub_register_module(
        hub,
        module_id,
        COG_CATEGORY_SOCIAL,
        "DuplicateModule",
        nullptr
    );
    EXPECT_EQ(result, -1) << "Duplicate registration should be rejected";
}

/* ========================================================================
 * EVENT SUBSCRIPTION TESTS
 * ======================================================================== */

/**
 * Test: EventSubscription
 * Verify module can subscribe to event type
 */
TEST_F(CognitiveIntegrationHubTest, EventSubscription) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = 1;
    int user_data = 123;

    // Register module first
    int result = cognitive_hub_register_module(
        hub,
        module_id,
        COG_CATEGORY_MEMORY,
        "SubscriberModule",
        nullptr
    );
    ASSERT_EQ(result, 0) << "Module registration required";

    // Subscribe to event
    result = cognitive_hub_subscribe(
        hub,
        module_id,
        COG_EVENT_MEMORY_ACCESS,
        test_event_callback,
        &user_data
    );
    EXPECT_EQ(result, 0) << "Subscription should succeed";
}

/**
 * Test: EventUnsubscription
 * Verify module can unsubscribe from event type
 */
TEST_F(CognitiveIntegrationHubTest, EventUnsubscription) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = 1;

    // Register module
    int result = cognitive_hub_register_module(
        hub,
        module_id,
        COG_CATEGORY_EXECUTIVE,
        "UnsubModule",
        nullptr
    );
    ASSERT_EQ(result, 0);

    // Subscribe
    result = cognitive_hub_subscribe(
        hub,
        module_id,
        COG_EVENT_DECISION_MADE,
        test_event_callback,
        nullptr
    );
    ASSERT_EQ(result, 0) << "Subscribe should succeed";

    // Unsubscribe
    result = cognitive_hub_unsubscribe(
        hub,
        module_id,
        COG_EVENT_DECISION_MADE
    );
    EXPECT_EQ(result, 0) << "Unsubscribe should succeed";
}

/* ========================================================================
 * EVENT PUBLISHING TESTS
 * ======================================================================== */

/**
 * Test: EventPublishing
 * Verify published events trigger subscriber callbacks
 */
TEST_F(CognitiveIntegrationHubTest, EventPublishing) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = 1;
    const uint32_t subscriber_id = 2;
    int user_data = 456;

    // Register publisher
    int result = cognitive_hub_register_module(
        hub,
        publisher_id,
        COG_CATEGORY_PERCEPTION,
        "Publisher",
        nullptr
    );
    ASSERT_EQ(result, 0);

    // Register subscriber
    result = cognitive_hub_register_module(
        hub,
        subscriber_id,
        COG_CATEGORY_MEMORY,
        "Subscriber",
        nullptr
    );
    ASSERT_EQ(result, 0);

    // Subscribe to event
    result = cognitive_hub_subscribe(
        hub,
        subscriber_id,
        COG_EVENT_INPUT_RECEIVED,
        test_event_callback,
        &user_data
    );
    ASSERT_EQ(result, 0);

    // Prepare event data
    cognitive_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = COG_EVENT_INPUT_RECEIVED;
    event_data.source_module_id = publisher_id;
    event_data.priority = COG_PRIORITY_NORMAL;
    event_data.payload = nullptr;
    event_data.payload_size = 0;

    // Publish event
    result = cognitive_hub_publish(
        hub,
        publisher_id,
        COG_EVENT_INPUT_RECEIVED,
        &event_data
    );
    EXPECT_EQ(result, 0) << "Publish should succeed";

    // Verify callback was called
    EXPECT_EQ(g_callback_count, 1) << "Callback should be called once";
    EXPECT_EQ(g_last_event_source, publisher_id) << "Event source should match publisher";
    EXPECT_EQ(g_last_event_type, COG_EVENT_INPUT_RECEIVED) << "Event type should match";
    EXPECT_EQ(g_last_user_data, &user_data) << "User data should be passed through";
}

/**
 * Test: MultipleSubscribers
 * Verify multiple modules receive the same event
 */
TEST_F(CognitiveIntegrationHubTest, MultipleSubscribers) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = 1;
    const uint32_t subscriber1_id = 2;
    const uint32_t subscriber2_id = 3;

    // Register all modules
    ASSERT_EQ(cognitive_hub_register_module(hub, publisher_id, COG_CATEGORY_PERCEPTION, "Publisher", nullptr), 0);
    ASSERT_EQ(cognitive_hub_register_module(hub, subscriber1_id, COG_CATEGORY_MEMORY, "Subscriber1", nullptr), 0);
    ASSERT_EQ(cognitive_hub_register_module(hub, subscriber2_id, COG_CATEGORY_REASONING, "Subscriber2", nullptr), 0);

    // Both subscribers subscribe to same event type
    ASSERT_EQ(cognitive_hub_subscribe(hub, subscriber1_id, COG_EVENT_OUTPUT_READY, test_event_callback, nullptr), 0);
    ASSERT_EQ(cognitive_hub_subscribe(hub, subscriber2_id, COG_EVENT_OUTPUT_READY, test_event_callback_2, nullptr), 0);

    // Publish event
    cognitive_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = COG_EVENT_OUTPUT_READY;
    event_data.source_module_id = publisher_id;
    event_data.priority = COG_PRIORITY_NORMAL;

    int result = cognitive_hub_publish(hub, publisher_id, COG_EVENT_OUTPUT_READY, &event_data);
    EXPECT_EQ(result, 0) << "Publish should succeed";

    // Verify both callbacks were called
    EXPECT_EQ(g_callback_count, 1) << "First callback should be called once";
    EXPECT_EQ(g_callback_count_2, 1) << "Second callback should be called once";
}

/**
 * Test: CategoryFiltering
 * Verify events can be published to specific categories
 */
TEST_F(CognitiveIntegrationHubTest, CategoryFiltering) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = 1;
    const uint32_t memory_module_id = 2;
    const uint32_t reasoning_module_id = 3;

    // Register modules in different categories
    ASSERT_EQ(cognitive_hub_register_module(hub, publisher_id, COG_CATEGORY_PERCEPTION, "Publisher", nullptr), 0);
    ASSERT_EQ(cognitive_hub_register_module(hub, memory_module_id, COG_CATEGORY_MEMORY, "MemoryModule", nullptr), 0);
    ASSERT_EQ(cognitive_hub_register_module(hub, reasoning_module_id, COG_CATEGORY_REASONING, "ReasoningModule", nullptr), 0);

    // Both subscribe to same event type
    ASSERT_EQ(cognitive_hub_subscribe(hub, memory_module_id, COG_EVENT_STATE_CHANGE, test_event_callback, nullptr), 0);
    ASSERT_EQ(cognitive_hub_subscribe(hub, reasoning_module_id, COG_EVENT_STATE_CHANGE, test_event_callback_2, nullptr), 0);

    // Publish to MEMORY category only
    cognitive_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = COG_EVENT_STATE_CHANGE;
    event_data.source_module_id = publisher_id;
    event_data.priority = COG_PRIORITY_NORMAL;

    int result = cognitive_hub_publish_to_category(
        hub,
        publisher_id,
        COG_CATEGORY_MEMORY,
        COG_EVENT_STATE_CHANGE,
        &event_data
    );
    EXPECT_EQ(result, 0) << "Publish to category should succeed";

    // Only memory module callback should have been called
    EXPECT_EQ(g_callback_count, 1) << "Memory module callback should be called";
    EXPECT_EQ(g_callback_count_2, 0) << "Reasoning module callback should NOT be called";
}

/* ========================================================================
 * INTER-MODULE QUERY TESTS
 * ======================================================================== */

/**
 * Test: QueryModule
 * Verify modules can query each other
 */
TEST_F(CognitiveIntegrationHubTest, QueryModule) {
    ASSERT_NE(hub, nullptr);

    const uint32_t requester_id = 1;
    const uint32_t target_id = 2;

    // Register both modules
    ASSERT_EQ(cognitive_hub_register_module(hub, requester_id, COG_CATEGORY_REASONING, "Requester", nullptr), 0);
    ASSERT_EQ(cognitive_hub_register_module(hub, target_id, COG_CATEGORY_MEMORY, "Target", nullptr), 0);

    // Register query handler for target
    ASSERT_EQ(cognitive_hub_register_query_handler(hub, target_id, test_query_handler), 0);

    // Prepare query
    cognitive_query_t query;
    memset(&query, 0, sizeof(query));
    query.query_type = COG_QUERY_STATUS;
    query.query_params = nullptr;
    query.params_size = 0;

    cognitive_query_result_t result_data;
    memset(&result_data, 0, sizeof(result_data));

    // Execute query
    int result = cognitive_hub_query_module(
        hub,
        requester_id,
        target_id,
        &query,
        &result_data
    );
    EXPECT_EQ(result, 0) << "Query should succeed";
    EXPECT_EQ(result_data.status, 0) << "Query result status should indicate success";
}

/**
 * Test: GetModuleInfo
 * Verify module info can be retrieved after registration
 */
TEST_F(CognitiveIntegrationHubTest, GetModuleInfo) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = 42;
    const char* module_name = "InfoTestModule";
    int context_value = 999;

    // Register module
    ASSERT_EQ(cognitive_hub_register_module(hub, module_id, COG_CATEGORY_SELF, module_name, &context_value), 0);

    // Get module info
    cognitive_module_info_t info;
    memset(&info, 0, sizeof(info));

    int result = cognitive_hub_get_module_info(hub, module_id, &info);
    EXPECT_EQ(result, 0) << "Get module info should succeed";

    // Verify all fields
    EXPECT_EQ(info.module_id, module_id);
    EXPECT_EQ(info.category, COG_CATEGORY_SELF);
    EXPECT_STREQ(info.name, module_name);
    EXPECT_EQ(info.context, &context_value);
    EXPECT_TRUE(info.is_active);
}

/* ========================================================================
 * STATISTICS TESTS
 * ======================================================================== */

/**
 * Test: StatsTracking
 * Verify statistics are updated during operations
 */
TEST_F(CognitiveIntegrationHubTest, StatsTracking) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = 1;
    const uint32_t subscriber_id = 2;

    // Register modules
    ASSERT_EQ(cognitive_hub_register_module(hub, publisher_id, COG_CATEGORY_PERCEPTION, "Publisher", nullptr), 0);
    ASSERT_EQ(cognitive_hub_register_module(hub, subscriber_id, COG_CATEGORY_MEMORY, "Subscriber", nullptr), 0);

    // Subscribe
    ASSERT_EQ(cognitive_hub_subscribe(hub, subscriber_id, COG_EVENT_INPUT_RECEIVED, test_event_callback, nullptr), 0);

    // Publish event
    cognitive_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = COG_EVENT_INPUT_RECEIVED;
    event_data.source_module_id = publisher_id;
    event_data.priority = COG_PRIORITY_NORMAL;

    ASSERT_EQ(cognitive_hub_publish(hub, publisher_id, COG_EVENT_INPUT_RECEIVED, &event_data), 0);

    // Get stats
    cognitive_hub_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify stats
    EXPECT_EQ(stats.registered_modules, 2u) << "Should have 2 registered modules";
    EXPECT_GE(stats.active_subscriptions, 1u) << "Should have at least 1 subscription";
    EXPECT_GE(stats.events_published, 1u) << "Should have at least 1 event published";
    EXPECT_GE(stats.events_delivered, 1u) << "Should have at least 1 event delivered";
}

/**
 * Test: StatsReset
 * Verify statistics can be reset to zero
 */
TEST_F(CognitiveIntegrationHubTest, StatsReset) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = 1;
    const uint32_t subscriber_id = 2;

    // Register modules and publish some events
    ASSERT_EQ(cognitive_hub_register_module(hub, publisher_id, COG_CATEGORY_PERCEPTION, "Publisher", nullptr), 0);
    ASSERT_EQ(cognitive_hub_register_module(hub, subscriber_id, COG_CATEGORY_MEMORY, "Subscriber", nullptr), 0);
    ASSERT_EQ(cognitive_hub_subscribe(hub, subscriber_id, COG_EVENT_INPUT_RECEIVED, test_event_callback, nullptr), 0);

    cognitive_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = COG_EVENT_INPUT_RECEIVED;
    event_data.source_module_id = publisher_id;
    event_data.priority = COG_PRIORITY_NORMAL;

    ASSERT_EQ(cognitive_hub_publish(hub, publisher_id, COG_EVENT_INPUT_RECEIVED, &event_data), 0);

    // Reset stats
    int result = cognitive_hub_reset_stats(hub);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    cognitive_hub_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    result = cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed after reset";

    // Event counters should be reset
    EXPECT_EQ(stats.events_published, 0u) << "Events published should be reset to 0";
    EXPECT_EQ(stats.events_delivered, 0u) << "Events delivered should be reset to 0";

    // Note: registered_modules and active_subscriptions may not be reset
    // as they reflect current state rather than cumulative counts
}

/* ========================================================================
 * EDGE CASE / ERROR HANDLING TESTS
 * ======================================================================== */

/**
 * Test: NullHubHandling
 * Verify functions handle NULL hub gracefully
 */
TEST_F(CognitiveIntegrationHubTest, NullHubHandling) {
    // All operations on NULL hub should return error
    EXPECT_EQ(cognitive_hub_register_module(nullptr, 1, COG_CATEGORY_MEMORY, "Test", nullptr), -1);
    EXPECT_EQ(cognitive_hub_unregister_module(nullptr, 1), -1);
    EXPECT_EQ(cognitive_hub_subscribe(nullptr, 1, COG_EVENT_STATE_CHANGE, test_event_callback, nullptr), -1);
    EXPECT_EQ(cognitive_hub_unsubscribe(nullptr, 1, COG_EVENT_STATE_CHANGE), -1);

    cognitive_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    EXPECT_EQ(cognitive_hub_publish(nullptr, 1, COG_EVENT_STATE_CHANGE, &event_data), -1);

    cognitive_hub_stats_t stats;
    EXPECT_EQ(cognitive_hub_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(cognitive_hub_reset_stats(nullptr), -1);

    cognitive_module_info_t info;
    EXPECT_EQ(cognitive_hub_get_module_info(nullptr, 1, &info), -1);
}

/**
 * Test: UnregisteredModuleOperations
 * Verify operations fail for unregistered modules
 */
TEST_F(CognitiveIntegrationHubTest, UnregisteredModuleOperations) {
    ASSERT_NE(hub, nullptr);

    const uint32_t unregistered_id = 999;

    // Subscribe with unregistered module should fail
    EXPECT_EQ(cognitive_hub_subscribe(hub, unregistered_id, COG_EVENT_STATE_CHANGE, test_event_callback, nullptr), -1);

    // Publish with unregistered module should fail
    cognitive_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = COG_EVENT_STATE_CHANGE;
    event_data.source_module_id = unregistered_id;
    EXPECT_EQ(cognitive_hub_publish(hub, unregistered_id, COG_EVENT_STATE_CHANGE, &event_data), -1);

    // Get info for unregistered module should fail
    cognitive_module_info_t info;
    EXPECT_EQ(cognitive_hub_get_module_info(hub, unregistered_id, &info), -1);
}

/**
 * Test: NullCallbackRejection
 * Verify NULL callback is rejected during subscription
 */
TEST_F(CognitiveIntegrationHubTest, NullCallbackRejection) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = 1;

    // Register module
    ASSERT_EQ(cognitive_hub_register_module(hub, module_id, COG_CATEGORY_MEMORY, "TestModule", nullptr), 0);

    // Subscribe with NULL callback should fail
    int result = cognitive_hub_subscribe(hub, module_id, COG_EVENT_STATE_CHANGE, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "NULL callback should be rejected";
}

/**
 * Test: ModuleActivation
 * Verify module activation/deactivation works
 */
TEST_F(CognitiveIntegrationHubTest, ModuleActivation) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = 1;

    // Register module
    ASSERT_EQ(cognitive_hub_register_module(hub, module_id, COG_CATEGORY_MEMORY, "TestModule", nullptr), 0);

    // Verify module starts active
    cognitive_module_info_t info;
    ASSERT_EQ(cognitive_hub_get_module_info(hub, module_id, &info), 0);
    EXPECT_TRUE(info.is_active);

    // Deactivate module
    EXPECT_EQ(cognitive_hub_set_module_active(hub, module_id, false), 0);

    // Verify module is inactive
    ASSERT_EQ(cognitive_hub_get_module_info(hub, module_id, &info), 0);
    EXPECT_FALSE(info.is_active);

    // Reactivate module
    EXPECT_EQ(cognitive_hub_set_module_active(hub, module_id, true), 0);

    // Verify module is active again
    ASSERT_EQ(cognitive_hub_get_module_info(hub, module_id, &info), 0);
    EXPECT_TRUE(info.is_active);
}

/**
 * Test: QueryUnregisteredTarget
 * Verify querying unregistered target fails gracefully
 */
TEST_F(CognitiveIntegrationHubTest, QueryUnregisteredTarget) {
    ASSERT_NE(hub, nullptr);

    const uint32_t requester_id = 1;
    const uint32_t unregistered_target_id = 999;

    // Register requester only
    ASSERT_EQ(cognitive_hub_register_module(hub, requester_id, COG_CATEGORY_REASONING, "Requester", nullptr), 0);

    // Prepare query
    cognitive_query_t query;
    memset(&query, 0, sizeof(query));
    query.query_type = COG_QUERY_STATUS;

    cognitive_query_result_t result_data;
    memset(&result_data, 0, sizeof(result_data));

    // Query unregistered target should fail
    int result = cognitive_hub_query_module(hub, requester_id, unregistered_target_id, &query, &result_data);
    EXPECT_EQ(result, -1) << "Query to unregistered target should fail";
}

/**
 * Test: DestroyNullHub
 * Verify destroying NULL hub is safe
 */
TEST_F(CognitiveIntegrationHubTest, DestroyNullHub) {
    // This should not crash
    cognitive_hub_destroy(nullptr);
    SUCCEED() << "Destroying NULL hub should be safe";
}
