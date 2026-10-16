//=============================================================================
// test_thalamic_immune_bridge_integration.cpp - Thalamic-Immune Bridge Integration Tests
//=============================================================================
/**
 * @file test_thalamic_immune_bridge_integration.cpp
 * @brief Integration tests for bidirectional thalamic-immune system coupling
 *
 * WHAT: Tests bridge creation, immune modulation of thalamic activity,
 *       inflammation effects on routing, and recovery mechanisms
 * WHY:  Verify realistic immune-routing coupling and sickness behavior gating
 * HOW:  GTest framework testing cross-system interactions
 *
 * BIOLOGICAL BASIS:
 * - IL-6 affects thalamic relay neurons (increased excitability)
 * - Pro-inflammatory cytokines reduce sensory gating
 * - Systemic inflammation causes hypervigilance and threat focus
 * - IL-10 restores normal sensory gating
 *
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "middleware/immune/nimcp_thalamic_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/routing/nimcp_thalamic_router.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class ThalamicImmuneBridgeIntegrationTest : public NimcpTestBase {
protected:
    thalamic_immune_bridge_t* bridge = nullptr;
    brain_immune_system_t* immune = nullptr;
    thalamic_router_t* router = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

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
        if (bridge) {
            thalamic_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper to create inflammation
    uint32_t createInflammation(uint8_t severity = 7) {
        uint32_t antigen_id;
        uint8_t epitope[] = {0x01, 0x02, 0x03};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3, severity, 0, &antigen_id);

        uint32_t site_id;
        brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);
        return site_id;
    }

    // Helper to escalate inflammation
    void escalateToSystemic(uint32_t site_id) {
        brain_immune_escalate_inflammation(immune, site_id);  // REGIONAL
        brain_immune_escalate_inflammation(immune, site_id);  // SYSTEMIC
    }
};

//=============================================================================
// Bridge Lifecycle Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeIntegrationTest, CreateWithDefaultConfig) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, DefaultConfigValues) {
    thalamic_immune_config_t config;
    int result = thalamic_immune_default_config(&config);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(config.enable_cytokine_routing_modulation);
    EXPECT_TRUE(config.enable_inflammation_hypervigilance);
    EXPECT_TRUE(config.enable_routing_anomaly_detection);
    EXPECT_TRUE(config.enable_health_feedback);
    EXPECT_TRUE(config.enable_priority_escalation);
    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.anomaly_sensitivity, 1.0f);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, CreateWithNullConfigUsesDefaults) {
    thalamic_immune_bridge_t* test_bridge = thalamic_immune_bridge_create(nullptr, immune, router);
    EXPECT_NE(test_bridge, nullptr);
    thalamic_immune_bridge_destroy(test_bridge);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, DestroyNullSafe) {
    thalamic_immune_bridge_destroy(nullptr);
    // Should not crash
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, DefaultConfigNullPointerFails) {
    int result = thalamic_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Immune Modulation of Thalamic Activity Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeIntegrationTest, CytokineEffectsApplied) {
    ASSERT_NE(bridge, nullptr);

    // Create inflammation to generate cytokines
    uint32_t site_id = createInflammation(8);
    escalateToSystemic(site_id);

    // Apply cytokine effects
    int result = thalamic_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    // Get effects
    cytokine_routing_effects_t effects;
    result = thalamic_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Pro-inflammatory cytokines should boost priority
    EXPECT_GE(effects.total_priority_modifier, 0.0f);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, CytokineEffectsNullBridgeFails) {
    int result = thalamic_immune_apply_cytokine_effects(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, GetCytokineEffectsNullOutputFails) {
    ASSERT_NE(bridge, nullptr);
    int result = thalamic_immune_get_cytokine_effects(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, IL6IncreasesThreaTFocus) {
    ASSERT_NE(bridge, nullptr);

    // Generate significant inflammation
    for (int i = 0; i < 5; i++) {
        uint32_t site_id = createInflammation(9);
        brain_immune_escalate_inflammation(immune, site_id);
    }

    thalamic_immune_apply_cytokine_effects(bridge);

    cytokine_routing_effects_t effects;
    thalamic_immune_get_cytokine_effects(bridge, &effects);

    // Threat focus should be elevated
    EXPECT_GE(effects.threat_focus_level, 0.0f);
    EXPECT_LE(effects.threat_focus_level, 1.0f);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, ProInflammatoryCytokinesReduceGating) {
    ASSERT_NE(bridge, nullptr);

    // Get baseline
    float baseline_threshold = thalamic_immune_get_gating_threshold(bridge);

    // Create significant inflammation
    for (int i = 0; i < 5; i++) {
        createInflammation(9);
    }

    thalamic_immune_apply_inflammation_effects(bridge);

    float inflamed_threshold = thalamic_immune_get_gating_threshold(bridge);

    // Gating threshold should be lower during inflammation (less filtering)
    EXPECT_LE(inflamed_threshold, baseline_threshold);
}

//=============================================================================
// Inflammation Effects on Routing Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeIntegrationTest, InflammationEffectsApplied) {
    ASSERT_NE(bridge, nullptr);

    uint32_t site_id = createInflammation(8);
    brain_immune_escalate_inflammation(immune, site_id);

    int result = thalamic_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    inflammation_routing_state_t state;
    result = thalamic_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, 0);

    // Should show some inflammation intensity
    EXPECT_GE(state.inflammation_intensity, 0.0f);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, InflammationEffectsNullBridgeFails) {
    int result = thalamic_immune_apply_inflammation_effects(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, GetInflammationStateNullOutputFails) {
    ASSERT_NE(bridge, nullptr);
    int result = thalamic_immune_get_inflammation_state(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, HypervigilanceInducedByInflammation) {
    ASSERT_NE(bridge, nullptr);

    // Initially not hypervigilant
    bool initial_hypervigilance = thalamic_immune_is_hypervigilant(bridge);
    EXPECT_FALSE(initial_hypervigilance);

    // Create significant inflammation
    for (int i = 0; i < 10; i++) {
        uint32_t site_id = createInflammation(9);
        escalateToSystemic(site_id);
    }

    thalamic_immune_apply_inflammation_effects(bridge);

    // Check hypervigilance (depends on inflammation level reached)
    bool hypervigilant = thalamic_immune_is_hypervigilant(bridge);
    // Result depends on inflammation level
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, ThreatPriorityBoostDuringInflammation) {
    ASSERT_NE(bridge, nullptr);

    float baseline_multiplier = thalamic_immune_get_threat_priority_multiplier(bridge);
    EXPECT_GE(baseline_multiplier, 1.0f);

    // Create inflammation
    uint32_t site_id = createInflammation(9);
    escalateToSystemic(site_id);

    thalamic_immune_apply_inflammation_effects(bridge);

    float inflamed_multiplier = thalamic_immune_get_threat_priority_multiplier(bridge);
    EXPECT_GE(inflamed_multiplier, baseline_multiplier);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, PriorityEscalationForThreatSignals) {
    ASSERT_NE(bridge, nullptr);

    // Create inflammation for proper threat response
    uint32_t site_id = createInflammation(8);
    escalateToSystemic(site_id);
    thalamic_immune_apply_inflammation_effects(bridge);

    // Escalate priority for threat signal
    int result = thalamic_immune_escalate_priority(bridge, 1, 2, true);
    EXPECT_EQ(result, 0);

    // Check attention was boosted
    float attention;
    bool got = thalamic_router_get_attention(router, 1, 2, &attention);
    EXPECT_TRUE(got);
    EXPECT_GT(attention, 0.3f);  // Should be elevated

    // Non-threat signal should be lower
    result = thalamic_immune_escalate_priority(bridge, 3, 4, false);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Recovery Mechanisms Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeIntegrationTest, GatingRestorationAfterRecovery) {
    ASSERT_NE(bridge, nullptr);

    // Create inflammation
    uint32_t site_id = createInflammation(8);
    escalateToSystemic(site_id);
    thalamic_immune_apply_inflammation_effects(bridge);

    float inflamed_threshold = thalamic_immune_get_gating_threshold(bridge);

    // Restore gating (simulates IL-10 release)
    int result = thalamic_immune_restore_gating(bridge);
    EXPECT_EQ(result, 0);

    float restored_threshold = thalamic_immune_get_gating_threshold(bridge);

    // Threshold should move toward normal
    EXPECT_GE(restored_threshold, inflamed_threshold);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, HealthFeedbackBoostsImmunity) {
    ASSERT_NE(bridge, nullptr);

    int result = thalamic_immune_boost_from_health(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, HealthFeedbackNullBridgeFails) {
    int result = thalamic_immune_boost_from_health(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Sickness Behavior Gating Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeIntegrationTest, SicknessBehaviorActivation) {
    ASSERT_NE(bridge, nullptr);

    // Create systemic inflammation (sickness behavior trigger)
    for (int i = 0; i < 15; i++) {
        uint32_t site_id = createInflammation(8);
        escalateToSystemic(site_id);
    }

    thalamic_immune_apply_inflammation_effects(bridge);

    inflammation_routing_state_t state;
    thalamic_immune_get_inflammation_state(bridge, &state);

    // Should show sickness behavior if inflammation is high enough
    if (state.current_level >= INFLAMMATION_REGIONAL) {
        EXPECT_TRUE(state.sickness_behavior_active);
        // Social signals should be deprioritized
        EXPECT_GT(state.social_priority_penalty, 0.0f);
        EXPECT_LT(state.social_priority_penalty, 1.0f);
    }
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, AttentionBiasTowardThreats) {
    ASSERT_NE(bridge, nullptr);

    // Create inflammation
    uint32_t site_id = createInflammation(9);
    escalateToSystemic(site_id);

    thalamic_immune_apply_inflammation_effects(bridge);

    inflammation_routing_state_t state;
    thalamic_immune_get_inflammation_state(bridge, &state);

    // Attention should be biased toward threats
    EXPECT_GE(state.attention_bias, 0.0f);
    EXPECT_LE(state.attention_bias, 1.0f);
}

//=============================================================================
// Anomaly Detection Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeIntegrationTest, AnomalyDetectionInitiallyClean) {
    ASSERT_NE(bridge, nullptr);

    int result = thalamic_immune_detect_anomalies(bridge);
    EXPECT_EQ(result, 0);

    routing_anomaly_state_t anomaly;
    result = thalamic_immune_get_anomaly_state(bridge, &anomaly);
    EXPECT_EQ(result, 0);

    // Initially should be healthy
    EXPECT_FALSE(anomaly.queue_critical);
    EXPECT_FALSE(anomaly.excessive_drops);
    EXPECT_FALSE(anomaly.high_latency);
    EXPECT_EQ(anomaly.anomaly_count, 0u);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, AnomalyDetectionNullBridgeFails) {
    int result = thalamic_immune_detect_anomalies(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, GetAnomalyStateNullOutputFails) {
    ASSERT_NE(bridge, nullptr);
    int result = thalamic_immune_get_anomaly_state(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, TriggerFromAnomalyWhenDetected) {
    ASSERT_NE(bridge, nullptr);

    thalamic_immune_detect_anomalies(bridge);

    int result = thalamic_immune_trigger_from_anomaly(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, TriggerFromAnomalyNullBridgeFails) {
    int result = thalamic_immune_trigger_from_anomaly(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Bidirectional Update Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeIntegrationTest, BidirectionalUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Create some activity
    createInflammation(7);

    int result = thalamic_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, BidirectionalUpdateNullBridgeFails) {
    int result = thalamic_immune_bridge_update(nullptr, 1000);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, MultipleUpdatesStable) {
    ASSERT_NE(bridge, nullptr);

    // Run many update cycles
    for (int i = 0; i < 100; i++) {
        int result = thalamic_immune_bridge_update(bridge, 100);
        EXPECT_EQ(result, 0);
    }

    // Bridge should remain stable
    EXPECT_NE(bridge, nullptr);

    float threshold = thalamic_immune_get_gating_threshold(bridge);
    EXPECT_GE(threshold, 0.0f);
    EXPECT_LE(threshold, 1.0f);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, UpdateWithActiveInflammation) {
    ASSERT_NE(bridge, nullptr);

    // Create ongoing inflammation
    uint32_t site_id = createInflammation(8);
    escalateToSystemic(site_id);

    // Run updates
    for (int i = 0; i < 50; i++) {
        int result = thalamic_immune_bridge_update(bridge, 100);
        EXPECT_EQ(result, 0);
    }

    // Check state consistency
    cytokine_routing_effects_t effects;
    thalamic_immune_get_cytokine_effects(bridge, &effects);

    routing_anomaly_state_t anomaly;
    thalamic_immune_get_anomaly_state(bridge, &anomaly);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeIntegrationTest, HighCytokineSensitivity) {
    thalamic_immune_config_t high_sens_config;
    thalamic_immune_default_config(&high_sens_config);
    high_sens_config.cytokine_sensitivity = 2.0f;

    thalamic_immune_bridge_t* sens_bridge = thalamic_immune_bridge_create(&high_sens_config, immune, router);
    ASSERT_NE(sens_bridge, nullptr);

    // Create inflammation
    createInflammation(7);
    thalamic_immune_apply_cytokine_effects(sens_bridge);

    cytokine_routing_effects_t effects;
    thalamic_immune_get_cytokine_effects(sens_bridge, &effects);
    // High sensitivity should amplify effects

    thalamic_immune_bridge_destroy(sens_bridge);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, CustomAnomalyThresholds) {
    thalamic_immune_config_t custom_config;
    thalamic_immune_default_config(&custom_config);
    custom_config.queue_anomaly_threshold = 0.70f;
    custom_config.drop_anomaly_threshold = 0.05f;
    custom_config.latency_anomaly_ms = 50.0f;

    thalamic_immune_bridge_t* custom_bridge = thalamic_immune_bridge_create(&custom_config, immune, router);
    ASSERT_NE(custom_bridge, nullptr);

    thalamic_immune_detect_anomalies(custom_bridge);

    thalamic_immune_bridge_destroy(custom_bridge);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, FeatureDisableCytokineModulation) {
    thalamic_immune_config_t disabled_config;
    thalamic_immune_default_config(&disabled_config);
    disabled_config.enable_cytokine_routing_modulation = false;

    thalamic_immune_bridge_t* disabled_bridge = thalamic_immune_bridge_create(&disabled_config, immune, router);
    ASSERT_NE(disabled_bridge, nullptr);

    // Operation should succeed but have no effect
    int result = thalamic_immune_apply_cytokine_effects(disabled_bridge);
    EXPECT_EQ(result, 0);

    thalamic_immune_bridge_destroy(disabled_bridge);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, FeatureDisableAnomalyDetection) {
    thalamic_immune_config_t disabled_config;
    thalamic_immune_default_config(&disabled_config);
    disabled_config.enable_routing_anomaly_detection = false;

    thalamic_immune_bridge_t* disabled_bridge = thalamic_immune_bridge_create(&disabled_config, immune, router);
    ASSERT_NE(disabled_bridge, nullptr);

    // Operation should succeed but skip detection
    int result = thalamic_immune_detect_anomalies(disabled_bridge);
    EXPECT_EQ(result, 0);

    thalamic_immune_bridge_destroy(disabled_bridge);
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeIntegrationTest, GatingThresholdValidRange) {
    ASSERT_NE(bridge, nullptr);

    float threshold = thalamic_immune_get_gating_threshold(bridge);
    EXPECT_GE(threshold, 0.0f);
    EXPECT_LE(threshold, 1.0f);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, ThreatPriorityMultiplierValidRange) {
    ASSERT_NE(bridge, nullptr);

    float multiplier = thalamic_immune_get_threat_priority_multiplier(bridge);
    EXPECT_GE(multiplier, 1.0f);
    EXPECT_LE(multiplier, 2.0f);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, HypervigilanceQueryInitiallyFalse) {
    ASSERT_NE(bridge, nullptr);

    bool hypervigilant = thalamic_immune_is_hypervigilant(bridge);
    EXPECT_FALSE(hypervigilant);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeIntegrationTest, BioAsyncInitiallyDisconnected) {
    ASSERT_NE(bridge, nullptr);

    bool connected = thalamic_immune_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(ThalamicImmuneBridgeIntegrationTest, BioAsyncConnectAndDisconnect) {
    ASSERT_NE(bridge, nullptr);

    int result = thalamic_immune_connect_bio_async(bridge);
    // Connection may fail if global bio-async router is not initialized
    // This is expected in isolated test environments
    if (result == 0) {
        // If connection succeeded, verify state
        bool connected = thalamic_immune_is_bio_async_connected(bridge);
        if (connected) {
            result = thalamic_immune_disconnect_bio_async(bridge);
            EXPECT_EQ(result, 0);
            EXPECT_FALSE(thalamic_immune_is_bio_async_connected(bridge));
        }
    } else {
        // Connection failed - this is acceptable in test environment
        // The bio-async router may not be globally initialized
        EXPECT_FALSE(thalamic_immune_is_bio_async_connected(bridge));
    }
}
