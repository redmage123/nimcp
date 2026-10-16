/**
 * @file test_feature_extractor_immune_integration.cpp
 * @brief Unit tests for Feature Extractor-Immune Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Comprehensive test suite for feature extractor-immune bridge
 * WHY:  Ensure correct bidirectional coupling between immune and feature extraction
 * HOW:  Test cytokine modulation, inflammation effects, anomaly detection, quality monitoring
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/immune/nimcp_feature_extractor_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/features/nimcp_feature_extractor.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class FeatureImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    feature_extractor_t feature_extractor;
    feature_immune_bridge_t* bridge;
    middleware_features_t* features;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Create feature extractor */
        feature_extractor_config_t extractor_config = feature_extractor_default_config();
        feature_extractor = feature_extractor_create(&extractor_config);
        ASSERT_NE(feature_extractor, nullptr);

        /* Create bridge with defaults */
        bridge = feature_immune_bridge_create(nullptr, immune_system, feature_extractor);
        ASSERT_NE(bridge, nullptr);

        /* Create features */
        features = middleware_features_create();
        ASSERT_NE(features, nullptr);

        /* Initialize features to normal values */
        features->mean_firing_rate = 10.0f;
        features->population_rate_std = 2.0f;
        features->mean_isi = 100.0f;
        features->isi_cv = 0.5f;
        features->spike_timing_precision = 5.0f;
        features->synchrony_index = 0.3f;
        features->burst_index = 0.2f;
        features->fano_factor = 1.0f;
        features->delta_power = 0.2f;
        features->theta_power = 0.3f;
        features->alpha_power = 0.25f;
        features->beta_power = 0.15f;
        features->gamma_power = 0.5f;
        features->spike_entropy = 2.5f;
        features->valid = true;
    }

    void TearDown() override {
        if (features) middleware_features_destroy(features);
        if (bridge) feature_immune_bridge_destroy(bridge);
        if (feature_extractor) feature_extractor_destroy(feature_extractor);
        if (immune_system) brain_immune_destroy(immune_system);
    }

    /* Helper: Simulate inflammation by creating site */
    void SimulateInflammation(brain_inflammation_level_t level) {
        uint32_t site_id;
        brain_immune_initiate_inflammation(immune_system, 0, 0, &site_id);

        /* Manually escalate to desired level */
        for (size_t i = 0; i < immune_system->inflammation_count; i++) {
            immune_system->inflammation_sites[i].level = level;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(FeatureImmuneIntegrationTest, DefaultConfigTest) {
    feature_immune_config_t config;
    int result = feature_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_feature_modulation);
    EXPECT_TRUE(config.enable_inflammation_precision_reduction);
    EXPECT_TRUE(config.enable_feature_immune_trigger);
    EXPECT_TRUE(config.enable_threat_feature_bias);
    EXPECT_TRUE(config.enable_quality_monitoring);

    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.anomaly_trigger_sensitivity, 1.0f);

    EXPECT_FLOAT_EQ(config.burst_threshold, FEATURE_BURST_THREAT_THRESHOLD);
    EXPECT_FLOAT_EQ(config.fano_threshold, FEATURE_FANO_THREAT_THRESHOLD);
    EXPECT_FLOAT_EQ(config.isi_cv_threshold, FEATURE_ISI_CV_THREAT_THRESHOLD);
}

TEST_F(FeatureImmuneIntegrationTest, CreateDestroyTest) {
    /* Bridge already created in SetUp */
    EXPECT_NE(bridge, nullptr);
    EXPECT_NE(bridge->immune_system, nullptr);
    EXPECT_NE(bridge->feature_extractor, nullptr);
    EXPECT_NE(bridge->base.mutex, nullptr);

    EXPECT_TRUE(bridge->enable_cytokine_feature_modulation);
    EXPECT_TRUE(bridge->enable_inflammation_precision_reduction);
    EXPECT_TRUE(bridge->enable_feature_immune_trigger);

    EXPECT_EQ(bridge->total_updates, 0u);
    EXPECT_EQ(bridge->cytokine_modulations, 0u);
    EXPECT_EQ(bridge->feature_triggered_responses, 0u);
}

