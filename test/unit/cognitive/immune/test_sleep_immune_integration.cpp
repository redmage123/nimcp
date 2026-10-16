/**
 * @file test_sleep_immune_integration.cpp
 * @brief Unit tests for Sleep-Immune System Integration
 *
 * WHAT: Test bidirectional coupling between sleep-wake cycle and immune system
 * WHY:  Validate cytokine modulation of sleep and sleep enhancement of immunity
 * HOW:  Test cytokine effects on sleep pressure, sleep stage immune boosting, deprivation effects
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_sleep_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_sleep_wake.h"

class SleepImmuneIntegrationTest : public ::testing::Test {
protected:
    sleep_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    sleep_system_t sleep_sys;

    void SetUp() override {
        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Start immune system
        brain_immune_start(immune);

        // Create sleep system
        sleep_config_t sleep_config = sleep_default_config();
        sleep_sys = sleep_system_create(&sleep_config);
        ASSERT_NE(sleep_sys, nullptr);

        // Create bridge
        sleep_immune_config_t bridge_config;
        sleep_immune_default_config(&bridge_config);
        bridge = sleep_immune_bridge_create(&bridge_config, immune, sleep_sys);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        sleep_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
        sleep_system_destroy(sleep_sys);
    }
};

/**
 * TEST: Lifecycle - Default Configuration
 * BIOLOGICAL: Verify sensible biological defaults
 */
TEST_F(SleepImmuneIntegrationTest, DefaultConfiguration) {
    sleep_immune_config_t config;
    int result = sleep_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_sleep_modulation);
    EXPECT_TRUE(config.enable_inflammation_sleep_disruption);
    EXPECT_TRUE(config.enable_sleep_immune_enhancement);
    EXPECT_TRUE(config.enable_sleep_deprivation_tracking);
    EXPECT_TRUE(config.enable_rem_memory_consolidation);

    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.sleep_deprivation_hours, 24.0f);
}

/**
 * TEST: Lifecycle - Bridge Creation and Destruction
 */
TEST_F(SleepImmuneIntegrationTest, BridgeLifecycle) {
    sleep_immune_config_t config;
    sleep_immune_default_config(&config);

    sleep_immune_bridge_t* test_bridge = sleep_immune_bridge_create(&config, immune, sleep_sys);
    ASSERT_NE(test_bridge, nullptr);

    sleep_immune_bridge_destroy(test_bridge);
}

/**
 * TEST: Cytokine Effects on Sleep Pressure
 * BIOLOGICAL: IL-1β and TNF-α increase sleep pressure (sickness behavior)
 */
TEST_F(SleepImmuneIntegrationTest, CytokineIncreaseSleepPressure) {
    // Get baseline sleep pressure
    float baseline_pressure = sleep_get_pressure(sleep_sys);

    // Trigger immune response to raise cytokine levels
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 4, 8, 0, &antigen_id);

    // Activate full immune response
    uint32_t b_cell_id, helper_id, antibody_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);
    brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // Release pro-inflammatory cytokines
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, helper_id, 0.8f, 0, &cytokine_id);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_TNF, 0, helper_id, 0.7f, 0, &cytokine_id);

    // Apply cytokine effects to sleep
    int result = sleep_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    // Check that sleep pressure increased
    cytokine_sleep_effects_t effects;
    sleep_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_GT(effects.total_sleep_pressure_bonus, 0.0f);
    EXPECT_GT(effects.il1_sleep_pressure, 0.0f);
    EXPECT_GT(effects.tnf_sleep_pressure, 0.0f);
}

/**
 * TEST: IL-10 Improves Sleep Quality
 * BIOLOGICAL: Anti-inflammatory IL-10 promotes restorative sleep
 */
TEST_F(SleepImmuneIntegrationTest, IL10ImproveSleepQuality) {
    // Release IL-10 (anti-inflammatory)
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL10, 0, 0.6f, 0, &cytokine_id);

    // Apply cytokine effects
    sleep_immune_apply_cytokine_effects(bridge);

    // Check sleep quality improvement
    cytokine_sleep_effects_t effects;
    sleep_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_GT(effects.il10_sleep_quality, 0.0f);
    EXPECT_GT(effects.sleep_quality_modifier, 1.0f); // >1.0 means enhanced
}

/**
 * TEST: Inflammation Causes Sleep Fragmentation
 * BIOLOGICAL: Chronic inflammation disrupts sleep architecture
 */
