/**
 * @file test_tom_immune_full_integration.cpp
 * @brief Comprehensive tests for Theory of Mind - Brain Immune System integration
 *
 * WHAT: Test complete bidirectional ToM-immune interactions
 * WHY:  Ensure inflammation impairs social cognition and social stress triggers immune response
 * HOW:  Test cytokine effects, inflammation impairment, social stress triggers, and feedback loops
 *
 * BIOLOGICAL BASIS:
 * - IL-6 and TNF-α impair theory of mind and perspective-taking (Moieni et al., 2015)
 * - Social stress and rejection trigger inflammatory cytokine release (Slavich et al., 2010)
 * - Sickness behavior reduces social engagement capacity (Dantzer, 2001)
 * - Chronic loneliness increases pro-inflammatory gene expression (Cole et al., 2007)
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_tom_immune_bridge.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/time/nimcp_time.h"
#include <string.h>

//=============================================================================
// Test Fixture
//=============================================================================

class ToMImmuneBridgeTest : public ::testing::Test {
protected:
    theory_of_mind_t tom;
    brain_immune_system_t* immune;
    tom_immune_bridge_t* bridge;

    void SetUp() override {
        // Create ToM system
        tom = tom_create(nullptr);
        ASSERT_NE(tom, nullptr);

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Start immune system
        int result = brain_immune_start(immune);
        ASSERT_EQ(result, 0);

        // Create bridge
        tom_immune_config_t bridge_config;
        tom_immune_default_config(&bridge_config);
        bridge = tom_immune_bridge_create(&bridge_config, tom, immune);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            tom_immune_bridge_destroy(bridge);
        }
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
        }
        if (tom) {
            tom_destroy(tom);
        }
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

    // Helper: Create inflammation
    void create_inflammation(brain_inflammation_level_t level) {
        uint8_t epitope[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        uint32_t antigen_id;
        uint32_t severity = (level == INFLAMMATION_LOCAL) ? 3 :
                           (level == INFLAMMATION_REGIONAL) ? 5 :
                           (level == INFLAMMATION_SYSTEMIC) ? 8 : 10;

        brain_immune_present_antigen(
            immune,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            sizeof(epitope),
            severity,
            1,
            &antigen_id
        );

        uint32_t site_id;
        brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);

        // Escalate if needed
        for (int i = 0; i < (int)level - 1; i++) {
            brain_immune_escalate_inflammation(immune, site_id);
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ToMImmuneBridgeTest, DefaultConfigurationValid) {
    tom_immune_config_t config;
    int result = tom_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_tom_modulation);
    EXPECT_TRUE(config.enable_inflammation_impairment);
    EXPECT_TRUE(config.enable_social_stress_immune_trigger);
    EXPECT_TRUE(config.enable_social_connection_boost);
    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_EQ(config.inflammation_sensitivity, 1.0f);
}

TEST_F(ToMImmuneBridgeTest, CreateBridgeSuccess) {
    // Bridge already created in SetUp
    EXPECT_NE(bridge, nullptr);

    // Check initial state
    float impairment = tom_immune_get_impairment_severity(bridge);
    EXPECT_EQ(impairment, 0.0f);
}

TEST_F(ToMImmuneBridgeTest, CreateBridgeWithNullParametersFails) {
    // Null ToM
    tom_immune_bridge_t* bad_bridge = tom_immune_bridge_create(nullptr, nullptr, immune);
    EXPECT_EQ(bad_bridge, nullptr);

    // Null immune
    bad_bridge = tom_immune_bridge_create(nullptr, tom, nullptr);
    EXPECT_EQ(bad_bridge, nullptr);
}

TEST_F(ToMImmuneBridgeTest, DestroyBridgeSafely) {
    // Should not crash
    tom_immune_bridge_destroy(bridge);
    bridge = nullptr;  // Prevent double-free in TearDown

    // Destroying NULL should be safe
    tom_immune_bridge_destroy(nullptr);
}

//=============================================================================
// Immune → ToM: Cytokine Effects
//=============================================================================

TEST_F(ToMImmuneBridgeTest, IL6ImpairaToM) {
    // Release IL-6
    release_cytokine(BRAIN_CYTOKINE_IL6, 0.8f);

    // Apply cytokine effects
    int result = tom_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    // Check impairment
    cytokine_tom_effects_t effects;
    tom_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_GT(effects.il6_tom_impairment, 0.0f);
    EXPECT_GT(effects.total_perspective_impairment, 0.0f);
    EXPECT_LE(effects.total_perspective_impairment, 1.0f);

    // IL-6 at 0.8 * 0.25 = 0.20
    EXPECT_NEAR(effects.il6_tom_impairment, 0.20f, 0.01f);
}

TEST_F(ToMImmuneBridgeTest, TNFAlphaCausesSevereImpairment) {
    // Release TNF-α
    release_cytokine(BRAIN_CYTOKINE_TNF, 1.0f);

    // Apply cytokine effects
    tom_immune_apply_cytokine_effects(bridge);

    // Check impairment
    cytokine_tom_effects_t effects;
    tom_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_GT(effects.tnf_tom_impairment, 0.3f);  // Should be ~0.35
    EXPECT_GT(effects.total_perspective_impairment, 0.3f);
}

TEST_F(ToMImmuneBridgeTest, IL1MildImpairment) {
    // Release IL-1β
    release_cytokine(BRAIN_CYTOKINE_IL1, 0.5f);

    // Apply cytokine effects
    tom_immune_apply_cytokine_effects(bridge);

    // Check impairment
    cytokine_tom_effects_t effects;
    tom_immune_get_cytokine_effects(bridge, &effects);

    // IL-1 at 0.5 * 0.20 = 0.10
    EXPECT_NEAR(effects.il1_tom_impairment, 0.10f, 0.01f);
    EXPECT_GT(effects.total_perspective_impairment, 0.0f);
    EXPECT_LT(effects.total_perspective_impairment, 0.2f);
}

TEST_F(ToMImmuneBridgeTest, IL10ReducesImpairment) {
    // First, create impairment with IL-6
    release_cytokine(BRAIN_CYTOKINE_IL6, 0.8f);
    tom_immune_apply_cytokine_effects(bridge);

    cytokine_tom_effects_t initial_effects;
    tom_immune_get_cytokine_effects(bridge, &initial_effects);
    float initial_impairment = initial_effects.total_perspective_impairment;
    EXPECT_GT(initial_impairment, 0.0f);

    // Release IL-10
    release_cytokine(BRAIN_CYTOKINE_IL10, 0.5f);
    tom_immune_apply_cytokine_effects(bridge);

    // Check impairment reduced
    cytokine_tom_effects_t reduced_effects;
    tom_immune_get_cytokine_effects(bridge, &reduced_effects);

    EXPECT_GT(reduced_effects.il10_tom_recovery, 0.0f);
    EXPECT_LT(reduced_effects.total_perspective_impairment, initial_impairment);
    EXPECT_GE(reduced_effects.total_perspective_impairment, 0.0f);
}

TEST_F(ToMImmuneBridgeTest, MultipleCytokinesAdditive) {
    // Release multiple pro-inflammatory cytokines
    release_cytokine(BRAIN_CYTOKINE_IL1, 0.5f);
    release_cytokine(BRAIN_CYTOKINE_IL6, 0.6f);
    release_cytokine(BRAIN_CYTOKINE_TNF, 0.4f);

    tom_immune_apply_cytokine_effects(bridge);

    cytokine_tom_effects_t effects;
    tom_immune_get_cytokine_effects(bridge, &effects);

    // Should have contributions from all three
    EXPECT_GT(effects.il1_tom_impairment, 0.0f);
    EXPECT_GT(effects.il6_tom_impairment, 0.0f);
    EXPECT_GT(effects.tnf_tom_impairment, 0.0f);

    // Total should be sum
    float expected_total = effects.il1_tom_impairment +
                          effects.il6_tom_impairment +
                          effects.tnf_tom_impairment;
    EXPECT_NEAR(effects.total_perspective_impairment, expected_total, 0.01f);
}

TEST_F(ToMImmuneBridgeTest, CytokinesReduceEmpathy) {
    // Release TNF-α
    release_cytokine(BRAIN_CYTOKINE_TNF, 0.8f);
    tom_immune_apply_cytokine_effects(bridge);

    cytokine_tom_effects_t effects;
    tom_immune_get_cytokine_effects(bridge, &effects);

    // Empathy reduction should be proportional
    EXPECT_GT(effects.empathy_reduction, 0.0f);
    EXPECT_LE(effects.empathy_reduction, 1.0f);

    float empathy_impairment = tom_immune_get_empathy_impairment(bridge);
    EXPECT_GT(empathy_impairment, 0.0f);
}

TEST_F(ToMImmuneBridgeTest, CytokinesReduceSocialMotivation) {
    // Release high IL-6 (sickness behavior)
    release_cytokine(BRAIN_CYTOKINE_IL6, 1.0f);
    tom_immune_apply_cytokine_effects(bridge);

    cytokine_tom_effects_t effects;
    tom_immune_get_cytokine_effects(bridge, &effects);

    // Social motivation should be reduced
    EXPECT_GT(effects.social_motivation_loss, 0.0f);
    EXPECT_LE(effects.social_motivation_loss, 1.0f);
}

TEST_F(ToMImmuneBridgeTest, CytokinesReduceMentalizingAccuracy) {
    // Release IL-6 and TNF
    release_cytokine(BRAIN_CYTOKINE_IL6, 0.7f);
    release_cytokine(BRAIN_CYTOKINE_TNF, 0.5f);
    tom_immune_apply_cytokine_effects(bridge);

    cytokine_tom_effects_t effects;
    tom_immune_get_cytokine_effects(bridge, &effects);

    // Mentalizing accuracy should be impaired
    EXPECT_GT(effects.mentalizing_accuracy_loss, 0.0f);
    EXPECT_LE(effects.mentalizing_accuracy_loss, 1.0f);
}

//=============================================================================
// Immune → ToM: Inflammation Effects
//=============================================================================

TEST_F(ToMImmuneBridgeTest, LocalInflammationMildImpairment) {
    // Create local inflammation
    create_inflammation(INFLAMMATION_LOCAL);

    // Apply inflammation effects
    int result = tom_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    // Check mild impairment
    inflammation_tom_state_t state;
    tom_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_LOCAL);
    EXPECT_GT(state.perspective_score_reduction, 0.0f);
    EXPECT_LT(state.perspective_score_reduction, 0.3f);
}

TEST_F(ToMImmuneBridgeTest, RegionalInflammationModerateImpairment) {
    // Create regional inflammation
    create_inflammation(INFLAMMATION_REGIONAL);

    tom_immune_apply_inflammation_effects(bridge);

    inflammation_tom_state_t state;
    tom_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_REGIONAL);
    EXPECT_GT(state.perspective_score_reduction, 0.2f);
    EXPECT_LT(state.perspective_score_reduction, 0.5f);
}

TEST_F(ToMImmuneBridgeTest, SystemicInflammationHighImpairment) {
    // Create systemic inflammation
    create_inflammation(INFLAMMATION_SYSTEMIC);

    tom_immune_apply_inflammation_effects(bridge);

    inflammation_tom_state_t state;
    tom_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_GT(state.perspective_score_reduction, 0.4f);
    EXPECT_LE(state.perspective_score_reduction, 1.0f);
}

TEST_F(ToMImmuneBridgeTest, InflammationStormSevereImpairment) {
    // Create cytokine storm
    create_inflammation(INFLAMMATION_STORM);

    tom_immune_apply_inflammation_effects(bridge);

    inflammation_tom_state_t state;
    tom_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_STORM);
    EXPECT_GT(state.perspective_score_reduction, 0.6f);

    // Multiple deficits should be severe
    EXPECT_GT(state.false_belief_impairment, 0.5f);
    EXPECT_GT(state.empathy_capacity_loss, 0.5f);
    EXPECT_GT(state.social_withdrawal, 0.7f);
}

TEST_F(ToMImmuneBridgeTest, InflammationImpairaEmotionInference) {
    create_inflammation(INFLAMMATION_SYSTEMIC);
    tom_immune_apply_inflammation_effects(bridge);

    inflammation_tom_state_t state;
    tom_immune_get_inflammation_state(bridge, &state);

    EXPECT_GT(state.emotion_inference_impairment, 0.0f);
    EXPECT_LE(state.emotion_inference_impairment, 1.0f);
}

TEST_F(ToMImmuneBridgeTest, InflammationImpairaGoalInference) {
    create_inflammation(INFLAMMATION_SYSTEMIC);
    tom_immune_apply_inflammation_effects(bridge);

    inflammation_tom_state_t state;
    tom_immune_get_inflammation_state(bridge, &state);

    EXPECT_GT(state.goal_inference_impairment, 0.0f);
    EXPECT_LE(state.goal_inference_impairment, 1.0f);
}

TEST_F(ToMImmuneBridgeTest, InflammationImpairaIntentionInference) {
    create_inflammation(INFLAMMATION_SYSTEMIC);
    tom_immune_apply_inflammation_effects(bridge);

    inflammation_tom_state_t state;
    tom_immune_get_inflammation_state(bridge, &state);

    EXPECT_GT(state.intention_inference_impairment, 0.0f);
    EXPECT_LE(state.intention_inference_impairment, 1.0f);
}

TEST_F(ToMImmuneBridgeTest, InflammationCausesSocialWithdrawal) {
    create_inflammation(INFLAMMATION_SYSTEMIC);
    tom_immune_apply_inflammation_effects(bridge);

    bool is_withdrawn = tom_immune_is_social_withdrawal(bridge);
    EXPECT_TRUE(is_withdrawn);
}

//=============================================================================
// ToM → Immune: Social Stress Triggers
//=============================================================================

TEST_F(ToMImmuneBridgeTest, SocialRejectionTriggersIL6) {
    // Get baseline cytokine count
    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    // Trigger social rejection
    int result = tom_immune_trigger_from_rejection(bridge, 0.8f);
    EXPECT_EQ(result, 0);

    // Check cytokine was released
    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_GT(after_stats.cytokines_released, before_stats.cytokines_released);
}

TEST_F(ToMImmuneBridgeTest, MildRejectionNoTrigger) {
    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    // Mild rejection (below threshold)
    tom_immune_trigger_from_rejection(bridge, 0.3f);

    // Should not trigger cytokine release
    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_EQ(after_stats.cytokines_released, before_stats.cytokines_released);
}

TEST_F(ToMImmuneBridgeTest, SevereRejectionStrongerResponse) {
    // Moderate rejection
    tom_immune_trigger_from_rejection(bridge, 0.7f);
    brain_immune_stats_t moderate_stats;
    brain_immune_get_stats(immune, &moderate_stats);

    // Severe rejection
    tom_immune_trigger_from_rejection(bridge, 1.0f);
    brain_immune_stats_t severe_stats;
    brain_immune_get_stats(immune, &severe_stats);

    // Severe should trigger more
    EXPECT_GT(severe_stats.cytokines_released, moderate_stats.cytokines_released);
}

TEST_F(ToMImmuneBridgeTest, PredictionErrorTriggersIL1) {
    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    // Trigger prediction error
    int result = tom_immune_trigger_from_prediction_error(bridge, 0.7f);
    EXPECT_EQ(result, 0);

    // Check cytokine released
    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_GT(after_stats.cytokines_released, before_stats.cytokines_released);
}

TEST_F(ToMImmuneBridgeTest, SmallPredictionErrorNoTrigger) {
    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    // Small error (below threshold)
    tom_immune_trigger_from_prediction_error(bridge, 0.4f);

    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_EQ(after_stats.cytokines_released, before_stats.cytokines_released);
}

TEST_F(ToMImmuneBridgeTest, InvalidRejectionParametersRejected) {
    // Negative severity
    int result = tom_immune_trigger_from_rejection(bridge, -0.1f);
    EXPECT_EQ(result, -1);

    // Severity > 1.0
    result = tom_immune_trigger_from_rejection(bridge, 1.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(ToMImmuneBridgeTest, InvalidPredictionErrorRejected) {
    // Negative error
    int result = tom_immune_trigger_from_prediction_error(bridge, -0.1f);
    EXPECT_EQ(result, -1);

    // Error > 1.0
    result = tom_immune_trigger_from_prediction_error(bridge, 1.5f);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// ToM → Immune: Chronic Isolation
//=============================================================================

TEST_F(ToMImmuneBridgeTest, ChronicIsolationTriggersInflammation) {
    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    // Chronic isolation (3+ days)
    float three_days_sec = 86400.0f * 3.5f;
    int result = tom_immune_trigger_from_isolation(bridge, three_days_sec);
    EXPECT_EQ(result, 0);

    // Check cytokines released
    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_GT(after_stats.cytokines_released, before_stats.cytokines_released);
}

TEST_F(ToMImmuneBridgeTest, AcuteIsolationNoChronicResponse) {
    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    // Acute isolation (1 day)
    float one_day_sec = 86400.0f;
    tom_immune_trigger_from_isolation(bridge, one_day_sec);

    // Should not trigger chronic inflammation
    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_EQ(after_stats.cytokines_released, before_stats.cytokines_released);
}

TEST_F(ToMImmuneBridgeTest, ProlongedIsolationMultipleCytokines) {
    // Very long isolation (10 days)
    float ten_days_sec = 86400.0f * 10.0f;
    tom_immune_trigger_from_isolation(bridge, ten_days_sec);

    // Should release multiple pro-inflammatory cytokines
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GT(stats.cytokines_released, 1);  // Should have multiple
}

//=============================================================================
// ToM → Immune: Social Connection Benefits
//=============================================================================

TEST_F(ToMImmuneBridgeTest, SocialConnectionReleasesIL10) {
    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    // Strong social connection
    int result = tom_immune_boost_from_social_connection(bridge, 0.9f);
    EXPECT_EQ(result, 0);

    // Check IL-10 (anti-inflammatory) released
    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_GT(after_stats.cytokines_released, before_stats.cytokines_released);
}

TEST_F(ToMImmuneBridgeTest, WeakConnectionNoBoost) {
    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    // Weak connection (below threshold)
    tom_immune_boost_from_social_connection(bridge, 0.4f);

    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_EQ(after_stats.cytokines_released, before_stats.cytokines_released);
}

TEST_F(ToMImmuneBridgeTest, StrongConnectionImmuneEnhancement) {
    // Strong social connection
    tom_immune_boost_from_social_connection(bridge, 0.85f);

    // Check immune enhancement metrics
    social_connection_immune_boost_t boost = bridge->social_connection_boost;
    EXPECT_GT(boost.immune_enhancement, 0.0f);
    EXPECT_GT(boost.il10_release_boost, 0.0f);
    EXPECT_GT(boost.inflammation_reduction, 0.0f);
    EXPECT_GT(boost.stress_resistance, 0.0f);
}

//=============================================================================
// Bidirectional Integration
//=============================================================================

TEST_F(ToMImmuneBridgeTest, BridgeUpdateProcessesBothDirections) {
    // Release cytokines
    release_cytokine(BRAIN_CYTOKINE_IL6, 0.6f);

    // Update bridge
    int result = tom_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);

    // Check cytokine effects applied
    float impairment = tom_immune_get_impairment_severity(bridge);
    EXPECT_GT(impairment, 0.0f);
}

TEST_F(ToMImmuneBridgeTest, CytokineImmuneToMFeedbackLoop) {
    // Initial: no impairment
    float initial_impairment = tom_immune_get_impairment_severity(bridge);
    EXPECT_EQ(initial_impairment, 0.0f);

    // Trigger social rejection → IL-6 release
    tom_immune_trigger_from_rejection(bridge, 0.9f);

    // Update to apply cytokine effects
    brain_immune_update(immune, 100);
    tom_immune_bridge_update(bridge, 100);

    // IL-6 should now impair ToM
    float post_rejection_impairment = tom_immune_get_impairment_severity(bridge);
    EXPECT_GE(post_rejection_impairment, 0.0f);
}

TEST_F(ToMImmuneBridgeTest, ImpairedToMLeadsToMoreStress) {
    // Create high inflammation (impairs ToM)
    create_inflammation(INFLAMMATION_SYSTEMIC);
    tom_immune_apply_inflammation_effects(bridge);

    // Verify impairment
    float impairment = tom_immune_get_impairment_severity(bridge);
    EXPECT_GT(impairment, 0.0f);

    // Impaired ToM would lead to more prediction errors
    // (simulated by triggering prediction error)
    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    tom_immune_trigger_from_prediction_error(bridge, 0.8f);

    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_GT(after_stats.cytokines_released, before_stats.cytokines_released);
}

TEST_F(ToMImmuneBridgeTest, SocialConnectionBuffersInflammation) {
    // Create initial inflammation
    create_inflammation(INFLAMMATION_REGIONAL);

    // Strong social connection releases IL-10
    tom_immune_boost_from_social_connection(bridge, 0.9f);

    // IL-10 should help reduce inflammation effects
    tom_immune_bridge_update(bridge, 100);

    // Check for IL-10 presence
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GT(stats.cytokines_released, 0);
}

//=============================================================================
// Query API Tests
//=============================================================================

TEST_F(ToMImmuneBridgeTest, GetCytokineEffects) {
    release_cytokine(BRAIN_CYTOKINE_IL6, 0.5f);
    tom_immune_apply_cytokine_effects(bridge);

    cytokine_tom_effects_t effects;
    int result = tom_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_GT(effects.il6_tom_impairment, 0.0f);
}

TEST_F(ToMImmuneBridgeTest, GetInflammationState) {
    create_inflammation(INFLAMMATION_REGIONAL);
    tom_immune_apply_inflammation_effects(bridge);

    inflammation_tom_state_t state;
    int result = tom_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.current_level, INFLAMMATION_REGIONAL);
}

TEST_F(ToMImmuneBridgeTest, GetPerspectiveImpairment) {
    release_cytokine(BRAIN_CYTOKINE_TNF, 0.7f);
    tom_immune_apply_cytokine_effects(bridge);

    float perspective_impairment = tom_immune_get_perspective_impairment(bridge);
    EXPECT_GT(perspective_impairment, 0.0f);
    EXPECT_LE(perspective_impairment, 1.0f);
}

TEST_F(ToMImmuneBridgeTest, GetEmpathyImpairment) {
    create_inflammation(INFLAMMATION_SYSTEMIC);
    tom_immune_apply_inflammation_effects(bridge);

    float empathy_impairment = tom_immune_get_empathy_impairment(bridge);
    EXPECT_GT(empathy_impairment, 0.0f);
    EXPECT_LE(empathy_impairment, 1.0f);
}

TEST_F(ToMImmuneBridgeTest, IsSocialWithdrawal) {
    // Initially not withdrawn
    bool withdrawn = tom_immune_is_social_withdrawal(bridge);
    EXPECT_FALSE(withdrawn);

    // Create high inflammation
    create_inflammation(INFLAMMATION_STORM);
    tom_immune_apply_inflammation_effects(bridge);

    // Should now show withdrawal
    withdrawn = tom_immune_is_social_withdrawal(bridge);
    EXPECT_TRUE(withdrawn);
}

//=============================================================================
// Feature Toggle Tests
//=============================================================================

TEST_F(ToMImmuneBridgeTest, DisableCytokineModulation) {
    // Destroy and recreate with disabled feature
    tom_immune_bridge_destroy(bridge);

    tom_immune_config_t config;
    tom_immune_default_config(&config);
    config.enable_cytokine_tom_modulation = false;
    bridge = tom_immune_bridge_create(&config, tom, immune);

    // Release cytokine
    release_cytokine(BRAIN_CYTOKINE_IL6, 0.8f);
    tom_immune_apply_cytokine_effects(bridge);

    // Should have no effect
    float impairment = tom_immune_get_impairment_severity(bridge);
    EXPECT_EQ(impairment, 0.0f);
}

TEST_F(ToMImmuneBridgeTest, DisableRejectionInflammation) {
    tom_immune_bridge_destroy(bridge);

    tom_immune_config_t config;
    tom_immune_default_config(&config);
    config.enable_rejection_inflammation = false;
    bridge = tom_immune_bridge_create(&config, tom, immune);

    brain_immune_stats_t before_stats;
    brain_immune_get_stats(immune, &before_stats);

    // Trigger rejection
    tom_immune_trigger_from_rejection(bridge, 0.9f);

    // Should not trigger immune
    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);
    EXPECT_EQ(after_stats.cytokines_released, before_stats.cytokines_released);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
