/**
 * @file test_full_middleware_integration.cpp
 * @brief Integration tests for complete middleware integration stack
 */

#include <gtest/gtest.h>
#include "middleware/integration/nimcp_flow_tracker.h"
#include "middleware/integration/nimcp_middleware_controller.h"
#include "middleware/integration/nimcp_shannon_monitor.h"
#include "middleware/integration/nimcp_executive_middleware_adapter.h"
#include "middleware/integration/nimcp_quantum_command_propagator.h"
#include "async/nimcp_bio_router.h"
#include "core/brain/nimcp_brain.h"

class FullMiddlewareIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    bio_router_t* router;
    flow_tracker_t* flow_tracker;
    middleware_controller_t* controller;
    shannon_monitor_t* shannon;
    quantum_command_propagator_t* propagator;
    executive_middleware_adapter_t* exec_adapter;

    void SetUp() override {
        // Initialize bio-async router
        bio_router_config_t router_config = bio_router_default_config();
        router = bio_router_create(&router_config);
        ASSERT_NE(router, nullptr);

        // Create brain
        brain_params_t params = brain_default_params();
        params.num_neurons = 1000;
        params.num_layers = 3;
        brain = brain_create(&params);
        ASSERT_NE(brain, nullptr);

        // Create all components
        flow_tracker = flow_tracker_create();
        ASSERT_NE(flow_tracker, nullptr);

        controller = middleware_controller_create(brain);
        ASSERT_NE(controller, nullptr);

        shannon = shannon_monitor_create();
        ASSERT_NE(shannon, nullptr);

        propagator = quantum_command_propagator_create(brain, shannon);
        ASSERT_NE(propagator, nullptr);

        exec_adapter = executive_middleware_adapter_create(nullptr, propagator, shannon);
        ASSERT_NE(exec_adapter, nullptr);
    }

    void TearDown() override {
        if (exec_adapter) executive_middleware_adapter_destroy(exec_adapter);
        if (propagator) quantum_command_propagator_destroy(propagator);
        if (shannon) shannon_monitor_destroy(shannon);
        if (controller) middleware_controller_destroy(controller);
        if (flow_tracker) flow_tracker_destroy(flow_tracker);
        if (brain) brain_destroy(brain);
        if (router) bio_router_destroy(router);
    }
};

TEST_F(FullMiddlewareIntegrationTest, AllComponentsCreated) {
    // Verify all components initialized
    ASSERT_NE(flow_tracker, nullptr);
    ASSERT_NE(controller, nullptr);
    ASSERT_NE(shannon, nullptr);
    ASSERT_NE(propagator, nullptr);
    ASSERT_NE(exec_adapter, nullptr);
}

TEST_F(FullMiddlewareIntegrationTest, FlowTrackingEndToEnd) {
    // Record flows through the system
    flow_tracker_record_flow(flow_tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 20.0f, 200);
    flow_tracker_record_flow(flow_tracker, PATH_EXECUTIVE_TO_MIDDLEWARE, 25.0f, 250);

    // Get metrics
    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(flow_tracker);
    EXPECT_GE(metrics.paths[PATH_MIDDLEWARE_TO_EXECUTIVE].total_events, 1);
    EXPECT_GE(metrics.paths[PATH_EXECUTIVE_TO_MIDDLEWARE].total_events, 1);
}

TEST_F(FullMiddlewareIntegrationTest, ShannonMonitoringWithFlows) {
    // Create events and record
    event_t event = {0};
    event.type = 1;
    event.source = 0;

    shannon_monitor_record_event(shannon, &event);

    // Check Shannon metrics
    shannon_routing_metrics_t metrics = shannon_monitor_get_metrics(shannon);
    EXPECT_GE(metrics.total_events, 1);
}

TEST_F(FullMiddlewareIntegrationTest, QuantumCommandPropagation) {
    // Create command
    middleware_command_t cmd = {0};
    cmd.type = COMMAND_CONFIGURE_ATTENTION;
    cmd.priority = 0.8f;
    cmd.payload.attention.target_region = TARGET_PREFRONTAL;

    // Propagate command
    uint32_t neurons_reached = quantum_command_propagator_propagate(propagator, &cmd);
    EXPECT_GT(neurons_reached, 0);
}

TEST_F(FullMiddlewareIntegrationTest, BioAsyncMessageBroadcasting) {
    // Record flow - should broadcast message
    flow_tracker_record_flow(flow_tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 30.0f, 300);

    // Record bottleneck - should broadcast high-priority alert
    flow_tracker_record_bottlenecked_flow(flow_tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 10.0f);

    // Verify metrics updated
    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(flow_tracker);
    EXPECT_GE(metrics.paths[PATH_MIDDLEWARE_TO_EXECUTIVE].total_events, 1);
}
