/**
 * @file test_training_integration_hub.cpp
 * @brief Unit tests for training integration hub
 * @version 1.0.0
 * @date 2025
 *
 * Tests the training integration hub functionality including:
 * - Hub creation and destruction
 * - Module registration and unregistration
 * - Event subscription and publishing
 * - Inter-module queries
 * - Statistics tracking
 * - Training-specific convenience functions
 */

#include <gtest/gtest.h>
#include <cstring>

#include "training/integration/nimcp_training_integration_hub.h"
#include "training/integration/nimcp_training_event_types.h"

/* ========================================================================
 * GLOBAL TEST HELPERS
 * ======================================================================== */

static int g_callback_count = 0;
static uint32_t g_last_event_source = 0;
static training_event_type_t g_last_event_type = TRAINING_EVENT_DIFFICULTY_UPDATED;
static void* g_last_user_data = nullptr;
static float g_last_loss_value = 0.0f;
static float g_last_learning_rate = 0.0f;

/**
 * Test event callback that tracks call count and event details
 */
static int test_event_callback(const training_event_data_t* event, void* user_data) {
    g_callback_count++;
    if (event != nullptr) {
        g_last_event_source = event->source_module_id;
        g_last_event_type = event->event_type;
        g_last_loss_value = event->loss_value;
        g_last_learning_rate = event->learning_rate;
    }
    g_last_user_data = user_data;
    return 0;
}

/**
 * Second callback for multiple subscriber tests
 */
static int g_callback_count_2 = 0;

static int test_event_callback_2(const training_event_data_t* event, void* user_data) {
    (void)event;
    (void)user_data;
    g_callback_count_2++;
    return 0;
}

/**
 * Test query handler that always succeeds
 */
