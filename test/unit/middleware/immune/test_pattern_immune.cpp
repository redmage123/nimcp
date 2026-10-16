/**
 * @file test_pattern_immune.cpp
 * @brief Unit tests for pattern-immune integration
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/immune/nimcp_pattern_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

class PatternImmuneTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    pattern_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Start immune system */
        brain_immune_start(immune_system);

        /* Create bridge with default config */
        bridge = pattern_immune_bridge_create(
            nullptr,  /* default config */
            immune_system,
            nullptr,  /* no oscillation detector */
            nullptr,  /* no synchrony detector */
            nullptr,  /* no sequence detector */
            nullptr   /* no pattern library */
        );
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            pattern_immune_bridge_destroy(bridge);
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST(PatternImmuneConfigTest, DefaultConfig) {
    pattern_immune_config_t config;
    int result = pattern_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_inflammation_degradation);
    EXPECT_TRUE(config.enable_oscillation_monitoring);
    EXPECT_TRUE(config.enable_synchrony_monitoring);
    EXPECT_TRUE(config.enable_sequence_monitoring);
    EXPECT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_EQ(config.max_anomalies, PATTERN_IMMUNE_MAX_ANOMALIES);
}

TEST(PatternImmuneConfigTest, NullConfigHandling) {
    int result = pattern_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST(PatternImmuneLifecycleTest, CreateDestroy) {
    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    brain_immune_system_t* immune = brain_immune_create(&immune_config);
    ASSERT_NE(immune, nullptr);

    pattern_immune_bridge_t* bridge = pattern_immune_bridge_create(
        nullptr, immune, nullptr, nullptr, nullptr, nullptr
    );
    EXPECT_NE(bridge, nullptr);

    pattern_immune_bridge_destroy(bridge);
    brain_immune_destroy(immune);
}

TEST(PatternImmuneLifecycleTest, CreateWithoutImmuneSystemFails) {
    pattern_immune_bridge_t* bridge = pattern_immune_bridge_create(
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
    );
    EXPECT_EQ(bridge, nullptr);
}

TEST(PatternImmuneLifecycleTest, DestroyNullSafe) {
    pattern_immune_bridge_destroy(nullptr);
    /* Should not crash */
}

/* ============================================================================
 * Accuracy Factor Tests
 * ============================================================================ */

TEST_F(PatternImmuneTest, AccuracyFactorNone) {
    float factor = pattern_immune_compute_accuracy_factor(bridge, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(factor, 1.0f);
}

TEST_F(PatternImmuneTest, AccuracyFactorLocal) {
    float factor = pattern_immune_compute_accuracy_factor(bridge, INFLAMMATION_LOCAL);
    EXPECT_FLOAT_EQ(factor, 0.90f);
}

TEST_F(PatternImmuneTest, AccuracyFactorRegional) {
    float factor = pattern_immune_compute_accuracy_factor(bridge, INFLAMMATION_REGIONAL);
    EXPECT_FLOAT_EQ(factor, 0.70f);
}

TEST_F(PatternImmuneTest, AccuracyFactorSystemic) {
    float factor = pattern_immune_compute_accuracy_factor(bridge, INFLAMMATION_SYSTEMIC);
    EXPECT_FLOAT_EQ(factor, 0.40f);
}

TEST_F(PatternImmuneTest, AccuracyFactorStorm) {
    float factor = pattern_immune_compute_accuracy_factor(bridge, INFLAMMATION_STORM);
    EXPECT_FLOAT_EQ(factor, 0.10f);
}

/* ============================================================================
 * Inflammation Effects Tests
 * ============================================================================ */

TEST_F(PatternImmuneTest, ApplyInflammationEffectsNoInflammation) {
    int result = pattern_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    inflammation_pattern_effects_t effects;
    pattern_immune_get_inflammation_effects(bridge, &effects);

    EXPECT_EQ(effects.inflammation_level, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(effects.oscillation_accuracy_factor, 1.0f);
    EXPECT_FLOAT_EQ(effects.synchrony_accuracy_factor, 1.0f);
    EXPECT_FLOAT_EQ(effects.sequence_accuracy_factor, 1.0f);
}

TEST_F(PatternImmuneTest, ApplyInflammationEffectsWithInflammation) {
    /* Create inflammation in immune system */
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 0, 0, &site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);

    /* Apply effects */
    int result = pattern_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    inflammation_pattern_effects_t effects;
    pattern_immune_get_inflammation_effects(bridge, &effects);

    /* Should have degradation */
    EXPECT_LT(effects.oscillation_accuracy_factor, 1.0f);
    EXPECT_LT(effects.synchrony_accuracy_factor, 1.0f);
    EXPECT_LT(effects.sequence_accuracy_factor, 1.0f);
    EXPECT_LT(effects.pattern_match_accuracy_factor, 1.0f);
}

TEST_F(PatternImmuneTest, ApplyInflammationEffectsDisabled) {
    /* Disable inflammation degradation */
    bridge->enable_inflammation_degradation = false;

    int result = pattern_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Should not apply effects when disabled */
}

TEST_F(PatternImmuneTest, InflammationEffectsScaleWithLevel) {
    inflammation_pattern_effects_t effects1, effects2;

    /* Local inflammation */
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 0, 0, &site_id);
    pattern_immune_apply_inflammation_effects(bridge);
    pattern_immune_get_inflammation_effects(bridge, &effects1);

    /* Escalate to regional */
    brain_immune_escalate_inflammation(immune_system, site_id);
    pattern_immune_apply_inflammation_effects(bridge);
    pattern_immune_get_inflammation_effects(bridge, &effects2);

    /* Regional should have more degradation than local */
    EXPECT_LT(effects2.oscillation_accuracy_factor, effects1.oscillation_accuracy_factor);
}

/* ============================================================================
 * Pathological Pattern Detection Tests
 * ============================================================================ */

TEST_F(PatternImmuneTest, DetectPathologicalOscillationSeizure) {
    oscillation_result_t osc_result = {};
    osc_result.bands[OSC_BAND_GAMMA].peak_frequency = 120.0f;  /* Seizure-like */
    osc_result.bands[OSC_BAND_GAMMA].power = 0.8f;

    int result = pattern_immune_detect_pathological_oscillation(bridge, &osc_result);
    EXPECT_EQ(result, 0);

    pathological_oscillation_state_t state;
    pattern_immune_get_pathological_oscillation_state(bridge, &state);

    EXPECT_TRUE(state.has_seizure_oscillation);
    EXPECT_GT(state.seizure_gamma_power, 0.0f);
}

TEST_F(PatternImmuneTest, DetectPathologicalOscillationDeltaIntrusion) {
    oscillation_result_t osc_result = {};
    osc_result.bands[OSC_BAND_DELTA].relative_power = 0.5f;  /* High delta during waking */

    int result = pattern_immune_detect_pathological_oscillation(bridge, &osc_result);
    EXPECT_EQ(result, 0);

    pathological_oscillation_state_t state;
    pattern_immune_get_pathological_oscillation_state(bridge, &state);

    EXPECT_TRUE(state.has_delta_intrusion);
    EXPECT_GT(state.delta_power_waking, 0.0f);
}

TEST_F(PatternImmuneTest, DetectPathologicalOscillationCreatesAnomaly) {
    oscillation_result_t osc_result = {};
    osc_result.bands[OSC_BAND_GAMMA].peak_frequency = 120.0f;

    pattern_immune_detect_pathological_oscillation(bridge, &osc_result);

    /* Check anomaly was created */
    pattern_anomaly_t anomalies[10];
    uint32_t num_anomalies = 0;
    pattern_immune_get_anomalies(bridge, anomalies, 10, &num_anomalies);

    EXPECT_GT(num_anomalies, 0u);
    EXPECT_EQ(anomalies[0].type, PATTERN_ANOMALY_SEIZURE_OSCILLATION);
    EXPECT_GT(anomalies[0].severity, 0.0f);
}

TEST_F(PatternImmuneTest, DetectPathologicalSynchronyHyper) {
    synchrony_result_t sync_result = {};
    sync_result.synchrony_index = 0.95f;  /* Hypersynchrony */

    int result = pattern_immune_detect_pathological_synchrony(bridge, &sync_result);
    EXPECT_EQ(result, 0);

    pathological_synchrony_state_t state;
    pattern_immune_get_pathological_synchrony_state(bridge, &state);

    EXPECT_TRUE(state.has_hypersynchrony);
    EXPECT_GT(state.synchrony_index, 0.9f);
}

TEST_F(PatternImmuneTest, DetectPathologicalSynchronyDesync) {
    synchrony_result_t sync_result = {};
    sync_result.synchrony_index = 0.15f;  /* Desynchronization */

    int result = pattern_immune_detect_pathological_synchrony(bridge, &sync_result);
    EXPECT_EQ(result, 0);

    pathological_synchrony_state_t state;
    pattern_immune_get_pathological_synchrony_state(bridge, &state);

    EXPECT_TRUE(state.has_desynchronization);
    EXPECT_LT(state.synchrony_index, 0.2f);
}

TEST_F(PatternImmuneTest, DetectPathologicalSequenceRepetitive) {
    sequence_detection_t detections[10];
    for (int i = 0; i < 10; i++) {
        detections[i].template_id = 42;  /* Same template repeated */
        detections[i].strength = 0.8f;
    }

    int result = pattern_immune_detect_pathological_sequence(bridge, detections, 10);
    EXPECT_EQ(result, 0);

    pathological_sequence_state_t state;
    pattern_immune_get_pathological_sequence_state(bridge, &state);

    EXPECT_TRUE(state.has_repetitive_sequences);
    EXPECT_GE(state.max_repetition_count, 5u);
}

/* ============================================================================
 * Anomaly Presentation Tests
 * ============================================================================ */

TEST_F(PatternImmuneTest, PresentAnomalyToImmune) {
    pattern_anomaly_t anomaly = {};
    anomaly.anomaly_id = 1;
    anomaly.type = PATTERN_ANOMALY_SEIZURE_OSCILLATION;
    anomaly.severity = 0.8f;
    anomaly.confidence = 0.9f;
    anomaly.immune_alerted = false;
    anomaly.signature_len = 16;
    memset(anomaly.pattern_signature, 0xAB, 16);

    int result = pattern_immune_present_anomaly(bridge, &anomaly);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(anomaly.immune_alerted);
    EXPECT_GT(anomaly.antigen_id, 0u);
}

TEST_F(PatternImmuneTest, PresentAnomalyAlreadyAlerted) {
    pattern_anomaly_t anomaly = {};
    anomaly.immune_alerted = true;

    int result = pattern_immune_present_anomaly(bridge, &anomaly);
    EXPECT_EQ(result, 0);  /* Should succeed but do nothing */
}

TEST_F(PatternImmuneTest, CreateSignature) {
    float features[5] = {0.1f, 0.5f, 0.8f, 0.3f, 0.9f};
    uint8_t signature[BRAIN_IMMUNE_EPITOPE_SIZE];
    size_t signature_len = 0;

    int result = pattern_immune_create_signature(
        PATTERN_ANOMALY_SEIZURE_OSCILLATION,
        features, 5,
        signature, &signature_len
    );

    EXPECT_EQ(result, 0);
    EXPECT_EQ(signature_len, BRAIN_IMMUNE_EPITOPE_SIZE);
    EXPECT_EQ(signature[0], (uint8_t)PATTERN_ANOMALY_SEIZURE_OSCILLATION);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(PatternImmuneTest, BridgeUpdate) {
    int result = pattern_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge->total_updates, 0u);
}

TEST_F(PatternImmuneTest, BridgeUpdatePresentsAnomalies) {
    /* Create anomaly manually */
    pattern_anomaly_t anomaly = {};
    anomaly.anomaly_id = bridge->next_anomaly_id++;
    anomaly.type = PATTERN_ANOMALY_HYPERSYNCHRONY;
    anomaly.severity = 0.7f;
    anomaly.confidence = 0.85f;
    anomaly.immune_alerted = false;
    anomaly.signature_len = 16;
    memset(anomaly.pattern_signature, 0xCD, 16);

    /* Add to bridge */
    bridge->anomalies[bridge->anomaly_count++] = anomaly;

    /* Update should present it */
    pattern_immune_bridge_update(bridge, 100);

    EXPECT_TRUE(bridge->anomalies[0].immune_alerted);
    EXPECT_GT(bridge->immune_alerts_triggered, 0u);
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(PatternImmuneTest, GetInflammationEffects) {
    inflammation_pattern_effects_t effects;
    int result = pattern_immune_get_inflammation_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(effects.oscillation_accuracy_factor, 1.0f);
}

TEST_F(PatternImmuneTest, GetPathologicalStates) {
    pathological_oscillation_state_t osc_state;
    pathological_synchrony_state_t sync_state;
    pathological_sequence_state_t seq_state;

    EXPECT_EQ(pattern_immune_get_pathological_oscillation_state(bridge, &osc_state), 0);
    EXPECT_EQ(pattern_immune_get_pathological_synchrony_state(bridge, &sync_state), 0);
    EXPECT_EQ(pattern_immune_get_pathological_sequence_state(bridge, &seq_state), 0);
}

TEST_F(PatternImmuneTest, GetAnomalies) {
    /* Create some anomalies */
    oscillation_result_t osc_result = {};
    osc_result.bands[OSC_BAND_GAMMA].peak_frequency = 120.0f;
    pattern_immune_detect_pathological_oscillation(bridge, &osc_result);

    synchrony_result_t sync_result = {};
    sync_result.synchrony_index = 0.95f;
    pattern_immune_detect_pathological_synchrony(bridge, &sync_result);

    /* Query anomalies */
    pattern_anomaly_t anomalies[10];
    uint32_t num_anomalies = 0;
    int result = pattern_immune_get_anomalies(bridge, anomalies, 10, &num_anomalies);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(num_anomalies, 2u);
}

TEST_F(PatternImmuneTest, IsDegraded) {
    /* Initially not degraded */
    EXPECT_FALSE(pattern_immune_is_degraded(bridge));

    /* Create inflammation */
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 0, 0, &site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);

    /* Apply effects */
    pattern_immune_apply_inflammation_effects(bridge);

    /* Should be degraded now */
    EXPECT_TRUE(pattern_immune_is_degraded(bridge));
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST(PatternImmuneUtilsTest, AnomalyTypeToString) {
    EXPECT_STREQ(pattern_anomaly_type_to_string(PATTERN_ANOMALY_NONE), "None");
    EXPECT_STREQ(pattern_anomaly_type_to_string(PATTERN_ANOMALY_SEIZURE_OSCILLATION),
                 "SeizureOscillation");
    EXPECT_STREQ(pattern_anomaly_type_to_string(PATTERN_ANOMALY_HYPERSYNCHRONY),
                 "Hypersynchrony");
    EXPECT_STREQ(pattern_anomaly_type_to_string(PATTERN_ANOMALY_DESYNCHRONIZATION),
                 "Desynchronization");
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(PatternImmuneTest, EndToEndInflammationDegradation) {
    /* 1. Start with no inflammation - should have full accuracy */
    pattern_immune_apply_inflammation_effects(bridge);
    EXPECT_FALSE(pattern_immune_is_degraded(bridge));

    /* 2. Create inflammation */
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 0, 0, &site_id);

    /* 3. Apply effects - should degrade */
    pattern_immune_apply_inflammation_effects(bridge);
    EXPECT_TRUE(pattern_immune_is_degraded(bridge));

    /* 4. Resolve inflammation */
    brain_immune_resolve_inflammation(immune_system, site_id);

    /* 5. Reapply - should restore accuracy */
    pattern_immune_apply_inflammation_effects(bridge);
    EXPECT_FALSE(pattern_immune_is_degraded(bridge));
}

TEST_F(PatternImmuneTest, EndToEndAnomalyDetectionAndPresentation) {
    /* 1. Detect pathological pattern */
    oscillation_result_t osc_result = {};
    osc_result.bands[OSC_BAND_GAMMA].peak_frequency = 120.0f;
    pattern_immune_detect_pathological_oscillation(bridge, &osc_result);

    /* 2. Verify anomaly created */
    EXPECT_GT(bridge->anomaly_count, 0u);
    EXPECT_FALSE(bridge->anomalies[0].immune_alerted);

    /* 3. Update bridge to present anomaly */
    pattern_immune_bridge_update(bridge, 100);

    /* 4. Verify anomaly presented to immune system */
    EXPECT_TRUE(bridge->anomalies[0].immune_alerted);
    EXPECT_GT(bridge->immune_alerts_triggered, 0u);

    /* 5. Verify immune system received antigen */
    const brain_antigen_t* antigen = brain_immune_get_antigen(
        immune_system,
        bridge->anomalies[0].antigen_id
    );
    EXPECT_NE(antigen, nullptr);
    EXPECT_EQ(antigen->source, ANTIGEN_SOURCE_ANOMALY);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(PatternImmuneTest, NullParameterHandling) {
    EXPECT_EQ(pattern_immune_apply_inflammation_effects(nullptr), -1);
    EXPECT_EQ(pattern_immune_detect_pathological_oscillation(nullptr, nullptr), -1);
    EXPECT_EQ(pattern_immune_present_anomaly(nullptr, nullptr), -1);
    EXPECT_EQ(pattern_immune_bridge_update(nullptr, 100), -1);
}

TEST_F(PatternImmuneTest, MaxAnomaliesCapacity) {
    oscillation_result_t osc_result = {};
    osc_result.bands[OSC_BAND_GAMMA].peak_frequency = 120.0f;

    /* Fill anomaly buffer */
    for (size_t i = 0; i < bridge->anomaly_capacity; i++) {
        pattern_immune_detect_pathological_oscillation(bridge, &osc_result);
    }

    EXPECT_EQ(bridge->anomaly_count, bridge->anomaly_capacity);

    /* Additional detections should not overflow */
    pattern_immune_detect_pathological_oscillation(bridge, &osc_result);
    EXPECT_EQ(bridge->anomaly_count, bridge->anomaly_capacity);
}

TEST_F(PatternImmuneTest, DisabledMonitoringFeatures) {
    /* Disable all monitoring */
    bridge->enable_oscillation_monitoring = false;
    bridge->enable_synchrony_monitoring = false;
    bridge->enable_sequence_monitoring = false;

    /* Detections should return early without creating anomalies */
    oscillation_result_t osc_result = {};
    osc_result.bands[OSC_BAND_GAMMA].peak_frequency = 120.0f;

    synchrony_result_t sync_result = {};
    sync_result.synchrony_index = 0.95f;

    pattern_immune_detect_pathological_oscillation(bridge, &osc_result);
    pattern_immune_detect_pathological_synchrony(bridge, &sync_result);

    EXPECT_EQ(bridge->anomaly_count, 0u);
}
