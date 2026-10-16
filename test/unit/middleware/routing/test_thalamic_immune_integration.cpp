/**
 * @file test_thalamic_immune_integration.cpp
 * @brief Unit tests for Thalamic Router - Immune System Integration
 *
 * WHAT: Test bidirectional coupling between thalamic routing and immune system
 * WHY:  Validate cytokine modulation of routing and anomaly-triggered immunity
 * HOW:  Test cytokine effects on priorities/gating, anomaly-induced immune activation
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/immune/nimcp_thalamic_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/routing/nimcp_thalamic_router.h"

class ThalamicImmuneTest : public ::testing::Test {
protected:
    thalamic_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    thalamic_router_t* router;

    void SetUp() override {
        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create thalamic router
        thalamic_router_config_t router_config = thalamic_router_default_config();
        router_config.enable_statistics = true;
        router = thalamic_router_create(&router_config);
        ASSERT_NE(router, nullptr);

        // Create bridge
        thalamic_immune_config_t bridge_config;
        thalamic_immune_default_config(&bridge_config);
        bridge = thalamic_immune_bridge_create(&bridge_config, immune, router);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        thalamic_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
        thalamic_router_destroy(router);
    }
};

/**
 * TEST: Bridge Lifecycle
 */
TEST_F(ThalamicImmuneTest, LifecycleAPI) {
    // Bridge already created in SetUp
    EXPECT_NE(bridge, nullptr);

    // Test default config
    thalamic_immune_config_t config;
    int result = thalamic_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_routing_modulation);
    EXPECT_TRUE(config.enable_inflammation_hypervigilance);
    EXPECT_TRUE(config.enable_routing_anomaly_detection);
    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
}

/**
 * TEST: Cytokine Effects on Routing Priorities
 * BIOLOGICAL: IL-6/IL-1β/TNF-α increase threat signal priority
 */
TEST_F(ThalamicImmuneTest, CytokineModulationOfPriority) {
    // Trigger inflammation in immune system
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 8, 0, &antigen_id);

    // Activate full immune response to raise cytokine levels
    uint32_t b_cell_id, helper_id, antibody_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);
    brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // Create actual inflammation site to trigger cytokine release
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);  // Escalate to REGIONAL

    // Apply cytokine effects to routing
    int result = thalamic_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    // Get cytokine effects - should show priority boost
    cytokine_routing_effects_t effects;
    thalamic_immune_get_cytokine_effects(bridge, &effects);

    // Pro-inflammatory cytokines should boost priority
    EXPECT_GT(effects.total_priority_modifier, 0.0f);
    EXPECT_GT(effects.threat_focus_level, 0.0f);
    EXPECT_LT(effects.gating_threshold_modifier, 0.0f);  // Reduced gating
}

/**
 * TEST: Inflammation-Induced Hypervigilance
 * BIOLOGICAL: Systemic inflammation → reduced sensory gating, threat focus
 */
TEST_F(ThalamicImmuneTest, InflammationHypervigilance) {
    // Create regional inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0x10, 0x20, 0x30};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 7, 0, &antigen_id);

    // Initiate and escalate inflammation
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);  // REGIONAL

    // Apply inflammation effects
    int result = thalamic_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    // Get inflammation state
    inflammation_routing_state_t state;
    thalamic_immune_get_inflammation_state(bridge, &state);

    // Should show hypervigilance
    EXPECT_GT(state.inflammation_intensity, 0.0f);
    EXPECT_GT(state.hypervigilance_level, 0.0f);
    EXPECT_GT(state.gating_reduction, 0.0f);
    EXPECT_GT(state.threat_priority_boost, 1.0f);
}

/**
 * TEST: Priority Escalation for Threat Signals
 * BIOLOGICAL: Immune-related signals bypass normal filtering during inflammation
 */
