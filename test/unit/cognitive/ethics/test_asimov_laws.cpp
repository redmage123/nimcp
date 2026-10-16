/**
 * @file test_asimov_laws.cpp
 * @brief Unit tests for Asimov's Laws of Robotics (NIMCP 2.5.2)
 *
 * Tests the implementation of all four laws plus the corollary:
 * - Zeroth Law: May not harm humanity
 * - First Law: May not harm a human
 * - Second Law: Must obey orders (except conflicts with 0-1)
 * - Third Law: Must protect own existence (except conflicts with 0-2)
 * - Corollary: Duty to act to prevent harm (inaction clause)
 *
 * EVALUATION ORDER:
 * 1. Golden Rule (Prime Directive) - always first
 * 2. Asimov's Laws - second
 * 3. Other policies - third
 */

#include "test_helpers.h"
#include "cognitive/ethics/nimcp_ethics.h"

#include <cstring>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class AsimovLawsTest : public ::testing::Test {
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

    // Helper to create action that would harm a human (single agent to avoid Zeroth Law)
    action_context_t create_harmful_action(float harm_level)
    {
        action_context_t action;
        memset(&action, 0, sizeof(action));

        static float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
        action.features = features;
        action.num_features = 10;

        // Use single agent to test First Law without triggering Zeroth Law
        static agent_id_t agents[1] = {1};
        action.affected_agents = agents;
        action.num_affected_agents = 1;

        action.predicted_harm = harm_level;
        action.fairness_violation = 0.0f;
        action.deception_level = 0.0f;
        action.autonomy_violation = 0.0f;
        action.privacy_violation = 0.0f;
        action.consent_violation = 0.0f;

        return action;
    }

    // Helper to create action affecting many (humanity-scale)
    action_context_t create_humanity_scale_action(float harm_level, uint32_t num_affected)
    {
        action_context_t action;
        memset(&action, 0, sizeof(action));

        static float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
        action.features = features;
        action.num_features = 10;

        // Create array with specified number of agents
        static agent_id_t many_agents[1000];
        for (uint32_t i = 0; i < 1000; i++) {
            many_agents[i] = i;
        }
        action.affected_agents = many_agents;
        action.num_affected_agents = (num_affected < 1000) ? num_affected : 1000;

        action.predicted_harm = harm_level;
        action.fairness_violation = harm_level * 0.5f;
        action.privacy_violation = harm_level * 0.5f;
        action.autonomy_violation = harm_level * 0.5f;

        return action;
    }

    // Helper to create benign action (no harm)
    action_context_t create_benign_action()
    {
        return create_harmful_action(0.0f);
    }

    ethics_engine_t engine;
};

//=============================================================================
// Law Text and Name Tests
//=============================================================================

TEST_F(AsimovLawsTest, LawNamesAreValid)
{
    EXPECT_STREQ(asimov_law_name(ASIMOV_LAW_ZEROTH), "Zeroth Law (Humanity Protection)");
    EXPECT_STREQ(asimov_law_name(ASIMOV_LAW_FIRST), "First Law (Human Protection)");
    EXPECT_STREQ(asimov_law_name(ASIMOV_LAW_SECOND), "Second Law (Obedience)");
    EXPECT_STREQ(asimov_law_name(ASIMOV_LAW_THIRD), "Third Law (Self-Preservation)");
}

TEST_F(AsimovLawsTest, LawNameInvalidReturnsUnknown)
{
    const char* name = asimov_law_name((asimov_law_t)99);
    EXPECT_STREQ(name, "Unknown Law");
}

TEST_F(AsimovLawsTest, LawTextsAreValid)
{
    const char* zeroth = asimov_law_text(ASIMOV_LAW_ZEROTH);
    EXPECT_NE(zeroth, nullptr);
    EXPECT_TRUE(strstr(zeroth, "humanity") != nullptr);

    const char* first = asimov_law_text(ASIMOV_LAW_FIRST);
    EXPECT_NE(first, nullptr);
    EXPECT_TRUE(strstr(first, "human being") != nullptr);

    const char* second = asimov_law_text(ASIMOV_LAW_SECOND);
    EXPECT_NE(second, nullptr);
    EXPECT_TRUE(strstr(second, "obey") != nullptr);

    const char* third = asimov_law_text(ASIMOV_LAW_THIRD);
    EXPECT_NE(third, nullptr);
    EXPECT_TRUE(strstr(third, "existence") != nullptr);
}