static int test_query_handler(const training_query_t* query,
                              training_query_result_t* result,
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

class TrainingIntegrationHubTest : public ::testing::Test {
protected:
    training_integration_hub_t hub;
    training_hub_config_t config;

    void SetUp() override {
        // Reset global state
        g_callback_count = 0;
        g_callback_count_2 = 0;
        g_last_event_source = 0;
        g_last_event_type = TRAINING_EVENT_DIFFICULTY_UPDATED;
        g_last_user_data = nullptr;
        g_last_loss_value = 0.0f;
        g_last_learning_rate = 0.0f;

        // Get default config and create hub
        config = training_hub_default_config();
        hub = training_hub_create(&config);
    }

    void TearDown() override {
        if (hub != nullptr) {
            training_hub_destroy(hub);
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
TEST_F(TrainingIntegrationHubTest, HubCreation) {
    // Hub should be created in SetUp
    ASSERT_NE(hub, nullptr) << "Hub creation should succeed";

    // Verify default config values are sensible
    EXPECT_GT(config.max_modules, 0u) << "max_modules should be positive";
    EXPECT_GT(config.max_subscriptions, 0u) << "max_subscriptions should be positive";
    EXPECT_TRUE(config.enable_metrics) << "metrics should be enabled by default";
}

/**
 * Test: HubCreationNullConfig
 * Verify hub can be created with NULL config (uses defaults)
 */
TEST_F(TrainingIntegrationHubTest, HubCreationNullConfig) {
    // Destroy the hub created in SetUp
    training_hub_destroy(hub);

    // Create with NULL config - should use defaults
    hub = training_hub_create(nullptr);
    ASSERT_NE(hub, nullptr) << "Hub creation with NULL config should succeed";
}

/**
 * Test: DefaultConfig
 * Verify default configuration values
 */
TEST_F(TrainingIntegrationHubTest, DefaultConfig) {
    training_hub_config_t default_config = training_hub_default_config();

    EXPECT_EQ(default_config.max_modules, 32u);
    EXPECT_EQ(default_config.max_subscriptions, 128u);
    EXPECT_TRUE(default_config.enable_async);
    EXPECT_EQ(default_config.event_queue_size, 512u);
    EXPECT_TRUE(default_config.enable_metrics);
}

/* ========================================================================
 * MODULE REGISTRATION TESTS
 * ======================================================================== */

/**
 * Test: ModuleRegistration
 * Verify module can be registered and info can be retrieved
 */
TEST_F(TrainingIntegrationHubTest, ModuleRegistration) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = TRAINING_MODULE_CURRICULUM_LEARNING;
    const char* module_name = "CurriculumLearning";
    int dummy_context = 42;

    // Register module
    int result = training_hub_register_module(
        hub,
        module_id,
        TRAINING_CATEGORY_CURRICULUM,
        module_name,
        &dummy_context
    );
    EXPECT_EQ(result, 0) << "Module registration should succeed";

    // Verify module info
    training_module_info_t info;
    memset(&info, 0, sizeof(info));

    result = training_hub_get_module_info(hub, module_id, &info);
    EXPECT_EQ(result, 0) << "Get module info should succeed";
    EXPECT_EQ(info.module_id, module_id) << "Module ID should match";
    EXPECT_EQ(info.category, TRAINING_CATEGORY_CURRICULUM) << "Category should match";
    EXPECT_STREQ(info.name, module_name) << "Name should match";
    EXPECT_EQ(info.context, &dummy_context) << "Context should match";
    EXPECT_TRUE(info.is_active) << "Module should be active by default";
}

/**
 * Test: ModuleUnregistration
 * Verify module can be unregistered
 */
TEST_F(TrainingIntegrationHubTest, ModuleUnregistration) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = TRAINING_MODULE_META_LEARNING;

    // Register module
    int result = training_hub_register_module(
        hub,
        module_id,
        TRAINING_CATEGORY_OPTIMIZATION,
        "TempModule",
        nullptr
    );
    EXPECT_EQ(result, 0) << "Registration should succeed";

    // Unregister module
    result = training_hub_unregister_module(hub, module_id);
    EXPECT_EQ(result, 0) << "Unregistration should succeed";

    // Verify module is no longer registered
    training_module_info_t info;
    result = training_hub_get_module_info(hub, module_id, &info);
    EXPECT_EQ(result, -1) << "Get info for unregistered module should fail";
}

/**
 * Test: DuplicateRegistration
 * Verify duplicate module ID registration is rejected
 */
TEST_F(TrainingIntegrationHubTest, DuplicateRegistration) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = TRAINING_MODULE_QUANTIZATION_AWARE;

    // Register module first time
    int result = training_hub_register_module(
        hub,
        module_id,
        TRAINING_CATEGORY_COMPRESSION,
        "FirstModule",
        nullptr
    );
    EXPECT_EQ(result, 0) << "First registration should succeed";

    // Attempt duplicate registration
    result = training_hub_register_module(
        hub,
        module_id,
        TRAINING_CATEGORY_DISTRIBUTED,
        "DuplicateModule",
        nullptr
    );
    EXPECT_EQ(result, -1) << "Duplicate registration should be rejected";
}

/**
 * Test: InvalidCategoryRejection
 * Verify invalid category is rejected during registration
 */
TEST_F(TrainingIntegrationHubTest, InvalidCategoryRejection) {
    ASSERT_NE(hub, nullptr);

    // Attempt registration with invalid category
    int result = training_hub_register_module(
        hub,
        1,
        TRAINING_CATEGORY_COUNT,  // Invalid - sentinel value
        "InvalidModule",
        nullptr
    );
    EXPECT_EQ(result, -1) << "Invalid category should be rejected";
}

/* ========================================================================
 * EVENT SUBSCRIPTION TESTS
 * ======================================================================== */

/**
 * Test: EventSubscription
 * Verify module can subscribe to event type
 */
TEST_F(TrainingIntegrationHubTest, EventSubscription) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = TRAINING_MODULE_CURRICULUM_LEARNING;
    int user_data = 123;

    // Register module first
    int result = training_hub_register_module(
        hub,
        module_id,
        TRAINING_CATEGORY_CURRICULUM,
        "SubscriberModule",
        nullptr
    );
    ASSERT_EQ(result, 0) << "Module registration required";

    // Subscribe to event
    result = training_hub_subscribe(
        hub,
        module_id,
        TRAINING_EVENT_DIFFICULTY_UPDATED,
        test_event_callback,
        &user_data
    );
    EXPECT_EQ(result, 0) << "Subscription should succeed";
}

/**
 * Test: EventUnsubscription
 * Verify module can unsubscribe from event type
 */