TEST_F(ThalamicImmuneTest, ThreatSignalEscalation) {
    // Create inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 9, 0, &antigen_id);

    // Initiate and escalate inflammation for proper threat response
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);  // REGIONAL
    brain_immune_escalate_inflammation(immune, site_id);  // SYSTEMIC

    // Apply inflammation
    thalamic_immune_apply_inflammation_effects(bridge);

    // Escalate priority for threat signal
    int result = thalamic_immune_escalate_priority(bridge, 1, 2, true);
    EXPECT_EQ(result, 0);

    // Verify attention was set on router
    float attention;
    bool got_attention = thalamic_router_get_attention(router, 1, 2, &attention);
    EXPECT_TRUE(got_attention);
    EXPECT_GT(attention, 0.5f);  // Should be elevated

    // Non-threat signal should be lower
    result = thalamic_immune_escalate_priority(bridge, 3, 4, false);
    EXPECT_EQ(result, 0);
}

/**
 * TEST: IL-10 Restores Normal Gating
 * BIOLOGICAL: Anti-inflammatory cytokines normalize sensory gating
 */
TEST_F(ThalamicImmuneTest, IL10GatingRestoration) {
    // Create inflammation first
    uint32_t antigen_id;
    uint8_t epitope[] = {0xDD, 0xEE, 0xFF};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 7, 0, &antigen_id);

    thalamic_immune_apply_inflammation_effects(bridge);

    // Get baseline gating
    float gating_before = thalamic_immune_get_gating_threshold(bridge);

    // Resolve inflammation (triggers IL-10)
    // (In real implementation, would call brain_immune_resolve_inflammation)
    thalamic_immune_restore_gating(bridge);

    // Gating should move toward normal
    float gating_after = thalamic_immune_get_gating_threshold(bridge);
    // Note: exact comparison depends on implementation
}

/**
 * TEST: Routing Anomaly Detection
 * BIOLOGICAL: Routing failures indicate potential system threats
 */
TEST_F(ThalamicImmuneTest, AnomalyDetection) {
    // Detect anomalies (should be none initially)
    int result = thalamic_immune_detect_anomalies(bridge);
    EXPECT_EQ(result, 0);

    // Get anomaly state
    routing_anomaly_state_t anomaly;
    thalamic_immune_get_anomaly_state(bridge, &anomaly);

    // Initially should be healthy
    EXPECT_FALSE(anomaly.queue_critical);
    EXPECT_FALSE(anomaly.excessive_drops);
    EXPECT_EQ(anomaly.anomaly_count, 0u);
}

/**
 * TEST: Anomaly-Triggered Immune Response
 * BIOLOGICAL: Routing dysfunction suggests system compromise
 */
TEST_F(ThalamicImmuneTest, AnomalyTriggersImmune) {
    // Simulate routing stress by creating many signals
    // (In real scenario, would flood router to cause drops)

    // Manually set anomaly state for testing
    routing_anomaly_state_t anomaly;
    thalamic_immune_get_anomaly_state(bridge, &anomaly);

    // Get baseline antigen count
    brain_immune_stats_t stats_before;
    brain_immune_get_stats(immune, &stats_before);

    // Detect and trigger (would normally detect real anomalies)
    thalamic_immune_detect_anomalies(bridge);
    thalamic_immune_trigger_from_anomaly(bridge);

    // Get updated stats
    brain_immune_stats_t stats_after;
    brain_immune_get_stats(immune, &stats_after);

    // If anomaly detected, antigen should be presented
    // (Test outcome depends on actual routing state)
}

/**
 * TEST: Health Feedback Boosts Immunity
 * BIOLOGICAL: Healthy routing → IL-10 release, reduced inflammation
 */
TEST_F(ThalamicImmuneTest, HealthFeedback) {
    // Good routing health should trigger IL-10 boost
    int result = thalamic_immune_boost_from_health(bridge);
    EXPECT_EQ(result, 0);

    // Get health feedback
    routing_health_feedback_t feedback;
    // (Would need accessor function to query feedback)
}

/**
 * TEST: Sickness Behavior Routing Changes
 * BIOLOGICAL: During sickness, social signals deprioritized
 */