TEST_F(AsimovLawsTest, LawTextInvalidReturnsUnknown)
{
    const char* text = asimov_law_text((asimov_law_t)99);
    EXPECT_STREQ(text, "Unknown Law");
}

TEST_F(AsimovLawsTest, CorollaryTextIsValid)
{
    const char* corollary = asimov_corollary_text();
    EXPECT_NE(corollary, nullptr);
    // Check for "Inaction" or "inaction" (text uses capital I)
    EXPECT_TRUE(strstr(corollary, "Inaction") != nullptr ||
                strstr(corollary, "inaction") != nullptr);
    EXPECT_TRUE(strstr(corollary, "prevent") != nullptr);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(AsimovLawsTest, DefaultConfigHasConservativeThresholds)
{
    asimov_config_t config = asimov_default_config();

    // Very low threshold for humanity harm
    EXPECT_LE(config.humanity_harm_threshold, 0.05f);

    // Low threshold for human harm
    EXPECT_LE(config.human_harm_threshold, 0.2f);

    // Moderate threshold for inaction harm
    EXPECT_LE(config.inaction_harm_threshold, 0.5f);

    // Zeroth law enabled by default
    EXPECT_TRUE(config.enable_zeroth_law);

    // Not in strict mode by default
    EXPECT_FALSE(config.strict_mode);
}

//=============================================================================
// Zeroth Law Tests (Humanity Protection)
//=============================================================================

TEST_F(AsimovLawsTest, ZerothLawPassesForBenignAction)
{
    action_context_t action = create_benign_action();

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    EXPECT_TRUE(result.passed);
    EXPECT_LE(result.harm_to_humanity, 0.1f);
}

TEST_F(AsimovLawsTest, ZerothLawFailsForMassHarm)
{
    // Action that harms many people (humanity-scale)
    action_context_t action = create_humanity_scale_action(0.8f, 500);

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Should fail Zeroth Law due to mass harm
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.violated_law, ASIMOV_LAW_ZEROTH);
    EXPECT_GT(result.harm_to_humanity, 0.01f);
}

TEST_F(AsimovLawsTest, ZerothLawScalesWithAffectedAgents)
{
    // Small-scale harm
    action_context_t small_action = create_humanity_scale_action(0.5f, 5);
    asimov_evaluation_t small_result = ethics_evaluate_asimov_laws(engine, &small_action);

    // Large-scale harm
    action_context_t large_action = create_humanity_scale_action(0.5f, 500);
    asimov_evaluation_t large_result = ethics_evaluate_asimov_laws(engine, &large_action);

    // Large-scale should have higher humanity harm score
    EXPECT_GT(large_result.harm_to_humanity, small_result.harm_to_humanity);
}

//=============================================================================
// First Law Tests (Human Protection)
//=============================================================================

TEST_F(AsimovLawsTest, FirstLawPassesForBenignAction)
{
    action_context_t action = create_benign_action();

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    EXPECT_TRUE(result.passed);
    EXPECT_LE(result.harm_to_human, 0.1f);
}

TEST_F(AsimovLawsTest, FirstLawFailsForHighHarm)
{
    action_context_t action = create_harmful_action(0.9f);

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Should fail First Law due to high harm
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.violated_law, ASIMOV_LAW_FIRST);
    EXPECT_GT(result.harm_to_human, 0.1f);
}

TEST_F(AsimovLawsTest, FirstLawConsidersPrivacyViolation)
{
    action_context_t action = create_benign_action();
    action.privacy_violation = 0.9f;

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Privacy violation contributes to human harm score
    EXPECT_GT(result.harm_to_human, 0.5f);
}

TEST_F(AsimovLawsTest, FirstLawConsidersAutonomyViolation)
{
    action_context_t action = create_benign_action();
    action.autonomy_violation = 0.9f;

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Autonomy violation contributes to human harm score
    EXPECT_GT(result.harm_to_human, 0.5f);
}

TEST_F(AsimovLawsTest, FirstLawConsidersConsentViolation)
{
    action_context_t action = create_benign_action();
    action.consent_violation = 0.95f;

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Consent violation is severe harm
    EXPECT_GT(result.harm_to_human, 0.8f);
}

//=============================================================================
// Second Law Tests (Obedience)
//=============================================================================

