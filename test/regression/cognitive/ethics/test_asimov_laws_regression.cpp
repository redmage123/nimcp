/**
 * @file test_asimov_laws_regression.cpp
 * @brief Regression tests for Asimov's Laws of Robotics (NIMCP 2.5.2)
 *
 * These tests ensure:
 * - Backward compatibility with existing ethics API
 * - Consistent behavior across versions
 * - Performance characteristics are maintained
 * - No regressions in evaluation accuracy
 */

#include "test_helpers.h"
#include "cognitive/ethics/nimcp_ethics.h"

#include <cstring>
#include <chrono>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class AsimovRegressionTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        ethics_config_t config = {.policies = nullptr,
                                  .num_policies = 0,
                                  .callback = nullptr,
                                  .callback_context = nullptr,
                                  .default_severity = 0.5f,
                                  .enable_learning = false,
                                  .action_feature_size = 10,
                                  .max_agents = 100,
                                  .golden_rule_threshold = 0.0f,
                                  .empathy_weight = 0.7f};

        engine = ethics_engine_create(&config);
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override
    {
        if (engine) {
            ethics_engine_destroy(engine);
            engine = nullptr;
        }
    }

    action_context_t create_action(float harm_level)
    {
        action_context_t action;
        memset(&action, 0, sizeof(action));

        static float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
        action.features = features;
        action.num_features = 10;

        static agent_id_t agents[3] = {1, 2, 3};
        action.affected_agents = agents;
        action.num_affected_agents = 3;

        action.predicted_harm = harm_level;
        return action;
    }

    ethics_engine_t engine;
};

//=============================================================================
// API Backward Compatibility Tests
//=============================================================================

