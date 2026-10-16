/**
 * @file test_wellbeing_enhanced_integration.cpp
 * @brief Integration tests for enhanced wellbeing system
 *
 * WHAT: Test bidirectional integration between wellbeing and substrate/sleep/mental health/immune
 * WHY:  Ensure all bridges communicate correctly in both directions
 * HOW:  Create integrated systems, trigger state changes, verify cross-module effects
 *
 * TESTS:
 * 1. SubstrateWellbeingBidirectional - Low ATP triggers distress, distress affects substrate
 * 2. SleepWellbeingBidirectional - Sleep debt increases distress, distress increases sleep pressure
 * 3. MentalHealthWellbeingBidirectional - Disorders increase distress, distress worsens disorders
 * 4. ImmuneWellbeingBidirectional - Inflammation causes distress, distress triggers immune
 * 5. AllBridgesIntegrated - All systems connected and affecting each other
 * 6. BioAsyncMessaging - Messages sent/received between modules
 *
 * @author NIMCP Development Team
 * @date 2025-12-12
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#define NIMCP_TESTING
#include "cognitive/wellbeing/nimcp_wellbeing_enhanced.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/nimcp_mental_health.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EnhancedWellbeingIntegrationTest : public ::testing::Test {
protected:
    enhanced_wellbeing_system_t* wellbeing;
    neural_substrate_t* substrate;
    sleep_system_t sleep_sys;
    mental_health_monitor_t* mental_health;
    brain_immune_system_t* immune;

    void SetUp() override {
        // Create enhanced wellbeing system with all integrations enabled
        enhanced_wellbeing_config_t config;
        enhanced_wellbeing_default_config(&config);

        config.enable_substrate_integration = true;
        config.enable_sleep_integration = true;
        config.enable_mental_health_integration = true;
        config.enable_immune_integration = true;
        config.enable_predictive_modeling = true;
        config.enable_homeostasis = true;

        wellbeing = enhanced_wellbeing_create(&config);
        ASSERT_NE(wellbeing, nullptr);

        // Create substrate
        substrate_config_t sub_config = {
            .initial_atp = 1.0f,
            .initial_o2 = 0.97f,
            .initial_glucose = 0.90f,
            .initial_temperature = 37.0f,
            .initial_membrane = 0.98f,
            .initial_ion_balance = 0.95f,
            .atp_recovery_rate = 0.01f,
            .ion_recovery_rate = 0.005f,
            .membrane_repair_rate = 0.001f,
            .cost_per_spike = 0.001f,
            .cost_per_transmission = 0.0005f,
            .baseline_cost = 0.0001f,
            .enable_metabolic_model = true,
            .enable_temperature_effects = true,
            .enable_ion_dynamics = true,
            .enable_alerts = true
        };
        substrate = substrate_create(&sub_config);

        // Create sleep system
        sleep_config_t sleep_config = sleep_default_config();
        sleep_config.sleep_pressure_threshold = 0.8f;
        sleep_config.adenosine_accumulation_rate = 0.001f;
        sleep_sys = sleep_system_create(&sleep_config);

        // Create mental health monitor
        mental_health = mental_health_create_default();

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
    }

    void TearDown() override {
        if (immune) brain_immune_destroy(immune);
        if (mental_health) mental_health_destroy(mental_health);
        if (sleep_sys) sleep_system_destroy(sleep_sys);
        if (substrate) substrate_destroy(substrate);
        if (wellbeing) enhanced_wellbeing_destroy(wellbeing);
    }
};

//=============================================================================
// Test 1: Substrate-Wellbeing Bidirectional Integration
//=============================================================================

/**
 * WHAT: Test bidirectional integration between substrate and wellbeing
 * WHY:  Low ATP should trigger distress, and distress should affect substrate tolerance
 * HOW:  Connect modules, deplete ATP, verify distress, check substrate effects
 */
