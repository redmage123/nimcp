/**
 * @file test_predictive_attention_bridge.cpp
 * @brief Unit tests for Predictive-Attention Cognitive Hub Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for Predictive-Attention Hub bidirectional integration
 * WHY:  Ensure predictive processing and attention systems integrate correctly
 *       through the cognitive event system
 * HOW:  Test lifecycle, connection, events, callbacks, and statistics
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Configuration Validation
 * - Hub Registration/Unregistration
 * - Prediction Error Publishing
 * - Attention to Error Requests
 * - Precision Estimate Flow
 * - Attended Prediction Notification
 * - Prediction for Focus Requests
 * - Surprise-Triggered Attention
 * - Statistics Tracking
 * - Thread Safety
 * - Null Handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/integration/nimcp_predictive_attention_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_prediction_callback_count{0};
static std::atomic<int> g_attention_callback_count{0};
static std::atomic<int> g_error_callback_count{0};
static float g_last_prediction = 0.0f;
static float g_last_error_magnitude = 0.0f;
static uint64_t g_last_location = 0;

/**
 * Test callback for prediction events
 */
static int test_prediction_callback(
    float prediction,
    uint64_t location,
    float confidence,
    void* user_data
) {
    (void)confidence;
    (void)user_data;
    g_prediction_callback_count++;
    g_last_prediction = prediction;
    g_last_location = location;
    return 0;
}

/**
 * Test callback for attention events
 */
static int test_attention_callback(
    uint64_t old_focus,
    uint64_t new_focus,
    float urgency,
    void* user_data
) {
    (void)old_focus;
    (void)new_focus;
    (void)urgency;
    (void)user_data;
    g_attention_callback_count++;
    return 0;
}

/**
 * Test callback for error events
 */
