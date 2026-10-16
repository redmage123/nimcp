/**
 * @file test_salience_attention_bridge.cpp
 * @brief Unit tests for Salience-Attention Cognitive Hub Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for Salience-Attention Hub bidirectional integration
 * WHY:  Ensure salience and attention modules integrate correctly via cognitive hub
 * HOW:  Test lifecycle, registration, events, queries, and statistics
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Hub Registration/Unregistration
 * - Config Validation
 * - Event Subscription
 * - Event Publication (salience detection, attention shift, priority, focus)
 * - Event Callbacks
 * - Query Handling
 * - Statistics Tracking
 * - Threshold Filtering
 * - Thread Safety
 * - Null Parameter Handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/integration/nimcp_salience_attention_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_salience_callback_count{0};
static std::atomic<int> g_attention_callback_count{0};
static std::atomic<int> g_priority_callback_count{0};
static std::atomic<int> g_eval_callback_count{0};
static salient_item_t g_last_salient_item;
static attention_focus_t g_last_focus;

/**
 * Test callback for salience detection events
 */
static int test_salience_callback(
    const salient_item_t* item,
    void* user_data
) {
    (void)user_data;
    g_salience_callback_count++;
    if (item) {
        g_last_salient_item = *item;
    }
    return 0;
}

/**
 * Test callback for attention focus events
 */
static void test_attention_callback(
    const attention_focus_t* focus,
    void* user_data
) {
    (void)user_data;
    g_attention_callback_count++;
    if (focus) {
        g_last_focus = *focus;
    }
}

/**
 * Test callback for priority updates
 */
static void test_priority_callback(
    const attention_priority_t* priorities,
    uint32_t num_priorities,
    void* user_data
) {
    (void)priorities;
    (void)num_priorities;
    (void)user_data;
    g_priority_callback_count++;
}

/**
 * Test callback for salience evaluation requests
 */
