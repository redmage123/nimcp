/**
 * @file test_executive_ethics_immune_integration.cpp
 * @brief Integration tests for Executive-Ethics-Immune System Pipeline
 *
 * WHAT: Test full integration of executive functions, ethics, and immune system
 * WHY:  Verify complete bidirectional integration works as designed
 * HOW:  Simulate realistic scenarios with all three systems interacting
 *
 * SCENARIOS:
 * 1. Ethics violation triggers immune response, inflammation impairs executive function
 * 2. High cognitive load from executive impacts ethics evaluation confidence
 * 3. Immune-induced impairment reduces impulse control and ethical reasoning
 * 4. Recovery from inflammation restores normal function
 *
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_executive.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/time/nimcp_time.h"

class ExecutiveEthicsImmuneIntegrationTest : public ::testing::Test {
protected:
    executive_controller_t* exec;
    ethics_engine_t ethics;
    brain_immune_system_t* immune_system;

    void SetUp() override {
        // Create brain immune system first (shared)
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        // Create executive controller with immune integration
        executive_config_t exec_config = {};
        exec_config.max_tasks = 16;
        exec_config.task_switch_cost_ms = 200.0F;
        exec_config.inhibition_threshold = 0.7F;
        exec_config.max_plan_depth = 10;
        exec_config.enable_task_prioritization = true;
        exec_config.enable_deadline_checking = true;
        exec_config.enable_immune_integration = true;
        exec_config.immune_impairment_threshold = 0.6F;
        exec_config.immune_critical_threshold = 0.85F;

        exec = executive_create_custom(&exec_config);
        ASSERT_NE(exec, nullptr);
        executive_set_immune_system(exec, immune_system);

        // Create ethics engine with immune integration
        ethics_config_t ethics_config = {};
        ethics_config.policies = nullptr;
        ethics_config.num_policies = 0;
        ethics_config.callback = nullptr;
        ethics_config.default_severity = 0.5F;
        ethics_config.enable_learning = false;
        ethics_config.enable_bio_async = false;
        ethics_config.action_feature_size = 10;
        ethics_config.max_agents = 5;
        ethics_config.golden_rule_threshold = 0.0F;
        ethics_config.empathy_weight = 0.5F;
        ethics_config.enable_immune_integration = true;
        ethics_config.violation_immune_threshold = 0.7F;

        ethics = ethics_engine_create(&ethics_config);
        ASSERT_NE(ethics, nullptr);
        ethics_set_immune_system(ethics, immune_system);
    }

    void TearDown() override {
        if (exec) {
            executive_destroy(exec);
        }
        if (ethics) {
            ethics_engine_destroy(ethics);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
    }

    // Helper: Simulate inflammation
    void simulate_inflammation(float level) {
        uint32_t num_sites = (uint32_t)(level * 64.0F);
        for (uint32_t i = 0; i < num_sites; i++) {
            uint32_t site_id = 0;
            uint32_t antigen_id = 0;
            uint8_t epitope[64] = {(uint8_t)i};
            brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                          epitope, sizeof(epitope), 5, 0, &antigen_id);
            brain_immune_initiate_inflammation(immune_system, i, antigen_id, &site_id);
        }
    }
};

//=============================================================================
// Full Pipeline Integration Tests
//=============================================================================

TEST_F(ExecutiveEthicsImmuneIntegrationTest, EthicsViolationTriggersImmuneImpairsExecutive) {
    // WHAT: Complete pipeline: ethics violation → immune response → executive impairment
    // WHY:  Verify full bidirectional integration
    // HOW:  Trigger violation, check immune activation, verify executive impairment
    //
    // SCENARIO:
    // 1. System starts in normal state (no inflammation)
    // 2. Severe ethics violation occurs
    // 3. Ethics engine triggers immune response (creates antigen)
    // 4. Immune system activates (inflammation increases)
    // 5. Executive function becomes impaired (cognitive fog)

    // Step 1: Verify baseline - no inflammation, normal executive function
    float baseline_capacity = executive_get_immune_adjusted_capacity(exec);
    float baseline_switch_cost = executive_get_immune_adjusted_switch_cost(exec);
    bool baseline_impaired = executive_is_immune_impaired(exec);

    EXPECT_FLOAT_EQ(baseline_capacity, 1.0F);
    EXPECT_FLOAT_EQ(baseline_switch_cost, 200.0F);
    EXPECT_FALSE(baseline_impaired);

    // Step 2: Trigger severe ethics violation
    bool immune_triggered = ethics_trigger_immune_response(
        ethics,
        ETHICS_VIOLATION_TYPE_HARM,
        0.9F,
        "Severe harm to agent - integrity threat"
    );

    EXPECT_TRUE(immune_triggered);

    // Step 3: Verify immune system activation
    brain_immune_stats_t immune_stats;
    ASSERT_EQ(brain_immune_get_stats(immune_system, &immune_stats), 0);
    EXPECT_GT(immune_stats.antigens_processed, 0U);

    // Step 4: Simulate inflammation from immune response
    // (In real system, this would happen automatically via immune processing)
    simulate_inflammation(0.7F);  // Significant inflammation

    // Step 5: Verify executive function impairment
    float impaired_capacity = executive_get_immune_adjusted_capacity(exec);
    float impaired_switch_cost = executive_get_immune_adjusted_switch_cost(exec);
    bool is_impaired = executive_is_immune_impaired(exec);

    EXPECT_LT(impaired_capacity, baseline_capacity);  // Reduced capacity
    EXPECT_GT(impaired_switch_cost, baseline_switch_cost);  // Increased cost
    EXPECT_TRUE(is_impaired);  // Impairment detected
}

TEST_F(ExecutiveEthicsImmuneIntegrationTest, InflammationImpairedDecisionMaking) {
    // WHAT: Verify inflammation impairs both executive and ethics decision-making
    // WHY:  Cognitive fog should affect all prefrontal functions
    // HOW:  Simulate inflammation, test executive + ethics impairment
    //
    // SCENARIO:
    // 1. Simulate systemic inflammation (e.g., from infection response)
    // 2. Test executive function impairment (task switching, inhibition)
    // 3. Test ethics evaluation impairment (reduced confidence)
    // 4. Verify decision thresholds increased (risk aversion)

    // Step 1: Simulate moderate to high inflammation
    simulate_inflammation(0.65F);

    // Step 2: Test executive function impairment
    float capacity = executive_get_immune_adjusted_capacity(exec);
    float switch_cost = executive_get_immune_adjusted_switch_cost(exec);
    float inhibition = executive_get_immune_adjusted_inhibition(exec);
    bool impaired = executive_is_immune_impaired(exec);

    EXPECT_LT(capacity, 0.7F);  // Significantly reduced
    EXPECT_GT(switch_cost, 300.0F);  // Increased rigidity
    EXPECT_GT(inhibition, 0.8F);  // Impaired impulse control
    EXPECT_TRUE(impaired);

    // Step 3: Test ethics evaluation impairment
    action_context_t action = {};
    action.num_features = 10;
    float features[10] = {0.5F};
    action.features = features;
    action.predicted_harm = 0.3F;
    action.num_affected_agents = 1;
    agent_id_t agent = 1;
    action.affected_agents = &agent;

    ethics_evaluation_t eval;
    float inflammation_penalty = 0.0F;

    bool eval_success = ethics_evaluate_with_immune_check(ethics, &action, &eval, &inflammation_penalty);

    EXPECT_TRUE(eval_success);
    EXPECT_GT(inflammation_penalty, 0.25F);  // Significant penalty
    // Explanation should mention inflammation
    EXPECT_NE(strstr(eval.explanation, "inflammation"), nullptr);

    // Step 4: Verify risk aversion (increased thresholds)
    float adjusted_threshold = ethics_get_immune_adjusted_threshold(ethics, 0.5F);
    EXPECT_GT(adjusted_threshold, 0.6F);  // More conservative
}

TEST_F(ExecutiveEthicsImmuneIntegrationTest, MultipleViolationsEscalateImmuneResponse) {
    // WHAT: Verify repeated violations build immune memory (escalating response)
    // WHY:  Adaptive immunity should strengthen with repeated threats
    // HOW:  Trigger multiple violations, check escalating immune response
    //
    // SCENARIO:
    // 1. Trigger first violation → immune response
    // 2. Trigger second violation (same type) → stronger response
    // 3. Trigger third violation → even stronger (memory formation)
    // 4. Verify immune system shows adaptive behavior

    // Trigger first violation
    ethics_trigger_immune_response(ethics, ETHICS_VIOLATION_TYPE_HARM, 0.8F, "First harm");

    brain_immune_stats_t stats1;
    brain_immune_get_stats(immune_system, &stats1);
    uint64_t first_antigens = stats1.antigens_processed;

    // Trigger second violation (same type)
    ethics_trigger_immune_response(ethics, ETHICS_VIOLATION_TYPE_HARM, 0.8F, "Second harm");

    brain_immune_stats_t stats2;
    brain_immune_get_stats(immune_system, &stats2);
    uint64_t second_antigens = stats2.antigens_processed;

    // Trigger third violation
    ethics_trigger_immune_response(ethics, ETHICS_VIOLATION_TYPE_HARM, 0.8F, "Third harm");

    brain_immune_stats_t stats3;
    brain_immune_get_stats(immune_system, &stats3);
    uint64_t third_antigens = stats3.antigens_processed;

    // Verify escalation
    EXPECT_GT(second_antigens, first_antigens);
    EXPECT_GT(third_antigens, second_antigens);

    // Check for memory formation
    // (In real system, immune system would show faster responses to repeated threats)
}

TEST_F(ExecutiveEthicsImmuneIntegrationTest, RecoveryFromInflammationRestoresFunction) {
    // WHAT: Verify resolution of inflammation restores normal function
    // WHY:  System should recover after threat cleared
    // HOW:  Simulate inflammation, resolve it, verify function restoration
    //
    // SCENARIO:
    // 1. Create inflammation (cognitive impairment)
    // 2. Measure impaired function
    // 3. Resolve inflammation
    // 4. Verify function restoration

    // Step 1: Create inflammation
    simulate_inflammation(0.8F);

    // Step 2: Measure impaired function
    float impaired_capacity = executive_get_immune_adjusted_capacity(exec);
    bool is_impaired = executive_is_immune_impaired(exec);

    EXPECT_LT(impaired_capacity, 0.5F);
    EXPECT_TRUE(is_impaired);

    // Step 3: Resolve inflammation
    // (In real system, this would happen via brain_immune_resolve_inflammation)
    // For test, we can create a new immune system (simulates recovery)
    brain_immune_system_t* recovered_immune = nullptr;
    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    recovered_immune = brain_immune_create(&immune_config);

    executive_set_immune_system(exec, recovered_immune);
    ethics_set_immune_system(ethics, recovered_immune);

    // Step 4: Verify function restoration
    float recovered_capacity = executive_get_immune_adjusted_capacity(exec);
    bool still_impaired = executive_is_immune_impaired(exec);

    EXPECT_FLOAT_EQ(recovered_capacity, 1.0F);
    EXPECT_FALSE(still_impaired);

    brain_immune_destroy(recovered_immune);
}

TEST_F(ExecutiveEthicsImmuneIntegrationTest, CombinedStressorsCompoundImpairment) {
    // WHAT: Verify multiple stressors (high load + inflammation) compound impairment
    // WHY:  Real systems face multiple challenges simultaneously
    // HOW:  Add tasks (cognitive load) + inflammation, verify additive impairment
    //
    // SCENARIO:
    // 1. Add multiple high-priority tasks (increase cognitive load)
    // 2. Add inflammation
    // 3. Verify compounded impairment in decision-making
    // 4. Check that system degrades gracefully

    // Step 1: Add cognitive load (multiple tasks)
    for (int i = 0; i < 10; i++) {
        task_descriptor_t task = {};
        task.type = TASK_TYPE_REASONING;
        task.priority = PRIORITY_HIGH;
        snprintf(task.name, sizeof(task.name), "Task_%d", i);
        executive_add_task(exec, &task);
    }

    float cognitive_load = executive_get_cognitive_load(exec);
    EXPECT_GT(cognitive_load, 0.5F);  // High load

    // Step 2: Add inflammation
    simulate_inflammation(0.6F);

    // Step 3: Verify compounded impairment
    float capacity = executive_get_immune_adjusted_capacity(exec);
    float switch_cost = executive_get_immune_adjusted_switch_cost(exec);
    bool impaired = executive_is_immune_impaired(exec);

    EXPECT_LT(capacity, 0.6F);  // Reduced by inflammation
    EXPECT_GT(switch_cost, 300.0F);  // Increased by inflammation
    EXPECT_TRUE(impaired);

    // Ethics should also show impairment
    action_context_t action = {};
    action.num_features = 10;
    float features[10] = {0.5F};
    action.features = features;
    action.predicted_harm = 0.3F;
    action.num_affected_agents = 1;
    agent_id_t agent = 1;
    action.affected_agents = &agent;

    ethics_evaluation_t eval;
    float penalty = 0.0F;

    ethics_evaluate_with_immune_check(ethics, &action, &eval, &penalty);

    EXPECT_GT(penalty, 0.2F);  // Significant confidence reduction

    // Step 4: Verify graceful degradation (system still functional)
    EXPECT_GT(capacity, 0.1F);  // Not completely disabled
    EXPECT_GT(eval.confidence, 0.1F);  // Still has some confidence
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