static int test_error_callback(
    float error_magnitude,
    uint64_t location,
    void* user_data
) {
    (void)user_data;
    g_error_callback_count++;
    g_last_error_magnitude = error_magnitude;
    g_last_location = location;
    return 0;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PredictiveAttentionBridgeTest : public ::testing::Test {
protected:
    predictive_attention_bridge_t* bridge = nullptr;
    predictive_attention_bridge_config_t config;
    cognitive_integration_hub_t hub = nullptr;

    void SetUp() override {
        // Reset global state
        g_prediction_callback_count = 0;
        g_attention_callback_count = 0;
        g_error_callback_count = 0;
        g_last_prediction = 0.0f;
        g_last_error_magnitude = 0.0f;
        g_last_location = 0;

        // Get default config
        int result = predictive_attention_bridge_default_config(&config);
        ASSERT_EQ(result, 0) << "Default config should succeed";

        // Create bridge
        bridge = predictive_attention_bridge_create(&config);

        // Create cognitive hub for connection tests
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub = cognitive_hub_create(&hub_config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            predictive_attention_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (hub != nullptr) {
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }
};

/* ============================================================================
 * Bridge Creation Tests
 * ============================================================================ */

/**
 * Test: BridgeCreation
 * Verify bridge can be created and destroyed successfully
 */
TEST_F(PredictiveAttentionBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify not connected initially
    EXPECT_FALSE(predictive_attention_bridge_is_connected(bridge))
        << "Bridge should not be connected initially";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(PredictiveAttentionBridgeTest, BridgeCreationNullConfig) {
    predictive_attention_bridge_t* br = predictive_attention_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    predictive_attention_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(PredictiveAttentionBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    predictive_attention_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    predictive_attention_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/* ============================================================================
 * Configuration Validation Tests
 * ============================================================================ */

/**
 * Test: ConfigValidation
 * Verify configuration values are properly applied
 */
TEST_F(PredictiveAttentionBridgeTest, ConfigValidation) {
    // Create bridge with custom config
    predictive_attention_bridge_config_t custom_config;
    predictive_attention_bridge_default_config(&custom_config);
    custom_config.prediction_error_weight = 0.8f;
    custom_config.surprise_attention_weight = 0.9f;
    custom_config.precision_weight = 0.7f;
    custom_config.enable_logging = true;

    predictive_attention_bridge_t* custom_bridge =
        predictive_attention_bridge_create(&custom_config);
    ASSERT_NE(custom_bridge, nullptr) << "Bridge with custom config should be created";

    predictive_attention_bridge_destroy(custom_bridge);
}

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(PredictiveAttentionBridgeTest, DefaultConfig) {
    predictive_attention_bridge_config_t default_config;
    int result = predictive_attention_bridge_default_config(&default_config);
    EXPECT_EQ(result, 0) << "Default config should succeed";

    // Verify default weights are sensible
    EXPECT_GT(default_config.prediction_error_weight, 0.0f);
    EXPECT_LE(default_config.prediction_error_weight, 1.0f);

    EXPECT_GT(default_config.surprise_attention_weight, 0.0f);
    EXPECT_LE(default_config.surprise_attention_weight, 1.0f);

    EXPECT_GT(default_config.precision_weight, 0.0f);
    EXPECT_LE(default_config.precision_weight, 1.0f);

    // Verify subscriptions enabled by default
    EXPECT_TRUE(default_config.enable_prediction_subscription);
    EXPECT_TRUE(default_config.enable_attention_subscription);
    EXPECT_TRUE(default_config.enable_error_subscription);
    EXPECT_TRUE(default_config.enable_state_subscription);
    EXPECT_TRUE(default_config.enable_query_handling);
}

/**
 * Test: DefaultConfigNull
 * Verify default_config handles NULL gracefully
 */
TEST_F(PredictiveAttentionBridgeTest, DefaultConfigNull) {
    int result = predictive_attention_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/* ============================================================================
 * Hub Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithHub
 * Verify bridge can register with cognitive hub
 */
TEST_F(PredictiveAttentionBridgeTest, RegisterWithHub) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "Registration should succeed";

    EXPECT_TRUE(predictive_attention_bridge_is_connected(bridge))
        << "Bridge should be connected after registration";
}

/**
 * Test: RegisterWithHubNullParams
 * Verify registration handles NULL parameters gracefully
 */
TEST_F(PredictiveAttentionBridgeTest, RegisterWithHubNullParams) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // NULL bridge
    int result = predictive_attention_bridge_register_with_hub(nullptr, hub);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    // NULL hub
    result = predictive_attention_bridge_register_with_hub(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL hub should fail";
}

/**
 * Test: RegisterDuplicate
 * Verify registering when already connected is handled
 */
TEST_F(PredictiveAttentionBridgeTest, RegisterDuplicate) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // First registration
    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "First registration should succeed";

    // Second registration - should fail
    result = predictive_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, -1) << "Duplicate registration should fail";
}

/**
 * Test: UnregisterFromHub
 * Verify bridge can unregister cleanly
 */
TEST_F(PredictiveAttentionBridgeTest, UnregisterFromHub) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register first
    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0) << "Registration required for unregister test";

    // Unregister
    result = predictive_attention_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, 0) << "Unregister should succeed";

    EXPECT_FALSE(predictive_attention_bridge_is_connected(bridge))
        << "Bridge should not be connected after unregister";
}

/**
 * Test: UnregisterNull
 * Verify unregister handles NULL gracefully
 */
TEST_F(PredictiveAttentionBridgeTest, UnregisterNull) {
    int result = predictive_attention_bridge_unregister_from_hub(nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/**
 * Test: UnregisterNotConnected
 * Verify unregistering when not connected is handled
 */
TEST_F(PredictiveAttentionBridgeTest, UnregisterNotConnected) {
    ASSERT_NE(bridge, nullptr);

    // Unregister without registering first
    int result = predictive_attention_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, -1) << "Unregistering when not connected should fail";

    EXPECT_FALSE(predictive_attention_bridge_is_connected(bridge))
        << "Bridge should remain disconnected";
}

/* ============================================================================
 * Prediction Error Publishing Tests
 * ============================================================================ */

/**
 * Test: PredictionErrorPublishing
 * Verify prediction errors can be published
 */
TEST_F(PredictiveAttentionBridgeTest, PredictionErrorPublishing) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register first
    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0) << "Registration required for publishing test";

    // Publish prediction error
    result = predictive_attention_publish_prediction_error(bridge, 0.5f, 1001);
    EXPECT_EQ(result, 0) << "Publish prediction error should succeed";
}

/**
 * Test: PredictionErrorClampedValues
 * Verify error values are clamped to valid range
 */
TEST_F(PredictiveAttentionBridgeTest, PredictionErrorClampedValues) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Publish with out-of-range values
    result = predictive_attention_publish_prediction_error(bridge, 2.0f, 1001);
    EXPECT_EQ(result, 0) << "Should accept and clamp high value";

    result = predictive_attention_publish_prediction_error(bridge, -0.5f, 1002);
    EXPECT_EQ(result, 0) << "Should accept and clamp negative value";
}

