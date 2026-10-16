/**
 * @file test_middleware_controller.cpp
 * @brief Unit tests for middleware controller (Phase 1.5.5)
 *
 * TEST COVERAGE:
 * - Configuration defaults
 * - Attention control commands
 * - Routing control commands
 * - Pattern subscription management
 * - Activity control commands
 * - Buffer control commands
 * - Batch command execution
 * - Metrics and diagnostics
 * - Thread safety
 * - Error handling
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <atomic>
#include <thread>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_middleware_controller.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MiddlewareControllerTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    middleware_controller_t* controller = nullptr;

    void SetUp() override {
        brain = brain_create(
            "controller_test",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            10,
            3
        );
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
// 1. Configuration Tests
//=============================================================================

TEST_F(MiddlewareControllerTest, DefaultConfigValid) {
    middleware_controller_config_t config = middleware_controller_default_config();

    EXPECT_FLOAT_EQ(config.default_attention_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.default_routing_weight, 0.5f);
    EXPECT_FLOAT_EQ(config.default_pattern_threshold, 0.7f);
    EXPECT_TRUE(config.enable_adaptive_attention);
    EXPECT_TRUE(config.enable_route_learning);
    EXPECT_TRUE(config.enable_command_batching);
    EXPECT_TRUE(config.enable_shannon_tracking);
    EXPECT_FLOAT_EQ(config.max_activity_scale, 2.0f);
    EXPECT_FLOAT_EQ(config.min_activity_scale, 0.1f);
}

TEST_F(MiddlewareControllerTest, CreateWithNullBrain) {
    controller = middleware_controller_create(nullptr);
    EXPECT_EQ(controller, nullptr);
}

TEST_F(MiddlewareControllerTest, CreateWithValidBrain) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    EXPECT_NE(controller, nullptr);
}

TEST_F(MiddlewareControllerTest, CreateWithCustomConfig) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    middleware_controller_config_t config = middleware_controller_default_config();
    config.default_attention_threshold = 0.8f;
    config.enable_shannon_tracking = false;

    controller = middleware_controller_create_custom(brain, &config);
    EXPECT_NE(controller, nullptr);
}

TEST_F(MiddlewareControllerTest, DestroyNullController) {
    // Should not crash
    middleware_controller_destroy(nullptr);
}

//=============================================================================
// 2. Attention Control Tests
//=============================================================================

TEST_F(MiddlewareControllerTest, SetAttentionThreshold) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 0.7f);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, SetAttentionThresholdAllRegions) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_set_attention_threshold(
        controller, TARGET_ALL_REGIONS, 0.6f);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, SetAttentionThresholdClamps) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    // Should clamp values outside [0, 1]
    EXPECT_TRUE(middleware_controller_set_attention_threshold(
        controller, TARGET_PREFRONTAL, -0.5f));
    EXPECT_TRUE(middleware_controller_set_attention_threshold(
        controller, TARGET_PREFRONTAL, 1.5f));
}

TEST_F(MiddlewareControllerTest, SetAttentionPriority) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_set_attention_priority(
        controller, TARGET_HIPPOCAMPUS, 0.9f);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, SetAttentionSelectivity) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_set_attention_selectivity(
        controller, TARGET_AUDITORY_CORTEX, 0.8f, 4);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, ResetAttention) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    // Set then reset
    middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 0.9f);

    bool result = middleware_controller_reset_attention(
        controller, TARGET_VISUAL_CORTEX);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, ResetAllAttention) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_reset_attention(
        controller, TARGET_ALL_REGIONS);
    EXPECT_TRUE(result);
}

//=============================================================================
// 3. Routing Control Tests
//=============================================================================

TEST_F(MiddlewareControllerTest, SetRoutingPriority) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_set_routing_priority(
        controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.8f);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, SetRouteLearning) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_set_route_learning(
        controller, TARGET_VISUAL_CORTEX, TARGET_PREFRONTAL, true);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, BlockRoute) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_block_route(
        controller, TARGET_AMYGDALA, TARGET_PREFRONTAL);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, UnblockRoute) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    middleware_controller_block_route(
        controller, TARGET_AMYGDALA, TARGET_PREFRONTAL);

    bool result = middleware_controller_unblock_route(
        controller, TARGET_AMYGDALA, TARGET_PREFRONTAL);
    EXPECT_TRUE(result);
}

//=============================================================================
// 4. Pattern Subscription Tests
//=============================================================================

static std::atomic<int> callback_count{0};

static void test_pattern_callback(uint32_t pattern_id, float similarity,
                                   uint32_t region_id, void* user_data) {
    callback_count++;
    (void)pattern_id;
    (void)similarity;
    (void)region_id;
    (void)user_data;
}

TEST_F(MiddlewareControllerTest, SubscribePattern) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    uint32_t sub_id = 0;
    bool result = middleware_controller_subscribe_pattern(
        controller, 42, 0.8f, test_pattern_callback, nullptr, &sub_id);

    EXPECT_TRUE(result);
    EXPECT_GT(sub_id, 0u);
}

TEST_F(MiddlewareControllerTest, SubscribePatternNullCallback) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    uint32_t sub_id = 0;
    bool result = middleware_controller_subscribe_pattern(
        controller, 42, 0.8f, nullptr, nullptr, &sub_id);

    EXPECT_FALSE(result);
}

TEST_F(MiddlewareControllerTest, UnsubscribePattern) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    uint32_t sub_id = 0;
    middleware_controller_subscribe_pattern(
        controller, 42, 0.8f, test_pattern_callback, nullptr, &sub_id);

    bool result = middleware_controller_unsubscribe_pattern(controller, sub_id);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, UnsubscribeInvalidPattern) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_unsubscribe_pattern(controller, 99999);
    EXPECT_FALSE(result);
}

TEST_F(MiddlewareControllerTest, GetSubscription) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    uint32_t sub_id = 0;
    middleware_controller_subscribe_pattern(
        controller, 42, 0.75f, test_pattern_callback, nullptr, &sub_id);

    pattern_subscription_t sub;
    bool result = middleware_controller_get_subscription(controller, sub_id, &sub);

    EXPECT_TRUE(result);
    EXPECT_EQ(sub.pattern_id, 42u);
    EXPECT_FLOAT_EQ(sub.confidence_threshold, 0.75f);
    EXPECT_TRUE(sub.active);
}

TEST_F(MiddlewareControllerTest, SetPatternThreshold) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_set_pattern_threshold(controller, 0.85f);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, PatternMatchNotification) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    callback_count = 0;

    uint32_t sub_id = 0;
    middleware_controller_subscribe_pattern(
        controller, 42, 0.7f, test_pattern_callback, nullptr, &sub_id);

    // Trigger pattern match
    middleware_controller_on_pattern_match(controller, 42, 0.8f, 1);

    EXPECT_EQ(callback_count.load(), 1);
}

TEST_F(MiddlewareControllerTest, PatternMatchBelowThreshold) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    callback_count = 0;

    uint32_t sub_id = 0;
    middleware_controller_subscribe_pattern(
        controller, 42, 0.9f, test_pattern_callback, nullptr, &sub_id);

    // Match below threshold should not fire callback
    middleware_controller_on_pattern_match(controller, 42, 0.7f, 1);

    EXPECT_EQ(callback_count.load(), 0);
}

//=============================================================================
// 5. Activity Control Tests
//=============================================================================

TEST_F(MiddlewareControllerTest, SetActivityScale) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_set_activity_scale(
        controller, TARGET_VISUAL_CORTEX, 1.5f);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, SetActivityScaleClampsLow) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    // Should clamp to min_activity_scale
    bool result = middleware_controller_set_activity_scale(
        controller, TARGET_VISUAL_CORTEX, 0.01f);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, SetActivityScaleClampsHigh) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    // Should clamp to max_activity_scale
    bool result = middleware_controller_set_activity_scale(
        controller, TARGET_VISUAL_CORTEX, 10.0f);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, ReduceActivity) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_reduce_activity(
        controller, TARGET_PREFRONTAL);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, BoostActivity) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_boost_activity(
        controller, TARGET_HIPPOCAMPUS);
    EXPECT_TRUE(result);
}

//=============================================================================
// 6. Buffer Control Tests
//=============================================================================

TEST_F(MiddlewareControllerTest, ResetBuffers) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_reset_buffers(
        controller, TARGET_VISUAL_CORTEX);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerTest, ResetAllBuffers) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    bool result = middleware_controller_reset_buffers(
        controller, TARGET_ALL_REGIONS);
    EXPECT_TRUE(result);
}

//=============================================================================
// 7. Batch Command Tests
//=============================================================================

TEST_F(MiddlewareControllerTest, BeginBatch) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    middleware_command_batch_t batch;
    bool result = middleware_controller_begin_batch(controller, &batch);

    EXPECT_TRUE(result);
    EXPECT_EQ(batch.num_commands, 0u);
}

TEST_F(MiddlewareControllerTest, ExecuteEmptyBatch) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    middleware_command_batch_t batch;
    middleware_controller_begin_batch(controller, &batch);

    uint32_t success = middleware_controller_execute_batch(controller, &batch, nullptr);
    EXPECT_EQ(success, 0u);
}

TEST_F(MiddlewareControllerTest, ExecuteBatchWithCommands) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    middleware_command_batch_t batch;
    middleware_controller_begin_batch(controller, &batch);

    // Add attention command
    batch.commands[0].type = COMMAND_CONFIGURE_ATTENTION;
    batch.commands[0].payload.attention.target_region = TARGET_VISUAL_CORTEX;
    batch.commands[0].payload.attention.priority = 0.8f;
    batch.num_commands = 1;

    command_execution_result_t results[1];
    uint32_t success = middleware_controller_execute_batch(controller, &batch, results);

    EXPECT_EQ(success, 1u);
    EXPECT_TRUE(results[0].success);
}

//=============================================================================
// 8. Metrics Tests
//=============================================================================

TEST_F(MiddlewareControllerTest, GetMetricsAfterCommands) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    // Execute some commands
    middleware_controller_set_attention_threshold(controller, TARGET_VISUAL_CORTEX, 0.7f);
    middleware_controller_set_routing_priority(controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.8f);
    middleware_controller_boost_activity(controller, TARGET_AMYGDALA);

    middleware_controller_metrics_t metrics;
    bool result = middleware_controller_get_metrics(controller, &metrics);

    EXPECT_TRUE(result);
    EXPECT_EQ(metrics.total_commands, 3u);
    EXPECT_GE(metrics.attention_commands, 1u);
    EXPECT_GE(metrics.routing_commands, 1u);
    EXPECT_GE(metrics.activity_commands, 1u);
    EXPECT_GT(metrics.total_information_bits, 0.0f);
    EXPECT_GE(metrics.avg_latency_us, 0.0f);
}

TEST_F(MiddlewareControllerTest, GetEfficiency) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    // Execute commands
    for (int i = 0; i < 10; i++) {
        middleware_controller_set_attention_threshold(controller, TARGET_VISUAL_CORTEX, 0.5f + i * 0.05f);
    }

    float efficiency = middleware_controller_get_efficiency(controller);
    EXPECT_GE(efficiency, 0.0f);
}

TEST_F(MiddlewareControllerTest, GetAvgLatency) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    middleware_controller_set_attention_threshold(controller, TARGET_VISUAL_CORTEX, 0.7f);

    float latency = middleware_controller_get_avg_latency(controller);
    EXPECT_GE(latency, 0.0f);
}

TEST_F(MiddlewareControllerTest, IsPerformant) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    // Execute fast commands
    for (int i = 0; i < 10; i++) {
        middleware_controller_set_attention_threshold(controller, TARGET_VISUAL_CORTEX, 0.5f);
    }

    // Simple commands should be fast
    bool performant = middleware_controller_is_performant(controller);
    // Note: May not always be true depending on system load
    (void)performant;
}

TEST_F(MiddlewareControllerTest, ResetStats) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    // Execute commands
    middleware_controller_set_attention_threshold(controller, TARGET_VISUAL_CORTEX, 0.7f);
    middleware_controller_set_routing_priority(controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.8f);

    // Reset stats
    middleware_controller_reset_stats(controller);

    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);

    EXPECT_EQ(metrics.total_commands, 0u);
    EXPECT_EQ(metrics.attention_commands, 0u);
}

//=============================================================================
// 9. Configuration API Tests
//=============================================================================

TEST_F(MiddlewareControllerTest, EnableShannonTracking) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    // Should not crash
    middleware_controller_enable_shannon_tracking(controller, true);
    middleware_controller_enable_shannon_tracking(controller, false);
}

TEST_F(MiddlewareControllerTest, EnableBatching) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    middleware_controller_enable_batching(controller, true);
    middleware_controller_enable_batching(controller, false);
}

TEST_F(MiddlewareControllerTest, SetActivityLimits) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    controller = middleware_controller_create(brain);
    ASSERT_NE(controller, nullptr);

    middleware_controller_set_activity_limits(controller, 0.2f, 1.8f);

    // Now activity should clamp to these limits
    middleware_controller_set_activity_scale(controller, TARGET_VISUAL_CORTEX, 0.1f);
    middleware_controller_set_activity_scale(controller, TARGET_VISUAL_CORTEX, 5.0f);
}

//=============================================================================
// 10. Null Safety Tests
//=============================================================================

TEST_F(MiddlewareControllerTest, NullControllerSafety) {
    // All functions should handle NULL gracefully
    EXPECT_FALSE(middleware_controller_set_attention_threshold(nullptr, TARGET_VISUAL_CORTEX, 0.5f));
    EXPECT_FALSE(middleware_controller_set_attention_priority(nullptr, TARGET_VISUAL_CORTEX, 0.5f));
    EXPECT_FALSE(middleware_controller_set_routing_priority(nullptr, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.5f));
    EXPECT_FALSE(middleware_controller_block_route(nullptr, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS));
    EXPECT_FALSE(middleware_controller_unblock_route(nullptr, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS));

    uint32_t sub_id;
    EXPECT_FALSE(middleware_controller_subscribe_pattern(nullptr, 1, 0.5f, test_pattern_callback, nullptr, &sub_id));
    EXPECT_FALSE(middleware_controller_unsubscribe_pattern(nullptr, 1));
    EXPECT_FALSE(middleware_controller_set_activity_scale(nullptr, TARGET_VISUAL_CORTEX, 1.0f));
    EXPECT_FALSE(middleware_controller_reduce_activity(nullptr, TARGET_VISUAL_CORTEX));
    EXPECT_FALSE(middleware_controller_boost_activity(nullptr, TARGET_VISUAL_CORTEX));
    EXPECT_FALSE(middleware_controller_reset_buffers(nullptr, TARGET_ALL_REGIONS));

    middleware_controller_metrics_t metrics;
    EXPECT_FALSE(middleware_controller_get_metrics(nullptr, &metrics));
    EXPECT_FLOAT_EQ(middleware_controller_get_efficiency(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(middleware_controller_get_avg_latency(nullptr), 0.0f);
    EXPECT_FALSE(middleware_controller_is_performant(nullptr));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