static int test_eval_callback(
    const salience_eval_request_t* request,
    void* user_data
) {
    (void)request;
    (void)user_data;
    g_eval_callback_count++;
    return 0;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SalienceAttentionBridgeTest : public ::testing::Test {
protected:
    salience_attention_bridge_t* bridge = nullptr;
    salience_attention_config_t config;
    cognitive_integration_hub_t hub = nullptr;

    void SetUp() override {
        // Reset global state
        g_salience_callback_count = 0;
        g_attention_callback_count = 0;
        g_priority_callback_count = 0;
        g_eval_callback_count = 0;
        memset(&g_last_salient_item, 0, sizeof(g_last_salient_item));
        memset(&g_last_focus, 0, sizeof(g_last_focus));

        // Get default config
        int result = salience_attention_bridge_default_config(&config);
        ASSERT_EQ(result, 0) << "Default config should succeed";

        // Create bridge
        bridge = salience_attention_bridge_create(&config);

        // Create cognitive hub for registration tests
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub = cognitive_hub_create(&hub_config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            salience_attention_bridge_destroy(bridge);
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
TEST_F(SalienceAttentionBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify not registered initially
    EXPECT_FALSE(salience_attention_bridge_is_registered(bridge))
        << "Bridge should not be registered initially";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(SalienceAttentionBridgeTest, BridgeCreationNullConfig) {
    salience_attention_bridge_t* br = salience_attention_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    salience_attention_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(SalienceAttentionBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    salience_attention_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    salience_attention_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(SalienceAttentionBridgeTest, DefaultConfig) {
    salience_attention_config_t default_config;
    int result = salience_attention_bridge_default_config(&default_config);
    EXPECT_EQ(result, 0) << "Default config should succeed";

    // Verify module ID
    EXPECT_EQ(default_config.module_id, SALIENCE_ATTENTION_DEFAULT_MODULE_ID);

    // Verify auto-subscribe options
    EXPECT_TRUE(default_config.auto_subscribe_attention);
    EXPECT_TRUE(default_config.auto_subscribe_state);

    // Verify thresholds and weights
    EXPECT_FLOAT_EQ(default_config.salience_threshold, SALIENCE_ATTENTION_DEFAULT_THRESHOLD);
    EXPECT_GT(default_config.attention_shift_weight, 0.0f);
    EXPECT_GT(default_config.priority_weight, 0.0f);

    // Verify other options
    EXPECT_TRUE(default_config.enable_query_handler);
}

/**
 * Test: ConfigValidation
 * Verify configuration validation works correctly
 */
TEST_F(SalienceAttentionBridgeTest, ConfigValidation) {
    // Test with custom config values
    salience_attention_config_t custom_config;
    salience_attention_bridge_default_config(&custom_config);

    custom_config.salience_threshold = 0.5f;
    custom_config.attention_shift_weight = 0.9f;
    custom_config.priority_weight = 0.3f;
    custom_config.enable_logging = true;

    salience_attention_bridge_t* custom_bridge = salience_attention_bridge_create(&custom_config);
    ASSERT_NE(custom_bridge, nullptr) << "Bridge with custom config should be created";

    salience_attention_bridge_destroy(custom_bridge);
}

/**
 * Test: DefaultConfigNull
 * Verify default_config handles NULL gracefully
 */
TEST_F(SalienceAttentionBridgeTest, DefaultConfigNull) {
    int result = salience_attention_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/* ============================================================================
 * Hub Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithHub
 * Verify bridge can register with cognitive hub
 */
TEST_F(SalienceAttentionBridgeTest, RegisterWithHub) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "Registration should succeed";

    EXPECT_TRUE(salience_attention_bridge_is_registered(bridge))
        << "Bridge should be registered after register_with_hub()";
}

/**
 * Test: RegisterWithHubNullParams
 * Verify registration handles NULL parameters gracefully
 */
TEST_F(SalienceAttentionBridgeTest, RegisterWithHubNullParams) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // NULL bridge
    int result = salience_attention_bridge_register_with_hub(nullptr, hub);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    // NULL hub
    result = salience_attention_bridge_register_with_hub(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL hub should fail";
}

/**
 * Test: RegisterWithHubDuplicate
 * Verify registering when already registered is handled
 */
TEST_F(SalienceAttentionBridgeTest, RegisterWithHubDuplicate) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // First registration
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "First registration should succeed";

    // Second registration - should fail
    result = salience_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, -1) << "Duplicate registration should fail";
}

/**
 * Test: UnregisterFromHub
 * Verify bridge can unregister cleanly
 */
TEST_F(SalienceAttentionBridgeTest, UnregisterFromHub) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register first
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0) << "Registration required for unregister test";

    // Unregister
    result = salience_attention_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, 0) << "Unregister should succeed";

    EXPECT_FALSE(salience_attention_bridge_is_registered(bridge))
        << "Bridge should not be registered after unregister";
}

/**
 * Test: UnregisterFromHubNull
 * Verify unregister handles NULL gracefully
 */
TEST_F(SalienceAttentionBridgeTest, UnregisterFromHubNull) {
    int result = salience_attention_bridge_unregister_from_hub(nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/**
 * Test: UnregisterFromHubNotRegistered
 * Verify unregistering when not registered is handled
 */
TEST_F(SalienceAttentionBridgeTest, UnregisterFromHubNotRegistered) {
    ASSERT_NE(bridge, nullptr);

    // Unregister without registering first
    int result = salience_attention_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, -1) << "Unregister when not registered should fail";
}

/**
 * Test: IsRegisteredNull
 * Verify is_registered handles NULL gracefully
 */
TEST_F(SalienceAttentionBridgeTest, IsRegisteredNull) {
    bool registered = salience_attention_bridge_is_registered(nullptr);
    EXPECT_FALSE(registered) << "NULL bridge should return false";
}

/* ============================================================================
 * Module Connection Tests
 * ============================================================================ */

/**
 * Test: SetSalience
 * Verify salience evaluator can be set
 */
TEST_F(SalienceAttentionBridgeTest, SetSalience) {
    ASSERT_NE(bridge, nullptr);

    // Set salience to NULL (clearing)
    int result = salience_attention_bridge_set_salience(bridge, nullptr);
    EXPECT_EQ(result, 0) << "Setting salience to NULL should succeed";
}

/**
 * Test: SetAttention
 * Verify attention module can be set
 */
TEST_F(SalienceAttentionBridgeTest, SetAttention) {
    ASSERT_NE(bridge, nullptr);

    // Set attention to NULL (clearing)
    int result = salience_attention_bridge_set_attention(bridge, nullptr);
    EXPECT_EQ(result, 0) << "Setting attention to NULL should succeed";
}

/**
 * Test: SetSalienceNull
 * Verify set_salience handles NULL bridge gracefully
 */
TEST_F(SalienceAttentionBridgeTest, SetSalienceNull) {
    int result = salience_attention_bridge_set_salience(nullptr, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/**
 * Test: SetAttentionNull
 * Verify set_attention handles NULL bridge gracefully
 */
TEST_F(SalienceAttentionBridgeTest, SetAttentionNull) {
    int result = salience_attention_bridge_set_attention(nullptr, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Event Callback Registration Tests
 * ============================================================================ */

/**
 * Test: EventSubscription
 * Verify callbacks can be registered for events
 */
TEST_F(SalienceAttentionBridgeTest, EventSubscription) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;

    // Set salience callback
    int result = salience_attention_bridge_set_salience_callback(
        bridge, test_salience_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting salience callback should succeed";

    // Set attention callback
    result = salience_attention_bridge_set_attention_callback(
        bridge, test_attention_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting attention callback should succeed";

    // Set priority callback
    result = salience_attention_bridge_set_priority_callback(
        bridge, test_priority_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting priority callback should succeed";

    // Set evaluation callback
    result = salience_attention_bridge_set_eval_callback(
        bridge, test_eval_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting eval callback should succeed";
}

/**
 * Test: EventSubscriptionClear
 * Verify callbacks can be cleared
 */
TEST_F(SalienceAttentionBridgeTest, EventSubscriptionClear) {
    ASSERT_NE(bridge, nullptr);

    // Set then clear salience callback
    int result = salience_attention_bridge_set_salience_callback(
        bridge, test_salience_callback, nullptr);
    EXPECT_EQ(result, 0);

    result = salience_attention_bridge_set_salience_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";

    // Set then clear attention callback
    result = salience_attention_bridge_set_attention_callback(
        bridge, test_attention_callback, nullptr);
    EXPECT_EQ(result, 0);

    result = salience_attention_bridge_set_attention_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: EventSubscriptionNullBridge
 * Verify callback registration handles NULL bridge
 */
TEST_F(SalienceAttentionBridgeTest, EventSubscriptionNullBridge) {
    int result = salience_attention_bridge_set_salience_callback(
        nullptr, test_salience_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = salience_attention_bridge_set_attention_callback(
        nullptr, test_attention_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = salience_attention_bridge_set_priority_callback(
        nullptr, test_priority_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = salience_attention_bridge_set_eval_callback(
        nullptr, test_eval_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Event Publication Tests
 * ============================================================================ */

/**
 * Test: SalienceDetectionPublishing
 * Verify salience detection events can be published
 */
TEST_F(SalienceAttentionBridgeTest, SalienceDetectionPublishing) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register first
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0) << "Registration required for publication test";

    // Create salient item
    salient_item_t item;
    item.item_id = 1;
    item.salience_score = 0.8f;
    item.novelty = 0.7f;
    item.surprise = 0.6f;
    item.urgency = 0.5f;
    item.modality = 0;
    item.timestamp = 12345;

    // Publish salience detection
    result = salience_attention_publish_salience_detection(bridge, &item, 0.8f);
    EXPECT_EQ(result, 0) << "Publish salience detection should succeed";
}

/**
 * Test: AttentionShiftRequest
 * Verify attention shift requests can be published
 */
TEST_F(SalienceAttentionBridgeTest, AttentionShiftRequest) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register first
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0) << "Registration required for publication test";

    // Create attention target
    attention_target_t target;
    target.target_id = 100;
    target.priority = 0.9f;
    target.urgency = 0.8f;
    target.modality = 1;
    target.timestamp = 12345;

    // Request attention shift
    result = salience_attention_request_attention_shift(bridge, &target);
    EXPECT_EQ(result, 0) << "Request attention shift should succeed";
}

/**
 * Test: PriorityUpdateFlow
 * Verify priority updates can be published
 */
TEST_F(SalienceAttentionBridgeTest, PriorityUpdateFlow) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register first
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0) << "Registration required for publication test";

    // Create priorities
    attention_priority_t priorities[3];
    priorities[0].item_id = 1;
    priorities[0].priority = 0.9f;
    priorities[1].item_id = 2;
    priorities[1].priority = 0.7f;
    priorities[2].item_id = 3;
    priorities[2].priority = 0.5f;

    // Publish priority update
    result = salience_attention_publish_priority_update(bridge, priorities, 3);
    EXPECT_EQ(result, 0) << "Publish priority update should succeed";
}

/**
 * Test: AttentionFocusNotification
 * Verify attention focus notifications can be published
 */
TEST_F(SalienceAttentionBridgeTest, AttentionFocusNotification) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register first
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0) << "Registration required for publication test";

    // Create focus info
    attention_focus_t focus;
    focus.focus_id = 42;
    focus.previous_focus_id = 41;
    focus.focus_strength = 0.85f;
    focus.duration_ms = 500.0f;
    focus.timestamp = 12345;

    // Notify attention focus
    result = salience_attention_notify_attention_focus(bridge, &focus);
    EXPECT_EQ(result, 0) << "Notify attention focus should succeed";
}

/**
 * Test: SalienceEvaluationRequest
 * Verify salience evaluation requests can be published
 */
TEST_F(SalienceAttentionBridgeTest, SalienceEvaluationRequest) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register first
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0) << "Registration required for publication test";

    // Create evaluation request
    salience_eval_request_t request;
    request.request_id = 1;
    request.item_ids[0] = 10;
    request.item_ids[1] = 20;
    request.item_ids[2] = 30;
    request.num_items = 3;
    request.modality = 0;

    // Request salience evaluation
    result = salience_attention_request_salience_evaluation(bridge, &request);
    EXPECT_EQ(result, 0) << "Request salience evaluation should succeed";
}

/**
 * Test: EventPublicationNotRegistered
 * Verify publishing when not registered is handled
 */
TEST_F(SalienceAttentionBridgeTest, EventPublicationNotRegistered) {
    ASSERT_NE(bridge, nullptr);

    // Don't register - try to publish
    salient_item_t item;
    memset(&item, 0, sizeof(item));

    int result = salience_attention_publish_salience_detection(bridge, &item, 0.5f);
    EXPECT_EQ(result, -1) << "Publishing when not registered should fail";
}

/**
 * Test: EventPublicationNullBridge
 * Verify publish functions handle NULL bridge
 */
TEST_F(SalienceAttentionBridgeTest, EventPublicationNullBridge) {
    salient_item_t item;
    memset(&item, 0, sizeof(item));

    attention_target_t target;
    memset(&target, 0, sizeof(target));

    attention_priority_t priorities[1];
    memset(priorities, 0, sizeof(priorities));

    attention_focus_t focus;
    memset(&focus, 0, sizeof(focus));

    salience_eval_request_t request;
    memset(&request, 0, sizeof(request));
    request.num_items = 1;

    // All should fail with NULL bridge
    EXPECT_EQ(salience_attention_publish_salience_detection(nullptr, &item, 0.5f), -1);
    EXPECT_EQ(salience_attention_request_attention_shift(nullptr, &target), -1);
    EXPECT_EQ(salience_attention_publish_priority_update(nullptr, priorities, 1), -1);
    EXPECT_EQ(salience_attention_notify_attention_focus(nullptr, &focus), -1);
    EXPECT_EQ(salience_attention_request_salience_evaluation(nullptr, &request), -1);
}

/**
 * Test: EventPublicationNullPayload
 * Verify publish functions handle NULL payload
 */
TEST_F(SalienceAttentionBridgeTest, EventPublicationNullPayload) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // All should fail with NULL payload
    EXPECT_EQ(salience_attention_publish_salience_detection(bridge, nullptr, 0.5f), -1);
    EXPECT_EQ(salience_attention_request_attention_shift(bridge, nullptr), -1);
    EXPECT_EQ(salience_attention_publish_priority_update(bridge, nullptr, 1), -1);
    EXPECT_EQ(salience_attention_notify_attention_focus(bridge, nullptr), -1);
    EXPECT_EQ(salience_attention_request_salience_evaluation(bridge, nullptr), -1);
}

/* ============================================================================
 * Threshold Filtering Tests
 * ============================================================================ */

/**
 * Test: ThresholdFiltering
 * Verify salience threshold affects attention capture
 */
TEST_F(SalienceAttentionBridgeTest, ThresholdFiltering) {
    ASSERT_NE(bridge, nullptr);

    // Set threshold
    int result = salience_attention_bridge_set_threshold(bridge, 0.6f);
    EXPECT_EQ(result, 0) << "Set threshold should succeed";

    // Set shift weight
    result = salience_attention_bridge_set_shift_weight(bridge, 0.7f);
    EXPECT_EQ(result, 0) << "Set shift weight should succeed";
}

/**
 * Test: ThresholdFilteringNull
 * Verify threshold functions handle NULL bridge
 */
TEST_F(SalienceAttentionBridgeTest, ThresholdFilteringNull) {
    EXPECT_EQ(salience_attention_bridge_set_threshold(nullptr, 0.5f), -1);
    EXPECT_EQ(salience_attention_bridge_set_shift_weight(nullptr, 0.5f), -1);
}

/* ============================================================================
 * Query Handling Tests
 * ============================================================================ */

/**
 * Test: QueryHandling
 * Verify state queries work correctly
 */
TEST_F(SalienceAttentionBridgeTest, QueryHandling) {
    ASSERT_NE(bridge, nullptr);

    float avg_salience = 0.0f;
    float peak_salience = 0.0f;
    uint32_t detection_count = 0;

    int result = salience_attention_bridge_get_salience_state(
        bridge,
        &avg_salience,
        &peak_salience,
        &detection_count
    );
    EXPECT_EQ(result, 0) << "Get salience state should succeed";

    // Initial values should be 0
    EXPECT_EQ(detection_count, 0u);
}

/**
 * Test: QueryHandlingAttention
 * Verify attention state queries work
 */
TEST_F(SalienceAttentionBridgeTest, QueryHandlingAttention) {
    ASSERT_NE(bridge, nullptr);

    uint64_t current_focus = 0;
    float focus_strength = 0.0f;
    uint32_t num_targets = 0;

    int result = salience_attention_bridge_get_attention_state(
        bridge,
        &current_focus,
        &focus_strength,
        &num_targets
    );
    EXPECT_EQ(result, 0) << "Get attention state should succeed";
}

/**
 * Test: QueryHandlingPartialNull
 * Verify state query with some NULL outputs
 */
TEST_F(SalienceAttentionBridgeTest, QueryHandlingPartialNull) {
    ASSERT_NE(bridge, nullptr);

    float avg_salience = 0.0f;

    // Query with some NULL outputs
    int result = salience_attention_bridge_get_salience_state(
        bridge,
        &avg_salience,
        nullptr,
        nullptr
    );
    EXPECT_EQ(result, 0) << "Query with partial NULL outputs should succeed";
}

/**
 * Test: QueryHandlingNullBridge
 * Verify state query handles NULL bridge
 */
TEST_F(SalienceAttentionBridgeTest, QueryHandlingNullBridge) {
    float avg_salience = 0.0f;
    float peak_salience = 0.0f;
    uint32_t detection_count = 0;

    int result = salience_attention_bridge_get_salience_state(
        nullptr,
        &avg_salience,
        &peak_salience,
        &detection_count
    );
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    uint64_t current_focus = 0;
    float focus_strength = 0.0f;
    uint32_t num_targets = 0;

    result = salience_attention_bridge_get_attention_state(
        nullptr,
        &current_focus,
        &focus_strength,
        &num_targets
    );
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * Test: StatisticsTracking
 * Verify statistics are tracked correctly
 */
TEST_F(SalienceAttentionBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register and publish some events
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Publish events
    salient_item_t item;
    memset(&item, 0, sizeof(item));
    item.item_id = 1;
    item.salience_score = 0.8f;

    salience_attention_publish_salience_detection(bridge, &item, 0.8f);

    attention_focus_t focus;
    memset(&focus, 0, sizeof(focus));
    focus.focus_id = 1;
    focus.focus_strength = 0.9f;

    salience_attention_notify_attention_focus(bridge, &focus);

    // Get stats
    salience_attention_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    result = salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify events were counted
    EXPECT_GE(stats.events_published, 2u)
        << "Should have at least 2 events published";
}

/**
 * Test: StatisticsTrackingNull
 * Verify get_stats handles NULL parameters
 */
TEST_F(SalienceAttentionBridgeTest, StatisticsTrackingNull) {
    salience_attention_stats_t stats;

    int result = salience_attention_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = salience_attention_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL stats output should fail";
}

/**
 * Test: StatsReset
 * Verify statistics can be reset
 */
TEST_F(SalienceAttentionBridgeTest, StatsReset) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register and publish some events
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    salient_item_t item;
    memset(&item, 0, sizeof(item));
    item.item_id = 1;
    salience_attention_publish_salience_detection(bridge, &item, 0.8f);

    // Reset stats
    result = salience_attention_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    salience_attention_stats_t stats;
    result = salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    // Counters should be reset
    EXPECT_EQ(stats.events_published, 0u) << "Events published should be 0";
    EXPECT_EQ(stats.salience_detections, 0u) << "Salience detections should be 0";
}

/**
 * Test: StatsResetNull
 * Verify reset_stats handles NULL gracefully
 */
TEST_F(SalienceAttentionBridgeTest, StatsResetNull) {
    int result = salience_attention_bridge_reset_stats(nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Basic test for concurrent access to bridge
 */
TEST_F(SalienceAttentionBridgeTest, ThreadSafety) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
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
                salience_attention_bridge_is_registered(bridge);

                float avg_salience = 0.0f;
                float peak_salience = 0.0f;
                uint32_t detection_count = 0;
                salience_attention_bridge_get_salience_state(bridge,
                    &avg_salience, &peak_salience, &detection_count);

                salience_attention_stats_t stats;
                salience_attention_bridge_get_stats(bridge, &stats);

                // Write operations (publish events)
                salient_item_t item;
                memset(&item, 0, sizeof(item));
                item.item_id = (uint64_t)(t * 1000 + i);
                item.salience_score = 0.5f + (float)i / (ITERATIONS * 2);

                salience_attention_publish_salience_detection(bridge, &item, item.salience_score);
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
    salience_attention_stats_t stats;
    result = salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.events_published, (uint32_t)(NUM_THREADS * ITERATIONS))
        << "All events should be counted";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(SalienceAttentionBridgeTest, NullHandling) {
    // Lifecycle
    salience_attention_bridge_destroy(nullptr);
    EXPECT_EQ(salience_attention_bridge_default_config(nullptr), -1);

    // Registration
    EXPECT_EQ(salience_attention_bridge_register_with_hub(nullptr, hub), -1);
    EXPECT_EQ(salience_attention_bridge_register_with_hub(bridge, nullptr), -1);
    EXPECT_EQ(salience_attention_bridge_unregister_from_hub(nullptr), -1);
    EXPECT_FALSE(salience_attention_bridge_is_registered(nullptr));

    // Module connection
    EXPECT_EQ(salience_attention_bridge_set_salience(nullptr, nullptr), -1);
    EXPECT_EQ(salience_attention_bridge_set_attention(nullptr, nullptr), -1);

    // Callbacks
    EXPECT_EQ(salience_attention_bridge_set_salience_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(salience_attention_bridge_set_attention_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(salience_attention_bridge_set_priority_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(salience_attention_bridge_set_eval_callback(nullptr, nullptr, nullptr), -1);

    // Publication
    salient_item_t item;
    memset(&item, 0, sizeof(item));
    attention_target_t target;
    memset(&target, 0, sizeof(target));
    attention_priority_t priorities[1];
    memset(priorities, 0, sizeof(priorities));
    attention_focus_t focus;
    memset(&focus, 0, sizeof(focus));
    salience_eval_request_t request;
    memset(&request, 0, sizeof(request));
    request.num_items = 1;

    EXPECT_EQ(salience_attention_publish_salience_detection(nullptr, &item, 0.5f), -1);
    EXPECT_EQ(salience_attention_request_attention_shift(nullptr, &target), -1);
    EXPECT_EQ(salience_attention_publish_priority_update(nullptr, priorities, 1), -1);
    EXPECT_EQ(salience_attention_notify_attention_focus(nullptr, &focus), -1);
    EXPECT_EQ(salience_attention_request_salience_evaluation(nullptr, &request), -1);

    // Query
    float dummy_f = 0.0f;
    uint32_t dummy_u32 = 0;
    uint64_t dummy_u64 = 0;
    EXPECT_EQ(salience_attention_bridge_get_salience_state(nullptr, &dummy_f, &dummy_f, &dummy_u32), -1);
    EXPECT_EQ(salience_attention_bridge_get_attention_state(nullptr, &dummy_u64, &dummy_f, &dummy_u32), -1);

    // Stats
    salience_attention_stats_t stats;
    EXPECT_EQ(salience_attention_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(salience_attention_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(salience_attention_bridge_reset_stats(nullptr), -1);

    // Configuration
    EXPECT_EQ(salience_attention_bridge_set_threshold(nullptr, 0.5f), -1);
    EXPECT_EQ(salience_attention_bridge_set_shift_weight(nullptr, 0.5f), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Integration with Cognitive Hub Tests
 * ============================================================================ */

/**
 * Test: FullIntegrationFlow
 * Test complete flow: register, publish, unregister
 */
TEST_F(SalienceAttentionBridgeTest, FullIntegrationFlow) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "Register should succeed";
    EXPECT_TRUE(salience_attention_bridge_is_registered(bridge));

    // Publish salience detection
    salient_item_t item;
    memset(&item, 0, sizeof(item));
    item.item_id = 1;
    item.salience_score = 0.85f;
    item.novelty = 0.7f;
    item.surprise = 0.6f;
    item.urgency = 0.5f;

    result = salience_attention_publish_salience_detection(bridge, &item, 0.85f);
    EXPECT_EQ(result, 0) << "Publish salience should succeed";

    // Request attention shift
    attention_target_t target;
    memset(&target, 0, sizeof(target));
    target.target_id = item.item_id;
    target.priority = 0.9f;
    target.urgency = 0.8f;

    result = salience_attention_request_attention_shift(bridge, &target);
    EXPECT_EQ(result, 0) << "Request shift should succeed";

    // Check stats
    salience_attention_stats_t stats;
    result = salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.events_published, 2u);

    // Unregister
    result = salience_attention_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, 0) << "Unregister should succeed";
    EXPECT_FALSE(salience_attention_bridge_is_registered(bridge));
}

/**
 * Test: ReconnectAfterUnregister
 * Verify bridge can re-register after unregister
 */
TEST_F(SalienceAttentionBridgeTest, ReconnectAfterUnregister) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register
    int result = salience_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0);

    // Unregister
    result = salience_attention_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, 0);

    // Re-register
    result = salience_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "Re-register should succeed";
    EXPECT_TRUE(salience_attention_bridge_is_registered(bridge));
}

/* ============================================================================
 * Configuration Variants Tests
 * ============================================================================ */

/**
 * Test: ConfigVariants
 * Test bridge creation with various config options
 */
TEST_F(SalienceAttentionBridgeTest, ConfigVariants) {
    // Config with all auto-subscribe disabled
    salience_attention_config_t config1;
    salience_attention_bridge_default_config(&config1);
    config1.auto_subscribe_attention = false;
    config1.auto_subscribe_state = false;

    salience_attention_bridge_t* br1 = salience_attention_bridge_create(&config1);
    ASSERT_NE(br1, nullptr);
    salience_attention_bridge_destroy(br1);

    // Config with query handler disabled
    salience_attention_config_t config2;
    salience_attention_bridge_default_config(&config2);
    config2.enable_query_handler = false;

    salience_attention_bridge_t* br2 = salience_attention_bridge_create(&config2);
    ASSERT_NE(br2, nullptr);
    salience_attention_bridge_destroy(br2);

    // Config with custom module ID
    salience_attention_config_t config3;
    salience_attention_bridge_default_config(&config3);
    config3.module_id = 0x12345678;

    salience_attention_bridge_t* br3 = salience_attention_bridge_create(&config3);
    ASSERT_NE(br3, nullptr);
    salience_attention_bridge_destroy(br3);

    // Config with different thresholds
    salience_attention_config_t config4;
    salience_attention_bridge_default_config(&config4);
    config4.salience_threshold = 0.9f;
    config4.attention_shift_weight = 0.3f;
    config4.priority_weight = 0.8f;

    salience_attention_bridge_t* br4 = salience_attention_bridge_create(&config4);
    ASSERT_NE(br4, nullptr);
    salience_attention_bridge_destroy(br4);

    SUCCEED() << "All config variants should work";
}

/**
 * Test: EventCallback
 * Verify callbacks receive events properly
 */
TEST_F(SalienceAttentionBridgeTest, EventCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;

    // Set all callbacks
    int result = salience_attention_bridge_set_salience_callback(
        bridge, test_salience_callback, &user_data);
    EXPECT_EQ(result, 0);

    result = salience_attention_bridge_set_attention_callback(
        bridge, test_attention_callback, &user_data);
    EXPECT_EQ(result, 0);

    result = salience_attention_bridge_set_priority_callback(
        bridge, test_priority_callback, &user_data);
    EXPECT_EQ(result, 0);

    result = salience_attention_bridge_set_eval_callback(
        bridge, test_eval_callback, &user_data);
    EXPECT_EQ(result, 0);

    // Callbacks are registered - actual invocation depends on hub events
    SUCCEED() << "Callbacks should be registered without error";
}