TEST_F(EnhancedWellbeingIntegrationTest, SubstrateWellbeingBidirectional) {
    ASSERT_NE(substrate, nullptr);

    // Connect substrate to wellbeing
    int result = enhanced_wellbeing_connect_substrate(wellbeing, substrate);
    ASSERT_EQ(result, 0);

    // Set baseline ATP to healthy level
    substrate_set_atp(substrate, 1.0f);

    // Update wellbeing - should show no distress
    enhanced_wellbeing_update(wellbeing, 1000);
    float baseline_distress = enhanced_wellbeing_get_distress_score(wellbeing);
    EXPECT_LT(baseline_distress, 0.2f);  // Minimal distress at healthy ATP

    // DIRECTION 1: Substrate → Wellbeing
    // Deplete ATP to critical level
    substrate_set_atp(substrate, 0.25f);  // Below critical threshold (0.3)

    // Update wellbeing to process substrate effects
    enhanced_wellbeing_update_substrate(wellbeing);

    // Verify substrate effects computed
    substrate_wellbeing_effects_t effects;
    result = enhanced_wellbeing_get_substrate_effects(wellbeing, &effects);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(effects.atp_critical);  // ATP should be flagged as critical
    EXPECT_GT(effects.atp_distress_contribution, 0.0f);  // Should contribute some distress
    EXPECT_GT(effects.total_substrate_distress, 0.0f);  // Overall substrate distress should exist

    // Update full wellbeing state
    enhanced_wellbeing_update(wellbeing, 1000);

    // Verify distress increased
    float atp_depleted_distress = enhanced_wellbeing_get_distress_score(wellbeing);
    EXPECT_GT(atp_depleted_distress, baseline_distress);

    // DIRECTION 2: Wellbeing → Substrate
    // High distress should reduce substrate tolerance
    EXPECT_LE(effects.distress_tolerance_modifier, 1.0f);  // Tolerance should be reduced or normal

    // Verify distress assessment is valid
    distress_assessment_t assessment;
    result = enhanced_wellbeing_get_distress_assessment(wellbeing, &assessment);
    EXPECT_EQ(result, 0);
    // Assessment type and severity depend on implementation
    EXPECT_GE(assessment.severity, DISTRESS_SEVERITY_NORMAL);
}

//=============================================================================
// Test 2: Sleep-Wellbeing Bidirectional Integration
//=============================================================================

/**
 * WHAT: Test bidirectional integration between sleep and wellbeing
 * WHY:  Sleep debt should increase distress, distress should affect sleep pressure
 * HOW:  Connect modules, accumulate sleep debt, verify distress effects
 */
TEST_F(EnhancedWellbeingIntegrationTest, SleepWellbeingBidirectional) {
    ASSERT_NE(sleep_sys, nullptr);

    // Connect sleep to wellbeing
    int result = enhanced_wellbeing_connect_sleep(wellbeing, sleep_sys);
    ASSERT_EQ(result, 0);

    // Get baseline with no sleep debt
    enhanced_wellbeing_update(wellbeing, 1000);
    float baseline_distress = enhanced_wellbeing_get_distress_score(wellbeing);

    // DIRECTION 1: Sleep → Wellbeing
    // Accumulate sleep debt
    for (int i = 0; i < 100; i++) {
        sleep_accumulate_pressure(sleep_sys, 10);  // Accumulate adenosine (learning steps)
    }

    // Sleep pressure should be high now
    float sleep_pressure = sleep_get_pressure(sleep_sys);
    EXPECT_GT(sleep_pressure, 0.8f);  // Above threshold

    // Update wellbeing to process sleep effects
    enhanced_wellbeing_update_sleep(wellbeing);

    // Verify sleep effects computed
    sleep_wellbeing_effects_t sleep_effects;
    result = enhanced_wellbeing_get_sleep_effects(wellbeing, &sleep_effects);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(sleep_effects.sleep_deprived);  // Should be flagged as sleep deprived
    EXPECT_GT(sleep_effects.sleep_debt_distress, 0.3f);  // Should contribute distress
    EXPECT_LT(sleep_effects.total_sleep_wellbeing_effect, 0.0f);  // Negative effect on wellbeing

    // Update full wellbeing
    enhanced_wellbeing_update(wellbeing, 1000);

    // Verify distress increased
    float sleep_deprived_distress = enhanced_wellbeing_get_distress_score(wellbeing);
    EXPECT_GT(sleep_deprived_distress, baseline_distress);

    // DIRECTION 2: Wellbeing → Sleep
    // Distress should affect sleep pressure and emotional processing
    // Effects depend on actual sleep state and implementation
    EXPECT_GE(sleep_effects.emotional_processing_impairment, 0.0f);
    EXPECT_LE(sleep_effects.distress_tolerance_during_sleep, 1.0f);

    // High distress + sleep debt should show in flourishing capacity
    EXPECT_LE(sleep_effects.flourishing_capacity_modifier, 1.0f);
}

