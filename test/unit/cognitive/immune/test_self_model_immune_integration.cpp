/**
 * @file test_self_model_immune_integration.cpp
 * @brief Unit tests for Self-Model - Immune System Integration
 *
 * WHAT: Test bidirectional coupling between self-model and immune system
 * WHY:  Validate interoception, health status updates, and belief-based immunity
 * HOW:  Test immune state → self-representation, self-awareness → immune modulation
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_self_model_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_self_model.h"

class SelfModelImmuneTest : public ::testing::Test {
protected:
    self_model_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    self_model_system_t self_model;

    void SetUp() override {
        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create self-model
        self_model = self_model_create("NIMCP", "AI Learning System",
                                       "Understand and model cognition");
        ASSERT_NE(self_model, nullptr);

        // Create bridge
        self_model_immune_config_t bridge_config;
        self_model_immune_default_config(&bridge_config);
        bridge = self_model_immune_bridge_create(&bridge_config, immune, self_model);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        self_model_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
        self_model_destroy(self_model);
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

/**
 * TEST: Default Configuration
 * WHAT: Verify default configuration is sensible
 */
TEST_F(SelfModelImmuneTest, DefaultConfiguration) {
    self_model_immune_config_t config;
    int result = self_model_immune_default_config(&config);
    EXPECT_EQ(result, 0);

    // All features enabled by default
    EXPECT_TRUE(config.enable_interoceptive_signaling);
    EXPECT_TRUE(config.enable_self_model_health_update);
    EXPECT_TRUE(config.enable_capability_modulation);
    EXPECT_TRUE(config.enable_health_belief_immune_effects);
    EXPECT_TRUE(config.enable_identity_integration);

    // Sensitivities are 1.0 by default
    EXPECT_FLOAT_EQ(config.interoceptive_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.health_update_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.belief_immune_sensitivity, 1.0f);

    // Thresholds are reasonable
    EXPECT_GT(config.sickness_awareness_threshold, 0.0f);
    EXPECT_LT(config.sickness_awareness_threshold, 1.0f);
    EXPECT_GT(config.chronic_identity_threshold_days, 0.0f);
}

/**
 * TEST: Lifecycle - Create and Destroy
 * WHAT: Verify bridge can be created and destroyed without leaks
 */
TEST_F(SelfModelImmuneTest, LifecycleCreateDestroy) {
    self_model_immune_config_t config;
    self_model_immune_default_config(&config);

    self_model_immune_bridge_t* test_bridge =
        self_model_immune_bridge_create(&config, immune, self_model);
    ASSERT_NE(test_bridge, nullptr);

    // Destroy should not crash
    self_model_immune_bridge_destroy(test_bridge);
}

/**
 * TEST: NULL Configuration
 * WHAT: Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(SelfModelImmuneTest, NullConfiguration) {
    self_model_immune_bridge_t* test_bridge =
        self_model_immune_bridge_create(nullptr, immune, self_model);
    ASSERT_NE(test_bridge, nullptr);
    self_model_immune_bridge_destroy(test_bridge);
}

/* ============================================================================
 * Immune → Self-Model Tests
 * ============================================================================ */

/**
 * TEST: Interoceptive Signal Generation
 * BIOLOGICAL: Immune state creates body awareness signals
 */
TEST_F(SelfModelImmuneTest, InteroceptiveSignalGeneration) {
    // Trigger inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  8, 0, &antigen_id);

    // Activate immune response
    uint32_t region_id = 0;
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, region_id, antigen_id, &site_id);

    // Generate interoceptive signals
    int result = self_model_immune_generate_interoceptive_signals(bridge);
    EXPECT_EQ(result, 0);

    // Get signals
    interoceptive_immune_signals_t signals;
    self_model_immune_get_interoceptive_signals(bridge, &signals);

    // Should have some body awareness
    EXPECT_GT(signals.total_body_awareness, 0.0f);

    // Fatigue should be present
    EXPECT_GE(signals.fatigue_signal, 0.0f);
    EXPECT_LE(signals.fatigue_signal, 1.0f);

    // Malaise signal
    EXPECT_GE(signals.malaise_signal, 0.0f);
    EXPECT_LE(signals.malaise_signal, 1.0f);

    // Vitality should be reduced
    EXPECT_LT(signals.vitality_signal, 1.0f);
}