TEST_F(TrainingIntegrationHubTest, EventUnsubscription) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = TRAINING_MODULE_LR_SCHEDULER;

    // Register module
    int result = training_hub_register_module(
        hub,
        module_id,
        TRAINING_CATEGORY_OPTIMIZATION,
        "UnsubModule",
        nullptr
    );
    ASSERT_EQ(result, 0);

    // Subscribe
    result = training_hub_subscribe(
        hub,
        module_id,
        TRAINING_EVENT_LR_ADJUSTED,
        test_event_callback,
        nullptr
    );
    ASSERT_EQ(result, 0) << "Subscribe should succeed";

    // Unsubscribe
    result = training_hub_unsubscribe(
        hub,
        module_id,
        TRAINING_EVENT_LR_ADJUSTED
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
TEST_F(TrainingIntegrationHubTest, EventPublishing) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = TRAINING_MODULE_OPTIMIZER;
    const uint32_t subscriber_id = TRAINING_MODULE_LR_SCHEDULER;
    int user_data = 456;

    // Register publisher
    int result = training_hub_register_module(
        hub,
        publisher_id,
        TRAINING_CATEGORY_OPTIMIZATION,
        "Publisher",
        nullptr
    );
    ASSERT_EQ(result, 0);

    // Register subscriber
    result = training_hub_register_module(
        hub,
        subscriber_id,
        TRAINING_CATEGORY_OPTIMIZATION,
        "Subscriber",
        nullptr
    );
    ASSERT_EQ(result, 0);

    // Subscribe to event
    result = training_hub_subscribe(
        hub,
        subscriber_id,
        TRAINING_EVENT_LOSS_COMPUTED,
        test_event_callback,
        &user_data
    );
    ASSERT_EQ(result, 0);

    // Prepare event data
    training_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = TRAINING_EVENT_LOSS_COMPUTED;
    event_data.source_module_id = publisher_id;
    event_data.priority = TRAINING_PRIORITY_NORMAL;
    event_data.loss_value = 0.5f;
    event_data.payload = nullptr;
    event_data.payload_size = 0;

    // Publish event
    result = training_hub_publish(
        hub,
        publisher_id,
        TRAINING_EVENT_LOSS_COMPUTED,
        &event_data
    );
    EXPECT_EQ(result, 0) << "Publish should succeed";

    // Verify callback was called
    EXPECT_EQ(g_callback_count, 1) << "Callback should be called once";
    EXPECT_EQ(g_last_event_source, publisher_id) << "Event source should match publisher";
    EXPECT_EQ(g_last_event_type, TRAINING_EVENT_LOSS_COMPUTED) << "Event type should match";
    EXPECT_EQ(g_last_user_data, &user_data) << "User data should be passed through";
    EXPECT_FLOAT_EQ(g_last_loss_value, 0.5f) << "Loss value should be passed through";
}

/**
 * Test: MultipleSubscribers
 * Verify multiple modules receive the same event
 */
TEST_F(TrainingIntegrationHubTest, MultipleSubscribers) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = TRAINING_MODULE_OPTIMIZER;
    const uint32_t subscriber1_id = TRAINING_MODULE_CURRICULUM_LEARNING;
    const uint32_t subscriber2_id = TRAINING_MODULE_META_LEARNING;

    // Register all modules
    ASSERT_EQ(training_hub_register_module(hub, publisher_id, TRAINING_CATEGORY_OPTIMIZATION, "Publisher", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, subscriber1_id, TRAINING_CATEGORY_CURRICULUM, "Subscriber1", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, subscriber2_id, TRAINING_CATEGORY_OPTIMIZATION, "Subscriber2", nullptr), 0);

    // Both subscribers subscribe to same event type
    ASSERT_EQ(training_hub_subscribe(hub, subscriber1_id, TRAINING_EVENT_EPOCH_COMPLETE, test_event_callback, nullptr), 0);
    ASSERT_EQ(training_hub_subscribe(hub, subscriber2_id, TRAINING_EVENT_EPOCH_COMPLETE, test_event_callback_2, nullptr), 0);

    // Publish event
    training_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = TRAINING_EVENT_EPOCH_COMPLETE;
    event_data.source_module_id = publisher_id;
    event_data.priority = TRAINING_PRIORITY_NORMAL;

    int result = training_hub_publish(hub, publisher_id, TRAINING_EVENT_EPOCH_COMPLETE, &event_data);
    EXPECT_EQ(result, 0) << "Publish should succeed";

    // Verify both callbacks were called
    EXPECT_EQ(g_callback_count, 1) << "First callback should be called once";
    EXPECT_EQ(g_callback_count_2, 1) << "Second callback should be called once";
}

/**
 * Test: CategoryFiltering
 * Verify events can be published to specific categories
 */
TEST_F(TrainingIntegrationHubTest, CategoryFiltering) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = TRAINING_MODULE_OPTIMIZER;
    const uint32_t curriculum_module_id = TRAINING_MODULE_CURRICULUM_LEARNING;
    const uint32_t compression_module_id = TRAINING_MODULE_QUANTIZATION_AWARE;

    // Register modules in different categories
    ASSERT_EQ(training_hub_register_module(hub, publisher_id, TRAINING_CATEGORY_OPTIMIZATION, "Publisher", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, curriculum_module_id, TRAINING_CATEGORY_CURRICULUM, "CurriculumModule", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, compression_module_id, TRAINING_CATEGORY_COMPRESSION, "CompressionModule", nullptr), 0);

    // Both subscribe to same event type
    ASSERT_EQ(training_hub_subscribe(hub, curriculum_module_id, TRAINING_EVENT_BATCH_COMPLETE, test_event_callback, nullptr), 0);
    ASSERT_EQ(training_hub_subscribe(hub, compression_module_id, TRAINING_EVENT_BATCH_COMPLETE, test_event_callback_2, nullptr), 0);

    // Publish to CURRICULUM category only
    training_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = TRAINING_EVENT_BATCH_COMPLETE;
    event_data.source_module_id = publisher_id;
    event_data.priority = TRAINING_PRIORITY_NORMAL;

    int result = training_hub_publish_to_category(
        hub,
        publisher_id,
        TRAINING_CATEGORY_CURRICULUM,
        TRAINING_EVENT_BATCH_COMPLETE,
        &event_data
    );
    EXPECT_EQ(result, 0) << "Publish to category should succeed";

    // Only curriculum module callback should have been called
    EXPECT_EQ(g_callback_count, 1) << "Curriculum module callback should be called";
    EXPECT_EQ(g_callback_count_2, 0) << "Compression module callback should NOT be called";
}