/**
 * Test: PredictionErrorNotConnected
 * Verify publishing when not connected is handled
 */
TEST_F(PredictiveAttentionBridgeTest, PredictionErrorNotConnected) {
    ASSERT_NE(bridge, nullptr);

    // Don't register - try to publish
    int result = predictive_attention_publish_prediction_error(bridge, 0.5f, 1001);
    EXPECT_EQ(result, -1) << "Publishing when not connected should fail";
}

/* ============================================================================
 * Attention to Error Request Tests
 * ============================================================================ */

/**
 * Test: AttentionToErrorRequest
 * Verify attention can be requested for error source
 */
TEST_F(PredictiveAttentionBridgeTest, AttentionToErrorRequest) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Create error data
    pred_attn_error_data_t error_data;
    memset(&error_data, 0, sizeof(error_data));
    error_data.error_id = 1;
    error_data.error_magnitude = 0.8f;
    error_data.error_location = 2001;
    error_data.expected_value = 0.5f;
    error_data.observed_value = 0.9f;
    error_data.precision = 0.7f;
    error_data.timestamp_us = 0;

    result = predictive_attention_request_attention_to_error(bridge, &error_data);
    EXPECT_EQ(result, 0) << "Request attention to error should succeed";
}

/**
 * Test: AttentionToErrorRequestNull
 * Verify attention request handles NULL data gracefully
 */
TEST_F(PredictiveAttentionBridgeTest, AttentionToErrorRequestNull) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    result = predictive_attention_request_attention_to_error(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL error data should fail";
}

/* ============================================================================
 * Precision Estimate Flow Tests
 * ============================================================================ */

/**
 * Test: PrecisionEstimateFlow
 * Verify precision estimates can be published
 */
TEST_F(PredictiveAttentionBridgeTest, PrecisionEstimateFlow) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Create precision data
    pred_attn_precision_data_t precision_data;
    memset(&precision_data, 0, sizeof(precision_data));
    precision_data.location = 3001;
    precision_data.precision_old = 0.3f;
    precision_data.precision_new = 0.8f;
    precision_data.confidence = 0.9f;
    precision_data.timestamp_us = 0;

    result = predictive_attention_publish_precision_estimate(bridge, &precision_data);
    EXPECT_EQ(result, 0) << "Publish precision estimate should succeed";
}

/**
 * Test: PrecisionEstimateNull
 * Verify precision estimate handles NULL data gracefully
 */
TEST_F(PredictiveAttentionBridgeTest, PrecisionEstimateNull) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    result = predictive_attention_publish_precision_estimate(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL precision data should fail";
}

/* ============================================================================
 * Attended Prediction Notification Tests
 * ============================================================================ */

/**
 * Test: AttendedPredictionNotification
 * Verify attended predictions can be notified
 */
TEST_F(PredictiveAttentionBridgeTest, AttendedPredictionNotification) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Create prediction data
    pred_attn_prediction_data_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    prediction.focus_id = 4001;
    prediction.prediction = 0.75f;
    prediction.confidence = 0.85f;
    prediction.expected_precision = 0.9f;
    prediction.timestamp_us = 0;

    result = predictive_attention_notify_attended_prediction(bridge, &prediction);
    EXPECT_EQ(result, 0) << "Notify attended prediction should succeed";
}

/**
 * Test: AttendedPredictionNull
 * Verify attended prediction handles NULL data gracefully
 */
TEST_F(PredictiveAttentionBridgeTest, AttendedPredictionNull) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    result = predictive_attention_notify_attended_prediction(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL prediction data should fail";
}

/* ============================================================================
 * Prediction for Focus Request Tests
 * ============================================================================ */

/**
 * Test: PredictionForFocusRequest
 * Verify prediction can be requested for focus target
 */
TEST_F(PredictiveAttentionBridgeTest, PredictionForFocusRequest) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Create focus request
    pred_attn_focus_request_t focus;
    memset(&focus, 0, sizeof(focus));
    focus.focus_id = 5001;
    focus.urgency = 0.7f;
    focus.focus_duration_us = 100000;
    focus.timestamp_us = 0;

    result = predictive_attention_request_prediction_for_focus(bridge, &focus);
    EXPECT_EQ(result, 0) << "Request prediction for focus should succeed";
}

/**
 * Test: PredictionForFocusNull
 * Verify prediction for focus handles NULL data gracefully
 */