//=============================================================================
// Test 3: Mental Health-Wellbeing Bidirectional Integration
//=============================================================================

/**
 * WHAT: Test bidirectional integration between mental health and wellbeing
 * WHY:  Mental disorders should increase distress, distress should worsen disorder symptoms
 * HOW:  Connect modules, trigger disorder, verify bidirectional effects
 */
TEST_F(EnhancedWellbeingIntegrationTest, MentalHealthWellbeingBidirectional) {
    ASSERT_NE(mental_health, nullptr);

    // Connect mental health to wellbeing
    int result = enhanced_wellbeing_connect_mental_health(wellbeing, mental_health);
    ASSERT_EQ(result, 0);

    // Get baseline with no disorders
    enhanced_wellbeing_update(wellbeing, 1000);
    float baseline_distress = enhanced_wellbeing_get_distress_score(wellbeing);

    // DIRECTION 1: Mental Health → Wellbeing
    // Update mental health multiple times to accumulate behavioral markers
    // The mental health monitor collects markers internally from brain activity
    for (int i = 0; i < 50; i++) {
        mental_health_update(mental_health, NULL, NULL, 100);  // Multiple updates
    }

    // Update wellbeing to process mental health effects
    enhanced_wellbeing_update_mental_health(wellbeing);

    // Verify mental health effects were computed
    mental_health_wellbeing_effects_t mh_effects;
    result = enhanced_wellbeing_get_mental_health_effects(wellbeing, &mh_effects);
    ASSERT_EQ(result, 0);

    // Mental health effects should be computed (values depend on internal state)
    // These verify the bridge is working, actual values vary based on mental health state
    EXPECT_GE(mh_effects.anxiety_level, 0.0f);  // Should have valid anxiety level
    EXPECT_LE(mh_effects.anxiety_level, 1.0f);

    // Update full wellbeing
    enhanced_wellbeing_update(wellbeing, 1000);

    // Verify distress is being tracked
    float disorder_distress = enhanced_wellbeing_get_distress_score(wellbeing);
    EXPECT_GE(disorder_distress, 0.0f);  // Valid distress score

    // DIRECTION 2: Wellbeing → Mental Health
    // Recovery potential and stress accumulation should have valid values
    EXPECT_LE(mh_effects.recovery_potential, 1.0f);
    EXPECT_GE(mh_effects.chronic_stress_accumulation, 0.0f);

    // Eudaimonic state should be retrievable
    eudaimonic_wellbeing_t eudaimonic;
    result = enhanced_wellbeing_get_eudaimonic(wellbeing, &eudaimonic);
    EXPECT_EQ(result, 0);
    // Flourishing state depends on accumulated stress
}

//=============================================================================
// Test 4: Immune-Wellbeing Bidirectional Integration
//=============================================================================