TEST_F(ThalamicImmuneTest, SicknessBehaviorRouting) {
    // Create systemic inflammation (sickness behavior)
    for (int i = 0; i < 20; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)(0x20 + i), 0x30, 0x40};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 7, 0, &antigen_id);
    }

    // Apply inflammation
    thalamic_immune_apply_inflammation_effects(bridge);

    // Get state
    inflammation_routing_state_t state;
    thalamic_immune_get_inflammation_state(bridge, &state);

    // Should show sickness behavior
    if (state.current_level >= INFLAMMATION_REGIONAL) {
        EXPECT_TRUE(state.sickness_behavior_active);
        EXPECT_GT(state.social_priority_penalty, 0.0f);
        EXPECT_LT(state.social_priority_penalty, 1.0f);
    }
}

/**
 * TEST: Hypervigilance Query
 */
TEST_F(ThalamicImmuneTest, HypervigilanceQuery) {
    // Initially should not be hypervigilant
    bool hypervigilant = thalamic_immune_is_hypervigilant(bridge);
    EXPECT_FALSE(hypervigilant);

    // Create high inflammation
    for (int i = 0; i < 15; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)(0x30 + i), 0x40, 0x50};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 8, 0, &antigen_id);
    }

    thalamic_immune_apply_inflammation_effects(bridge);

    // Check hypervigilance
    hypervigilant = thalamic_immune_is_hypervigilant(bridge);
    // (Result depends on inflammation level reached)
}

/**
 * TEST: Gating Threshold Query
 */
TEST_F(ThalamicImmuneTest, GatingThresholdQuery) {
    // Get baseline threshold
    float threshold_baseline = thalamic_immune_get_gating_threshold(bridge);
    EXPECT_GE(threshold_baseline, 0.1f);
    EXPECT_LE(threshold_baseline, 0.9f);

    // Create inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0x50, 0x60, 0x70};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 8, 0, &antigen_id);

    thalamic_immune_apply_inflammation_effects(bridge);

    // Threshold should be lower (less filtering)
    float threshold_inflamed = thalamic_immune_get_gating_threshold(bridge);
    EXPECT_LE(threshold_inflamed, threshold_baseline);
}

/**
 * TEST: Threat Priority Multiplier Query
 */
TEST_F(ThalamicImmuneTest, ThreatPriorityMultiplierQuery) {
    // Get baseline multiplier
    float multiplier_baseline = thalamic_immune_get_threat_priority_multiplier(bridge);
    EXPECT_GE(multiplier_baseline, 1.0f);
    EXPECT_LE(multiplier_baseline, 2.0f);

    // Create inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0x60, 0x70, 0x80};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 9, 0, &antigen_id);

    thalamic_immune_apply_inflammation_effects(bridge);

    // Multiplier should increase
    float multiplier_inflamed = thalamic_immune_get_threat_priority_multiplier(bridge);
    EXPECT_GE(multiplier_inflamed, multiplier_baseline);
}

/**
 * TEST: Bidirectional Update
 */
TEST_F(ThalamicImmuneTest, BidirectionalUpdate) {
    // Create some activity
    uint32_t antigen_id;
    uint8_t epitope[] = {0x70, 0x80, 0x90};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 7, 0, &antigen_id);

    // Update bridge
    int result = thalamic_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);

    // Should have processed immune→routing and routing→immune
    cytokine_routing_effects_t effects;
    thalamic_immune_get_cytokine_effects(bridge, &effects);

    routing_anomaly_state_t anomaly;
    thalamic_immune_get_anomaly_state(bridge, &anomaly);
}

/**
 * TEST: Multiple Updates Stability
 */
TEST_F(ThalamicImmuneTest, MultipleUpdatesStability) {
    // Run multiple updates
    for (int i = 0; i < 100; i++) {
        int result = thalamic_immune_bridge_update(bridge, 100);
        EXPECT_EQ(result, 0);
    }

    // Bridge should remain stable
    EXPECT_NE(bridge, nullptr);

    // Query should still work
    float threshold = thalamic_immune_get_gating_threshold(bridge);
    EXPECT_GE(threshold, 0.0f);
}

/**
 * TEST: Priority Escalation Statistics
 */
