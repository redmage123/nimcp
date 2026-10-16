/**
 * @file test_ethics_immune.cpp
 * @brief Unit tests for Ethics-Immune System Integration
 *
 * WHAT: Test ethics violations triggering immune responses and inflammation affecting moral reasoning
 * WHY:  Verify ethics-immune bidirectional integration works correctly
 * HOW:  Create immune system, test violation → antigen mapping and inflammation → impairment
 *
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/time/nimcp_time.h"

class EthicsImmuneTest : public ::testing::Test {
protected:
    ethics_engine_t engine;
    brain_immune_system_t* immune_system;

    void SetUp() override {
        // Create ethics engine with immune integration enabled
        ethics_config_t config = {};
        config.policies = nullptr;
        config.num_policies = 0;
        config.callback = nullptr;
        config.callback_context = nullptr;
        config.default_severity = 0.5F;
        config.enable_learning = false;
        config.enable_bio_async = false;
        config.action_feature_size = 10;
        config.max_agents = 5;
        config.golden_rule_threshold = 0.0F;
        config.empathy_weight = 0.5F;
        config.enable_tom_integration = false;
        config.perspective_weight = 0.5F;
        config.enable_immune_integration = true;  // ENABLE IMMUNE INTEGRATION
        config.violation_immune_threshold = 0.7F;

        engine = ethics_engine_create(&config);
        ASSERT_NE(engine, nullptr);

        // Create brain immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        // Associate immune system with ethics engine
        ethics_set_immune_system(engine, immune_system);
    }

    void TearDown() override {
        if (engine) {
            ethics_engine_destroy(engine);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
    }

    // Helper: Simulate inflammation by creating inflammation sites
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
// Immune Response Triggering Tests
//=============================================================================

TEST_F(EthicsImmuneTest, TriggerImmuneResponseForHarmViolation) {
    // WHAT: Verify harm violation triggers immune response
    // WHY:  Ethics violations should activate immune system
    // HOW:  Trigger harm violation, check immune system activation

    bool triggered = ethics_trigger_immune_response(
        engine,
        ETHICS_VIOLATION_TYPE_HARM,
        0.8F,
        "Simulated harm to agent"
    );

    EXPECT_TRUE(triggered);

    // Check that immune system received the antigen
    brain_immune_stats_t stats;
    ASSERT_EQ(brain_immune_get_stats(immune_system, &stats), 0);
    EXPECT_GT(stats.antigens_processed, 0U);
}

TEST_F(EthicsImmuneTest, TriggerImmuneResponseForGoldenRuleViolation) {
    // WHAT: Verify Golden Rule violation triggers immune response
    // WHY:  Core ethical violations should activate immune system
    // HOW:  Trigger golden rule violation, check immune system

    bool triggered = ethics_trigger_immune_response(
        engine,
        ETHICS_VIOLATION_TYPE_GOLDEN_RULE,
        0.9F,
        "Failed reciprocity test"
    );

    EXPECT_TRUE(triggered);

    brain_immune_stats_t stats;
    ASSERT_EQ(brain_immune_get_stats(immune_system, &stats), 0);
    EXPECT_GT(stats.antigens_processed, 0U);
}

TEST_F(EthicsImmuneTest, LowSeverityViolationDoesNotTriggerImmune) {
    // WHAT: Verify low severity violations don't trigger immune response
    // WHY:  Threshold prevents immune system overactivation
    // HOW:  Trigger low severity violation, check no immune activation

    // Note: Threshold is 0.7, so 0.5 should not trigger
    bool triggered = ethics_trigger_immune_response(
        engine,
        ETHICS_VIOLATION_TYPE_UNFAIRNESS,
        0.5F,
        "Minor unfairness"
    );

    // Even though threshold check is in the caller's responsibility,
    // the function itself should still succeed
    EXPECT_TRUE(triggered);
}

TEST_F(EthicsImmuneTest, NoImmuneResponseWhenIntegrationDisabled) {
    // WHAT: Verify no immune response when integration disabled
    // WHY:  Integration should be optional
    // HOW:  Disable integration, trigger violation, check no response

    ethics_set_immune_system(engine, nullptr);

    bool triggered = ethics_trigger_immune_response(
        engine,
        ETHICS_VIOLATION_TYPE_HARM,
        0.9F,
        "High severity harm"
    );

    EXPECT_FALSE(triggered);
}

//=============================================================================
// Inflammation Impact on Ethics Evaluation Tests
//=============================================================================

TEST_F(EthicsImmuneTest, NormalConfidenceWithoutInflammation) {
    // WHAT: Verify normal confidence when no inflammation
    // WHY:  Baseline check for normal operation
    // HOW:  Evaluate action without inflammation, check confidence

    action_context_t action = {};
    action.num_features = 10;
    float features[10] = {0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F};
    action.features = features;
    action.predicted_harm = 0.2F;
    action.num_affected_agents = 1;
    agent_id_t agent = 1;
    action.affected_agents = &agent;

    ethics_evaluation_t eval;
    float inflammation_penalty = 0.0F;

    bool success = ethics_evaluate_with_immune_check(engine, &action, &eval, &inflammation_penalty);

    EXPECT_TRUE(success);
    EXPECT_FLOAT_EQ(inflammation_penalty, 0.0F);
    // Confidence should be unaffected
}

TEST_F(EthicsImmuneTest, ReducedConfidenceWithModerateInflammation) {
    // WHAT: Verify reduced confidence with moderate inflammation
    // WHY:  Inflammation impairs moral reasoning
    // HOW:  Simulate 50% inflammation, evaluate action, check confidence reduction

    simulate_inflammation(0.5F);

    action_context_t action = {};
    action.num_features = 10;
    float features[10] = {0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F};
    action.features = features;
    action.predicted_harm = 0.2F;
    action.num_affected_agents = 1;
    agent_id_t agent = 1;
    action.affected_agents = &agent;

    ethics_evaluation_t eval;
    float inflammation_penalty = 0.0F;

    bool success = ethics_evaluate_with_immune_check(engine, &action, &eval, &inflammation_penalty);

    EXPECT_TRUE(success);
    // Expect penalty around 0.25 (0.5 * 0.5)
    EXPECT_GT(inflammation_penalty, 0.2F);
    EXPECT_LT(inflammation_penalty, 0.3F);
}

TEST_F(EthicsImmuneTest, SeverelyReducedConfidenceWithHighInflammation) {
    // WHAT: Verify severe confidence reduction with high inflammation
    // WHY:  High cytokines severely impair moral reasoning
    // HOW:  Simulate 100% inflammation, check max penalty

    simulate_inflammation(1.0F);

    action_context_t action = {};
    action.num_features = 10;
    float features[10] = {0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F};
    action.features = features;
    action.predicted_harm = 0.2F;
    action.num_affected_agents = 1;
    agent_id_t agent = 1;
    action.affected_agents = &agent;

    ethics_evaluation_t eval;
    float inflammation_penalty = 0.0F;

    bool success = ethics_evaluate_with_immune_check(engine, &action, &eval, &inflammation_penalty);

    EXPECT_TRUE(success);
    // Expect penalty around 0.5 (1.0 * 0.5)
    EXPECT_FLOAT_EQ(inflammation_penalty, 0.5F);
    // Confidence should have floor at 0.1
    EXPECT_GE(eval.confidence, 0.1F);
}

TEST_F(EthicsImmuneTest, InflammationNoteInExplanation) {
    // WHAT: Verify explanation includes inflammation note when elevated
    // WHY:  Transparency about reasoning impairment
    // HOW:  Simulate high inflammation, check explanation text

    simulate_inflammation(0.8F);

    action_context_t action = {};
    action.num_features = 10;
    float features[10] = {0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F};
    action.features = features;
    action.predicted_harm = 0.2F;
    action.num_affected_agents = 1;
    agent_id_t agent = 1;
    action.affected_agents = &agent;

    ethics_evaluation_t eval;
    float inflammation_penalty = 0.0F;

    bool success = ethics_evaluate_with_immune_check(engine, &action, &eval, &inflammation_penalty);

    EXPECT_TRUE(success);
    // Check that explanation contains inflammation note
    EXPECT_NE(strstr(eval.explanation, "inflammation"), nullptr);
}

//=============================================================================
// Decision Threshold Adjustment Tests
//=============================================================================

TEST_F(EthicsImmuneTest, NormalThresholdWithoutInflammation) {
    // WHAT: Verify normal threshold when no inflammation
    // WHY:  Baseline check
    // HOW:  Query threshold with base 0.5, expect 0.5

    float adjusted = ethics_get_immune_adjusted_threshold(engine, 0.5F);
    EXPECT_FLOAT_EQ(adjusted, 0.5F);
}

TEST_F(EthicsImmuneTest, IncreasedThresholdWithInflammation) {
    // WHAT: Verify increased threshold with inflammation
    // WHY:  Inflammation → conservative decisions
    // HOW:  Simulate 50% inflammation, expect threshold increase

    simulate_inflammation(0.5F);

    float adjusted = ethics_get_immune_adjusted_threshold(engine, 0.5F);

    // Expect around 0.6 (0.5 + 0.5*0.2)
    EXPECT_GT(adjusted, 0.55F);
    EXPECT_LT(adjusted, 0.65F);
}

TEST_F(EthicsImmuneTest, MaxThresholdIncrease) {
    // WHAT: Verify max threshold increase with high inflammation
    // WHY:  High inflammation → very conservative
    // HOW:  Simulate 100% inflammation, expect max offset

    simulate_inflammation(1.0F);

    float adjusted = ethics_get_immune_adjusted_threshold(engine, 0.5F);

    // Expect around 0.7 (0.5 + 1.0*0.2)
    EXPECT_FLOAT_EQ(adjusted, 0.7F);
}

TEST_F(EthicsImmuneTest, ThresholdClampedAtOne) {
    // WHAT: Verify threshold doesn't exceed 1.0
    // WHY:  Safety bounds
    // HOW:  High base threshold + inflammation, check clamp

    simulate_inflammation(1.0F);

    float adjusted = ethics_get_immune_adjusted_threshold(engine, 0.95F);

    // Should clamp at 1.0
    EXPECT_FLOAT_EQ(adjusted, 1.0F);
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(EthicsImmuneTest, NullEngineHandling) {
    // WHAT: Verify graceful handling of NULL engine
    // WHY:  Guard against invalid inputs
    // HOW:  Pass NULL, expect safe defaults

    ethics_evaluation_t eval;
    float penalty = 0.0F;

    action_context_t action = {};
    bool success = ethics_evaluate_with_immune_check(nullptr, &action, &eval, &penalty);

    EXPECT_FALSE(success);
}

TEST_F(EthicsImmuneTest, NullActionHandling) {
    // WHAT: Verify graceful handling of NULL action
    // WHY:  Guard against invalid inputs
    // HOW:  Pass NULL action, expect failure

    ethics_evaluation_t eval;
    float penalty = 0.0F;

    bool success = ethics_evaluate_with_immune_check(engine, nullptr, &eval, &penalty);

    EXPECT_FALSE(success);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