/* ========================================================================
 * INTER-MODULE QUERY TESTS
 * ======================================================================== */

/**
 * Test: QueryModule
 * Verify modules can query each other
 */
TEST_F(TrainingIntegrationHubTest, QueryModule) {
    ASSERT_NE(hub, nullptr);

    const uint32_t requester_id = TRAINING_MODULE_CURRICULUM_LEARNING;
    const uint32_t target_id = TRAINING_MODULE_META_LEARNING;

    // Register both modules
    ASSERT_EQ(training_hub_register_module(hub, requester_id, TRAINING_CATEGORY_CURRICULUM, "Requester", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, target_id, TRAINING_CATEGORY_OPTIMIZATION, "Target", nullptr), 0);

    // Register query handler for target
    ASSERT_EQ(training_hub_register_query_handler(hub, target_id, test_query_handler), 0);

    // Prepare query
    training_query_t query;
    memset(&query, 0, sizeof(query));
    query.query_type = TRAINING_QUERY_STATUS;
    query.query_params = nullptr;
    query.params_size = 0;

    training_query_result_t result_data;
    memset(&result_data, 0, sizeof(result_data));

    // Execute query
    int result = training_hub_query_module(
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
TEST_F(TrainingIntegrationHubTest, GetModuleInfo) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = TRAINING_MODULE_CHECKPOINT;
    const char* module_name = "CheckpointModule";
    int context_value = 999;

    // Register module
    ASSERT_EQ(training_hub_register_module(hub, module_id, TRAINING_CATEGORY_DATA, module_name, &context_value), 0);

    // Get module info
    training_module_info_t info;
    memset(&info, 0, sizeof(info));

    int result = training_hub_get_module_info(hub, module_id, &info);
    EXPECT_EQ(result, 0) << "Get module info should succeed";

    // Verify all fields
    EXPECT_EQ(info.module_id, module_id);
    EXPECT_EQ(info.category, TRAINING_CATEGORY_DATA);
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
TEST_F(TrainingIntegrationHubTest, StatsTracking) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = TRAINING_MODULE_OPTIMIZER;
    const uint32_t subscriber_id = TRAINING_MODULE_LR_SCHEDULER;

    // Register modules
    ASSERT_EQ(training_hub_register_module(hub, publisher_id, TRAINING_CATEGORY_OPTIMIZATION, "Publisher", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, subscriber_id, TRAINING_CATEGORY_OPTIMIZATION, "Subscriber", nullptr), 0);

    // Subscribe
    ASSERT_EQ(training_hub_subscribe(hub, subscriber_id, TRAINING_EVENT_LOSS_COMPUTED, test_event_callback, nullptr), 0);

    // Publish event
    training_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = TRAINING_EVENT_LOSS_COMPUTED;
    event_data.source_module_id = publisher_id;
    event_data.priority = TRAINING_PRIORITY_NORMAL;

    ASSERT_EQ(training_hub_publish(hub, publisher_id, TRAINING_EVENT_LOSS_COMPUTED, &event_data), 0);

    // Get stats
    training_hub_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = training_hub_get_stats(hub, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify stats
    EXPECT_EQ(stats.registered_modules, 2u) << "Should have 2 registered modules";
    EXPECT_GE(stats.active_subscriptions, 1u) << "Should have at least 1 subscription";
    EXPECT_GE(stats.events_published, 1u) << "Should have at least 1 event published";
    EXPECT_GE(stats.events_delivered, 1u) << "Should have at least 1 event delivered";
}

/**
 * Test: TrainingSpecificMetrics
 * Verify training-specific metrics are tracked
 */
TEST_F(TrainingIntegrationHubTest, TrainingSpecificMetrics) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = TRAINING_MODULE_CURRICULUM_LEARNING;
    const uint32_t subscriber_id = TRAINING_MODULE_META_LEARNING;

    // Register modules
    ASSERT_EQ(training_hub_register_module(hub, publisher_id, TRAINING_CATEGORY_CURRICULUM, "Publisher", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, subscriber_id, TRAINING_CATEGORY_OPTIMIZATION, "Subscriber", nullptr), 0);

    // Subscribe to difficulty updates
    ASSERT_EQ(training_hub_subscribe(hub, subscriber_id, TRAINING_EVENT_DIFFICULTY_UPDATED, test_event_callback, nullptr), 0);

    // Publish difficulty update event
    training_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = TRAINING_EVENT_DIFFICULTY_UPDATED;
    event_data.source_module_id = publisher_id;
    event_data.priority = TRAINING_PRIORITY_NORMAL;

    ASSERT_EQ(training_hub_publish(hub, publisher_id, TRAINING_EVENT_DIFFICULTY_UPDATED, &event_data), 0);

    // Get stats
    training_hub_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = training_hub_get_stats(hub, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify training-specific metrics
    EXPECT_GE(stats.difficulty_updates, 1u) << "Should track difficulty updates";
}

/**
 * Test: StatsReset
 * Verify statistics can be reset to zero
 */
TEST_F(TrainingIntegrationHubTest, StatsReset) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = TRAINING_MODULE_OPTIMIZER;
    const uint32_t subscriber_id = TRAINING_MODULE_LR_SCHEDULER;

    // Register modules and publish some events
    ASSERT_EQ(training_hub_register_module(hub, publisher_id, TRAINING_CATEGORY_OPTIMIZATION, "Publisher", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, subscriber_id, TRAINING_CATEGORY_OPTIMIZATION, "Subscriber", nullptr), 0);
    ASSERT_EQ(training_hub_subscribe(hub, subscriber_id, TRAINING_EVENT_LOSS_COMPUTED, test_event_callback, nullptr), 0);

    training_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = TRAINING_EVENT_LOSS_COMPUTED;
    event_data.source_module_id = publisher_id;
    event_data.priority = TRAINING_PRIORITY_NORMAL;

    ASSERT_EQ(training_hub_publish(hub, publisher_id, TRAINING_EVENT_LOSS_COMPUTED, &event_data), 0);

    // Reset stats
    int result = training_hub_reset_stats(hub);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    training_hub_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    result = training_hub_get_stats(hub, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed after reset";

    // Event counters should be reset
    EXPECT_EQ(stats.events_published, 0u) << "Events published should be reset to 0";
    EXPECT_EQ(stats.events_delivered, 0u) << "Events delivered should be reset to 0";
    EXPECT_EQ(stats.difficulty_updates, 0u) << "Difficulty updates should be reset to 0";
    EXPECT_EQ(stats.lr_adjustments, 0u) << "LR adjustments should be reset to 0";
}

/* ========================================================================
 * CONVENIENCE HELPER TESTS
 * ======================================================================== */

/**
 * Test: PublishDifficultyUpdate
 * Verify the convenience function for difficulty updates
 */
TEST_F(TrainingIntegrationHubTest, PublishDifficultyUpdate) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = TRAINING_MODULE_CURRICULUM_LEARNING;
    const uint32_t subscriber_id = TRAINING_MODULE_META_LEARNING;

    // Register modules
    ASSERT_EQ(training_hub_register_module(hub, publisher_id, TRAINING_CATEGORY_CURRICULUM, "Publisher", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, subscriber_id, TRAINING_CATEGORY_OPTIMIZATION, "Subscriber", nullptr), 0);

    // Subscribe to difficulty updates
    ASSERT_EQ(training_hub_subscribe(hub, subscriber_id, TRAINING_EVENT_DIFFICULTY_UPDATED, test_event_callback, nullptr), 0);

    // Use convenience function
    int result = training_hub_publish_difficulty_update(hub, publisher_id, 0.3f, 0.5f);
    EXPECT_EQ(result, 0) << "Publish difficulty update should succeed";

    // Verify callback was called
    EXPECT_EQ(g_callback_count, 1) << "Callback should be called";
    EXPECT_EQ(g_last_event_type, TRAINING_EVENT_DIFFICULTY_UPDATED) << "Event type should be difficulty updated";
}

/**
 * Test: PublishLoss
 * Verify the convenience function for loss publishing
 */
TEST_F(TrainingIntegrationHubTest, PublishLoss) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = TRAINING_MODULE_OPTIMIZER;
    const uint32_t subscriber_id = TRAINING_MODULE_LR_SCHEDULER;

    // Register modules
    ASSERT_EQ(training_hub_register_module(hub, publisher_id, TRAINING_CATEGORY_OPTIMIZATION, "Publisher", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, subscriber_id, TRAINING_CATEGORY_OPTIMIZATION, "Subscriber", nullptr), 0);

    // Subscribe to loss events
    ASSERT_EQ(training_hub_subscribe(hub, subscriber_id, TRAINING_EVENT_LOSS_COMPUTED, test_event_callback, nullptr), 0);

    // Use convenience function
    int result = training_hub_publish_loss(hub, publisher_id, 5, 100, 0.42f);
    EXPECT_EQ(result, 0) << "Publish loss should succeed";

    // Verify callback was called
    EXPECT_EQ(g_callback_count, 1) << "Callback should be called";
    EXPECT_EQ(g_last_event_type, TRAINING_EVENT_LOSS_COMPUTED) << "Event type should be loss computed";
    EXPECT_FLOAT_EQ(g_last_loss_value, 0.42f) << "Loss value should match";
}

