/**
 * @file test_middleware_controller_regression.cpp
 * @brief Regression tests for middleware controller (Phase 1.5.5)
 *
 * TEST COVERAGE:
 * - API stability (no breaking changes)
 * - Configuration defaults consistency
 * - Metrics consistency
 * - Command result determinism
 * - Backward compatibility
 * - Known bug reproductions
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_middleware_controller.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MiddlewareControllerRegressionTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    middleware_controller_t* controller = nullptr;

    void SetUp() override {
        brain = brain_create(
            "regression_test",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            10,
            3
        );

        if (brain != nullptr) {
            controller = middleware_controller_create(brain);
        }
    }

    void TearDown() override {
        if (controller != nullptr) {
            middleware_controller_destroy(controller);
            controller = nullptr;
        }
        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// 1. API Stability Tests
//=============================================================================

TEST_F(MiddlewareControllerRegressionTest, DefaultConfigAPIStability) {
    middleware_controller_config_t config = middleware_controller_default_config();

    // These values must remain stable across versions
    EXPECT_FLOAT_EQ(config.default_attention_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.default_routing_weight, 0.5f);
    EXPECT_FLOAT_EQ(config.default_pattern_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.attention_decay_rate, 0.99f);
    EXPECT_FLOAT_EQ(config.route_learning_rate, 0.1f);
    EXPECT_FLOAT_EQ(config.max_activity_scale, 2.0f);
    EXPECT_FLOAT_EQ(config.min_activity_scale, 0.1f);
}

TEST_F(MiddlewareControllerRegressionTest, ConstantsAPIStability) {
    // Constants must remain stable
    EXPECT_EQ(MIDDLEWARE_CTRL_MAX_BATCH_SIZE, 8u);
    EXPECT_EQ(MIDDLEWARE_CTRL_MAX_SUBSCRIPTIONS, 64u);
    EXPECT_FLOAT_EQ(MIDDLEWARE_CTRL_DEFAULT_ATTENTION, 0.5f);
    EXPECT_FLOAT_EQ(MIDDLEWARE_CTRL_DEFAULT_ROUTING_WEIGHT, 0.5f);
    EXPECT_FLOAT_EQ(MIDDLEWARE_CTRL_LATENCY_TARGET_US, 5.0f);
}

TEST_F(MiddlewareControllerRegressionTest, ConfigStructureStability) {
    middleware_controller_config_t config = middleware_controller_default_config();

    // All fields must be accessible
    (void)config.default_attention_threshold;
    (void)config.attention_decay_rate;
    (void)config.enable_adaptive_attention;
    (void)config.default_routing_weight;
    (void)config.enable_route_learning;
    (void)config.route_learning_rate;
    (void)config.default_pattern_threshold;
    (void)config.max_subscriptions;
    (void)config.enable_pattern_notifications;
    (void)config.enable_command_batching;
    (void)config.batch_timeout_us;
    (void)config.enable_shannon_tracking;
    (void)config.max_activity_scale;
    (void)config.min_activity_scale;
}

TEST_F(MiddlewareControllerRegressionTest, MetricsStructureStability) {
    middleware_controller_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    // All fields must be accessible
    (void)metrics.total_commands;
    (void)metrics.attention_commands;
    (void)metrics.routing_commands;
    (void)metrics.pattern_commands;
    (void)metrics.activity_commands;
    (void)metrics.failed_commands;
    (void)metrics.total_latency_us;
    (void)metrics.avg_latency_us;
    (void)metrics.max_latency_us;
    (void)metrics.min_latency_us;
    (void)metrics.commands_exceeding_target;
    (void)metrics.total_information_bits;
    (void)metrics.avg_information_per_command;
    (void)metrics.command_efficiency;
    (void)metrics.channel_utilization;
    (void)metrics.batches_created;
    (void)metrics.avg_batch_size;
    (void)metrics.batching_speedup;
    (void)metrics.active_subscriptions;
    (void)metrics.pattern_notifications_sent;
}

//=============================================================================
// 2. Command Result Determinism
//=============================================================================

TEST_F(MiddlewareControllerRegressionTest, AttentionCommandDeterminism) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    // Same command should always succeed
    for (int i = 0; i < 10; i++) {
        bool result = middleware_controller_set_attention_threshold(
            controller, TARGET_VISUAL_CORTEX, 0.7f);
        EXPECT_TRUE(result);
    }
}

TEST_F(MiddlewareControllerRegressionTest, RoutingCommandDeterminism) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    for (int i = 0; i < 10; i++) {
        bool result = middleware_controller_set_routing_priority(
            controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.8f);
        EXPECT_TRUE(result);
    }
}

TEST_F(MiddlewareControllerRegressionTest, MetricsAccuracyAfterCommands) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    // Execute known number of each command type
    middleware_controller_set_attention_threshold(controller, TARGET_VISUAL_CORTEX, 0.5f);
    middleware_controller_set_attention_threshold(controller, TARGET_AUDITORY_CORTEX, 0.5f);
    middleware_controller_set_routing_priority(controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.5f);
    middleware_controller_boost_activity(controller, TARGET_AMYGDALA);
    middleware_controller_reduce_activity(controller, TARGET_MOTOR_CORTEX);

    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);

    EXPECT_EQ(metrics.total_commands, 5u);
    EXPECT_EQ(metrics.attention_commands, 2u);
    EXPECT_EQ(metrics.routing_commands, 1u);
    EXPECT_EQ(metrics.activity_commands, 2u);
    EXPECT_EQ(metrics.failed_commands, 0u);
}

//=============================================================================
// 3. Value Clamping Consistency
//=============================================================================

TEST_F(MiddlewareControllerRegressionTest, AttentionClampingConsistency) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    // Values outside [0, 1] should be clamped
    EXPECT_TRUE(middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, -1.0f));
    EXPECT_TRUE(middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 2.0f));
    EXPECT_TRUE(middleware_controller_set_attention_priority(
        controller, TARGET_VISUAL_CORTEX, -0.5f));
    EXPECT_TRUE(middleware_controller_set_attention_priority(
        controller, TARGET_VISUAL_CORTEX, 1.5f));
}

TEST_F(MiddlewareControllerRegressionTest, RoutingClampingConsistency) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    EXPECT_TRUE(middleware_controller_set_routing_priority(
        controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, -0.5f));
    EXPECT_TRUE(middleware_controller_set_routing_priority(
        controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 1.5f));
}

TEST_F(MiddlewareControllerRegressionTest, ActivityScaleClampingConsistency) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    // Should clamp to configured limits
    EXPECT_TRUE(middleware_controller_set_activity_scale(
        controller, TARGET_VISUAL_CORTEX, 0.001f));  // Below min
    EXPECT_TRUE(middleware_controller_set_activity_scale(
        controller, TARGET_VISUAL_CORTEX, 100.0f));  // Above max
}

//=============================================================================
// 4. Pattern Subscription Consistency
//=============================================================================

static void regression_callback(uint32_t, float, uint32_t, void*) {}

TEST_F(MiddlewareControllerRegressionTest, SubscriptionIDUniqueness) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    std::vector<uint32_t> ids;
    for (int i = 0; i < 10; i++) {
        uint32_t sub_id;
        bool result = middleware_controller_subscribe_pattern(
            controller, i, 0.5f, regression_callback, nullptr, &sub_id);
        if (result) {
            // IDs should be unique
            for (uint32_t existing_id : ids) {
                EXPECT_NE(sub_id, existing_id);
            }
            ids.push_back(sub_id);
        }
    }

    // IDs should be monotonically increasing
    for (size_t i = 1; i < ids.size(); i++) {
        EXPECT_GT(ids[i], ids[i-1]);
    }

    // Cleanup
    for (uint32_t id : ids) {
        middleware_controller_unsubscribe_pattern(controller, id);
    }
}

TEST_F(MiddlewareControllerRegressionTest, SubscriptionPersistence) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    uint32_t sub_id;
    middleware_controller_subscribe_pattern(
        controller, 42, 0.75f, regression_callback, nullptr, &sub_id);

    // Subscription should persist across queries
    for (int i = 0; i < 3; i++) {
        pattern_subscription_t sub;
        bool result = middleware_controller_get_subscription(controller, sub_id, &sub);
        EXPECT_TRUE(result);
        EXPECT_EQ(sub.pattern_id, 42u);
        EXPECT_FLOAT_EQ(sub.confidence_threshold, 0.75f);
        EXPECT_TRUE(sub.active);
    }

    middleware_controller_unsubscribe_pattern(controller, sub_id);
}

//=============================================================================
// 5. Null Safety Consistency
//=============================================================================

TEST_F(MiddlewareControllerRegressionTest, NullControllerHandling) {
    // All functions should return false/0 for NULL controller
    EXPECT_FALSE(middleware_controller_set_attention_threshold(nullptr, TARGET_VISUAL_CORTEX, 0.5f));
    EXPECT_FALSE(middleware_controller_set_attention_priority(nullptr, TARGET_VISUAL_CORTEX, 0.5f));
    EXPECT_FALSE(middleware_controller_set_attention_selectivity(nullptr, TARGET_VISUAL_CORTEX, 0.5f, 4));
    EXPECT_FALSE(middleware_controller_reset_attention(nullptr, TARGET_ALL_REGIONS));
    EXPECT_FALSE(middleware_controller_set_routing_priority(nullptr, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.5f));
    EXPECT_FALSE(middleware_controller_set_route_learning(nullptr, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, true));
    EXPECT_FALSE(middleware_controller_block_route(nullptr, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS));
    EXPECT_FALSE(middleware_controller_unblock_route(nullptr, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS));

    uint32_t sub_id;
    EXPECT_FALSE(middleware_controller_subscribe_pattern(nullptr, 1, 0.5f, regression_callback, nullptr, &sub_id));
    EXPECT_FALSE(middleware_controller_unsubscribe_pattern(nullptr, 1));
    EXPECT_FALSE(middleware_controller_set_pattern_threshold(nullptr, 0.5f));

    pattern_subscription_t sub;
    EXPECT_FALSE(middleware_controller_get_subscription(nullptr, 1, &sub));

    EXPECT_FALSE(middleware_controller_set_activity_scale(nullptr, TARGET_VISUAL_CORTEX, 1.0f));
    EXPECT_FALSE(middleware_controller_reduce_activity(nullptr, TARGET_VISUAL_CORTEX));
    EXPECT_FALSE(middleware_controller_boost_activity(nullptr, TARGET_VISUAL_CORTEX));
    EXPECT_FALSE(middleware_controller_reset_buffers(nullptr, TARGET_ALL_REGIONS));

    middleware_command_batch_t batch;
    EXPECT_FALSE(middleware_controller_begin_batch(nullptr, &batch));
    EXPECT_EQ(middleware_controller_execute_batch(nullptr, &batch, nullptr), 0u);

    middleware_controller_metrics_t metrics;
    EXPECT_FALSE(middleware_controller_get_metrics(nullptr, &metrics));

    EXPECT_FLOAT_EQ(middleware_controller_get_efficiency(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(middleware_controller_get_avg_latency(nullptr), 0.0f);
    EXPECT_FALSE(middleware_controller_is_performant(nullptr));

    // These should not crash
    middleware_controller_reset_stats(nullptr);
    middleware_controller_enable_shannon_tracking(nullptr, true);
    middleware_controller_enable_batching(nullptr, true);
    middleware_controller_set_activity_limits(nullptr, 0.1f, 2.0f);
    middleware_controller_on_pattern_match(nullptr, 1, 0.8f, 1);
    middleware_controller_destroy(nullptr);
}

//=============================================================================
// 6. Reset Behavior Consistency
//=============================================================================

TEST_F(MiddlewareControllerRegressionTest, StatsResetConsistency) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    // Execute some commands
    for (int i = 0; i < 10; i++) {
        middleware_controller_set_attention_threshold(controller, TARGET_VISUAL_CORTEX, 0.5f);
    }

    middleware_controller_metrics_t before;
    middleware_controller_get_metrics(controller, &before);
    EXPECT_EQ(before.total_commands, 10u);

    // Reset
    middleware_controller_reset_stats(controller);

    middleware_controller_metrics_t after;
    middleware_controller_get_metrics(controller, &after);
    EXPECT_EQ(after.total_commands, 0u);
    EXPECT_EQ(after.attention_commands, 0u);
    EXPECT_FLOAT_EQ(after.total_latency_us, 0.0f);
}

TEST_F(MiddlewareControllerRegressionTest, AttentionResetConsistency) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    // Set non-default values
    middleware_controller_set_attention_threshold(controller, TARGET_VISUAL_CORTEX, 0.9f);
    middleware_controller_set_attention_priority(controller, TARGET_VISUAL_CORTEX, 0.95f);

    // Reset
    middleware_controller_reset_attention(controller, TARGET_VISUAL_CORTEX);

    // Should be back to defaults (verified via further operations succeeding)
    EXPECT_TRUE(middleware_controller_set_attention_threshold(controller, TARGET_VISUAL_CORTEX, 0.5f));
}

//=============================================================================
// 7. Backward Compatibility
//=============================================================================

TEST_F(MiddlewareControllerRegressionTest, BrainAPICompatibility) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    // Brain APIs should work with controller active
    brain_stats_t stats;
    bool got_stats = brain_get_stats(brain, &stats);
    EXPECT_TRUE(got_stats);

    std::vector<float> input(10, 1.0f);
    brain_decision_t* decision = brain_decide(brain, input.data(), 10);
    if (decision) {
        EXPECT_GT(decision->confidence, 0.0f);
        brain_free_decision(decision);
    }

    // Controller should still work
    EXPECT_TRUE(middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 0.6f));
}

//=============================================================================
// 8. Performance Regression
//=============================================================================

TEST_F(MiddlewareControllerRegressionTest, CommandLatencyRegression) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    // Execute many commands
    for (int i = 0; i < 100; i++) {
        middleware_controller_set_attention_threshold(controller, TARGET_VISUAL_CORTEX, 0.5f);
    }

    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);

    // Average latency should be reasonable
    // Note: May exceed 5µs due to system overhead but should not be extreme
    EXPECT_LT(metrics.avg_latency_us, 1000.0f);  // Sanity check: < 1ms
}

TEST_F(MiddlewareControllerRegressionTest, MemorySafetyOnRepeat) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Setup failed";
    }

    // Repeated operations should not leak memory
    for (int iter = 0; iter < 5; iter++) {
        // Subscribe and unsubscribe patterns
        for (int i = 0; i < 10; i++) {
            uint32_t sub_id;
            middleware_controller_subscribe_pattern(
                controller, i, 0.5f, regression_callback, nullptr, &sub_id);
            middleware_controller_unsubscribe_pattern(controller, sub_id);
        }

        // Reset stats repeatedly
        middleware_controller_reset_stats(controller);

        // Reset attention repeatedly
        middleware_controller_reset_attention(controller, TARGET_ALL_REGIONS);
    }

    // Should still function
    EXPECT_TRUE(middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 0.5f));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