TEST_F(FeatureImmuneIntegrationTest, CreateWithNullSystemsTest) {
    feature_immune_bridge_t* bad_bridge;

    /* Try create with null immune */
    bad_bridge = feature_immune_bridge_create(nullptr, nullptr, feature_extractor);
    EXPECT_EQ(bad_bridge, nullptr);

    /* Try create with null extractor */
    bad_bridge = feature_immune_bridge_create(nullptr, immune_system, nullptr);
    EXPECT_EQ(bad_bridge, nullptr);

    /* Try create with both null */
    bad_bridge = feature_immune_bridge_create(nullptr, nullptr, nullptr);
    EXPECT_EQ(bad_bridge, nullptr);
}

TEST_F(FeatureImmuneIntegrationTest, CreateWithCustomConfigTest) {
    feature_immune_config_t config;
    feature_immune_default_config(&config);

    /* Customize */
    config.enable_threat_feature_bias = false;
    config.cytokine_sensitivity = 1.5f;
    config.burst_threshold = 0.8f;

    feature_immune_bridge_t* custom_bridge =
        feature_immune_bridge_create(&config, immune_system, feature_extractor);

    ASSERT_NE(custom_bridge, nullptr);
    EXPECT_FALSE(custom_bridge->enable_threat_feature_bias);

    feature_immune_bridge_destroy(custom_bridge);
}

/* ============================================================================
 * Cytokine → Feature Tests
 * ============================================================================ */

TEST_F(FeatureImmuneIntegrationTest, ApplyCytokineEffectsTest) {
    int result = feature_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Should update cytokine effects */
    EXPECT_GE(bridge->cytokine_modulations, 1u);

    /* Precision factor should be in valid range */
    EXPECT_GE(bridge->cytokine_effects.total_precision_factor, 0.0f);
    EXPECT_LE(bridge->cytokine_effects.total_precision_factor, 1.0f);

    /* Noise and bandwidth should be in range */
    EXPECT_GE(bridge->cytokine_effects.noise_amplification, 0.0f);
    EXPECT_LE(bridge->cytokine_effects.noise_amplification, 1.0f);
    EXPECT_GE(bridge->cytokine_effects.bandwidth_reduction, 0.0f);
    EXPECT_LE(bridge->cytokine_effects.bandwidth_reduction, 1.0f);
}

TEST_F(FeatureImmuneIntegrationTest, CytokineEffectsDisabledTest) {
    /* Disable cytokine modulation */
    bridge->enable_cytokine_feature_modulation = false;

    uint32_t before_count = bridge->cytokine_modulations;
    int result = feature_immune_apply_cytokine_effects(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->cytokine_modulations, before_count);
}

TEST_F(FeatureImmuneIntegrationTest, GetCytokineEffectsTest) {
    /* Apply effects first */
    feature_immune_apply_cytokine_effects(bridge);

    cytokine_feature_effects_t effects;
    int result = feature_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_GE(effects.total_precision_factor, 0.0f);
    EXPECT_LE(effects.total_precision_factor, 1.0f);
}

/* ============================================================================
 * Inflammation → Feature Tests
 * ============================================================================ */