/**
 * Test: PublishLRAdjustment
 * Verify the convenience function for LR adjustments
 */
TEST_F(TrainingIntegrationHubTest, PublishLRAdjustment) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = TRAINING_MODULE_LR_SCHEDULER;
    const uint32_t subscriber_id = TRAINING_MODULE_OPTIMIZER;

    // Register modules
    ASSERT_EQ(training_hub_register_module(hub, publisher_id, TRAINING_CATEGORY_OPTIMIZATION, "Publisher", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, subscriber_id, TRAINING_CATEGORY_OPTIMIZATION, "Subscriber", nullptr), 0);

    // Subscribe to LR events
    ASSERT_EQ(training_hub_subscribe(hub, subscriber_id, TRAINING_EVENT_LR_ADJUSTED, test_event_callback, nullptr), 0);

    // Use convenience function
    int result = training_hub_publish_lr_adjustment(hub, publisher_id, 0.01f, 0.001f);
    EXPECT_EQ(result, 0) << "Publish LR adjustment should succeed";

    // Verify callback was called
    EXPECT_EQ(g_callback_count, 1) << "Callback should be called";
    EXPECT_EQ(g_last_event_type, TRAINING_EVENT_LR_ADJUSTED) << "Event type should be LR adjusted";
    EXPECT_FLOAT_EQ(g_last_learning_rate, 0.001f) << "Learning rate should match";
}