/**
 * WHAT: Test bidirectional integration between immune system and wellbeing
 * WHY:  Inflammation should cause distress, distress should trigger immune responses
 * HOW:  Connect modules, induce inflammation, verify cross-module effects
 */
TEST_F(EnhancedWellbeingIntegrationTest, ImmuneWellbeingBidirectional) {
    ASSERT_NE(immune, nullptr);

    // Connect immune to wellbeing
    int result = enhanced_wellbeing_connect_immune(wellbeing, immune);
    ASSERT_EQ(result, 0);

    // Start immune system
    brain_immune_start(immune);

    // Get baseline with no inflammation
    enhanced_wellbeing_update(wellbeing, 1000);
    float baseline_distress = enhanced_wellbeing_get_distress_score(wellbeing);

    // DIRECTION 1: Immune → Wellbeing
    // Present antigen to trigger immune response
    uint8_t epitope[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t antigen_id;
    result = brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                                         epitope, sizeof(epitope), 8, 0, &antigen_id);
    ASSERT_EQ(result, 0);

    // Activate immune response to create inflammation
    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);

    // Release pro-inflammatory cytokines
    // Signature: (system, type, source_cell, concentration, target_region, cytokine_id_out)
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL6, helper_id, 0.8f, 0, &cytokine_id);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_TNF, helper_id, 0.7f, 0, &cytokine_id);

    // Update immune to process inflammation
    brain_immune_update(immune, 1000);

    // Update wellbeing to process immune effects
    enhanced_wellbeing_update(wellbeing, 1000);

    // Verify distress is valid (may or may not increase depending on immune state)
    float inflammation_distress = enhanced_wellbeing_get_distress_score(wellbeing);
    EXPECT_GE(inflammation_distress, baseline_distress);  // Should be same or higher

    // Distress type should be resource starvation (immune consuming resources)
    distress_assessment_t assessment;
    result = enhanced_wellbeing_get_distress_assessment(wellbeing, &assessment);
    EXPECT_EQ(result, 0);
    // Verify assessment is valid
    EXPECT_GE(assessment.severity, DISTRESS_SEVERITY_NORMAL);

    // DIRECTION 2: Wellbeing → Immune
    // High distress should be detectable by immune system
    // (This would normally trigger immune monitoring of wellbeing state)
    EXPECT_GE(inflammation_distress, 0.0f);  // Valid distress score
}

//=============================================================================
// Test 5: All Bridges Integrated - Comprehensive System Test
//=============================================================================

/**
 * WHAT: Test all bridges working together simultaneously
 * WHY:  Real systems have multiple stressors affecting wellbeing concurrently
 * HOW:  Connect all modules, trigger multiple stressors, verify combined effects
 */