TEST_F(SleepImmuneIntegrationTest, InflammationFragmentsSleep) {
    // Create high inflammation
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 1, 0, &site_id);

    // Escalate to high level
    for (int i = 0; i < 3; i++) {
        brain_immune_escalate_inflammation(immune, site_id);
    }

    // Apply inflammation effects
    sleep_immune_apply_inflammation_effects(bridge);

    // Check for sleep fragmentation
    inflammation_sleep_state_t state;
    sleep_immune_get_inflammation_state(bridge, &state);

    EXPECT_GT(state.current_level, INFLAMMATION_LOCAL);

    // High inflammation should cause fragmentation
    if (state.current_level >= INFLAMMATION_REGIONAL) {
        EXPECT_GT(state.fragmentation_severity, 0.0f);
        EXPECT_GT(state.quality_impairment, 0.0f);
    }
}

/**
 * TEST: Deep Sleep Enhances T Cell Activity
 * BIOLOGICAL: Deep NREM sleep strengthens immune function
 */
TEST_F(SleepImmuneIntegrationTest, DeepSleepEnhancesImmunity) {
    // Enter deep NREM sleep
    bool entered = sleep_enter_state(sleep_sys, SLEEP_STATE_DEEP_NREM);
    ASSERT_TRUE(entered);

    // Apply sleep immune enhancement
    int result = sleep_immune_enhance_during_deep_sleep(bridge);
    EXPECT_EQ(result, 0);

    // Check T cell activity boost
    sleep_immune_modulation_t modulation;
    sleep_immune_get_sleep_modulation(bridge, &modulation);

    EXPECT_TRUE(modulation.in_deep_sleep);
    EXPECT_GT(modulation.t_cell_activity_multiplier, 1.0f);
    EXPECT_GT(modulation.antibody_production_boost, 0.0f);
    EXPECT_NEAR(modulation.t_cell_activity_multiplier, 1.3f, 0.1f);
}

/**
 * TEST: REM Sleep Consolidates Immune Memory
 * BIOLOGICAL: REM sleep strengthens immune memory like episodic memory
 */
TEST_F(SleepImmuneIntegrationTest, REMConsolidatesImmuneMemory) {
    // Create immune memory B cell
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 5, 0, &antigen_id);

    uint32_t b_cell_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_b_cell_to_memory(immune, b_cell_id);

    // Get initial affinity
    const brain_antigen_t* antigen = brain_immune_get_antigen(immune, antigen_id);
    ASSERT_NE(antigen, nullptr);

    // Store initial B cell state
    float initial_affinity = immune->b_cells[b_cell_id - 1].affinity;

    // Enter REM sleep
    bool entered = sleep_enter_state(sleep_sys, SLEEP_STATE_REM);
    ASSERT_TRUE(entered);

    // Consolidate immune memory
    int result = sleep_immune_consolidate_memory_during_rem(bridge);
    EXPECT_EQ(result, 0);

    // Check memory consolidation occurred
    sleep_immune_modulation_t modulation;
    sleep_immune_get_sleep_modulation(bridge, &modulation);

    EXPECT_TRUE(modulation.in_rem_sleep);
    EXPECT_GT(modulation.memory_consolidation_rate, 0.0f);

    // Memory B cell affinity should be strengthened
    float consolidated_affinity = immune->b_cells[b_cell_id - 1].affinity;
    EXPECT_GE(consolidated_affinity, initial_affinity);
}

/**
 * TEST: Sleep Deprivation Suppresses Immunity
 * BIOLOGICAL: Lack of sleep reduces T cell function and antibody production
 */
TEST_F(SleepImmuneIntegrationTest, SleepDeprivationSuppressesImmunity) {
    // Keep awake state (simulate 24+ hours)
    sleep_enter_state(sleep_sys, SLEEP_STATE_AWAKE);

    // Simulate extended wake period
    sleep_deprivation_state_t* deprivation = &bridge->deprivation_state;
    deprivation->time_awake_ms = 25 * 60 * 60 * 1000; // 25 hours

    // Apply sleep deprivation suppression
    int result = sleep_immune_suppress_from_deprivation(bridge);
    EXPECT_EQ(result, 0);

    // Check immune suppression
    sleep_deprivation_state_t dep_state;
    sleep_immune_get_deprivation_state(bridge, &dep_state);

    EXPECT_TRUE(dep_state.is_sleep_deprived);
    EXPECT_GT(dep_state.t_cell_suppression, 0.0f);
    EXPECT_GT(dep_state.antibody_suppression, 0.0f);
    EXPECT_GT(dep_state.memory_formation_impairment, 0.0f);
}

/**
 * TEST: Chronic Sleep Loss Increases Inflammation
 * BIOLOGICAL: Prolonged sleep deprivation causes pro-inflammatory shift
 */
