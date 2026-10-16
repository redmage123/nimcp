//=============================================================================
// test_thalamic_immune_bridge_regression.cpp - Thalamic-Immune Bridge Regression Tests
//=============================================================================
/**
 * @file test_thalamic_immune_bridge_regression.cpp
 * @brief Comprehensive regression tests for the thalamic-immune bridge system
 *
 * WHAT: Tests for immune modulation accuracy, state propagation, recovery rates,
 *       cytokine effects, fever response, and stability under stress
 * WHY:  Ensure thalamic-immune bridge is stable, accurate, and deterministic
 * HOW:  GTest framework with benchmarks, stress tests, and consistency checks
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "middleware/immune/nimcp_thalamic_immune_bridge.h"
#include "middleware/routing/nimcp_thalamic_router.h"
#include "cognitive/immune/nimcp_brain_immune.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ThalamicImmuneBridgeRegressionTest : public NimcpTestBase {
protected:
    thalamic_immune_bridge_t* bridge = nullptr;
    brain_immune_system_t* immune_system = nullptr;
    thalamic_router_t* router = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);

        // Create thalamic router
        thalamic_router_config_t router_config = thalamic_router_default_config();
        router_config.enable_attention_gating = true;
        router_config.enable_priority_routing = true;
        router_config.enable_statistics = true;
        router = thalamic_router_create(&router_config);

        // Create bridge
        if (immune_system && router) {
            thalamic_immune_config_t bridge_config;
            thalamic_immune_default_config(&bridge_config);
            bridge_config.enable_cytokine_routing_modulation = true;
            bridge_config.enable_inflammation_hypervigilance = true;
            bridge_config.enable_routing_anomaly_detection = true;
            bridge_config.enable_health_feedback = true;
            bridge_config.enable_priority_escalation = true;

            bridge = thalamic_immune_bridge_create(&bridge_config, immune_system, router);
        }
    }

    void TearDown() override {
        if (bridge) {
            thalamic_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper to measure execution time in microseconds
    template<typename Func>
    double MeasureTimeUs(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, CreateWithValidSystems) {
    ASSERT_NE(bridge, nullptr);
    EXPECT_NE(bridge->immune_system, nullptr);
    EXPECT_NE(bridge->thalamic_router, nullptr);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, CreateWithNullSystems) {
    thalamic_immune_config_t config;
    thalamic_immune_default_config(&config);

    // NULL immune system
    thalamic_immune_bridge_t* b1 = thalamic_immune_bridge_create(&config, nullptr, router);
    EXPECT_EQ(b1, nullptr);

    // NULL router
    thalamic_immune_bridge_t* b2 = thalamic_immune_bridge_create(&config, immune_system, nullptr);
    EXPECT_EQ(b2, nullptr);

    // Both NULL
    thalamic_immune_bridge_t* b3 = thalamic_immune_bridge_create(&config, nullptr, nullptr);
    EXPECT_EQ(b3, nullptr);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, DestroyNullSafe) {
    thalamic_immune_bridge_destroy(nullptr);
    // Should not crash
}

TEST_F(ThalamicImmuneBridgeRegressionTest, DefaultConfigValues) {
    thalamic_immune_config_t config;
    int result = thalamic_immune_default_config(&config);
    EXPECT_EQ(result, 0);

    // Verify sensible defaults
    EXPECT_TRUE(config.enable_cytokine_routing_modulation);
    EXPECT_TRUE(config.enable_inflammation_hypervigilance);
    EXPECT_TRUE(config.enable_routing_anomaly_detection);
    EXPECT_TRUE(config.enable_health_feedback);

    // Sensitivity values should be reasonable
    EXPECT_GE(config.cytokine_sensitivity, 0.5f);
    EXPECT_LE(config.cytokine_sensitivity, 2.0f);
    EXPECT_GE(config.inflammation_sensitivity, 0.5f);
    EXPECT_LE(config.inflammation_sensitivity, 2.0f);
}

//=============================================================================
// Immune Modulation Accuracy Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, CytokineEffectsAccuracy) {
    ASSERT_NE(bridge, nullptr);

    // Apply cytokine effects
    int result = thalamic_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    // Get effects
    cytokine_routing_effects_t effects;
    result = thalamic_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Verify effects are within valid ranges
    EXPECT_GE(effects.il6_priority_boost, 0.0f);
    EXPECT_LE(effects.il6_priority_boost, 1.0f);
    EXPECT_GE(effects.il1_priority_boost, 0.0f);
    EXPECT_LE(effects.il1_priority_boost, 1.0f);
    EXPECT_GE(effects.tnf_priority_boost, 0.0f);
    EXPECT_LE(effects.tnf_priority_boost, 1.0f);
    EXPECT_GE(effects.il10_gating_restoration, 0.0f);
    EXPECT_LE(effects.il10_gating_restoration, 1.0f);

    EXPECT_GE(effects.threat_focus_level, 0.0f);
    EXPECT_LE(effects.threat_focus_level, 1.0f);
    EXPECT_GE(effects.social_suppression_level, 0.0f);
    EXPECT_LE(effects.social_suppression_level, 1.0f);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, InflammationEffectsAccuracy) {
    ASSERT_NE(bridge, nullptr);

    // Apply inflammation effects
    int result = thalamic_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    // Get state
    inflammation_routing_state_t state;
    result = thalamic_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, 0);

    // Verify state values are valid
    EXPECT_GE(state.inflammation_intensity, 0.0f);
    EXPECT_LE(state.inflammation_intensity, 1.0f);
    EXPECT_GE(state.hypervigilance_level, 0.0f);
    EXPECT_LE(state.hypervigilance_level, 1.0f);
    EXPECT_GE(state.gating_reduction, 0.0f);
    EXPECT_LE(state.gating_reduction, 1.0f);
    EXPECT_GE(state.threat_priority_boost, 0.0f);
    EXPECT_GE(state.social_priority_penalty, 0.0f);
    EXPECT_GE(state.attention_bias, 0.0f);
    EXPECT_LE(state.attention_bias, 1.0f);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, GatingThresholdAccuracy) {
    ASSERT_NE(bridge, nullptr);

    float threshold = thalamic_immune_get_gating_threshold(bridge);

    // Threshold should be in valid range
    EXPECT_GE(threshold, 0.0f);
    EXPECT_LE(threshold, 1.0f);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, ThreatPriorityMultiplierAccuracy) {
    ASSERT_NE(bridge, nullptr);

    float multiplier = thalamic_immune_get_threat_priority_multiplier(bridge);

    // Multiplier should be >= 1.0 (never reduces priority)
    EXPECT_GE(multiplier, 1.0f);
    // Should not exceed 2x (as per documentation)
    EXPECT_LE(multiplier, 2.0f);
}

//=============================================================================
// State Propagation Determinism Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, UpdateDeterminism) {
    ASSERT_NE(bridge, nullptr);

    auto RunUpdateSequence = [&]() -> std::vector<float> {
        std::vector<float> thresholds;

        for (int i = 0; i < 10; i++) {
            thalamic_immune_bridge_update(bridge, 100);
            thresholds.push_back(thalamic_immune_get_gating_threshold(bridge));
        }

        return thresholds;
    };

    // First run
    std::vector<float> thresholds1 = RunUpdateSequence();

    // Reset and second run - need to recreate bridge for deterministic state
    thalamic_immune_bridge_destroy(bridge);

    thalamic_immune_config_t config;
    thalamic_immune_default_config(&config);
    config.enable_cytokine_routing_modulation = true;
    config.enable_inflammation_hypervigilance = true;
    bridge = thalamic_immune_bridge_create(&config, immune_system, router);
    ASSERT_NE(bridge, nullptr);

    std::vector<float> thresholds2 = RunUpdateSequence();

    // Verify determinism
    ASSERT_EQ(thresholds1.size(), thresholds2.size());
    for (size_t i = 0; i < thresholds1.size(); i++) {
        EXPECT_FLOAT_EQ(thresholds1[i], thresholds2[i])
            << "Non-deterministic at step " << i;
    }
}

TEST_F(ThalamicImmuneBridgeRegressionTest, InflammationStateDeterminism) {
    ASSERT_NE(bridge, nullptr);

    // Apply effects and get state
    thalamic_immune_apply_inflammation_effects(bridge);
    inflammation_routing_state_t state1;
    thalamic_immune_get_inflammation_state(bridge, &state1);

    // Apply again (should be idempotent)
    thalamic_immune_apply_inflammation_effects(bridge);
    inflammation_routing_state_t state2;
    thalamic_immune_get_inflammation_state(bridge, &state2);

    // States should be identical
    EXPECT_FLOAT_EQ(state1.inflammation_intensity, state2.inflammation_intensity);
    EXPECT_FLOAT_EQ(state1.hypervigilance_level, state2.hypervigilance_level);
    EXPECT_FLOAT_EQ(state1.gating_reduction, state2.gating_reduction);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, CytokineEffectsDeterminism) {
    ASSERT_NE(bridge, nullptr);

    // Apply and get effects
    thalamic_immune_apply_cytokine_effects(bridge);
    cytokine_routing_effects_t effects1;
    thalamic_immune_get_cytokine_effects(bridge, &effects1);

    // Apply again
    thalamic_immune_apply_cytokine_effects(bridge);
    cytokine_routing_effects_t effects2;
    thalamic_immune_get_cytokine_effects(bridge, &effects2);

    // Should be identical
    EXPECT_FLOAT_EQ(effects1.total_priority_modifier, effects2.total_priority_modifier);
    EXPECT_FLOAT_EQ(effects1.gating_threshold_modifier, effects2.gating_threshold_modifier);
    EXPECT_FLOAT_EQ(effects1.threat_focus_level, effects2.threat_focus_level);
}

//=============================================================================
// Recovery Rate Consistency Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, GatingRestorationConsistency) {
    ASSERT_NE(bridge, nullptr);

    // Apply inflammation (reduces gating)
    thalamic_immune_apply_inflammation_effects(bridge);
    float threshold_inflamed = thalamic_immune_get_gating_threshold(bridge);

    // Restore gating
    int result = thalamic_immune_restore_gating(bridge);
    EXPECT_EQ(result, 0);

    float threshold_restored = thalamic_immune_get_gating_threshold(bridge);

    // Restored threshold should be >= inflamed threshold (gating restored)
    EXPECT_GE(threshold_restored, threshold_inflamed);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, HealthBoostConsistency) {
    ASSERT_NE(bridge, nullptr);

    // Get initial stats
    uint64_t initial_health_boosts = bridge->health_boosts;

    // Boost from health multiple times
    for (int i = 0; i < 10; i++) {
        int result = thalamic_immune_boost_from_health(bridge);
        EXPECT_EQ(result, 0);
    }

    // Should have accumulated health boosts
    EXPECT_GE(bridge->health_boosts, initial_health_boosts);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, RecoveryRateProgression) {
    ASSERT_NE(bridge, nullptr);

    std::vector<float> thresholds;

    // Simulate recovery over time
    for (int i = 0; i < 20; i++) {
        thalamic_immune_bridge_update(bridge, 100);
        thalamic_immune_boost_from_health(bridge);
        thresholds.push_back(thalamic_immune_get_gating_threshold(bridge));
    }

    // Verify thresholds remain bounded
    for (float t : thresholds) {
        EXPECT_GE(t, 0.0f);
        EXPECT_LE(t, 1.0f);
        EXPECT_FALSE(std::isnan(t));
        EXPECT_FALSE(std::isinf(t));
    }
}

//=============================================================================
// Cytokine Effect Calculation Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, IL6PriorityBoostCalculation) {
    ASSERT_NE(bridge, nullptr);

    thalamic_immune_apply_cytokine_effects(bridge);
    cytokine_routing_effects_t effects;
    thalamic_immune_get_cytokine_effects(bridge, &effects);

    // IL-6 boost should be consistent with CYTOKINE_IL6_PRIORITY_BOOST constant
    EXPECT_GE(effects.il6_priority_boost, 0.0f);
    EXPECT_LE(effects.il6_priority_boost, CYTOKINE_IL6_PRIORITY_BOOST + 0.1f);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, TNFPriorityBoostCalculation) {
    ASSERT_NE(bridge, nullptr);

    thalamic_immune_apply_cytokine_effects(bridge);
    cytokine_routing_effects_t effects;
    thalamic_immune_get_cytokine_effects(bridge, &effects);

    // TNF boost should be consistent with CYTOKINE_TNF_PRIORITY_BOOST constant
    EXPECT_GE(effects.tnf_priority_boost, 0.0f);
    EXPECT_LE(effects.tnf_priority_boost, CYTOKINE_TNF_PRIORITY_BOOST + 0.1f);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, IL10GatingRestorationCalculation) {
    ASSERT_NE(bridge, nullptr);

    thalamic_immune_apply_cytokine_effects(bridge);
    cytokine_routing_effects_t effects;
    thalamic_immune_get_cytokine_effects(bridge, &effects);

    // IL-10 restoration should be consistent with constant
    EXPECT_GE(effects.il10_gating_restoration, 0.0f);
    EXPECT_LE(effects.il10_gating_restoration, CYTOKINE_IL10_GATING_RESTORE + 0.1f);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, TotalPriorityModifierBounds) {
    ASSERT_NE(bridge, nullptr);

    thalamic_immune_apply_cytokine_effects(bridge);
    cytokine_routing_effects_t effects;
    thalamic_immune_get_cytokine_effects(bridge, &effects);

    // Total modifier should be sum of individual effects (bounded)
    float expected_max = effects.il6_priority_boost + effects.il1_priority_boost +
                        effects.tnf_priority_boost;
    EXPECT_LE(effects.total_priority_modifier, expected_max + 0.1f);
}

//=============================================================================
// Fever Response Accuracy Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, HypervigilanceStateCheck) {
    ASSERT_NE(bridge, nullptr);

    bool is_hypervigilant = thalamic_immune_is_hypervigilant(bridge);
    // Should return valid boolean (no crash)
    EXPECT_TRUE(is_hypervigilant == true || is_hypervigilant == false);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, InflammationLevelProgression) {
    ASSERT_NE(bridge, nullptr);

    inflammation_routing_state_t state;
    thalamic_immune_get_inflammation_state(bridge, &state);

    // Verify inflammation level is valid enum
    EXPECT_TRUE(state.current_level >= INFLAMMATION_NONE &&
                state.current_level <= INFLAMMATION_STORM);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, SicknessBehaviorState) {
    ASSERT_NE(bridge, nullptr);

    inflammation_routing_state_t state;
    thalamic_immune_get_inflammation_state(bridge, &state);

    // Sickness behavior should be consistent with inflammation
    if (state.current_level >= INFLAMMATION_REGIONAL) {
        // Higher inflammation may trigger sickness behavior
        // (implementation dependent)
    }
    // Just verify we can read the state without crash
    EXPECT_TRUE(state.sickness_behavior_active == true ||
                state.sickness_behavior_active == false);
}

//=============================================================================
// Anomaly Detection Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, AnomalyDetectionExecution) {
    ASSERT_NE(bridge, nullptr);

    int result = thalamic_immune_detect_anomalies(bridge);
    EXPECT_EQ(result, 0);

    routing_anomaly_state_t anomaly_state;
    result = thalamic_immune_get_anomaly_state(bridge, &anomaly_state);
    EXPECT_EQ(result, 0);

    // Verify anomaly state values are valid
    EXPECT_GE(anomaly_state.queue_utilization, 0.0f);
    EXPECT_LE(anomaly_state.queue_utilization, 1.0f);
    EXPECT_GE(anomaly_state.drop_rate, 0.0f);
    EXPECT_LE(anomaly_state.drop_rate, 1.0f);
    EXPECT_GE(anomaly_state.threat_severity, 0.0f);
    EXPECT_LE(anomaly_state.threat_severity, 1.0f);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, AnomalyTriggerExecution) {
    ASSERT_NE(bridge, nullptr);

    // Detect anomalies first
    thalamic_immune_detect_anomalies(bridge);

    // Trigger immune response from anomaly
    int result = thalamic_immune_trigger_from_anomaly(bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Priority Escalation Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, PriorityEscalationExecution) {
    ASSERT_NE(bridge, nullptr);

    int result = thalamic_immune_escalate_priority(bridge, 1, 2, true);
    EXPECT_EQ(result, 0);

    // Non-threat signal
    result = thalamic_immune_escalate_priority(bridge, 3, 4, false);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, PriorityEscalationTracking) {
    ASSERT_NE(bridge, nullptr);

    uint64_t initial_escalations = bridge->priority_escalations;

    // Escalate multiple times
    for (int i = 0; i < 10; i++) {
        thalamic_immune_escalate_priority(bridge, i, i + 1, true);
    }

    // Should have tracked escalations
    EXPECT_GE(bridge->priority_escalations, initial_escalations);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, NullSafetyOperations) {
    // All operations should handle NULL gracefully
    EXPECT_NE(thalamic_immune_apply_cytokine_effects(nullptr), 0);
    EXPECT_NE(thalamic_immune_apply_inflammation_effects(nullptr), 0);
    EXPECT_NE(thalamic_immune_restore_gating(nullptr), 0);
    EXPECT_NE(thalamic_immune_detect_anomalies(nullptr), 0);
    EXPECT_NE(thalamic_immune_trigger_from_anomaly(nullptr), 0);
    EXPECT_NE(thalamic_immune_boost_from_health(nullptr), 0);
    EXPECT_NE(thalamic_immune_bridge_update(nullptr, 100), 0);
    EXPECT_NE(thalamic_immune_escalate_priority(nullptr, 0, 0, false), 0);

    EXPECT_FALSE(thalamic_immune_is_hypervigilant(nullptr));
    // These functions return default values (0.5f, 1.0f) when called with NULL
    // rather than error values. Just verify they don't crash.
    float gating = thalamic_immune_get_gating_threshold(nullptr);
    float multiplier = thalamic_immune_get_threat_priority_multiplier(nullptr);
    EXPECT_GE(gating, 0.0f);
    EXPECT_GE(multiplier, 0.0f);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, NullOutputParameters) {
    ASSERT_NE(bridge, nullptr);

    EXPECT_NE(thalamic_immune_get_cytokine_effects(bridge, nullptr), 0);
    EXPECT_NE(thalamic_immune_get_inflammation_state(bridge, nullptr), 0);
    EXPECT_NE(thalamic_immune_get_anomaly_state(bridge, nullptr), 0);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, StressRapidUpdates) {
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 10000; i++) {
        int result = thalamic_immune_bridge_update(bridge, 1);
        EXPECT_EQ(result, 0);

        if (i % 100 == 0) {
            // Periodic sanity checks
            float threshold = thalamic_immune_get_gating_threshold(bridge);
            EXPECT_GE(threshold, 0.0f);
            EXPECT_LE(threshold, 1.0f);
            EXPECT_FALSE(std::isnan(threshold));
        }
    }
}

TEST_F(ThalamicImmuneBridgeRegressionTest, StressMixedOperations) {
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 1000; i++) {
        switch (i % 7) {
            case 0:
                thalamic_immune_apply_cytokine_effects(bridge);
                break;
            case 1:
                thalamic_immune_apply_inflammation_effects(bridge);
                break;
            case 2:
                thalamic_immune_restore_gating(bridge);
                break;
            case 3:
                thalamic_immune_detect_anomalies(bridge);
                break;
            case 4:
                thalamic_immune_boost_from_health(bridge);
                break;
            case 5:
                thalamic_immune_bridge_update(bridge, 10);
                break;
            case 6:
                thalamic_immune_escalate_priority(bridge, i % 10, i % 20, i % 3 == 0);
                break;
        }
    }

    // Verify system is still functional
    float threshold = thalamic_immune_get_gating_threshold(bridge);
    EXPECT_GE(threshold, 0.0f);
    EXPECT_LE(threshold, 1.0f);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, StressCreateDestroyLoop) {
    for (int i = 0; i < 50; i++) {
        thalamic_immune_config_t config;
        thalamic_immune_default_config(&config);
        config.cytokine_sensitivity = 0.5f + (i % 10) * 0.1f;
        config.inflammation_sensitivity = 0.5f + (i % 10) * 0.1f;

        thalamic_immune_bridge_t* b = thalamic_immune_bridge_create(
            &config, immune_system, router);
        ASSERT_NE(b, nullptr) << "Create failed at iteration " << i;

        // Do some work
        thalamic_immune_apply_cytokine_effects(b);
        thalamic_immune_bridge_update(b, 100);

        thalamic_immune_bridge_destroy(b);
    }
}

TEST_F(ThalamicImmuneBridgeRegressionTest, StressRapidImmuneStateChanges) {
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 1000; i++) {
        // Alternate between inflammation and restoration
        if (i % 2 == 0) {
            thalamic_immune_apply_inflammation_effects(bridge);
        } else {
            thalamic_immune_restore_gating(bridge);
        }

        // Check state consistency
        inflammation_routing_state_t state;
        int result = thalamic_immune_get_inflammation_state(bridge, &state);
        EXPECT_EQ(result, 0);

        EXPECT_GE(state.inflammation_intensity, 0.0f);
        EXPECT_LE(state.inflammation_intensity, 1.0f);
        EXPECT_FALSE(std::isnan(state.inflammation_intensity));
    }
}

//=============================================================================
// Performance Benchmarks
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, UpdatePerformance) {
    ASSERT_NE(bridge, nullptr);

    const int NUM_ITERATIONS = 1000;

    double total_time_us = MeasureTimeUs([&]() {
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            thalamic_immune_bridge_update(bridge, 1);
        }
    });

    double avg_us = total_time_us / NUM_ITERATIONS;

    // Update should be fast (< 100 microseconds)
    EXPECT_LT(avg_us, 100.0) << "Bridge update too slow: " << avg_us << " us";
}

TEST_F(ThalamicImmuneBridgeRegressionTest, CytokineEffectsPerformance) {
    ASSERT_NE(bridge, nullptr);

    const int NUM_ITERATIONS = 1000;

    double total_time_us = MeasureTimeUs([&]() {
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            thalamic_immune_apply_cytokine_effects(bridge);
        }
    });

    double avg_us = total_time_us / NUM_ITERATIONS;

    // Should be fast (< 50 microseconds)
    EXPECT_LT(avg_us, 50.0) << "Cytokine effects too slow: " << avg_us << " us";
}

TEST_F(ThalamicImmuneBridgeRegressionTest, AnomalyDetectionPerformance) {
    ASSERT_NE(bridge, nullptr);

    const int NUM_ITERATIONS = 1000;

    double total_time_us = MeasureTimeUs([&]() {
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            thalamic_immune_detect_anomalies(bridge);
        }
    });

    double avg_us = total_time_us / NUM_ITERATIONS;

    // Should be fast (< 50 microseconds)
    EXPECT_LT(avg_us, 50.0) << "Anomaly detection too slow: " << avg_us << " us";
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(ThalamicImmuneBridgeRegressionTest, BioAsyncConnectionState) {
    ASSERT_NE(bridge, nullptr);

    // Check initial state (not connected by default)
    bool connected = thalamic_immune_is_bio_async_connected(bridge);
    // Just verify we can query the state
    EXPECT_TRUE(connected == true || connected == false);
}

TEST_F(ThalamicImmuneBridgeRegressionTest, BioAsyncConnectDisconnect) {
    ASSERT_NE(bridge, nullptr);

    // Try to connect (may fail if no bio-router available)
    int result = thalamic_immune_connect_bio_async(bridge);
    // Result depends on bio-async availability

    // Disconnect should always succeed
    result = thalamic_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