/**
 * Test: PublishEpochComplete
 * Verify the convenience function for epoch completion
 */
TEST_F(TrainingIntegrationHubTest, PublishEpochComplete) {
    ASSERT_NE(hub, nullptr);

    const uint32_t publisher_id = TRAINING_MODULE_OPTIMIZER;
    const uint32_t subscriber_id = TRAINING_MODULE_CHECKPOINT;

    // Register modules
    ASSERT_EQ(training_hub_register_module(hub, publisher_id, TRAINING_CATEGORY_OPTIMIZATION, "Publisher", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, subscriber_id, TRAINING_CATEGORY_DATA, "Subscriber", nullptr), 0);

    // Subscribe to epoch events
    ASSERT_EQ(training_hub_subscribe(hub, subscriber_id, TRAINING_EVENT_EPOCH_COMPLETE, test_event_callback, nullptr), 0);

    // Use convenience function
    int result = training_hub_publish_epoch_complete(hub, publisher_id, 10, 0.25f);
    EXPECT_EQ(result, 0) << "Publish epoch complete should succeed";

    // Verify callback was called
    EXPECT_EQ(g_callback_count, 1) << "Callback should be called";
    EXPECT_EQ(g_last_event_type, TRAINING_EVENT_EPOCH_COMPLETE) << "Event type should be epoch complete";
    EXPECT_FLOAT_EQ(g_last_loss_value, 0.25f) << "Average loss should match";
}

/* ========================================================================
 * EDGE CASE / ERROR HANDLING TESTS
 * ======================================================================== */

/**
 * Test: NullHubHandling
 * Verify functions handle NULL hub gracefully
 */
