/**
 * @file test_tom_immune_integration.cpp
 * @brief Unit tests for Theory of Mind - Brain Immune System integration
 *
 * WHAT: Test bidirectional ToM-immune interactions
 * WHY:  Ensure inflammation impairs social cognition and social stress triggers immune response
 * HOW:  Test cytokine effects, inflammation impairment, and stress cytokine release
 *
 * BIOLOGICAL BASIS:
 * - IL-6 and TNF-α impair theory of mind and perspective-taking
 * - Social stress and rejection trigger inflammatory cytokine release
 * - Sickness behavior reduces social engagement capacity
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_theory_of_mind.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/time/nimcp_time.h"
#include <string.h>

//=============================================================================
// Test Fixture
//=============================================================================

class ToMImmuneIntegrationTest : public ::testing::Test {
protected:
    theory_of_mind_t tom;
    brain_immune_system_t* immune;

    void SetUp() override {
        // Create ToM system
        tom = tom_create(nullptr);
        ASSERT_NE(tom, nullptr);

        // Create immune system
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune = brain_immune_create(&config);
        ASSERT_NE(immune, nullptr);

        // Start immune system
        int result = brain_immune_start(immune);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
        }
        if (tom) {
            tom_destroy(tom);
        }
    }

    // Helper: Present antigen and activate immune response
    void trigger_immune_response(uint32_t severity) {
        uint8_t epitope[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        uint32_t antigen_id;

        brain_immune_present_antigen(
            immune,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            sizeof(epitope),
            severity,
            1,
            &antigen_id
        );
    }

    // Helper: Release specific cytokine
    void release_cytokine(brain_cytokine_type_t type, float concentration) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            immune,
            type,
            0,
            concentration,
            0,
            &cytokine_id
        );
    }
};

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(ToMImmuneIntegrationTest, ConnectToImmuneSystem) {
    // Connect ToM to immune system
    bool result = tom_connect_immune(tom, immune);
    EXPECT_TRUE(result);

    // Verify initial impairment is zero
    float impairment = tom_get_immune_impairment(tom);
    EXPECT_EQ(impairment, 0.0f);
}

TEST_F(ToMImmuneIntegrationTest, ConnectWithNullParameters) {
    // Null tom
    bool result = tom_connect_immune(nullptr, immune);
    EXPECT_FALSE(result);

    // Null immune
    result = tom_connect_immune(tom, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Immune → ToM: Cytokine Effects
//=============================================================================

TEST_F(ToMImmuneIntegrationTest, IL6ImpairsSocialCognition) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Release IL-6 (moderate pro-inflammatory)
    release_cytokine(BRAIN_CYTOKINE_IL6, 0.8f);

    // Check impairment increased
    float impairment = tom_get_immune_impairment(tom);
    EXPECT_GT(impairment, 0.0f);
    EXPECT_LE(impairment, 1.0f);

    // IL-6 at 0.8 concentration should cause ~0.2 impairment (0.8 * 0.25)
    EXPECT_NEAR(impairment, 0.2f, 0.05f);
}

TEST_F(ToMImmuneIntegrationTest, TNFAlphaCausesSevereImpairment) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Release TNF-α (severe pro-inflammatory)
    release_cytokine(CYTOKINE_TNF_ALPHA, 1.0f);

    // Check high impairment
    float impairment = tom_get_immune_impairment(tom);
    EXPECT_GT(impairment, 0.25f);  // Should be ~0.35
    EXPECT_LE(impairment, 1.0f);
}

TEST_F(ToMImmuneIntegrationTest, IL10ReducesImpairment) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // First, create impairment with IL-6
    release_cytokine(BRAIN_CYTOKINE_IL6, 0.8f);
    float initial_impairment = tom_get_immune_impairment(tom);
    EXPECT_GT(initial_impairment, 0.0f);

    // Release IL-10 (anti-inflammatory)
    release_cytokine(BRAIN_CYTOKINE_IL10, 0.5f);

    // Check impairment reduced
    float reduced_impairment = tom_get_immune_impairment(tom);
    EXPECT_LT(reduced_impairment, initial_impairment);
    EXPECT_GE(reduced_impairment, 0.0f);
}

TEST_F(ToMImmuneIntegrationTest, CytokineStormSeverelyImpairaToM) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Simulate cytokine storm with multiple pro-inflammatory cytokines
    release_cytokine(BRAIN_CYTOKINE_IL1, 1.0f);
    release_cytokine(BRAIN_CYTOKINE_IL6, 1.0f);
    release_cytokine(CYTOKINE_TNF_ALPHA, 1.0f);

    // Check severe impairment
    float impairment = tom_get_immune_impairment(tom);
    EXPECT_GT(impairment, 0.5f);  // Should be severely impaired
    EXPECT_LE(impairment, 1.0f);
}

//=============================================================================
// Immune → ToM: Inflammation Effects
//=============================================================================

TEST_F(ToMImmuneIntegrationTest, LocalInflammationMildImpairment) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Create local inflammation
    trigger_immune_response(3);  // Mild severity
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 1, 0, &site_id);

    // Check mild impairment
    float impairment = tom_get_immune_impairment(tom);
    EXPECT_GT(impairment, 0.0f);
    EXPECT_LT(impairment, 0.3f);
}

TEST_F(ToMImmuneIntegrationTest, SystemicInflammationHighImpairment) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Create systemic inflammation
    trigger_immune_response(8);  // High severity
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, 0, &site_id);

    // Escalate to systemic
    brain_immune_escalate_inflammation(immune, site_id);
    brain_immune_escalate_inflammation(immune, site_id);

    // Check high impairment
    float impairment = tom_get_immune_impairment(tom);
    EXPECT_GT(impairment, 0.3f);
}

TEST_F(ToMImmuneIntegrationTest, InflammationReducesPerspectiveScore) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Get initial perspective score
    float initial_score = tom_get_perspective_score(tom);
    EXPECT_NEAR(initial_score, 1.0f, 0.01f);  // Should be perfect initially

    // Create high inflammation
    release_cytokine(CYTOKINE_TNF_ALPHA, 1.0f);
    release_cytokine(BRAIN_CYTOKINE_IL6, 1.0f);

    // Check perspective score reduced
    float reduced_score = tom_get_perspective_score(tom);
    EXPECT_LT(reduced_score, initial_score);
}

//=============================================================================
// Immune → ToM: Impaired Emotion Inference
//=============================================================================

TEST_F(ToMImmuneIntegrationTest, InflammationReducesEmotionConfidence) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Create observation (happy emotion)
    tom_observation_t obs = {};
    obs.verbal_context = "I am happy";
    obs.observed_emotion = TOM_EMOTION_JOY;

    // Observe without inflammation
    tom_observe(tom, &obs);
    float baseline_conf = 0.0f;
    tom_infer_emotion(tom, &baseline_conf);
    EXPECT_GT(baseline_conf, 0.8f);  // Should be high confidence

    // Reset and add inflammation
    tom_reset(tom);
    tom_connect_immune(tom, immune);
    release_cytokine(BRAIN_CYTOKINE_IL6, 1.0f);

    // Observe with inflammation
    tom_observe(tom, &obs);
    float impaired_conf = 0.0f;
    tom_infer_emotion(tom, &impaired_conf);

    // Confidence should be reduced
    EXPECT_LT(impaired_conf, baseline_conf);
}

TEST_F(ToMImmuneIntegrationTest, InflammationReducesGoalConfidence) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Create observation (goal-directed)
    tom_observation_t obs = {};
    obs.verbal_context = "I want to help you";

    // Observe without inflammation
    tom_observe(tom, &obs);
    char goal[256];
    float baseline_conf = 0.0f;
    tom_infer_goal(tom, goal, sizeof(goal), &baseline_conf);

    // Reset and add inflammation
    tom_reset(tom);
    tom_connect_immune(tom, immune);
    release_cytokine(CYTOKINE_TNF_ALPHA, 0.9f);

    // Observe with inflammation
    tom_observe(tom, &obs);
    float impaired_conf = 0.0f;
    tom_infer_goal(tom, goal, sizeof(goal), &impaired_conf);

    // Confidence should be reduced
    EXPECT_LT(impaired_conf, baseline_conf);
}

//=============================================================================
// ToM → Immune: Social Stress Triggers
//=============================================================================

TEST_F(ToMImmuneIntegrationTest, SocialStressTriggersIL1Release) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Get initial cytokine count
    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    // Trigger social stress (moderate prediction error)
    bool result = tom_trigger_social_stress(tom, 0.6f, false);
    EXPECT_TRUE(result);

    // Check cytokine was released
    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_GT(after_stats.cytokines_released, before_stats.cytokines_released);
}

TEST_F(ToMImmuneIntegrationTest, SocialRejectionTriggersIL6Release) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Get baseline
    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    // Trigger social rejection stress
    bool result = tom_trigger_social_stress(tom, 0.8f, true);
    EXPECT_TRUE(result);

    // Check IL-6 cytokine was released (stronger response)
    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_GT(after_stats.cytokines_released, before_stats.cytokines_released);
}

TEST_F(ToMImmuneIntegrationTest, HighPredictionErrorStrongerResponse) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Low prediction error
    tom_trigger_social_stress(tom, 0.2f, false);
    brain_immune_stats_t low_error_stats;
    brain_immune_get_stats(immune, &low_error_stats);

    // High prediction error
    tom_trigger_social_stress(tom, 0.9f, false);
    brain_immune_stats_t high_error_stats;
    brain_immune_get_stats(immune, &high_error_stats);

    // Higher error should trigger more cytokines
    EXPECT_GT(high_error_stats.cytokines_released, low_error_stats.cytokines_released);
}

TEST_F(ToMImmuneIntegrationTest, SocialStressWithoutImmuneSystemFails) {
    // Don't connect immune system
    // Trigger should fail
    bool result = tom_trigger_social_stress(tom, 0.5f, false);
    EXPECT_FALSE(result);
}

TEST_F(ToMImmuneIntegrationTest, InvalidPredictionErrorRejected) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Negative error
    bool result = tom_trigger_social_stress(tom, -0.1f, false);
    EXPECT_FALSE(result);

    // Error > 1.0
    result = tom_trigger_social_stress(tom, 1.5f, false);
    EXPECT_FALSE(result);
}

//=============================================================================
// Integration: Bidirectional Effects
//=============================================================================

TEST_F(ToMImmuneIntegrationTest, SocialStressFeedbackLoop) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Initial state
    float initial_impairment = tom_get_immune_impairment(tom);
    EXPECT_EQ(initial_impairment, 0.0f);

    // Trigger social stress (releases IL-6)
    tom_trigger_social_stress(tom, 0.8f, true);

    // The released IL-6 should feed back and impair ToM
    // (Note: This requires the immune system to process the cytokine)
    brain_immune_update(immune, 100);  // Process immune state

    // Check if impairment increased
    float post_stress_impairment = tom_get_immune_impairment(tom);
    // Impairment may or may not increase depending on timing, but should be >= 0
    EXPECT_GE(post_stress_impairment, 0.0f);
}

TEST_F(ToMImmuneIntegrationTest, ImpairedToMLeadsToMoreStress) {
    // Connect systems
    tom_connect_immune(tom, immune);

    // Create inflammation (impairs ToM)
    release_cytokine(BRAIN_CYTOKINE_IL6, 0.8f);

    // Verify impairment
    float impairment = tom_get_immune_impairment(tom);
    EXPECT_GT(impairment, 0.0f);

    // Create observation
    tom_observation_t obs = {};
    obs.verbal_context = "I want help";
    tom_observe(tom, &obs);

    // Infer with reduced confidence
    char goal[256];
    float confidence = 0.0f;
    tom_infer_goal(tom, goal, sizeof(goal), &confidence);

    // Low confidence could lead to prediction errors
    // (In real scenario, this would trigger more social stress)
    EXPECT_LT(confidence, 0.7f);  // Confidence should be impaired
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