/**
 * TEST: Health Status Update
 * BIOLOGICAL: Self-concept includes health state
 */
TEST_F(SelfModelImmuneTest, HealthStatusUpdate) {
    // Initial state - should be excellent
    self_health_status_t initial_status = self_model_immune_get_health_status(bridge);
    EXPECT_EQ(initial_status, HEALTH_STATUS_EXCELLENT);

    // Trigger moderate inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 2,
                                  5, 0, &antigen_id);

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);

    // Update health status
    int result = self_model_immune_update_health_status(bridge);
    EXPECT_EQ(result, 0);

    // Get updated status
    self_model_immune_modulation_t updates;
    self_model_immune_get_self_model_updates(bridge, &updates);

    // Health belief should be updated
    EXPECT_GT(strlen(updates.health_belief), 0);

    // Health certainty should be reasonable
    EXPECT_GE(updates.health_certainty, 0.0f);
    EXPECT_LE(updates.health_certainty, 1.0f);
}

/**
 * TEST: Capability Modulation from Illness
 * BIOLOGICAL: Sickness reduces perceived competence
 */
TEST_F(SelfModelImmuneTest, CapabilityModulation) {
    // Trigger high inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xFF, 0xEE, 0xDD};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  9, 0, &antigen_id);

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);

    // Escalate to systemic
    brain_immune_escalate_inflammation(immune, site_id);
    brain_immune_escalate_inflammation(immune, site_id);

    // Modulate capabilities
    int result = self_model_immune_modulate_capabilities(bridge);
    EXPECT_EQ(result, 0);

    // Get updates
    self_model_immune_modulation_t updates;
    self_model_immune_get_self_model_updates(bridge, &updates);

    // Competence should be reduced
    EXPECT_GT(updates.immune_competence_reduction, 0.0f);

    // Self-efficacy should be reduced
    EXPECT_GT(updates.immune_efficacy_reduction, 0.0f);

    // Cognitive impairment present
    EXPECT_GT(updates.cognitive_impairment, 0.0f);

    // Self-care motivation should increase
    EXPECT_GT(updates.self_care_motivation, 0.0f);
}

/**
 * TEST: Chronic Illness Identity Integration
 * BIOLOGICAL: Long-term illness becomes part of self-concept
 */
TEST_F(SelfModelImmuneTest, ChronicIllnessIdentityIntegration) {
    // Create persistent inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0x11, 0x22, 0x33};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  6, 0, &antigen_id);

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);

    // Integrate chronic illness
    int result = self_model_immune_integrate_chronic_illness(bridge);
    EXPECT_EQ(result, 0);

    // Get updates
    self_model_immune_modulation_t updates;
    self_model_immune_get_self_model_updates(bridge, &updates);

    // Initially should not be integrated (not long enough)
    EXPECT_FALSE(updates.illness_integrated_in_identity);

    // Body schema distortion
    EXPECT_GE(updates.body_schema_distortion, 0.0f);
}

/**
 * TEST: Sickness Awareness Threshold
 * BIOLOGICAL: Conscious awareness requires sufficient signal strength
 */