TEST_F(FeatureImmuneIntegrationTest, ApplyInflammationEffectsNoneTest) {
    int result = feature_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    /* No inflammation = no reduction */
    EXPECT_EQ(bridge->inflammation_state.current_level, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(bridge->inflammation_state.precision_multiplier, 1.0f);
    EXPECT_FALSE(bridge->inflammation_state.is_chronic);
}

TEST_F(FeatureImmuneIntegrationTest, ApplyInflammationEffectsLocalTest) {
    SimulateInflammation(INFLAMMATION_LOCAL);

    int result = feature_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(bridge->inflammation_state.current_level, INFLAMMATION_LOCAL);
    EXPECT_FLOAT_EQ(bridge->inflammation_state.precision_multiplier,
                    INFLAMMATION_PRECISION_LOCAL);
    EXPECT_GT(bridge->inflammation_state.rate_coding_impairment, 0.0f);
}

TEST_F(FeatureImmuneIntegrationTest, ApplyInflammationEffectsRegionalTest) {
    SimulateInflammation(INFLAMMATION_REGIONAL);

    int result = feature_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(bridge->inflammation_state.current_level, INFLAMMATION_REGIONAL);
    EXPECT_FLOAT_EQ(bridge->inflammation_state.precision_multiplier,
                    INFLAMMATION_PRECISION_REGIONAL);
}

TEST_F(FeatureImmuneIntegrationTest, ApplyInflammationEffectsSystemicTest) {
    SimulateInflammation(INFLAMMATION_SYSTEMIC);

    int result = feature_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(bridge->inflammation_state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_FLOAT_EQ(bridge->inflammation_state.precision_multiplier,
                    INFLAMMATION_PRECISION_SYSTEMIC);

    /* Should have significant impairments */
    EXPECT_GT(bridge->inflammation_state.temporal_coding_impairment, 0.3f);
    EXPECT_GT(bridge->inflammation_state.population_coding_impairment, 0.3f);
    EXPECT_GT(bridge->inflammation_state.oscillation_impairment, 0.3f);
}

TEST_F(FeatureImmuneIntegrationTest, ApplyInflammationEffectsStormTest) {
    SimulateInflammation(INFLAMMATION_STORM);

    int result = feature_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(bridge->inflammation_state.current_level, INFLAMMATION_STORM);
    EXPECT_FLOAT_EQ(bridge->inflammation_state.precision_multiplier,
                    INFLAMMATION_PRECISION_STORM);

    /* Severe impairments */
    EXPECT_GT(bridge->inflammation_state.rate_coding_impairment, 0.4f);
    EXPECT_GT(bridge->inflammation_state.temporal_coding_impairment, 0.6f);
    EXPECT_GT(bridge->inflammation_state.population_coding_impairment, 0.5f);
    EXPECT_GT(bridge->inflammation_state.oscillation_impairment, 0.7f);
}

TEST_F(FeatureImmuneIntegrationTest, GetInflammationStateTest) {
    SimulateInflammation(INFLAMMATION_REGIONAL);
    feature_immune_apply_inflammation_effects(bridge);

    inflammation_feature_state_t state;
    int result = feature_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.current_level, INFLAMMATION_REGIONAL);
    EXPECT_FLOAT_EQ(state.precision_multiplier, INFLAMMATION_PRECISION_REGIONAL);
}

TEST_F(FeatureImmuneIntegrationTest, ComputePrecisionReductionTest) {
    /* Test with no inflammation */
    float precision = feature_immune_compute_precision_reduction(bridge);
    EXPECT_FLOAT_EQ(precision, 1.0f);

    /* Test with systemic inflammation */
    SimulateInflammation(INFLAMMATION_SYSTEMIC);
    feature_immune_apply_inflammation_effects(bridge);

    precision = feature_immune_compute_precision_reduction(bridge);
    EXPECT_LT(precision, 1.0f);
    EXPECT_GT(precision, 0.0f);
}

TEST_F(FeatureImmuneIntegrationTest, GetPrecisionFactorTest) {
    float factor = feature_immune_get_precision_factor(bridge);
    EXPECT_GE(factor, 0.0f);
    EXPECT_LE(factor, 1.0f);

    /* With inflammation */
    SimulateInflammation(INFLAMMATION_SYSTEMIC);
    feature_immune_apply_inflammation_effects(bridge);

    factor = feature_immune_get_precision_factor(bridge);
    EXPECT_LT(factor, 1.0f);
}

TEST_F(FeatureImmuneIntegrationTest, ThreatFeatureBiasTest) {
    SimulateInflammation(INFLAMMATION_REGIONAL);
    feature_immune_apply_inflammation_effects(bridge);

    /* Should have threat bias */
    EXPECT_GT(bridge->inflammation_state.threat_feature_bias, 0.0f);
    EXPECT_GT(bridge->inflammation_state.non_threat_suppression, 0.0f);

    int result = feature_immune_apply_threat_bias(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Feature → Immune Tests
 * ============================================================================ */

TEST_F(FeatureImmuneIntegrationTest, TriggerFromAnomaliesNormalFeaturesTest) {
    /* Normal features should not trigger */
    int result = feature_immune_trigger_from_anomalies(bridge, features);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(bridge->immune_trigger.burst_anomaly);
    EXPECT_FALSE(bridge->immune_trigger.fano_anomaly);
    EXPECT_FALSE(bridge->immune_trigger.isi_cv_anomaly);
    EXPECT_FALSE(bridge->immune_trigger.sync_anomaly);
    EXPECT_FALSE(bridge->immune_trigger.entropy_collapse);
    EXPECT_FALSE(bridge->immune_trigger.gamma_collapse);

    EXPECT_EQ(bridge->immune_trigger.immune_severity, 0u);
    EXPECT_EQ(bridge->feature_triggered_responses, 0u);
}

TEST_F(FeatureImmuneIntegrationTest, TriggerFromBurstAnomalyTest) {
    /* Set high burst index */
    features->burst_index = 0.85f;

    int result = feature_immune_trigger_from_anomalies(bridge, features);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(bridge->immune_trigger.burst_anomaly);
    EXPECT_GT(bridge->immune_trigger.burst_severity, 0.0f);
    EXPECT_EQ(bridge->immune_trigger.immune_severity, FEATURE_SEVERITY_BURST_ANOMALY);
    EXPECT_EQ(bridge->feature_triggered_responses, 1u);
}

TEST_F(FeatureImmuneIntegrationTest, TriggerFromFanoAnomalyTest) {
    /* Set high Fano factor */
    features->fano_factor = 4.5f;

    int result = feature_immune_trigger_from_anomalies(bridge, features);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(bridge->immune_trigger.fano_anomaly);
    EXPECT_GT(bridge->immune_trigger.fano_severity, 0.0f);
    EXPECT_EQ(bridge->immune_trigger.immune_severity, FEATURE_SEVERITY_FANO_ANOMALY);
}

TEST_F(FeatureImmuneIntegrationTest, TriggerFromISI_CV_AnomalyTest) {
    /* Set high ISI CV */
    features->isi_cv = 2.5f;

    int result = feature_immune_trigger_from_anomalies(bridge, features);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(bridge->immune_trigger.isi_cv_anomaly);
    EXPECT_GT(bridge->immune_trigger.isi_cv_severity, 0.0f);
    EXPECT_EQ(bridge->immune_trigger.immune_severity, FEATURE_SEVERITY_ISI_ANOMALY);
}

TEST_F(FeatureImmuneIntegrationTest, TriggerFromSyncAnomalyTest) {
    /* Set high synchrony */
    features->synchrony_index = 0.95f;

    int result = feature_immune_trigger_from_anomalies(bridge, features);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(bridge->immune_trigger.sync_anomaly);
    EXPECT_GT(bridge->immune_trigger.sync_severity, 0.0f);
    EXPECT_EQ(bridge->immune_trigger.immune_severity, FEATURE_SEVERITY_SYNC_ANOMALY);
}

TEST_F(FeatureImmuneIntegrationTest, DetectDeadNeuronsTest) {
    /* Set zero entropy */
    features->spike_entropy = 0.05f;

    int result = feature_immune_detect_dead_neurons(bridge, features);
    EXPECT_EQ(result, 0);

    /* Should also be detected in anomaly trigger */
    feature_immune_trigger_from_anomalies(bridge, features);
    EXPECT_TRUE(bridge->immune_trigger.entropy_collapse);
    EXPECT_EQ(bridge->immune_trigger.immune_severity, FEATURE_SEVERITY_ENTROPY_ZERO);
}

TEST_F(FeatureImmuneIntegrationTest, DetectBindingFailureTest) {
    /* Set zero gamma */
    features->gamma_power = 0.05f;

    int result = feature_immune_detect_binding_failure(bridge, features);
    EXPECT_EQ(result, 0);

    /* Should also be detected in anomaly trigger */
    feature_immune_trigger_from_anomalies(bridge, features);
    EXPECT_TRUE(bridge->immune_trigger.gamma_collapse);
    EXPECT_EQ(bridge->immune_trigger.immune_severity, FEATURE_SEVERITY_GAMMA_COLLAPSE);
}

TEST_F(FeatureImmuneIntegrationTest, TriggerFromMultipleAnomaliesTest) {
    /* Set multiple anomalies */
    features->burst_index = 0.8f;
    features->fano_factor = 3.5f;
    features->isi_cv = 2.2f;

    int result = feature_immune_trigger_from_anomalies(bridge, features);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(bridge->immune_trigger.burst_anomaly);
    EXPECT_TRUE(bridge->immune_trigger.fano_anomaly);
    EXPECT_TRUE(bridge->immune_trigger.isi_cv_anomaly);

    /* Total threat should be elevated */
    EXPECT_GT(bridge->immune_trigger.total_threat_level, 0.0f);

    /* Should pick highest severity (ISI CV = 8) */
    EXPECT_EQ(bridge->immune_trigger.immune_severity, FEATURE_SEVERITY_ISI_ANOMALY);
}

TEST_F(FeatureImmuneIntegrationTest, IsThreatDetectedTest) {
    /* Normal features - no threat */
    feature_immune_trigger_from_anomalies(bridge, features);
    EXPECT_FALSE(feature_immune_is_threat_detected(bridge));

    /* Anomalous features - threat */
    features->burst_index = 0.8f;
    feature_immune_trigger_from_anomalies(bridge, features);
    EXPECT_TRUE(feature_immune_is_threat_detected(bridge));
}

/* ============================================================================
 * Quality Monitoring Tests
 * ============================================================================ */

TEST_F(FeatureImmuneIntegrationTest, EscalateFromDegradationNormalTest) {
    /* Normal precision - no escalation */
    int result = feature_immune_escalate_from_degradation(bridge, features);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(bridge->quality_monitor.chronic_degradation);
    EXPECT_FALSE(bridge->quality_monitor.immune_activated);
    EXPECT_EQ(bridge->quality_escalations, 0u);
}

TEST_F(FeatureImmuneIntegrationTest, EscalateFromDegradationChronicTest) {
    /* Simulate chronic low quality by setting systemic inflammation */
    SimulateInflammation(INFLAMMATION_SYSTEMIC);

    /* Run updates to accumulate degradation time */
    for (int i = 0; i < 100; i++) {
        feature_immune_apply_inflammation_effects(bridge);
        feature_immune_escalate_from_degradation(bridge, features);
    }

    /* Eventually should detect chronic degradation */
    /* Note: Actual chronic detection requires more time, but we can check accumulation */
    EXPECT_GT(bridge->quality_monitor.degradation_duration_sec, 0.0f);
}

TEST_F(FeatureImmuneIntegrationTest, GetQualityScoreTest) {
    float quality = feature_immune_get_quality_score(bridge);
    EXPECT_GE(quality, 0.0f);
    EXPECT_LE(quality, 1.0f);

    /* With inflammation, quality should drop */
    SimulateInflammation(INFLAMMATION_SYSTEMIC);
    feature_immune_apply_inflammation_effects(bridge);

    float degraded_quality = feature_immune_get_quality_score(bridge);
    EXPECT_LT(degraded_quality, quality);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(FeatureImmuneIntegrationTest, BridgeUpdateTest) {
    uint64_t delta_ms = 100;

    int result = feature_immune_bridge_update(bridge, features, delta_ms);
    EXPECT_EQ(result, 0);

    EXPECT_GE(bridge->total_updates, 1u);
    EXPECT_GE(bridge->cytokine_modulations, 1u);
}

TEST_F(FeatureImmuneIntegrationTest, BridgeUpdateWithAnomaliesTest) {
    /* Set anomalous features */
    features->burst_index = 0.8f;
    features->fano_factor = 3.5f;

    uint64_t delta_ms = 100;
    int result = feature_immune_bridge_update(bridge, features, delta_ms);
    EXPECT_EQ(result, 0);

    EXPECT_GE(bridge->total_updates, 1u);
    EXPECT_GE(bridge->feature_triggered_responses, 1u);
    EXPECT_TRUE(feature_immune_is_threat_detected(bridge));
}

TEST_F(FeatureImmuneIntegrationTest, BridgeUpdateWithInflammationTest) {
    SimulateInflammation(INFLAMMATION_REGIONAL);

    uint64_t delta_ms = 100;
    int result = feature_immune_bridge_update(bridge, features, delta_ms);
    EXPECT_EQ(result, 0);

    /* Should have applied inflammation effects */
    EXPECT_LT(feature_immune_get_precision_factor(bridge), 1.0f);
}

TEST_F(FeatureImmuneIntegrationTest, BridgeUpdateWithoutFeaturesTest) {
    /* Update without features (only applies immune → feature effects) */
    uint64_t delta_ms = 100;
    int result = feature_immune_bridge_update(bridge, nullptr, delta_ms);
    EXPECT_EQ(result, 0);

    EXPECT_GE(bridge->total_updates, 1u);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(FeatureImmuneIntegrationTest, NullPointerGuardsTest) {
    /* Test all APIs with null bridge */
    EXPECT_EQ(feature_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(feature_immune_apply_inflammation_effects(nullptr), -1);
    EXPECT_FLOAT_EQ(feature_immune_compute_precision_reduction(nullptr), 1.0f);
    EXPECT_EQ(feature_immune_apply_threat_bias(nullptr), -1);

    EXPECT_EQ(feature_immune_trigger_from_anomalies(nullptr, features), -1);
    EXPECT_EQ(feature_immune_trigger_from_anomalies(bridge, nullptr), -1);
    EXPECT_EQ(feature_immune_escalate_from_degradation(nullptr, features), -1);
    EXPECT_EQ(feature_immune_detect_dead_neurons(nullptr, features), -1);
    EXPECT_EQ(feature_immune_detect_binding_failure(nullptr, features), -1);

    EXPECT_EQ(feature_immune_bridge_update(nullptr, features, 100), -1);

    cytokine_feature_effects_t effects;
    EXPECT_EQ(feature_immune_get_cytokine_effects(nullptr, &effects), -1);
    EXPECT_EQ(feature_immune_get_cytokine_effects(bridge, nullptr), -1);

    inflammation_feature_state_t state;
    EXPECT_EQ(feature_immune_get_inflammation_state(nullptr, &state), -1);
    EXPECT_EQ(feature_immune_get_inflammation_state(bridge, nullptr), -1);

    EXPECT_FLOAT_EQ(feature_immune_get_precision_factor(nullptr), 1.0f);
    EXPECT_FALSE(feature_immune_is_threat_detected(nullptr));
    EXPECT_FLOAT_EQ(feature_immune_get_quality_score(nullptr), 1.0f);
}

TEST_F(FeatureImmuneIntegrationTest, DestroyNullBridgeTest) {
    /* Should be safe */
    feature_immune_bridge_destroy(nullptr);
}

TEST_F(FeatureImmuneIntegrationTest, ExtremeFeaturesTest) {
    /* Set all features to extreme values */
    features->burst_index = 1.0f;
    features->fano_factor = 10.0f;
    features->isi_cv = 5.0f;
    features->synchrony_index = 1.0f;
    features->spike_entropy = 0.0f;
    features->gamma_power = 0.0f;

    int result = feature_immune_trigger_from_anomalies(bridge, features);
    EXPECT_EQ(result, 0);

    /* Should detect all anomalies */
    EXPECT_TRUE(bridge->immune_trigger.burst_anomaly);
    EXPECT_TRUE(bridge->immune_trigger.fano_anomaly);
    EXPECT_TRUE(bridge->immune_trigger.isi_cv_anomaly);
    EXPECT_TRUE(bridge->immune_trigger.sync_anomaly);
    EXPECT_TRUE(bridge->immune_trigger.entropy_collapse);
    EXPECT_TRUE(bridge->immune_trigger.gamma_collapse);

    /* Critical severity (entropy = 0) */
    EXPECT_EQ(bridge->immune_trigger.immune_severity, FEATURE_SEVERITY_ENTROPY_ZERO);
}

/* ============================================================================
 * Integration Scenario Tests
 * ============================================================================ */

TEST_F(FeatureImmuneIntegrationTest, InflammationReducesPrecisionScenarioTest) {
    /* Scenario: Inflammation reduces feature extraction precision */

    /* Start with no inflammation */
    float initial_precision = feature_immune_get_precision_factor(bridge);
    EXPECT_FLOAT_EQ(initial_precision, 1.0f);

    /* Induce local inflammation */
    SimulateInflammation(INFLAMMATION_LOCAL);
    feature_immune_bridge_update(bridge, features, 100);

    float local_precision = feature_immune_get_precision_factor(bridge);
    EXPECT_LT(local_precision, initial_precision);

    /* Escalate to systemic inflammation */
    SimulateInflammation(INFLAMMATION_SYSTEMIC);
    feature_immune_bridge_update(bridge, features, 100);

    float systemic_precision = feature_immune_get_precision_factor(bridge);
    EXPECT_LT(systemic_precision, local_precision);
}

TEST_F(FeatureImmuneIntegrationTest, AnomaliesTriggersImmuneScenarioTest) {
    /* Scenario: Feature anomalies trigger immune response */

    /* Start normal */
    feature_immune_bridge_update(bridge, features, 100);
    EXPECT_FALSE(feature_immune_is_threat_detected(bridge));

    /* Introduce burst anomaly */
    features->burst_index = 0.8f;
    feature_immune_bridge_update(bridge, features, 100);

    EXPECT_TRUE(feature_immune_is_threat_detected(bridge));
    EXPECT_GE(bridge->feature_triggered_responses, 1u);
    EXPECT_GE(bridge->anomalies_detected, 1u);
}

TEST_F(FeatureImmuneIntegrationTest, BidirectionalCouplingScenarioTest) {
    /* Scenario: Inflammation reduces precision, low quality escalates inflammation */

    /* Induce systemic inflammation */
    SimulateInflammation(INFLAMMATION_SYSTEMIC);

    /* Run multiple updates */
    for (int i = 0; i < 50; i++) {
        feature_immune_bridge_update(bridge, features, 100);
    }

    /* Precision should be reduced */
    EXPECT_LT(feature_immune_get_precision_factor(bridge), 1.0f);

    /* Quality should be tracked */
    EXPECT_LT(bridge->quality_monitor.mean_precision, 1.0f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