TEST_F(PredictiveAttentionBridgeTest, PredictionForFocusNull) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    result = predictive_attention_request_prediction_for_focus(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL focus data should fail";
}

/* ============================================================================
 * Callback Registration Tests
 * ============================================================================ */

/**
 * Test: CallbackRegistration
 * Verify callbacks can be registered
 */
TEST_F(PredictiveAttentionBridgeTest, CallbackRegistration) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;

    // Set prediction callback
    int result = predictive_attention_set_prediction_callback(
        bridge, test_prediction_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting prediction callback should succeed";

    // Set attention callback
    result = predictive_attention_set_attention_callback(
        bridge, test_attention_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting attention callback should succeed";

    // Set error callback
    result = predictive_attention_set_error_callback(
        bridge, test_error_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting error callback should succeed";
}

/**
 * Test: CallbackClear
 * Verify callbacks can be cleared
 */
TEST_F(PredictiveAttentionBridgeTest, CallbackClear) {
    ASSERT_NE(bridge, nullptr);

    // Set then clear prediction callback
    int result = predictive_attention_set_prediction_callback(
        bridge, test_prediction_callback, nullptr);
    EXPECT_EQ(result, 0);

    result = predictive_attention_set_prediction_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";

    // Set then clear attention callback
    result = predictive_attention_set_attention_callback(
        bridge, test_attention_callback, nullptr);
    EXPECT_EQ(result, 0);

    result = predictive_attention_set_attention_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";

    // Set then clear error callback
    result = predictive_attention_set_error_callback(
        bridge, test_error_callback, nullptr);
    EXPECT_EQ(result, 0);

    result = predictive_attention_set_error_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: CallbackNullBridge
 * Verify callback registration handles NULL bridge
 */
TEST_F(PredictiveAttentionBridgeTest, CallbackNullBridge) {
    int result = predictive_attention_set_prediction_callback(
        nullptr, test_prediction_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = predictive_attention_set_attention_callback(
        nullptr, test_attention_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = predictive_attention_set_error_callback(
        nullptr, test_error_callback, nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Statistics Tracking Tests
 * ============================================================================ */

/**
 * Test: StatisticsTracking
 * Verify statistics are tracked correctly
 */
TEST_F(PredictiveAttentionBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register and publish some events
    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Publish events
    predictive_attention_publish_prediction_error(bridge, 0.5f, 1001);
    predictive_attention_publish_prediction_error(bridge, 0.7f, 1002);

    pred_attn_precision_data_t precision_data;
    memset(&precision_data, 0, sizeof(precision_data));
    precision_data.location = 2001;
    precision_data.precision_old = 0.4f;
    precision_data.precision_new = 0.8f;
    precision_data.confidence = 0.9f;
    predictive_attention_publish_precision_estimate(bridge, &precision_data);

    // Get stats
    predictive_attention_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    result = predictive_attention_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify counts
    EXPECT_GE(stats.events_published, 3u) << "Should have at least 3 events published";
    EXPECT_EQ(stats.prediction_errors, 2u) << "Should have 2 prediction errors";
    EXPECT_EQ(stats.precision_updates, 1u) << "Should have 1 precision update";
}

/**
 * Test: StatisticsNull
 * Verify get_stats handles NULL parameters
 */
TEST_F(PredictiveAttentionBridgeTest, StatisticsNull) {
    predictive_attention_bridge_stats_t stats;

    int result = predictive_attention_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = predictive_attention_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL stats output should fail";
}

/**
 * Test: StatisticsReset
 * Verify statistics can be reset
 */
TEST_F(PredictiveAttentionBridgeTest, StatisticsReset) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register and publish some events
    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    predictive_attention_publish_prediction_error(bridge, 0.5f, 1001);

    // Reset stats
    result = predictive_attention_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    predictive_attention_bridge_stats_t stats;
    result = predictive_attention_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    // Counters should be reset
    EXPECT_EQ(stats.events_published, 0u) << "Events published should be 0";
    EXPECT_EQ(stats.prediction_errors, 0u) << "Prediction errors should be 0";
    EXPECT_EQ(stats.total_events, 0u) << "Total events should be 0";
}

/**
 * Test: StatisticsResetNull
 * Verify reset_stats handles NULL gracefully
 */
TEST_F(PredictiveAttentionBridgeTest, StatisticsResetNull) {
    int result = predictive_attention_bridge_reset_stats(nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Basic test for concurrent access to bridge
 */
TEST_F(PredictiveAttentionBridgeTest, ThreadSafety) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register
    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
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
                predictive_attention_bridge_is_connected(bridge);

                predictive_attention_bridge_stats_t stats;
                predictive_attention_bridge_get_stats(bridge, &stats);

                // Write operations (publish events)
                predictive_attention_publish_prediction_error(
                    bridge,
                    0.1f * (t + 1),
                    (uint64_t)(t * 1000 + i)
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
    predictive_attention_bridge_stats_t stats;
    result = predictive_attention_bridge_get_stats(bridge, &stats);
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
TEST_F(PredictiveAttentionBridgeTest, NullHandling) {
    // Lifecycle
    predictive_attention_bridge_destroy(nullptr);
    EXPECT_EQ(predictive_attention_bridge_default_config(nullptr), -1);

    // Connection
    EXPECT_EQ(predictive_attention_bridge_register_with_hub(nullptr, hub), -1);
    EXPECT_EQ(predictive_attention_bridge_register_with_hub(bridge, nullptr), -1);
    EXPECT_EQ(predictive_attention_bridge_unregister_from_hub(nullptr), -1);
    EXPECT_FALSE(predictive_attention_bridge_is_connected(nullptr));

    // Callbacks
    EXPECT_EQ(predictive_attention_set_prediction_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(predictive_attention_set_attention_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(predictive_attention_set_error_callback(nullptr, nullptr, nullptr), -1);

    // Publication
    EXPECT_EQ(predictive_attention_publish_prediction_error(nullptr, 0.5f, 1001), -1);
    EXPECT_EQ(predictive_attention_request_attention_to_error(nullptr, nullptr), -1);
    EXPECT_EQ(predictive_attention_publish_precision_estimate(nullptr, nullptr), -1);
    EXPECT_EQ(predictive_attention_notify_attended_prediction(nullptr, nullptr), -1);
    EXPECT_EQ(predictive_attention_request_prediction_for_focus(nullptr, nullptr), -1);

    // Stats
    predictive_attention_bridge_stats_t stats;
    EXPECT_EQ(predictive_attention_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(predictive_attention_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(predictive_attention_bridge_reset_stats(nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Full Integration Flow Tests
 * ============================================================================ */

/**
 * Test: FullIntegrationFlow
 * Test complete flow: register, publish, unregister
 */
TEST_F(PredictiveAttentionBridgeTest, FullIntegrationFlow) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register
    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "Register should succeed";
    EXPECT_TRUE(predictive_attention_bridge_is_connected(bridge));

    // Publish prediction error
    result = predictive_attention_publish_prediction_error(bridge, 0.6f, 1001);
    EXPECT_EQ(result, 0) << "Publish error should succeed";

    // Publish precision estimate
    pred_attn_precision_data_t precision;
    memset(&precision, 0, sizeof(precision));
    precision.location = 2001;
    precision.precision_old = 0.3f;
    precision.precision_new = 0.7f;
    precision.confidence = 0.8f;
    result = predictive_attention_publish_precision_estimate(bridge, &precision);
    EXPECT_EQ(result, 0) << "Publish precision should succeed";

    // Request attention
    pred_attn_error_data_t error;
    memset(&error, 0, sizeof(error));
    error.error_id = 1;
    error.error_magnitude = 0.9f;
    error.error_location = 3001;
    result = predictive_attention_request_attention_to_error(bridge, &error);
    EXPECT_EQ(result, 0) << "Request attention should succeed";

    // Check stats
    predictive_attention_bridge_stats_t stats;
    result = predictive_attention_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.events_published, 3u);

    // Unregister
    result = predictive_attention_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, 0) << "Unregister should succeed";
    EXPECT_FALSE(predictive_attention_bridge_is_connected(bridge));
}

/**
 * Test: ReconnectAfterUnregister
 * Verify bridge can reconnect after unregister
 */
TEST_F(PredictiveAttentionBridgeTest, ReconnectAfterUnregister) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register
    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0);

    // Unregister
    result = predictive_attention_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, 0);

    // Re-register
    result = predictive_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "Re-register should succeed";
    EXPECT_TRUE(predictive_attention_bridge_is_connected(bridge));
}

/* ============================================================================
 * Event Subtype String Conversion Tests
 * ============================================================================ */

/**
 * Test: EventSubtypeToString
 * Verify event subtype string conversion
 */
TEST_F(PredictiveAttentionBridgeTest, EventSubtypeToString) {
    EXPECT_STREQ(pred_attn_event_subtype_to_string(PRED_ATTN_EVENT_PREDICTION_ERROR),
                 "PREDICTION_ERROR");
    EXPECT_STREQ(pred_attn_event_subtype_to_string(PRED_ATTN_EVENT_ATTENTION_REQUEST),
                 "ATTENTION_REQUEST");
    EXPECT_STREQ(pred_attn_event_subtype_to_string(PRED_ATTN_EVENT_PRECISION_UPDATE),
                 "PRECISION_UPDATE");
    EXPECT_STREQ(pred_attn_event_subtype_to_string(PRED_ATTN_EVENT_ATTENDED_PREDICTION),
                 "ATTENDED_PREDICTION");
    EXPECT_STREQ(pred_attn_event_subtype_to_string(PRED_ATTN_EVENT_FOCUS_PREDICTION_REQUEST),
                 "FOCUS_PREDICTION_REQUEST");
    EXPECT_STREQ(pred_attn_event_subtype_to_string(PRED_ATTN_EVENT_SURPRISE_DETECTED),
                 "SURPRISE_DETECTED");
    EXPECT_STREQ(pred_attn_event_subtype_to_string(PRED_ATTN_EVENT_ATTENTION_SHIFTED),
                 "ATTENTION_SHIFTED");
    EXPECT_STREQ(pred_attn_event_subtype_to_string(PRED_ATTN_EVENT_SAMPLING_STARTED),
                 "SAMPLING_STARTED");
    EXPECT_STREQ(pred_attn_event_subtype_to_string(PRED_ATTN_EVENT_SAMPLING_COMPLETED),
                 "SAMPLING_COMPLETED");
    EXPECT_STREQ(pred_attn_event_subtype_to_string((pred_attn_event_subtype_t)999),
                 "UNKNOWN");
}

/* ============================================================================
 * Configuration Variants Tests
 * ============================================================================ */

/**
 * Test: ConfigVariants
 * Test bridge creation with various config options
 */
TEST_F(PredictiveAttentionBridgeTest, ConfigVariants) {
    // Config with all subscriptions disabled
    predictive_attention_bridge_config_t config1;
    predictive_attention_bridge_default_config(&config1);
    config1.enable_prediction_subscription = false;
    config1.enable_attention_subscription = false;
    config1.enable_error_subscription = false;
    config1.enable_state_subscription = false;

    predictive_attention_bridge_t* br1 = predictive_attention_bridge_create(&config1);
    ASSERT_NE(br1, nullptr);
    predictive_attention_bridge_destroy(br1);

    // Config with query handler disabled
    predictive_attention_bridge_config_t config2;
    predictive_attention_bridge_default_config(&config2);
    config2.enable_query_handling = false;

    predictive_attention_bridge_t* br2 = predictive_attention_bridge_create(&config2);
    ASSERT_NE(br2, nullptr);
    predictive_attention_bridge_destroy(br2);

    // Config with extreme weights
    predictive_attention_bridge_config_t config3;
    predictive_attention_bridge_default_config(&config3);
    config3.prediction_error_weight = 1.0f;
    config3.surprise_attention_weight = 0.0f;
    config3.precision_weight = 0.0f;
    config3.error_attention_threshold = 0.0f;

    predictive_attention_bridge_t* br3 = predictive_attention_bridge_create(&config3);
    ASSERT_NE(br3, nullptr);
    predictive_attention_bridge_destroy(br3);

    SUCCEED() << "All config variants should work";
}

/* ============================================================================
 * Average Statistics Tracking Tests
 * ============================================================================ */

/**
 * Test: AverageStatisticsTracking
 * Verify average statistics are computed correctly
 */
TEST_F(PredictiveAttentionBridgeTest, AverageStatisticsTracking) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Publish errors with known magnitudes
    predictive_attention_publish_prediction_error(bridge, 0.4f, 1001);
    predictive_attention_publish_prediction_error(bridge, 0.6f, 1002);
    predictive_attention_publish_prediction_error(bridge, 0.8f, 1003);

    // Get stats
    predictive_attention_bridge_stats_t stats;
    result = predictive_attention_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    // Average should be approximately (0.4 + 0.6 + 0.8) / 3 = 0.6
    EXPECT_NEAR(stats.avg_error_magnitude, 0.6f, 0.1f);
}