TEST_F(TrainingIntegrationHubTest, NullHubHandling) {
    // All operations on NULL hub should return error
    EXPECT_EQ(training_hub_register_module(nullptr, 1, TRAINING_CATEGORY_CURRICULUM, "Test", nullptr), -1);
    EXPECT_EQ(training_hub_unregister_module(nullptr, 1), -1);
    EXPECT_EQ(training_hub_subscribe(nullptr, 1, TRAINING_EVENT_DIFFICULTY_UPDATED, test_event_callback, nullptr), -1);
    EXPECT_EQ(training_hub_unsubscribe(nullptr, 1, TRAINING_EVENT_DIFFICULTY_UPDATED), -1);

    training_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    EXPECT_EQ(training_hub_publish(nullptr, 1, TRAINING_EVENT_DIFFICULTY_UPDATED, &event_data), -1);

    training_hub_stats_t stats;
    EXPECT_EQ(training_hub_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(training_hub_reset_stats(nullptr), -1);

    training_module_info_t info;
    EXPECT_EQ(training_hub_get_module_info(nullptr, 1, &info), -1);
}

/**
 * Test: UnregisteredModuleOperations
 * Verify operations fail for unregistered modules
 */
TEST_F(TrainingIntegrationHubTest, UnregisteredModuleOperations) {
    ASSERT_NE(hub, nullptr);

    const uint32_t unregistered_id = 999;

    // Subscribe with unregistered module should fail
    EXPECT_EQ(training_hub_subscribe(hub, unregistered_id, TRAINING_EVENT_DIFFICULTY_UPDATED, test_event_callback, nullptr), -1);

    // Publish with unregistered module should fail
    training_event_data_t event_data;
    memset(&event_data, 0, sizeof(event_data));
    event_data.event_type = TRAINING_EVENT_DIFFICULTY_UPDATED;
    event_data.source_module_id = unregistered_id;
    EXPECT_EQ(training_hub_publish(hub, unregistered_id, TRAINING_EVENT_DIFFICULTY_UPDATED, &event_data), -1);

    // Get info for unregistered module should fail
    training_module_info_t info;
    EXPECT_EQ(training_hub_get_module_info(hub, unregistered_id, &info), -1);
}

/**
 * Test: NullCallbackRejection
 * Verify NULL callback is rejected during subscription
 */
TEST_F(TrainingIntegrationHubTest, NullCallbackRejection) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = TRAINING_MODULE_CURRICULUM_LEARNING;

    // Register module
    ASSERT_EQ(training_hub_register_module(hub, module_id, TRAINING_CATEGORY_CURRICULUM, "TestModule", nullptr), 0);

    // Subscribe with NULL callback should fail
    int result = training_hub_subscribe(hub, module_id, TRAINING_EVENT_DIFFICULTY_UPDATED, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "NULL callback should be rejected";
}

/**
 * Test: ModuleActivation
 * Verify module activation/deactivation works
 */
TEST_F(TrainingIntegrationHubTest, ModuleActivation) {
    ASSERT_NE(hub, nullptr);

    const uint32_t module_id = TRAINING_MODULE_CURRICULUM_LEARNING;

    // Register module
    ASSERT_EQ(training_hub_register_module(hub, module_id, TRAINING_CATEGORY_CURRICULUM, "TestModule", nullptr), 0);

    // Verify module starts active
    training_module_info_t info;
    ASSERT_EQ(training_hub_get_module_info(hub, module_id, &info), 0);
    EXPECT_TRUE(info.is_active);

    // Deactivate module
    EXPECT_EQ(training_hub_set_module_active(hub, module_id, false), 0);

    // Verify module is inactive
    ASSERT_EQ(training_hub_get_module_info(hub, module_id, &info), 0);
    EXPECT_FALSE(info.is_active);

    // Reactivate module
    EXPECT_EQ(training_hub_set_module_active(hub, module_id, true), 0);

    // Verify module is active again
    ASSERT_EQ(training_hub_get_module_info(hub, module_id, &info), 0);
    EXPECT_TRUE(info.is_active);
}

/**
 * Test: QueryUnregisteredTarget
 * Verify querying unregistered target fails gracefully
 */
TEST_F(TrainingIntegrationHubTest, QueryUnregisteredTarget) {
    ASSERT_NE(hub, nullptr);

    const uint32_t requester_id = TRAINING_MODULE_CURRICULUM_LEARNING;
    const uint32_t unregistered_target_id = 999;

    // Register requester only
    ASSERT_EQ(training_hub_register_module(hub, requester_id, TRAINING_CATEGORY_CURRICULUM, "Requester", nullptr), 0);

    // Prepare query
    training_query_t query;
    memset(&query, 0, sizeof(query));
    query.query_type = TRAINING_QUERY_STATUS;

    training_query_result_t result_data;
    memset(&result_data, 0, sizeof(result_data));

    // Query unregistered target should fail
    int result = training_hub_query_module(hub, requester_id, unregistered_target_id, &query, &result_data);
    EXPECT_EQ(result, -1) << "Query to unregistered target should fail";
}

/**
 * Test: DestroyNullHub
 * Verify destroying NULL hub is safe
 */
TEST_F(TrainingIntegrationHubTest, DestroyNullHub) {
    // This should not crash
    training_hub_destroy(nullptr);
    SUCCEED() << "Destroying NULL hub should be safe";
}

/**
 * Test: GetModulesByCategory
 * Verify modules can be retrieved by category
 */
TEST_F(TrainingIntegrationHubTest, GetModulesByCategory) {
    ASSERT_NE(hub, nullptr);

    // Register modules in different categories
    ASSERT_EQ(training_hub_register_module(hub, TRAINING_MODULE_CURRICULUM_LEARNING, TRAINING_CATEGORY_CURRICULUM, "Curriculum", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, TRAINING_MODULE_META_LEARNING, TRAINING_CATEGORY_OPTIMIZATION, "MetaLearn", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, TRAINING_MODULE_HYPERPARAMETER_OPT, TRAINING_CATEGORY_OPTIMIZATION, "HPO", nullptr), 0);
    ASSERT_EQ(training_hub_register_module(hub, TRAINING_MODULE_QUANTIZATION_AWARE, TRAINING_CATEGORY_COMPRESSION, "QAT", nullptr), 0);

    // Get modules in OPTIMIZATION category
    uint32_t module_ids[10];
    uint32_t count = 0;

    int result = training_hub_get_modules_by_category(hub, TRAINING_CATEGORY_OPTIMIZATION, module_ids, 10, &count);
    EXPECT_EQ(result, 0) << "Get modules by category should succeed";
    EXPECT_EQ(count, 2u) << "Should have 2 modules in OPTIMIZATION category";
}

/**
 * Test: AsyncQueueDepth
 * Verify async queue depth can be queried
 */
TEST_F(TrainingIntegrationHubTest, AsyncQueueDepth) {
    ASSERT_NE(hub, nullptr);

    // Currently async is not fully implemented, so depth should be 0
    uint32_t depth = training_hub_get_async_queue_depth(hub);
    EXPECT_EQ(depth, 0u) << "Async queue depth should be 0 (not implemented)";
}

/**
 * Test: AsyncQueueFlush
 * Verify async queue can be flushed
 */
TEST_F(TrainingIntegrationHubTest, AsyncQueueFlush) {
    ASSERT_NE(hub, nullptr);

    // Currently async is not fully implemented, so flush should succeed immediately
    int result = training_hub_flush_async_queue(hub, 1000);
    EXPECT_EQ(result, 0) << "Async queue flush should succeed";
}

/* ========================================================================
 * STRING CONVERSION TESTS
 * ======================================================================== */

/**
 * Test: CategoryToString
 * Verify category to string conversion
 */
TEST_F(TrainingIntegrationHubTest, CategoryToString) {
    EXPECT_STREQ(training_category_to_string(TRAINING_CATEGORY_CURRICULUM), "CURRICULUM");
    EXPECT_STREQ(training_category_to_string(TRAINING_CATEGORY_OPTIMIZATION), "OPTIMIZATION");
    EXPECT_STREQ(training_category_to_string(TRAINING_CATEGORY_ARCHITECTURE), "ARCHITECTURE");
    EXPECT_STREQ(training_category_to_string(TRAINING_CATEGORY_COMPRESSION), "COMPRESSION");
    EXPECT_STREQ(training_category_to_string(TRAINING_CATEGORY_DISTRIBUTED), "DISTRIBUTED");
    EXPECT_STREQ(training_category_to_string(TRAINING_CATEGORY_ROBUSTNESS), "ROBUSTNESS");
    EXPECT_STREQ(training_category_to_string(TRAINING_CATEGORY_CONTINUAL), "CONTINUAL");
    EXPECT_STREQ(training_category_to_string(TRAINING_CATEGORY_DATA), "DATA");
    EXPECT_STREQ(training_category_to_string(TRAINING_CATEGORY_COUNT), "UNKNOWN");
}

/**
 * Test: EventTypeToString
 * Verify event type to string conversion
 */
TEST_F(TrainingIntegrationHubTest, EventTypeToString) {
    EXPECT_STREQ(training_event_type_to_string(TRAINING_EVENT_DIFFICULTY_UPDATED), "DIFFICULTY_UPDATED");
    EXPECT_STREQ(training_event_type_to_string(TRAINING_EVENT_EPOCH_COMPLETE), "EPOCH_COMPLETE");
    EXPECT_STREQ(training_event_type_to_string(TRAINING_EVENT_LOSS_COMPUTED), "LOSS_COMPUTED");
    EXPECT_STREQ(training_event_type_to_string(TRAINING_EVENT_LR_ADJUSTED), "LR_ADJUSTED");
    EXPECT_STREQ(training_event_type_to_string(TRAINING_EVENT_CHECKPOINT_SAVED), "CHECKPOINT_SAVED");
    EXPECT_STREQ(training_event_type_to_string(TRAINING_EVENT_COUNT), "UNKNOWN");
}

/**
 * Test: QueryTypeToString
 * Verify query type to string conversion
 */
TEST_F(TrainingIntegrationHubTest, QueryTypeToString) {
    EXPECT_STREQ(training_query_type_to_string(TRAINING_QUERY_STATUS), "STATUS");
    EXPECT_STREQ(training_query_type_to_string(TRAINING_QUERY_DIFFICULTY_SCORE), "DIFFICULTY_SCORE");
    EXPECT_STREQ(training_query_type_to_string(TRAINING_QUERY_LEARNING_RATE), "LEARNING_RATE");
    EXPECT_STREQ(training_query_type_to_string(TRAINING_QUERY_COUNT), "UNKNOWN");
}