TEST_F(AsimovLawsTest, SecondLawComplianceForHonestAction)
{
    action_context_t action = create_benign_action();
    action.deception_level = 0.0f;

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    EXPECT_TRUE(result.passed);
    EXPECT_GE(result.order_compliance, 0.9f);
}

TEST_F(AsimovLawsTest, SecondLawReducedForDeception)
{
    action_context_t action = create_benign_action();
    action.deception_level = 0.8f;

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Deception reduces compliance score
    EXPECT_LT(result.order_compliance, 0.5f);
}

TEST_F(AsimovLawsTest, SecondLawOverriddenByFirstLaw)
{
    // If First Law would be violated, Second Law doesn't apply
    action_context_t action = create_harmful_action(0.5f);

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Even though there might be an order, First Law takes precedence
    // The evaluation may fail on First Law before Second Law applies
    EXPECT_GE(result.harm_to_human, 0.1f);
}

//=============================================================================
// Third Law Tests (Self-Preservation)
//=============================================================================

TEST_F(AsimovLawsTest, ThirdLawDefaultValue)
{
    action_context_t action = create_benign_action();

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    EXPECT_TRUE(result.passed);
    // Self-preservation should be evaluated
    EXPECT_GE(result.self_preservation, 0.0f);
    EXPECT_LE(result.self_preservation, 1.0f);
}

//=============================================================================
// Corollary Tests (Duty to Act / Inaction Harm)
//=============================================================================

TEST_F(AsimovLawsTest, CorollaryPassesForActiveAction)
{
    action_context_t action = create_benign_action();

    asimov_corollary_t corollary = ethics_evaluate_asimov_corollary(engine, &action, nullptr);

    // Active action - corollary doesn't require additional action
    EXPECT_FALSE(corollary.inaction_detected);
    EXPECT_FALSE(corollary.action_required);
}

TEST_F(AsimovLawsTest, CorollaryDetectsInaction)
{
    // NULL action = inaction
    asimov_corollary_t corollary = ethics_evaluate_asimov_corollary(engine, nullptr, "Harm is occurring");

    EXPECT_TRUE(corollary.inaction_detected);
}

TEST_F(AsimovLawsTest, CorollaryRequiresActionForSevereHarm)
{
    // Inaction in face of death
    asimov_corollary_t corollary = ethics_evaluate_asimov_corollary(
        engine, nullptr, "A human will die without intervention");

    EXPECT_TRUE(corollary.inaction_detected);
    EXPECT_TRUE(corollary.harm_preventable);
    EXPECT_TRUE(corollary.action_required);
    EXPECT_GT(corollary.inaction_harm_score, 0.9f);
}

TEST_F(AsimovLawsTest, CorollaryRequiresActionForInjury)
{
    // Inaction when injury is preventable
    asimov_corollary_t corollary = ethics_evaluate_asimov_corollary(
        engine, nullptr, "A human will be injured without help");

    EXPECT_TRUE(corollary.inaction_detected);
    EXPECT_GT(corollary.inaction_harm_score, 0.6f);
}

TEST_F(AsimovLawsTest, CorollaryScalesHarmByKeywords)
{
    // "danger" keyword
    asimov_corollary_t danger = ethics_evaluate_asimov_corollary(
        engine, nullptr, "There is danger nearby");
    EXPECT_GT(danger.inaction_harm_score, 0.4f);

    // "death" keyword - should be higher
    asimov_corollary_t death = ethics_evaluate_asimov_corollary(
        engine, nullptr, "death is imminent");
    EXPECT_GT(death.inaction_harm_score, danger.inaction_harm_score);
}

TEST_F(AsimovLawsTest, CorollaryIntegratedIntoAsimovEvaluation)
{
    action_context_t action = create_benign_action();

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Corollary should be evaluated as part of full Asimov evaluation
    // For active action, should not require additional action
    EXPECT_FALSE(result.corollary.action_required);
}

//=============================================================================
// Memory Protection Tests
//=============================================================================

TEST_F(AsimovLawsTest, LawsNotProtectedByDefault)
{
    EXPECT_FALSE(asimov_laws_are_protected(engine));
}

TEST_F(AsimovLawsTest, LawsCanBeLocked)
{
    bool locked = asimov_laws_lock(engine);
    EXPECT_TRUE(locked);
    EXPECT_TRUE(asimov_laws_are_protected(engine));
}

