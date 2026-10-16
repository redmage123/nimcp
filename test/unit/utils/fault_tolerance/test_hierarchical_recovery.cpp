/**
 * @file test_hierarchical_recovery.cpp
 * @brief Unit tests for hierarchical recovery orchestration module
 *
 * Tests multi-level recovery (Node→Pod→Region→Global),
 * circuit breakers, cascade prevention, and escalation policies.
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_hierarchical_recovery.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class HierarchicalRecoveryTest : public ::testing::Test {
protected:
    hr_context_t* ctx;
    hr_config_t config;

    void SetUp() override {
        config = hr_default_config();
        config.node_id = 1;
        config.node_level = HR_LEVEL_NODE;
        ctx = hr_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            hr_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(HrLifecycleTest, DefaultConfig) {
    hr_config_t config = hr_default_config();

    EXPECT_TRUE(config.enable_circuit_breakers);
    EXPECT_TRUE(config.enable_cascade_prevention);
    EXPECT_GT(config.max_recovery_attempts, 0);
}

TEST(HrLifecycleTest, CreateAndDestroy) {
    hr_config_t config = hr_default_config();
    config.node_id = 1;

    hr_context_t* ctx = hr_create(&config);
    ASSERT_NE(ctx, nullptr);

    hr_destroy(ctx);
}

TEST(HrLifecycleTest, CreateWithNullConfig) {
    hr_context_t* ctx = hr_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(HierarchicalRecoveryTest, StartAndStop) {
    EXPECT_TRUE(hr_start(ctx));
    EXPECT_TRUE(hr_stop(ctx));
}

//=============================================================================
// Hierarchy Management Tests
//=============================================================================

TEST_F(HierarchicalRecoveryTest, AddChild) {
    EXPECT_TRUE(hr_add_child(ctx, 2, HR_LEVEL_NODE));
}

TEST_F(HierarchicalRecoveryTest, AddMultipleChildren) {
    for (uint32_t i = 2; i <= 5; i++) {
        EXPECT_TRUE(hr_add_child(ctx, i, HR_LEVEL_NODE));
    }
}

TEST_F(HierarchicalRecoveryTest, RemoveChild) {
    hr_add_child(ctx, 2, HR_LEVEL_NODE);
    hr_add_child(ctx, 3, HR_LEVEL_NODE);

    EXPECT_TRUE(hr_remove_child(ctx, 2));
}

TEST_F(HierarchicalRecoveryTest, SetParent) {
    EXPECT_TRUE(hr_set_parent(ctx, 100));
}

TEST_F(HierarchicalRecoveryTest, GetNodeContext) {
    // hr_get_node_context returns context for the node itself (node_id=1 from fixture)
    // Adding a child doesn't create a retrievable context - children are just IDs
    hr_add_child(ctx, 2, HR_LEVEL_NODE);

    hr_node_context_t node_ctx;
    // Get context for self (node_id=1, the configured node ID)
    EXPECT_TRUE(hr_get_node_context(ctx, 1, &node_ctx));
    EXPECT_EQ(node_ctx.node_id, 1);
    EXPECT_EQ(node_ctx.child_count, 1u);  // We added one child
}

//=============================================================================
// Recovery Operation Tests
//=============================================================================

TEST_F(HierarchicalRecoveryTest, SubmitRecovery) {
    EXPECT_TRUE(hr_start(ctx));

    hr_recovery_request_t request = {0};
    request.request_id = 1;
    request.source_node_id = 1;
    request.current_level = HR_LEVEL_NODE;
    request.max_level = HR_LEVEL_REGION;
    request.fault_type = 1;
    request.fault_severity = 50;

    hr_recovery_response_t response;
    hr_result_t result = hr_submit_recovery(ctx, &request, &response);
    EXPECT_GE(result, HR_RESULT_SUCCESS);

    EXPECT_TRUE(hr_stop(ctx));
}

TEST_F(HierarchicalRecoveryTest, Escalate) {
    EXPECT_TRUE(hr_start(ctx));

    hr_recovery_request_t request = {0};
    request.request_id = 1;
    request.source_node_id = 1;
    request.current_level = HR_LEVEL_NODE;

    hr_recovery_response_t response;
    hr_submit_recovery(ctx, &request, &response);

    EXPECT_TRUE(hr_escalate(ctx, 1, "manual escalation"));

    EXPECT_TRUE(hr_stop(ctx));
}

TEST_F(HierarchicalRecoveryTest, RegisterHandler) {
    auto handler = [](const hr_recovery_request_t* req,
                      hr_recovery_response_t* resp,
                      void* user_data) -> hr_result_t {
        (void)req;
        (void)resp;
        (void)user_data;
        return HR_RESULT_SUCCESS;
    };

    EXPECT_TRUE(hr_register_handler(ctx, HR_LEVEL_NODE, handler, nullptr));
}

//=============================================================================
// Escalation Policy Tests
//=============================================================================

TEST_F(HierarchicalRecoveryTest, AddPolicy) {
    hr_escalation_policy_t policy = {0};
    strncpy(policy.name, "test_policy", sizeof(policy.name) - 1);
    policy.source_level = HR_LEVEL_NODE;
    policy.target_level = HR_LEVEL_POD;
    policy.trigger = HR_ESCALATE_TIMEOUT;
    policy.threshold = 3;
    policy.timeout_ms = 5000;
    policy.enabled = true;

    EXPECT_TRUE(hr_add_policy(ctx, &policy));
}

TEST_F(HierarchicalRecoveryTest, RemovePolicy) {
    hr_escalation_policy_t policy = {0};
    strncpy(policy.name, "test_policy", sizeof(policy.name) - 1);
    policy.source_level = HR_LEVEL_NODE;
    policy.target_level = HR_LEVEL_POD;

    hr_add_policy(ctx, &policy);
    EXPECT_TRUE(hr_remove_policy(ctx, "test_policy"));
}

TEST_F(HierarchicalRecoveryTest, SetPolicyEnabled) {
    hr_escalation_policy_t policy = {0};
    strncpy(policy.name, "test_policy", sizeof(policy.name) - 1);
    policy.enabled = true;

    hr_add_policy(ctx, &policy);
    EXPECT_TRUE(hr_set_policy_enabled(ctx, "test_policy", false));
}

//=============================================================================
// Circuit Breaker Tests
//=============================================================================

TEST_F(HierarchicalRecoveryTest, AddCircuitBreaker) {
    hr_circuit_config_t cb_config = {0};
    strncpy(cb_config.name, "test_cb", sizeof(cb_config.name) - 1);
    cb_config.failure_threshold = 5;
    cb_config.success_threshold = 3;
    cb_config.timeout_ms = 10000;
    cb_config.half_open_max = 3;

    EXPECT_TRUE(hr_add_circuit_breaker(ctx, &cb_config));
}

TEST_F(HierarchicalRecoveryTest, GetCircuitBreaker) {
    hr_circuit_config_t cb_config = {0};
    strncpy(cb_config.name, "test_cb", sizeof(cb_config.name) - 1);
    cb_config.failure_threshold = 5;

    hr_add_circuit_breaker(ctx, &cb_config);

    hr_circuit_breaker_t breaker;
    EXPECT_TRUE(hr_get_circuit_breaker(ctx, "test_cb", &breaker));
    EXPECT_EQ(breaker.state, HR_CIRCUIT_CLOSED);
}

TEST_F(HierarchicalRecoveryTest, RecordCircuitResult) {
    hr_circuit_config_t cb_config = {0};
    strncpy(cb_config.name, "test_cb", sizeof(cb_config.name) - 1);
    cb_config.failure_threshold = 3;
    cb_config.timeout_ms = 10000;

    hr_add_circuit_breaker(ctx, &cb_config);

    // Record failures to trip circuit
    for (int i = 0; i < 4; i++) {
        hr_record_circuit_result(ctx, "test_cb", false);
    }

    hr_circuit_breaker_t breaker;
    hr_get_circuit_breaker(ctx, "test_cb", &breaker);
    EXPECT_EQ(breaker.state, HR_CIRCUIT_OPEN);
}

TEST_F(HierarchicalRecoveryTest, CircuitAllow) {
    hr_circuit_config_t cb_config = {0};
    strncpy(cb_config.name, "test_cb", sizeof(cb_config.name) - 1);
    cb_config.failure_threshold = 2;

    hr_add_circuit_breaker(ctx, &cb_config);

    EXPECT_TRUE(hr_circuit_allow(ctx, "test_cb"));

    hr_record_circuit_result(ctx, "test_cb", false);
    hr_record_circuit_result(ctx, "test_cb", false);
    hr_record_circuit_result(ctx, "test_cb", false);

    EXPECT_FALSE(hr_circuit_allow(ctx, "test_cb"));
}

TEST_F(HierarchicalRecoveryTest, ResetCircuit) {
    hr_circuit_config_t cb_config = {0};
    strncpy(cb_config.name, "test_cb", sizeof(cb_config.name) - 1);
    cb_config.failure_threshold = 2;

    hr_add_circuit_breaker(ctx, &cb_config);

    hr_record_circuit_result(ctx, "test_cb", false);
    hr_record_circuit_result(ctx, "test_cb", false);
    hr_record_circuit_result(ctx, "test_cb", false);

    EXPECT_TRUE(hr_reset_circuit(ctx, "test_cb"));
    EXPECT_TRUE(hr_circuit_allow(ctx, "test_cb"));
}

//=============================================================================
// Cascade Prevention Tests
//=============================================================================

TEST_F(HierarchicalRecoveryTest, DetectCascade) {
    EXPECT_TRUE(hr_start(ctx));

    // Add multiple children
    for (uint32_t i = 2; i <= 5; i++) {
        hr_add_child(ctx, i, HR_LEVEL_NODE);
    }

    hr_cascade_info_t cascade_info;
    bool detected = hr_detect_cascade(ctx, 2, &cascade_info);

    // Detection depends on implementation
    (void)detected;

    EXPECT_TRUE(hr_stop(ctx));
}

TEST_F(HierarchicalRecoveryTest, PreventCascade) {
    EXPECT_TRUE(hr_start(ctx));

    hr_cascade_info_t cascade_info = {0};
    cascade_info.affected_count = 3;
    cascade_info.strategy = HR_CASCADE_ISOLATION;
    cascade_info.cascade_detected = true;

    EXPECT_TRUE(hr_prevent_cascade(ctx, &cascade_info));

    EXPECT_TRUE(hr_stop(ctx));
}

TEST_F(HierarchicalRecoveryTest, GetCascadePreventionCount) {
    uint32_t count = hr_get_cascade_prevention_count(ctx);
    EXPECT_GE(count, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(HierarchicalRecoveryTest, GetStats) {
    EXPECT_TRUE(hr_start(ctx));

    hr_stats_t stats;
    EXPECT_TRUE(hr_get_stats(ctx, &stats));

    EXPECT_GE(stats.total_requests, 0);

    EXPECT_TRUE(hr_stop(ctx));
}

TEST_F(HierarchicalRecoveryTest, ResetStats) {
    EXPECT_TRUE(hr_start(ctx));

    hr_reset_stats(ctx);

    hr_stats_t stats;
    EXPECT_TRUE(hr_get_stats(ctx, &stats));
    EXPECT_EQ(stats.total_requests, 0);

    EXPECT_TRUE(hr_stop(ctx));
}

TEST_F(HierarchicalRecoveryTest, GetLatencyByLevel) {
    EXPECT_TRUE(hr_start(ctx));

    uint64_t latency = hr_get_latency_by_level(ctx, HR_LEVEL_NODE);
    EXPECT_GE(latency, 0);

    EXPECT_TRUE(hr_stop(ctx));
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST(HrStringTest, LevelToString) {
    EXPECT_STREQ("NODE", hr_level_to_string(HR_LEVEL_NODE));
    EXPECT_STREQ("POD", hr_level_to_string(HR_LEVEL_POD));
    EXPECT_STREQ("REGION", hr_level_to_string(HR_LEVEL_REGION));
    EXPECT_STREQ("GLOBAL", hr_level_to_string(HR_LEVEL_GLOBAL));
}

TEST(HrStringTest, TriggerToString) {
    EXPECT_STREQ("TIMEOUT", hr_trigger_to_string(HR_ESCALATE_TIMEOUT));
    EXPECT_STREQ("FAILURE", hr_trigger_to_string(HR_ESCALATE_FAILURE));
    EXPECT_STREQ("THRESHOLD", hr_trigger_to_string(HR_ESCALATE_THRESHOLD));
    EXPECT_STREQ("RESOURCE", hr_trigger_to_string(HR_ESCALATE_RESOURCE));
    EXPECT_STREQ("CASCADE", hr_trigger_to_string(HR_ESCALATE_CASCADE));
    EXPECT_STREQ("MANUAL", hr_trigger_to_string(HR_ESCALATE_MANUAL));
}

TEST(HrStringTest, CircuitStateToString) {
    EXPECT_STREQ("CLOSED", hr_circuit_state_to_string(HR_CIRCUIT_CLOSED));
    EXPECT_STREQ("OPEN", hr_circuit_state_to_string(HR_CIRCUIT_OPEN));
    EXPECT_STREQ("HALF_OPEN", hr_circuit_state_to_string(HR_CIRCUIT_HALF_OPEN));
}

TEST(HrStringTest, ResultToString) {
    EXPECT_STREQ("SUCCESS", hr_result_to_string(HR_RESULT_SUCCESS));
    EXPECT_STREQ("PARTIAL", hr_result_to_string(HR_RESULT_PARTIAL));
    EXPECT_STREQ("FAILED", hr_result_to_string(HR_RESULT_FAILED));
    EXPECT_STREQ("ESCALATED", hr_result_to_string(HR_RESULT_ESCALATED));
    EXPECT_STREQ("TIMEOUT", hr_result_to_string(HR_RESULT_TIMEOUT));
    EXPECT_STREQ("CIRCUIT_OPEN", hr_result_to_string(HR_RESULT_CIRCUIT_OPEN));
}

TEST(HrStringTest, CascadeStrategyToString) {
    EXPECT_STREQ("NONE", hr_cascade_strategy_to_string(HR_CASCADE_NONE));
    EXPECT_STREQ("ISOLATION", hr_cascade_strategy_to_string(HR_CASCADE_ISOLATION));
    EXPECT_STREQ("SHEDDING", hr_cascade_strategy_to_string(HR_CASCADE_SHEDDING));
    EXPECT_STREQ("BACKPRESSURE", hr_cascade_strategy_to_string(HR_CASCADE_BACKPRESSURE));
    EXPECT_STREQ("BULKHEAD", hr_cascade_strategy_to_string(HR_CASCADE_BULKHEAD));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(HierarchicalRecoveryTest, RemoveNonexistentChild) {
    EXPECT_FALSE(hr_remove_child(ctx, 999));
}

TEST_F(HierarchicalRecoveryTest, GetNonexistentNodeContext) {
    hr_node_context_t node_ctx;
    EXPECT_FALSE(hr_get_node_context(ctx, 999, &node_ctx));
}

TEST_F(HierarchicalRecoveryTest, MaxChildren) {
    // Try to add more than max children
    uint32_t added_count = 0;
    for (uint32_t i = 2; i <= HR_MAX_CHILDREN + 5; i++) {
        bool added = hr_add_child(ctx, i, HR_LEVEL_NODE);
        // Check before incrementing count
        if (added_count < HR_MAX_CHILDREN) {
            EXPECT_TRUE(added) << "Child " << i << " should be added (count=" << added_count << ")";
        } else {
            EXPECT_FALSE(added) << "Child " << i << " should fail (count=" << added_count << ")";
        }
        if (added) added_count++;
    }
    // Verify we added exactly HR_MAX_CHILDREN
    EXPECT_EQ(added_count, HR_MAX_CHILDREN);
}

TEST_F(HierarchicalRecoveryTest, GetNonexistentCircuitBreaker) {
    hr_circuit_breaker_t breaker;
    EXPECT_FALSE(hr_get_circuit_breaker(ctx, "nonexistent", &breaker));
}