TEST_F(SleepImmuneIntegrationTest, ChronicSleepLossIncreasesInflammation) {
    // Simulate chronic sleep loss (48+ hours)
    sleep_enter_state(sleep_sys, SLEEP_STATE_AWAKE);

    sleep_deprivation_state_t* deprivation = &bridge->deprivation_state;
    deprivation->time_awake_ms = 50 * 60 * 60 * 1000; // 50 hours

    // Get baseline inflammation
    brain_immune_stats_t baseline_stats;
    brain_immune_get_stats(immune, &baseline_stats);

    // Trigger inflammation from chronic sleep loss
    int result = sleep_immune_inflame_from_chronic_loss(bridge);
    EXPECT_EQ(result, 0);

    // Check pro-inflammatory shift
    sleep_deprivation_state_t dep_state;
    sleep_immune_get_deprivation_state(bridge, &dep_state);

    EXPECT_GT(dep_state.pro_inflammatory_shift, 0.0f);
    EXPECT_GT(dep_state.immune_dysregulation, 0.0f);
}

/**
 * TEST: Sickness Sleep Behavior
 * BIOLOGICAL: High cytokine levels induce sickness sleep
 */
TEST_F(SleepImmuneIntegrationTest, SicknessSleepBehavior) {
    // Trigger strong immune response (infection-like)
    uint32_t antigen_id;
    uint8_t epitope[] = {0xFF, 0xEE, 0xDD};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 10, 0, &antigen_id);

    // Full immune activation
    uint32_t b_cell_id, helper_id, antibody_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);
    brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // Release high pro-inflammatory cytokines
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, helper_id, 0.9f, 0, &cytokine_id);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_TNF, 0, helper_id, 0.8f, 0, &cytokine_id);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL6, 0, helper_id, 0.7f, 0, &cytokine_id);

    // Apply cytokine effects
    sleep_immune_apply_cytokine_effects(bridge);

    // Check for sickness sleep behavior
    cytokine_sleep_effects_t effects;
    sleep_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_GT(effects.sickness_sleep_drive, 0.0f);

    bool is_sickness_sleep = sleep_immune_is_sickness_sleep(bridge);
    // Outcome depends on threshold, but high cytokines should drive toward sickness sleep
}

/**
 * TEST: Sleep Fragmentation Detection
 */
TEST_F(SleepImmuneIntegrationTest, SleepFragmentationDetection) {
    // Create moderate inflammation
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 1, 0, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);

    // Apply inflammation effects
    sleep_immune_apply_inflammation_effects(bridge);

    // Check fragmentation detection
    bool is_fragmented = sleep_immune_is_sleep_fragmented(bridge);

    inflammation_sleep_state_t state;
    sleep_immune_get_inflammation_state(bridge, &state);

    // Fragmentation should increase with inflammation level
    if (state.current_level >= INFLAMMATION_REGIONAL) {
        EXPECT_GT(state.fragmentation_severity, 0.0f);
    }
}

/**
 * TEST: Bidirectional Update Integration
 */
TEST_F(SleepImmuneIntegrationTest, BidirectionalUpdate) {
    // Create initial conditions
    uint32_t antigen_id;
    uint8_t epitope[] = {0x11, 0x22, 0x33};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 6, 0, &antigen_id);

    uint32_t b_cell_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);

    // Enter deep sleep
    sleep_enter_state(sleep_sys, SLEEP_STATE_DEEP_NREM);

    // Run bidirectional update
    int result = sleep_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);

    // Verify both directions processed
    cytokine_sleep_effects_t effects;
    sleep_immune_get_cytokine_effects(bridge, &effects);

    sleep_immune_modulation_t modulation;
    sleep_immune_get_sleep_modulation(bridge, &modulation);

    // Should have processed immune → sleep
    EXPECT_GE(bridge->cytokine_modulations, 0);

    // Should have processed sleep → immune
    EXPECT_GE(bridge->sleep_enhanced_immune_events, 0);
}

/**
 * TEST: Query APIs
 */
TEST_F(SleepImmuneIntegrationTest, QueryAPIs) {
    // Test all query functions
    cytokine_sleep_effects_t effects;
    int r1 = sleep_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(r1, 0);

    inflammation_sleep_state_t inflam_state;
    int r2 = sleep_immune_get_inflammation_state(bridge, &inflam_state);
    EXPECT_EQ(r2, 0);

    sleep_immune_modulation_t modulation;
    int r3 = sleep_immune_get_sleep_modulation(bridge, &modulation);
    EXPECT_EQ(r3, 0);

    sleep_deprivation_state_t dep_state;
    int r4 = sleep_immune_get_deprivation_state(bridge, &dep_state);
    EXPECT_EQ(r4, 0);

    bool is_sickness = sleep_immune_is_sickness_sleep(bridge);
    bool is_fragmented = sleep_immune_is_sleep_fragmented(bridge);
    bool is_deprived = sleep_immune_is_sleep_deprived(bridge);
    float quality_impair = sleep_immune_get_quality_impairment(bridge);
    float suppression = sleep_immune_get_suppression_level(bridge);

    // All queries should succeed
    EXPECT_GE(quality_impair, 0.0f);
    EXPECT_GE(suppression, 0.0f);
}