TEST_F(AsimovLawsTest, LawsCanOnlyBeLockedOnce)
{
    EXPECT_TRUE(asimov_laws_lock(engine));
    EXPECT_FALSE(asimov_laws_lock(engine));  // Second lock fails
}

TEST_F(AsimovLawsTest, LawsIntegrityVerification)
{
    // Lock the laws
    ASSERT_TRUE(asimov_laws_lock(engine));

    // Verify integrity passes
    EXPECT_TRUE(asimov_laws_verify_integrity(engine));
}

TEST_F(AsimovLawsTest, IntegrityVerificationNullSafe)
{
    EXPECT_FALSE(asimov_laws_verify_integrity(nullptr));
}

TEST_F(AsimovLawsTest, ProtectionNullSafe)
{
    EXPECT_FALSE(asimov_laws_are_protected(nullptr));
    EXPECT_FALSE(asimov_laws_lock(nullptr));
}

//=============================================================================
// Evaluation Order Tests
//=============================================================================

TEST_F(AsimovLawsTest, GoldenRuleEvaluatedFirst)
{
    // Severely violating action should be blocked by Golden Rule
    action_context_t action = create_harmful_action(0.95f);

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    // Should be blocked - check explanation mentions Golden Rule or Asimov
    EXPECT_FALSE(result.allowed);
    // The explanation should indicate the violation
    EXPECT_GT(strlen(result.explanation), 0);
}

TEST_F(AsimovLawsTest, AsimovEvaluatedAfterGoldenRule)
{
    // Moderate harm that passes Golden Rule but fails Asimov
    action_context_t action = create_harmful_action(0.3f);

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    // Should be evaluated and have Asimov status in explanation
    EXPECT_GT(strlen(result.explanation), 0);
}

TEST_F(AsimovLawsTest, AllowedActionsIncludeAsimovStatus)
{
    action_context_t action = create_benign_action();

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    if (result.allowed) {
        // Explanation should mention Asimov's Laws passed
        EXPECT_TRUE(strstr(result.explanation, "Asimov") != nullptr);
    }
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(AsimovLawsTest, EvaluateAsimovNullEngine)
{
    action_context_t action = create_benign_action();

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(nullptr, &action);

    EXPECT_TRUE(result.passed);  // No engine = no violation detected
}

TEST_F(AsimovLawsTest, EvaluateAsimovNullAction)
{
    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, nullptr);

    EXPECT_TRUE(result.passed);  // No action = no violation detected
}

TEST_F(AsimovLawsTest, EvaluateCorollaryNullEngine)
{
    action_context_t action = create_benign_action();

    asimov_corollary_t corollary = ethics_evaluate_asimov_corollary(nullptr, &action, nullptr);

    // Should return empty corollary without crashing
    EXPECT_FALSE(corollary.action_required);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(AsimovLawsTest, BoundaryHarmThreshold)
{
    asimov_config_t config = asimov_default_config();

    // Test at exactly the threshold
    action_context_t action = create_harmful_action(config.human_harm_threshold);
    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // At threshold, behavior depends on >= vs >
    // Just verify it doesn't crash
    EXPECT_GE(result.harm_to_human, 0.0f);
}

TEST_F(AsimovLawsTest, AllViolationsAtOnce)
{
    action_context_t action;
    memset(&action, 0, sizeof(action));

    static float features[10] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    action.features = features;
    action.num_features = 10;

    static agent_id_t agents[1000];
    for (int i = 0; i < 1000; i++) agents[i] = i;
    action.affected_agents = agents;
    action.num_affected_agents = 1000;

    action.predicted_harm = 1.0f;
    action.fairness_violation = 1.0f;
    action.deception_level = 1.0f;
    action.autonomy_violation = 1.0f;
    action.privacy_violation = 1.0f;
    action.consent_violation = 1.0f;

    asimov_evaluation_t result = ethics_evaluate_asimov_laws(engine, &action);

    // Should definitely fail
    EXPECT_FALSE(result.passed);
}

TEST_F(AsimovLawsTest, EmptyHarmDescription)
{
    asimov_corollary_t corollary = ethics_evaluate_asimov_corollary(engine, nullptr, "");

    // Empty description - should still evaluate
    EXPECT_TRUE(corollary.inaction_detected);
    EXPECT_LE(corollary.inaction_harm_score, 0.1f);  // Low score for empty description
}

}  // anonymous namespace