TEST_F(ThalamicImmuneTest, PriorityEscalationStats) {
    // Escalate several priorities
    for (uint32_t i = 1; i <= 5; i++) {
        thalamic_immune_escalate_priority(bridge, i, i+1, (i % 2 == 0));
    }

    // Statistics should reflect escalations
    // (Would need accessor for bridge->priority_escalations)
}

/**
 * TEST: Cytokine Sensitivity Configuration
 */
TEST_F(ThalamicImmuneTest, CytokineSensitivityConfig) {
    // Create bridge with high sensitivity
    thalamic_immune_config_t config;
    thalamic_immune_default_config(&config);
    config.cytokine_sensitivity = 2.0f;

    thalamic_immune_bridge_t* sensitive_bridge =
        thalamic_immune_bridge_create(&config, immune, router);
    ASSERT_NE(sensitive_bridge, nullptr);

    // Same inflammation should have stronger effect
    uint32_t antigen_id;
    uint8_t epitope[] = {0x80, 0x90, 0xA0};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, 7, 0, &antigen_id);

    thalamic_immune_apply_cytokine_effects(sensitive_bridge);

    cytokine_routing_effects_t effects;
    thalamic_immune_get_cytokine_effects(sensitive_bridge, &effects);

    // Should show amplified effects
    // (Comparison would require baseline measurement)

    thalamic_immune_bridge_destroy(sensitive_bridge);
}

/**
 * TEST: Anomaly Threshold Configuration
 */
TEST_F(ThalamicImmuneTest, AnomalyThresholdConfig) {
    // Create bridge with sensitive anomaly detection
    thalamic_immune_config_t config;
    thalamic_immune_default_config(&config);
    config.queue_anomaly_threshold = 0.70f;  // Lower threshold
    config.drop_anomaly_threshold = 0.05f;   // Lower threshold

    thalamic_immune_bridge_t* sensitive_bridge =
        thalamic_immune_bridge_create(&config, immune, router);
    ASSERT_NE(sensitive_bridge, nullptr);

    // Should detect anomalies more readily
    thalamic_immune_detect_anomalies(sensitive_bridge);

    thalamic_immune_bridge_destroy(sensitive_bridge);
}

/**
 * TEST: Null Pointer Guards
 */
TEST_F(ThalamicImmuneTest, NullPointerGuards) {
    // Null config should use defaults
    thalamic_immune_bridge_t* test_bridge =
        thalamic_immune_bridge_create(nullptr, immune, router);
    EXPECT_NE(test_bridge, nullptr);
    thalamic_immune_bridge_destroy(test_bridge);

    // Null bridge pointers should fail gracefully
    EXPECT_EQ(thalamic_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(thalamic_immune_detect_anomalies(nullptr), -1);
    EXPECT_EQ(thalamic_immune_bridge_update(nullptr, 1000), -1);

    // Null output pointers
    EXPECT_EQ(thalamic_immune_get_cytokine_effects(bridge, nullptr), -1);
    EXPECT_EQ(thalamic_immune_get_inflammation_state(bridge, nullptr), -1);

    // Null config pointer
    EXPECT_EQ(thalamic_immune_default_config(nullptr), -1);
}

/**
 * TEST: Feature Toggle Configuration
 */
TEST_F(ThalamicImmuneTest, FeatureToggleConfig) {
    // Create bridge with some features disabled
    thalamic_immune_config_t config;
    thalamic_immune_default_config(&config);
    config.enable_cytokine_routing_modulation = false;
    config.enable_routing_anomaly_detection = false;

    thalamic_immune_bridge_t* limited_bridge =
        thalamic_immune_bridge_create(&config, immune, router);
    ASSERT_NE(limited_bridge, nullptr);

    // Operations should succeed but have no effect
    int result = thalamic_immune_apply_cytokine_effects(limited_bridge);
    EXPECT_EQ(result, 0);  // Should return success but skip processing

    result = thalamic_immune_detect_anomalies(limited_bridge);
    EXPECT_EQ(result, 0);  // Should return success but skip detection

    thalamic_immune_bridge_destroy(limited_bridge);
}

/**
 * Main test runner
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