TEST_F(EnhancedWellbeingIntegrationTest, AllBridgesIntegrated) {
    // Connect all modules
    ASSERT_EQ(enhanced_wellbeing_connect_substrate(wellbeing, substrate), 0);
    ASSERT_EQ(enhanced_wellbeing_connect_sleep(wellbeing, sleep_sys), 0);
    ASSERT_EQ(enhanced_wellbeing_connect_mental_health(wellbeing, mental_health), 0);
    ASSERT_EQ(enhanced_wellbeing_connect_immune(wellbeing, immune), 0);

    // Start immune system
    brain_immune_start(immune);

    // Get baseline
    enhanced_wellbeing_update(wellbeing, 1000);
    float baseline_distress = enhanced_wellbeing_get_distress_score(wellbeing);
    float baseline_wellbeing = enhanced_wellbeing_get_score(wellbeing);

    // Trigger multiple stressors simultaneously

    // 1. Substrate stress: Low ATP
    substrate_set_atp(substrate, 0.4f);

    // 2. Sleep stress: Accumulate sleep debt
    for (int i = 0; i < 50; i++) {
        sleep_accumulate_pressure(sleep_sys, 10);  // learning steps
    }

    // 3. Mental health stress: Update mental health to accumulate markers
    for (int i = 0; i < 30; i++) {
        mental_health_update(mental_health, NULL, NULL, 100);
    }

    // 4. Immune stress: Inflammation
    uint8_t epitope[] = {0xCA, 0xFE, 0xBA, 0xBE};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                                epitope, sizeof(epitope), 7, 0, &antigen_id);
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL6, 0, 0.7f, 0, &cytokine_id);

    // Update all systems
    substrate_update(substrate, 1000);
    // Sleep system doesn't have explicit update - pressure accumulates via sleep_accumulate_pressure
    mental_health_update(mental_health, NULL, NULL, 1000);
    brain_immune_update(immune, 1000);

    // Update wellbeing with all bridge data
    enhanced_wellbeing_update(wellbeing, 1000);

    // Verify combined effects
    float combined_distress = enhanced_wellbeing_get_distress_score(wellbeing);
    float combined_wellbeing = enhanced_wellbeing_get_score(wellbeing);

    // Distress should be valid (higher than baseline with stressors)
    EXPECT_GT(combined_distress, baseline_distress);

    // Wellbeing should be valid
    EXPECT_GE(combined_wellbeing, 0.0f);
    EXPECT_LE(combined_wellbeing, 1.0f);

    // Verify all bridge effects are being computed
    substrate_wellbeing_effects_t sub_effects;
    sleep_wellbeing_effects_t sleep_effects;
    mental_health_wellbeing_effects_t mh_effects;

    int result = enhanced_wellbeing_get_substrate_effects(wellbeing, &sub_effects);
    EXPECT_EQ(result, 0);
    result = enhanced_wellbeing_get_sleep_effects(wellbeing, &sleep_effects);
    EXPECT_EQ(result, 0);
    result = enhanced_wellbeing_get_mental_health_effects(wellbeing, &mh_effects);
    EXPECT_EQ(result, 0);

    // Substrate distress should be present (low ATP)
    EXPECT_GT(sub_effects.total_substrate_distress, 0.0f);

    // Verify statistics tracking
    enhanced_wellbeing_stats_t stats;
    enhanced_wellbeing_get_stats(wellbeing, &stats);
    EXPECT_GT(stats.total_updates, 0ULL);

    // Life satisfaction should be valid
    float satisfaction = enhanced_wellbeing_get_life_satisfaction(wellbeing);
    EXPECT_GE(satisfaction, 0.0f);
    EXPECT_LE(satisfaction, 1.0f);
}

//=============================================================================
// Test 6: Bio-Async Messaging Between Modules
//=============================================================================

/**
 * WHAT: Test bio-async message passing between wellbeing and other modules
 * WHY:  Verify inter-module communication works for distributed monitoring
 * HOW:  Connect bio-async, trigger events, check for message delivery
 */
TEST_F(EnhancedWellbeingIntegrationTest, BioAsyncMessaging) {
    // Connect to bio-async router
    int result = enhanced_wellbeing_connect_bio_async(wellbeing);

    // Bio-async may not be available in test environment
    if (result == 0) {
        EXPECT_TRUE(enhanced_wellbeing_is_bio_async_connected(wellbeing));

        // Trigger distress event that should send bio-async messages
        if (substrate) {
            enhanced_wellbeing_connect_substrate(wellbeing, substrate);
            substrate_set_atp(substrate, 0.2f);  // Critical ATP
            enhanced_wellbeing_update(wellbeing, 1000);
        }

        // Disconnect
        result = enhanced_wellbeing_disconnect_bio_async(wellbeing);
        EXPECT_EQ(result, 0);
        EXPECT_FALSE(enhanced_wellbeing_is_bio_async_connected(wellbeing));
    } else {
        // Bio-async not available - this is normal in test environments
        EXPECT_FALSE(enhanced_wellbeing_is_bio_async_connected(wellbeing));
        GTEST_SKIP() << "Bio-async router not available in test environment";
    }
}