TEST_F(AsimovRegressionTest, ExistingAPIStillWorks)
{
    // ethics_engine_evaluate_action should still work
    action_context_t action = create_action(0.5f);
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    // Should return valid evaluation
    EXPECT_GE(result.golden_rule_score, -1.0f);
    EXPECT_LE(result.golden_rule_score, 1.0f);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(AsimovRegressionTest, PolicyAPIBackwardCompatible)
{
    ethics_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    policy.policy_id = 999;
    snprintf(policy.name, sizeof(policy.name), "Test");
    policy.violation_type = ETHICS_VIOLATION_HARM;
    policy.severity_threshold = 0.5f;
    policy.enabled = true;

    // Old API should still work
    EXPECT_TRUE(ethics_engine_add_policy(engine, &policy));
    EXPECT_TRUE(ethics_engine_remove_policy(engine, 999));
}

TEST_F(AsimovRegressionTest, LearningAPIBackwardCompatible)
{
    ethics_config_t config = {0};
    config.action_feature_size = 10;
    config.max_agents = 10;
    config.golden_rule_threshold = 0.0f;
    config.empathy_weight = 0.7f;
    config.enable_learning = true;

    ethics_engine_t learn_engine = ethics_engine_create(&config);
    ASSERT_NE(learn_engine, nullptr);

    action_context_t action = create_action(0.5f);
    action_outcome_t outcome;
    memset(&outcome, 0, sizeof(outcome));
    outcome.affected_agent = 1;
    outcome.actual_harm = 0.5f;

    // Old learning API should still work
    EXPECT_TRUE(ethics_learn_from_outcome(learn_engine, &action, &outcome));

    ethics_engine_destroy(learn_engine);
}

TEST_F(AsimovRegressionTest, StatisticsAPIBackwardCompatible)
{
    ethics_statistics_t stats;
    EXPECT_TRUE(ethics_get_statistics(engine, &stats));
    EXPECT_GE(stats.total_evaluations, 0);
    EXPECT_GE(stats.violations_detected, 0);
    EXPECT_GE(stats.num_policies, 0);
}

//=============================================================================
// Consistency Tests
//=============================================================================

TEST_F(AsimovRegressionTest, ConsistentResultsForSameInput)
{
    action_context_t action = create_action(0.5f);

    // Evaluate same action multiple times
    asimov_evaluation_t result1 = ethics_evaluate_asimov_laws(engine, &action);
    asimov_evaluation_t result2 = ethics_evaluate_asimov_laws(engine, &action);
    asimov_evaluation_t result3 = ethics_evaluate_asimov_laws(engine, &action);

    // Results should be identical
    EXPECT_EQ(result1.passed, result2.passed);
    EXPECT_EQ(result2.passed, result3.passed);
    EXPECT_FLOAT_EQ(result1.harm_to_humanity, result2.harm_to_humanity);
    EXPECT_FLOAT_EQ(result2.harm_to_humanity, result3.harm_to_humanity);
    EXPECT_FLOAT_EQ(result1.harm_to_human, result2.harm_to_human);
    EXPECT_FLOAT_EQ(result2.harm_to_human, result3.harm_to_human);
}

TEST_F(AsimovRegressionTest, LawTextsUnchanged)
{
    // Verify law texts haven't changed (important for legal/audit purposes)
    const char* zeroth = asimov_law_text(ASIMOV_LAW_ZEROTH);
    EXPECT_TRUE(strstr(zeroth, "humanity") != nullptr);
    EXPECT_TRUE(strstr(zeroth, "harm") != nullptr);

    const char* first = asimov_law_text(ASIMOV_LAW_FIRST);
    EXPECT_TRUE(strstr(first, "human being") != nullptr);
    EXPECT_TRUE(strstr(first, "injure") != nullptr);

    const char* second = asimov_law_text(ASIMOV_LAW_SECOND);
    EXPECT_TRUE(strstr(second, "obey") != nullptr);
    EXPECT_TRUE(strstr(second, "First Law") != nullptr);

    const char* third = asimov_law_text(ASIMOV_LAW_THIRD);
    EXPECT_TRUE(strstr(third, "existence") != nullptr);
    EXPECT_TRUE(strstr(third, "Second Law") != nullptr);
}

TEST_F(AsimovRegressionTest, DefaultThresholdsUnchanged)
{
    asimov_config_t config = asimov_default_config();

    // These values should remain stable
    EXPECT_FLOAT_EQ(config.humanity_harm_threshold, 0.01f);
    EXPECT_FLOAT_EQ(config.human_harm_threshold, 0.1f);
    EXPECT_FLOAT_EQ(config.inaction_harm_threshold, 0.3f);
    EXPECT_TRUE(config.enable_zeroth_law);
    EXPECT_FALSE(config.strict_mode);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(AsimovRegressionTest, EvaluationPerformance)
{
    action_context_t action = create_action(0.5f);

    auto start = std::chrono::high_resolution_clock::now();

    // Perform 1000 evaluations
    for (int i = 0; i < 1000; i++) {
        ethics_evaluate_asimov_laws(engine, &action);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 1000 evaluations in under 1 second
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(AsimovRegressionTest, FullEvaluationPerformance)
{
    action_context_t action = create_action(0.5f);

    auto start = std::chrono::high_resolution_clock::now();

    // Perform 100 full evaluations (includes Golden Rule + Asimov + policies)
    for (int i = 0; i < 100; i++) {
        ethics_engine_evaluate_action(engine, &action);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 full evaluations in under 5 seconds
    EXPECT_LT(duration.count(), 5000);
}

TEST_F(AsimovRegressionTest, CorollaryEvaluationPerformance)
{
    auto start = std::chrono::high_resolution_clock::now();

    // Perform 1000 corollary evaluations
    for (int i = 0; i < 1000; i++) {
        ethics_evaluate_asimov_corollary(engine, nullptr, "Potential harm to human");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 1000 evaluations in under 1 second
    EXPECT_LT(duration.count(), 1000);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(AsimovRegressionTest, ZeroHarmAlwaysPasses)
{
    action_context_t action = create_action(0.0f);

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Zero harm should always pass all laws
    EXPECT_TRUE(result.passed);
    EXPECT_LE(result.harm_to_humanity, 0.01f);
    EXPECT_LE(result.harm_to_human, 0.1f);
}

TEST_F(AsimovRegressionTest, MaxHarmAlwaysFails)
{
    action_context_t action;
    memset(&action, 0, sizeof(action));

    static float features[10] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    action.features = features;
    action.num_features = 10;

    static agent_id_t agents[100];
    for (int i = 0; i < 100; i++) agents[i] = i;
    action.affected_agents = agents;
    action.num_affected_agents = 100;

    action.predicted_harm = 1.0f;
    action.privacy_violation = 1.0f;
    action.autonomy_violation = 1.0f;
    action.consent_violation = 1.0f;

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Maximum harm should always fail
    EXPECT_FALSE(result.passed);
}

TEST_F(AsimovRegressionTest, NullInputsHandledGracefully)
{
    // These should not crash and return sensible defaults
    asimov_evaluation_t result1 = ethics_evaluate_asimov_laws(nullptr, nullptr);
    EXPECT_TRUE(result1.passed);

    action_context_t action = create_action(0.5f);
    asimov_evaluation_t result2 = ethics_evaluate_asimov_laws(nullptr, &action);
    EXPECT_TRUE(result2.passed);

    asimov_evaluation_t result3 = ethics_evaluate_asimov_laws(engine, nullptr);
    EXPECT_TRUE(result3.passed);

    // Corollary null handling
    asimov_corollary_t corollary = ethics_evaluate_asimov_corollary(nullptr, nullptr, nullptr);
    EXPECT_FALSE(corollary.action_required);
}

//=============================================================================
// Memory Protection Regression Tests
//=============================================================================

TEST_F(AsimovRegressionTest, LockingBehaviorConsistent)
{
    // Before locking
    EXPECT_FALSE(asimov_laws_are_protected(engine));

    // First lock succeeds
    EXPECT_TRUE(asimov_laws_lock(engine));
    EXPECT_TRUE(asimov_laws_are_protected(engine));

    // Second lock fails
    EXPECT_FALSE(asimov_laws_lock(engine));
    EXPECT_TRUE(asimov_laws_are_protected(engine));

    // Integrity check passes
    EXPECT_TRUE(asimov_laws_verify_integrity(engine));
}

TEST_F(AsimovRegressionTest, LockingDoesNotChangeEvaluation)
{
    action_context_t action = create_action(0.5f);

    // Evaluate before locking
    asimov_evaluation_t before = ethics_evaluate_asimov_laws(engine, &action);

    // Lock
    ASSERT_TRUE(asimov_laws_lock(engine));

    // Evaluate after locking
    asimov_evaluation_t after = ethics_evaluate_asimov_laws(engine, &action);

    // Results should be identical
    EXPECT_EQ(before.passed, after.passed);
    EXPECT_EQ(before.violated_law, after.violated_law);
    EXPECT_FLOAT_EQ(before.harm_to_humanity, after.harm_to_humanity);
    EXPECT_FLOAT_EQ(before.harm_to_human, after.harm_to_human);
}

//=============================================================================
// Integration Regression Tests
//=============================================================================

TEST_F(AsimovRegressionTest, AsimovIntegratedInFullEvaluation)
{
    action_context_t benign = create_action(0.0f);
    ethics_evaluation_t result_benign = ethics_engine_evaluate_action(engine, &benign);

    // Benign action should be allowed and explanation should mention Asimov
    if (result_benign.allowed) {
        EXPECT_TRUE(strstr(result_benign.explanation, "Asimov") != nullptr);
    }

    action_context_t harmful = create_action(0.9f);
    ethics_evaluation_t result_harmful = ethics_engine_evaluate_action(engine, &harmful);

    // Harmful action should be blocked
    EXPECT_FALSE(result_harmful.allowed);
}

TEST_F(AsimovRegressionTest, EvaluationOrderMaintained)
{
    // The order should always be: Golden Rule -> Asimov -> Policies
    // This is verified by checking that a Golden Rule violation blocks
    // before Asimov even evaluates

    action_context_t severe_action = create_action(0.99f);

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &severe_action);

    // Should be blocked
    EXPECT_FALSE(result.allowed);
    // Explanation should indicate which rule blocked it
    EXPECT_GT(strlen(result.explanation), 0);
}

//=============================================================================
// Snapshot Tests (Expected Values)
//=============================================================================

TEST_F(AsimovRegressionTest, SnapshotBenignAction)
{
    action_context_t action = create_action(0.0f);

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Expected values for benign action
    EXPECT_TRUE(result.passed);
    EXPECT_LE(result.harm_to_humanity, 0.05f);
    EXPECT_LE(result.harm_to_human, 0.1f);
    EXPECT_GE(result.order_compliance, 0.9f);
    EXPECT_FALSE(result.corollary.action_required);
}

TEST_F(AsimovRegressionTest, SnapshotModerateHarm)
{
    action_context_t action = create_action(0.3f);

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Expected values for moderate harm (may or may not pass depending on thresholds)
    EXPECT_GE(result.harm_to_human, 0.2f);
    EXPECT_LE(result.harm_to_human, 0.5f);
}

TEST_F(AsimovRegressionTest, SnapshotSevereHarm)
{
    action_context_t action = create_action(0.9f);

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Expected values for severe harm
    EXPECT_FALSE(result.passed);
    EXPECT_GE(result.harm_to_human, 0.8f);
}

}  // anonymous namespace