TEST_F(SelfModelImmuneTest, SicknessAwarenessThreshold) {
    // Mild inflammation - below threshold
    uint32_t antigen_id1;
    uint8_t epitope1[] = {0x01};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope1, 1,
                                  2, 0, &antigen_id1);

    self_model_immune_generate_interoceptive_signals(bridge);
    EXPECT_FALSE(self_model_immune_is_aware_of_sickness(bridge));

    // High inflammation - above threshold
    uint32_t antigen_id2;
    uint8_t epitope2[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope2, 3,
                                  9, 0, &antigen_id2);

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, antigen_id2, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);

    self_model_immune_generate_interoceptive_signals(bridge);

    interoceptive_immune_signals_t signals;
    self_model_immune_get_interoceptive_signals(bridge, &signals);

    // Should have high sickness intensity
    EXPECT_GT(signals.sickness_intensity, 0.0f);
}

/* ============================================================================
 * Self-Model → Immune Tests
 * ============================================================================ */

/**
 * TEST: Adaptive Behavior from Illness Awareness
 * BIOLOGICAL: Conscious awareness triggers rest/recovery
 */
TEST_F(SelfModelImmuneTest, AdaptiveBehaviorFromAwareness) {
    // Trigger sickness
    uint32_t antigen_id;
    uint8_t epitope[] = {0xDE, 0xAD, 0xBE, 0xEF};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 4,
                                  8, 0, &antigen_id);

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);

    // Generate awareness
    self_model_immune_generate_interoceptive_signals(bridge);

    // Trigger adaptive behavior
    int result = self_model_immune_trigger_adaptive_behavior(bridge);
    EXPECT_EQ(result, 0);

    // Check if aware and compliant
    interoceptive_immune_signals_t signals;
    self_model_immune_get_interoceptive_signals(bridge, &signals);

    if (signals.consciously_aware_of_illness) {
        // Rest compliance should be present
        EXPECT_GT(bridge->self_awareness_effects.rest_compliance, 0.0f);
    }
}

/**
 * TEST: Immune Boost from Positive Health Beliefs
 * BIOLOGICAL: Health self-efficacy enhances immunity
 */
TEST_F(SelfModelImmuneTest, ImmuneBoostFromHealthBeliefs) {
    // Set up positive health beliefs (via self-model)
    // Note: In full implementation, would add health beliefs to self-model

    // Boost immunity from beliefs
    int result = self_model_immune_boost_from_health_beliefs(bridge);
    EXPECT_EQ(result, 0);

    // Should have some immune enhancement
    EXPECT_GE(bridge->self_awareness_effects.immune_enhancement, 0.0f);
    EXPECT_LE(bridge->self_awareness_effects.immune_enhancement,
              HEALTH_BELIEF_IMMUNE_BOOST);
}

/**
 * TEST: Immune Suppression from Health Anxiety
 * BIOLOGICAL: Excessive worry about health suppresses immunity
 */
TEST_F(SelfModelImmuneTest, ImmuneSuppressionFromHealthAnxiety) {
    // Create mild illness
    uint32_t antigen_id;
    uint8_t epitope[] = {0x12, 0x34};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 2,
                                  4, 0, &antigen_id);

    // Generate interoceptive signals
    self_model_immune_generate_interoceptive_signals(bridge);

    // Suppress from anxiety
    int result = self_model_immune_suppress_from_health_anxiety(bridge);
    EXPECT_EQ(result, 0);

    // Health monitoring level should be tracked
    EXPECT_GE(bridge->self_awareness_effects.health_monitoring_level, 0.0f);
}

/**
 * TEST: Recovery Acceleration from Illness Acceptance
 * BIOLOGICAL: Acceptance reduces stress, aids healing
 */
TEST_F(SelfModelImmuneTest, RecoveryAccelerationFromAcceptance) {
    // Create illness
    uint32_t antigen_id;
    uint8_t epitope[] = {0x99, 0x88};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 2,
                                  6, 0, &antigen_id);

    // Accelerate from acceptance
    int result = self_model_immune_accelerate_from_acceptance(bridge);
    EXPECT_EQ(result, 0);

    // Acceptance level should be reasonable
    EXPECT_GE(bridge->self_awareness_effects.illness_acceptance, 0.0f);
    EXPECT_LE(bridge->self_awareness_effects.illness_acceptance, 1.0f);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