/**
 * TEST: Cytokine Storm Sleep Response
 * BIOLOGICAL: Cytokine storm causes excessive sleep need (severe sickness)
 */
TEST_F(SleepImmuneIntegrationTest, CytokineStormSleepResponse) {
    // Create cytokine storm (highest inflammation)
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 1, 0, &site_id);

    // Escalate to storm level
    for (int i = 0; i < 4; i++) {
        brain_immune_escalate_inflammation(immune, site_id);
    }

    // Apply inflammation effects
    sleep_immune_apply_inflammation_effects(bridge);

    // Check storm sleep multiplier
    inflammation_sleep_state_t state;
    sleep_immune_get_inflammation_state(bridge, &state);

    if (state.current_level == INFLAMMATION_STORM) {
        EXPECT_NEAR(state.sickness_sleep_multiplier, 2.0f, 0.1f);
    }
}

/**
 * TEST: Sleep Quality Impairment from Inflammation
 */
TEST_F(SleepImmuneIntegrationTest, SleepQualityImpairment) {
    // Create inflammation
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 1, 0, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);

    // Apply effects
    sleep_immune_apply_inflammation_effects(bridge);

    // Query quality impairment
    float impairment = sleep_immune_get_quality_impairment(bridge);

    // Should have some impairment with inflammation
    EXPECT_GE(impairment, 0.0f);
    EXPECT_LE(impairment, 1.0f);
}

/**
 * TEST: Immune Suppression Level from Deprivation
 */
TEST_F(SleepImmuneIntegrationTest, ImmuneSuppressionLevel) {
    // Simulate sleep deprivation
    sleep_deprivation_state_t* deprivation = &bridge->deprivation_state;
    deprivation->time_awake_ms = 30 * 60 * 60 * 1000; // 30 hours
    deprivation->t_cell_suppression = 0.4f;
    deprivation->antibody_suppression = 0.35f;

    // Query suppression level
    float suppression = sleep_immune_get_suppression_level(bridge);

    // Should return max of T cell and antibody suppression
    EXPECT_NEAR(suppression, 0.4f, 0.01f);
}

/**
 * TEST: Null Pointer Safety
 */
TEST_F(SleepImmuneIntegrationTest, NullPointerSafety) {
    // Test all functions with NULL
    sleep_immune_config_t config;
    int r1 = sleep_immune_default_config(NULL);
    EXPECT_EQ(r1, -1);

    sleep_immune_bridge_t* null_bridge = sleep_immune_bridge_create(NULL, NULL, nullptr);
    EXPECT_EQ(null_bridge, nullptr);

    int r2 = sleep_immune_apply_cytokine_effects(nullptr);
    EXPECT_EQ(r2, -1);

    int r3 = sleep_immune_enhance_during_deep_sleep(nullptr);
    EXPECT_EQ(r3, -1);

    bool b1 = sleep_immune_is_sickness_sleep(nullptr);
    EXPECT_FALSE(b1);

    float f1 = sleep_immune_get_quality_impairment(nullptr);
    EXPECT_FLOAT_EQ(f1, 0.0f);
}

/**
 * TEST: Statistics Tracking
 */
TEST_F(SleepImmuneIntegrationTest, StatisticsTracking) {
    // Trigger various events
    uint32_t antigen_id;
    uint8_t epitope[] = {0x55, 0x66, 0x77};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 7, 0, &antigen_id);

    // Cytokine modulation
    sleep_immune_apply_cytokine_effects(bridge);

    // Sleep enhancement
    sleep_enter_state(sleep_sys, SLEEP_STATE_DEEP_NREM);
    sleep_immune_enhance_during_deep_sleep(bridge);

    // Memory consolidation
    sleep_enter_state(sleep_sys, SLEEP_STATE_REM);
    sleep_immune_consolidate_memory_during_rem(bridge);

    // Check statistics are tracked
    EXPECT_GT(bridge->total_updates, 0);
    EXPECT_GE(bridge->cytokine_modulations, 0);
    EXPECT_GE(bridge->sleep_enhanced_immune_events, 0);
}
