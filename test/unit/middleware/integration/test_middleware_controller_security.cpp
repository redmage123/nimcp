/**
 * @file test_middleware_controller_security.cpp
 * @brief Unit tests for middleware controller BBB security integration
 */

#include <gtest/gtest.h>
#include "middleware/integration/nimcp_middleware_controller.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "core/brain/nimcp_brain.h"

class MiddlewareControllerSecurityTest : public ::testing::Test {
protected:
    middleware_controller_t* controller;
    brain_t brain;
    bbb_system_t bbb;

    void SetUp() override {
        // Create minimal brain
        brain_params_t params = brain_default_params();
        params.num_neurons = 100;
        params.num_layers = 1;
        brain = brain_create(&params);
        ASSERT_NE(brain, nullptr);

        // Initialize BBB
        bbb = nimcp_bbb_get_global_system();

        // Create controller
        middleware_controller_config_t config = middleware_controller_default_config();
        controller = middleware_controller_create_custom(brain, &config);
        ASSERT_NE(controller, nullptr);
    }

    void TearDown() override {
        if (controller) {
            middleware_controller_destroy(controller);
        }
        if (brain) {
            brain_destroy(brain);
        }
    }
};

TEST_F(MiddlewareControllerSecurityTest, CreateWithBBBValidation) {
    // Controller should be created successfully after BBB validation
    ASSERT_NE(controller, nullptr);
}

TEST_F(MiddlewareControllerSecurityTest, AttentionThresholdCommand) {
    // Set attention threshold - should pass security checks
    bool result = middleware_controller_set_attention_threshold(
        controller, TARGET_PREFRONTAL, 0.7f);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerSecurityTest, RoutingPriorityCommand) {
    // Set routing priority - should pass security checks
    bool result = middleware_controller_set_routing_priority(
        controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.8f);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerSecurityTest, ActivityScaleCommand) {
    // Set activity scale - should pass security checks
    bool result = middleware_controller_set_activity_scale(
        controller, TARGET_PREFRONTAL, 1.2f);
    EXPECT_TRUE(result);
}

TEST_F(MiddlewareControllerSecurityTest, GetMetrics) {
    // Execute some commands
    middleware_controller_set_attention_threshold(controller, TARGET_PREFRONTAL, 0.7f);
    middleware_controller_set_routing_priority(controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.8f);

    // Get metrics
    middleware_controller_metrics_t metrics;
    bool result = middleware_controller_get_metrics(controller, &metrics);
    EXPECT_TRUE(result);
    EXPECT_GE(metrics.total_commands, 2);
}