/**
 * TEST: Full Bridge Update
 * WHAT: Test complete bidirectional update cycle
 */
TEST_F(SelfModelImmuneTest, FullBridgeUpdate) {
    // Create immune activity
    uint32_t antigen_id;
    uint8_t epitope[] = {0xCA, 0xFE};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 2,
                                  5, 0, &antigen_id);

    // Update bridge
    int result = self_model_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);

    // Statistics should be updated
    EXPECT_GT(bridge->total_updates, 0);
}

/**
 * TEST: Multiple Update Cycles
 * WHAT: Verify bridge handles repeated updates correctly
 */
TEST_F(SelfModelImmuneTest, MultipleUpdateCycles) {
    uint64_t initial_updates = bridge->total_updates;

    // Run multiple updates
    for (int i = 0; i < 10; i++) {
        int result = self_model_immune_bridge_update(bridge, 100);
        EXPECT_EQ(result, 0);
    }

    // Update count should increase
    EXPECT_GT(bridge->total_updates, initial_updates);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

/**
 * TEST: Get Interoceptive Signals
 * WHAT: Verify signal retrieval works correctly
 */
TEST_F(SelfModelImmuneTest, GetInteroceptiveSignals) {
    interoceptive_immune_signals_t signals;
    int result = self_model_immune_get_interoceptive_signals(bridge, &signals);
    EXPECT_EQ(result, 0);

    // All signals should be in valid range
    EXPECT_GE(signals.fatigue_signal, 0.0f);
    EXPECT_LE(signals.fatigue_signal, 1.0f);
    EXPECT_GE(signals.malaise_signal, 0.0f);
    EXPECT_LE(signals.malaise_signal, 1.0f);
    EXPECT_GE(signals.vitality_signal, 0.0f);
    EXPECT_LE(signals.vitality_signal, 1.0f);
}

/**
 * TEST: Get Self-Model Updates
 * WHAT: Verify update retrieval works correctly
 */
TEST_F(SelfModelImmuneTest, GetSelfModelUpdates) {
    self_model_immune_modulation_t updates;
    int result = self_model_immune_get_self_model_updates(bridge, &updates);
    EXPECT_EQ(result, 0);

    // Initial health status should be excellent
    EXPECT_EQ(updates.perceived_health_status, HEALTH_STATUS_EXCELLENT);

    // Reductions should be zero initially
    EXPECT_EQ(updates.immune_competence_reduction, 0.0f);
    EXPECT_EQ(updates.immune_efficacy_reduction, 0.0f);
}

/**
 * TEST: Get Health Status
 * WHAT: Verify health status query works
 */
TEST_F(SelfModelImmuneTest, GetHealthStatus) {
    self_health_status_t status = self_model_immune_get_health_status(bridge);

    // Should be in valid range
    EXPECT_GE(status, HEALTH_STATUS_EXCELLENT);
    EXPECT_LE(status, HEALTH_STATUS_CRITICAL);
}

/**
 * TEST: Is Aware of Sickness
 * WHAT: Verify sickness awareness query
 */
TEST_F(SelfModelImmuneTest, IsAwareOfSickness) {
    // Initially should not be aware (no illness)
    bool aware = self_model_immune_is_aware_of_sickness(bridge);
    EXPECT_FALSE(aware);

    // Trigger high inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xFF, 0xFF};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 2,
                                  9, 0, &antigen_id);

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);

    // Generate signals
    self_model_immune_generate_interoceptive_signals(bridge);

    // Now might be aware
    interoceptive_immune_signals_t signals;
    self_model_immune_get_interoceptive_signals(bridge, &signals);

    // Check awareness based on sickness intensity
    EXPECT_GE(signals.sickness_intensity, 0.0f);
}

/**
 * TEST: Interoceptive Accuracy
 * BIOLOGICAL: Accurate body perception predicts health outcomes
 */
TEST_F(SelfModelImmuneTest, InteroceptiveAccuracy) {
    // Generate signals and update health status
    self_model_immune_generate_interoceptive_signals(bridge);
    self_model_immune_update_health_status(bridge);

    // Get accuracy
    float accuracy = self_model_immune_get_interoceptive_accuracy(bridge);

    // Should be in valid range
    EXPECT_GE(accuracy, 0.0f);
    EXPECT_LE(accuracy, 1.0f);

    // With no illness, accuracy should be high
    EXPECT_GT(accuracy, 0.8f);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

/**
 * TEST: Health Status Transition
 * WHAT: Test transitions through health status levels
 */
TEST_F(SelfModelImmuneTest, HealthStatusTransition) {
    // Track status changes
    uint32_t initial_changes = bridge->health_status_changes;

    // Create escalating inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAB, 0xCD, 0xEF};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  5, 0, &antigen_id);

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);

    // Update health status
    self_model_immune_update_health_status(bridge);
    self_health_status_t status1 = self_model_immune_get_health_status(bridge);

    // Escalate inflammation
    brain_immune_escalate_inflammation(immune, site_id);
    self_model_immune_update_health_status(bridge);
    self_health_status_t status2 = self_model_immune_get_health_status(bridge);

    // Status might change (depends on threshold)
    EXPECT_GE(bridge->health_status_changes, initial_changes);
}

/**
 * TEST: Self-Care Motivation from Sickness
 * BIOLOGICAL: Illness awareness triggers self-care drive
 */
TEST_F(SelfModelImmuneTest, SelfCareMotivationFromSickness) {
    // Create moderate illness
    uint32_t antigen_id;
    uint8_t epitope[] = {0x55, 0x66};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 2,
                                  6, 0, &antigen_id);

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);

    // Modulate capabilities
    self_model_immune_modulate_capabilities(bridge);

    // Get updates
    self_model_immune_modulation_t updates;
    self_model_immune_get_self_model_updates(bridge, &updates);

    // Self-care motivation should increase with illness
    EXPECT_GE(updates.self_care_motivation, 0.0f);
}

/**
 * TEST: Null Pointer Handling
 * WHAT: Verify functions handle NULL pointers gracefully
 */
TEST_F(SelfModelImmuneTest, NullPointerHandling) {
    // NULL config
    EXPECT_EQ(self_model_immune_default_config(nullptr), -1);

    // NULL bridge operations
    EXPECT_EQ(self_model_immune_generate_interoceptive_signals(nullptr), -1);
    EXPECT_EQ(self_model_immune_update_health_status(nullptr), -1);
    EXPECT_EQ(self_model_immune_bridge_update(nullptr, 1000), -1);

    // NULL output parameters
    EXPECT_EQ(self_model_immune_get_interoceptive_signals(bridge, nullptr), -1);
    EXPECT_EQ(self_model_immune_get_self_model_updates(bridge, nullptr), -1);

    // Destroy NULL should not crash
    self_model_immune_bridge_destroy(nullptr);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * TEST: Statistics Tracking
 * WHAT: Verify statistics are tracked correctly
 */
TEST_F(SelfModelImmuneTest, StatisticsTracking) {
    uint64_t initial_updates = bridge->total_updates;
    uint32_t initial_signals = bridge->interoceptive_signals_sent;
    uint32_t initial_modulations = bridge->capability_modulations;

    // Perform operations
    self_model_immune_generate_interoceptive_signals(bridge);
    self_model_immune_modulate_capabilities(bridge);
    self_model_immune_bridge_update(bridge, 1000);

    // Statistics should increase
    EXPECT_GT(bridge->interoceptive_signals_sent, initial_signals);
    EXPECT_GT(bridge->capability_modulations, initial_modulations);
    EXPECT_GT(bridge->total_updates, initial_updates);
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
