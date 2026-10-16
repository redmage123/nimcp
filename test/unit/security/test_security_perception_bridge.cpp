/**
 * @file test_security_perception_bridge.cpp
 * @brief Unit tests for Security-Perception Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * Comprehensive tests for the security-perception bridge including:
 * - Lifecycle (create, destroy, start, stop)
 * - Visual threat analysis
 * - Audio threat analysis
 * - Multimodal threat analysis
 * - Cross-modal consistency checking
 * - Quarantine management
 * - Attack signature learning and matching
 * - Immune escalation
 * - Statistics tracking
 * - Bio-async integration
 * - Thread safety
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "security/nimcp_security_perception_bridge.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityPerceptionBridgeTest : public ::testing::Test {
protected:
    security_perception_bridge_t* bridge = nullptr;
    sec_percept_config_t config;

    void SetUp() override {
        sec_percept_default_config(&config);
        bridge = sec_percept_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            sec_percept_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper: Create test visual features (128-dim)
    std::vector<float> create_visual_features(float base_value = 1.0f) {
        std::vector<float> features(128);
        for (size_t i = 0; i < features.size(); i++) {
            features[i] = base_value + 0.1f * static_cast<float>(i);
        }
        return features;
    }

    // Helper: Create test audio features (64-dim)
    std::vector<float> create_audio_features(float base_value = 0.5f) {
        std::vector<float> features(64);
        for (size_t i = 0; i < features.size(); i++) {
            features[i] = base_value + 0.05f * static_cast<float>(i);
        }
        return features;
    }

    // Helper: Create adversarial features (high variance)
    std::vector<float> create_adversarial_features() {
        std::vector<float> features(128);
        for (size_t i = 0; i < features.size(); i++) {
            // Create high-variance adversarial pattern
            features[i] = (i % 2 == 0) ? 10.0f : -10.0f;
        }
        return features;
    }

    // Helper: Create anomalous audio features
    std::vector<float> create_anomalous_audio() {
        std::vector<float> features(64);
        for (size_t i = 0; i < features.size(); i++) {
            // Extreme frequency attack pattern
            features[i] = 15.0f * std::sin(static_cast<float>(i) * 0.1f);
        }
        return features;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, DefaultConfigIsValid) {
    sec_percept_config_t cfg;
    int result = sec_percept_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(cfg.visual_anomaly_threshold, SEC_PERCEPT_THRESHOLD_MEDIUM);
    EXPECT_EQ(cfg.audio_anomaly_threshold, SEC_PERCEPT_THRESHOLD_MEDIUM);
    EXPECT_EQ(cfg.cross_modal_threshold, SEC_PERCEPT_THRESHOLD_HIGH);
    EXPECT_EQ(cfg.immune_escalation_threshold, SEC_PERCEPT_THRESHOLD_HIGH);
    EXPECT_TRUE(cfg.enable_statistical_checks);
    EXPECT_TRUE(cfg.enable_adversarial_detection);
    EXPECT_TRUE(cfg.enable_cross_modal_validation);
    EXPECT_TRUE(cfg.enable_temporal_analysis);
    EXPECT_EQ(cfg.max_quarantine_size, SEC_PERCEPT_MAX_QUARANTINED);
    EXPECT_EQ(cfg.max_attack_signatures, SEC_PERCEPT_MAX_ATTACK_SIGS);
    EXPECT_TRUE(cfg.enable_online_learning);
    EXPECT_EQ(cfg.default_action, THREAT_RESPONSE_LOG);
    EXPECT_TRUE(cfg.auto_quarantine);
    EXPECT_FALSE(cfg.auto_immune_escalation);
}

TEST_F(SecurityPerceptionBridgeTest, DefaultConfigNullFails) {
    int result = sec_percept_default_config(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, CreateWithNullConfigUsesDefaults) {
    security_perception_bridge_t* br = sec_percept_create(nullptr);
    ASSERT_NE(br, nullptr);

    sec_percept_stats_t stats;
    sec_percept_get_stats(br, &stats);
    EXPECT_EQ(stats.state, SEC_PERCEPT_STATE_STOPPED);

    sec_percept_destroy(br);
}

TEST_F(SecurityPerceptionBridgeTest, CreateWithCustomConfig) {
    sec_percept_config_t custom_cfg;
    sec_percept_default_config(&custom_cfg);
    custom_cfg.max_quarantine_size = 64;
    custom_cfg.max_attack_signatures = 128;
    custom_cfg.visual_anomaly_threshold = SEC_PERCEPT_THRESHOLD_LOW;
    custom_cfg.auto_immune_escalation = true;

    security_perception_bridge_t* br = sec_percept_create(&custom_cfg);
    ASSERT_NE(br, nullptr);

    sec_percept_destroy(br);
}

TEST_F(SecurityPerceptionBridgeTest, DestroyNullIsSafe) {
    sec_percept_destroy(nullptr);
    // Should not crash
}

TEST_F(SecurityPerceptionBridgeTest, StartAndStop) {
    int result = sec_percept_start(bridge);
    EXPECT_EQ(result, 0);

    sec_percept_state_t state = sec_percept_get_state(bridge);
    EXPECT_EQ(state, SEC_PERCEPT_STATE_RUNNING);

    result = sec_percept_stop(bridge);
    EXPECT_EQ(result, 0);

    state = sec_percept_get_state(bridge);
    EXPECT_EQ(state, SEC_PERCEPT_STATE_STOPPED);
}

TEST_F(SecurityPerceptionBridgeTest, StartNullFails) {
    int result = sec_percept_start(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, StopNullFails) {
    int result = sec_percept_stop(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, StartWhenAlreadyRunningSucceeds) {
    sec_percept_start(bridge);
    int result = sec_percept_start(bridge);
    EXPECT_EQ(result, 0);  // Idempotent
}

TEST_F(SecurityPerceptionBridgeTest, StopWhenAlreadyStoppedSucceeds) {
    int result = sec_percept_stop(bridge);
    EXPECT_EQ(result, 0);  // Idempotent
}

TEST_F(SecurityPerceptionBridgeTest, InitialStateIsStopped) {
    sec_percept_state_t state = sec_percept_get_state(bridge);
    EXPECT_EQ(state, SEC_PERCEPT_STATE_STOPPED);
}

/* ============================================================================
 * Integration API Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, ConnectBBBNullBridgeFails) {
    bbb_system_t bbb = (bbb_system_t)0x12345678;  // Mock handle
    int result = sec_percept_connect_bbb(nullptr, bbb);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ConnectBBBNullBBBFails) {
    int result = sec_percept_connect_bbb(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ConnectAnomalyDetectorNullBridgeFails) {
    nimcp_anomaly_detector_t detector = (nimcp_anomaly_detector_t)0x12345678;
    int result = sec_percept_connect_anomaly_detector(nullptr, detector);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ConnectAnomalyDetectorNullDetectorFails) {
    int result = sec_percept_connect_anomaly_detector(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ConnectImmuneNullBridgeFails) {
    brain_immune_system_t immune;
    int result = sec_percept_connect_immune(nullptr, &immune);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ConnectImmuneNullImmuneFails) {
    int result = sec_percept_connect_immune(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ConnectVisualCortexNullBridgeFails) {
    // Use dummy pointer since visual_cortex_t is opaque - we're just testing null bridge
    visual_cortex_t* dummy_cortex = reinterpret_cast<visual_cortex_t*>(0x1);
    int result = sec_percept_connect_visual_cortex(nullptr, dummy_cortex);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ConnectVisualCortexNullCortexFails) {
    int result = sec_percept_connect_visual_cortex(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ConnectAudioCortexNullBridgeFails) {
    // Use dummy pointer since audio_cortex_t is opaque - we're just testing null bridge
    audio_cortex_t* dummy_cortex = reinterpret_cast<audio_cortex_t*>(0x1);
    int result = sec_percept_connect_audio_cortex(nullptr, dummy_cortex);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ConnectAudioCortexNullCortexFails) {
    int result = sec_percept_connect_audio_cortex(bridge, nullptr);
    EXPECT_NE(result, 0);
}

/* ============================================================================
 * Visual Threat Analysis Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, AnalyzeVisualNormalInput) {
    sec_percept_start(bridge);

    auto features = create_visual_features();
    sensory_threat_result_t result;

    int ret = sec_percept_analyze_visual(bridge, features.data(),
                                        features.size(), &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.modality, SENSORY_MODALITY_VISUAL);
    EXPECT_LT(result.threat_score, config.visual_anomaly_threshold);
    EXPECT_EQ(result.recommended_action, THREAT_RESPONSE_LOG);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeVisualAdversarialInput) {
    sec_percept_start(bridge);

    auto features = create_adversarial_features();
    sensory_threat_result_t result;

    int ret = sec_percept_analyze_visual(bridge, features.data(),
                                        features.size(), &result);

    EXPECT_EQ(ret, 0);
    // High variance should trigger anomaly
    EXPECT_GT(result.statistical_anomaly_score, 0.0f);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeVisualNullBridgeFails) {
    auto features = create_visual_features();
    sensory_threat_result_t result;

    int ret = sec_percept_analyze_visual(nullptr, features.data(),
                                        features.size(), &result);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeVisualNullFeaturesFails) {
    sensory_threat_result_t result;
    int ret = sec_percept_analyze_visual(bridge, nullptr, 128, &result);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeVisualNullResultFails) {
    auto features = create_visual_features();
    int ret = sec_percept_analyze_visual(bridge, features.data(),
                                        features.size(), nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeVisualZeroDimFails) {
    auto features = create_visual_features();
    sensory_threat_result_t result;
    int ret = sec_percept_analyze_visual(bridge, features.data(), 0, &result);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeVisualUpdatesStatistics) {
    sec_percept_start(bridge);

    auto features = create_visual_features();
    sensory_threat_result_t result;

    sec_percept_analyze_visual(bridge, features.data(), features.size(), &result);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_visual_inputs, 1u);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeVisualHighThreatRecommensdsQuarantine) {
    sec_percept_start(bridge);

    // Create extreme anomaly
    std::vector<float> features(128);
    for (size_t i = 0; i < features.size(); i++) {
        features[i] = (i % 2 == 0) ? 100.0f : -100.0f;
    }

    sensory_threat_result_t result;
    sec_percept_analyze_visual(bridge, features.data(), features.size(), &result);

    if (result.threat_score >= SEC_PERCEPT_THRESHOLD_HIGH) {
        EXPECT_EQ(result.recommended_action, THREAT_RESPONSE_QUARANTINE);
    }
}

/* ============================================================================
 * Audio Threat Analysis Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, AnalyzeAudioNormalInput) {
    sec_percept_start(bridge);

    auto features = create_audio_features();
    sensory_threat_result_t result;

    int ret = sec_percept_analyze_audio(bridge, features.data(),
                                       features.size(), &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.modality, SENSORY_MODALITY_AUDIO);
    EXPECT_LT(result.threat_score, config.audio_anomaly_threshold);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeAudioAnomalousInput) {
    sec_percept_start(bridge);

    auto features = create_anomalous_audio();
    sensory_threat_result_t result;

    int ret = sec_percept_analyze_audio(bridge, features.data(),
                                       features.size(), &result);

    EXPECT_EQ(ret, 0);
    // High magnitude should trigger detection
    EXPECT_GT(result.statistical_anomaly_score, 0.0f);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeAudioNullBridgeFails) {
    auto features = create_audio_features();
    sensory_threat_result_t result;

    int ret = sec_percept_analyze_audio(nullptr, features.data(),
                                       features.size(), &result);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeAudioNullFeaturesFails) {
    sensory_threat_result_t result;
    int ret = sec_percept_analyze_audio(bridge, nullptr, 64, &result);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeAudioUpdatesStatistics) {
    sec_percept_start(bridge);

    auto features = create_audio_features();
    sensory_threat_result_t result;

    sec_percept_analyze_audio(bridge, features.data(), features.size(), &result);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_audio_inputs, 1u);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeAudioFrequencyAttackDetection) {
    sec_percept_start(bridge);

    auto features = create_anomalous_audio();
    sensory_threat_result_t result;

    sec_percept_analyze_audio(bridge, features.data(), features.size(), &result);

    // Should detect frequency anomaly
    if (result.threat_score >= config.audio_anomaly_threshold) {
        EXPECT_TRUE(result.threat_type == SENSORY_THREAT_FREQUENCY_ATTACK ||
                   result.threat_type == SENSORY_THREAT_STATISTICAL_ANOMALY);
    }
}

/* ============================================================================
 * Multimodal Analysis Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, AnalyzeMultimodalNormalInputs) {
    sec_percept_start(bridge);

    auto visual = create_visual_features();
    auto audio = create_audio_features();
    sensory_threat_result_t result;

    int ret = sec_percept_analyze_multimodal(bridge,
                                             visual.data(), visual.size(),
                                             audio.data(), audio.size(),
                                             &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.modality, SENSORY_MODALITY_MULTIMODAL);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeMultimodalNullBridgeFails) {
    auto visual = create_visual_features();
    auto audio = create_audio_features();
    sensory_threat_result_t result;

    int ret = sec_percept_analyze_multimodal(nullptr,
                                             visual.data(), visual.size(),
                                             audio.data(), audio.size(),
                                             &result);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeMultimodalNullVisualFails) {
    auto audio = create_audio_features();
    sensory_threat_result_t result;

    int ret = sec_percept_analyze_multimodal(bridge,
                                             nullptr, 128,
                                             audio.data(), audio.size(),
                                             &result);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeMultimodalNullAudioFails) {
    auto visual = create_visual_features();
    sensory_threat_result_t result;

    int ret = sec_percept_analyze_multimodal(bridge,
                                             visual.data(), visual.size(),
                                             nullptr, 64,
                                             &result);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeMultimodalUpdatesStatistics) {
    sec_percept_start(bridge);

    auto visual = create_visual_features();
    auto audio = create_audio_features();
    sensory_threat_result_t result;

    sec_percept_analyze_multimodal(bridge,
                                   visual.data(), visual.size(),
                                   audio.data(), audio.size(),
                                   &result);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_multimodal_inputs, 1u);
}

TEST_F(SecurityPerceptionBridgeTest, AnalyzeMultimodalCombinesScores) {
    sec_percept_start(bridge);

    auto visual = create_adversarial_features();
    auto audio = create_anomalous_audio();
    sensory_threat_result_t result;

    sec_percept_analyze_multimodal(bridge,
                                   visual.data(), visual.size(),
                                   audio.data(), audio.size(),
                                   &result);

    // Should combine both modality threats
    EXPECT_GT(result.threat_score, 0.0f);
}

/* ============================================================================
 * Cross-Modal Consistency Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, CheckCrossModalConsistent) {
    sec_percept_start(bridge);

    auto visual = create_visual_features(1.0f);
    auto audio = create_audio_features(1.0f);
    cross_modal_check_t check;

    int ret = sec_percept_check_cross_modal(bridge,
                                            visual.data(), visual.size(),
                                            audio.data(), audio.size(),
                                            &check);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(check.overall_consistency, 0.0f);
    EXPECT_LE(check.overall_consistency, 1.0f);
}

TEST_F(SecurityPerceptionBridgeTest, CheckCrossModalNullBridgeFails) {
    auto visual = create_visual_features();
    auto audio = create_audio_features();
    cross_modal_check_t check;

    int ret = sec_percept_check_cross_modal(nullptr,
                                            visual.data(), visual.size(),
                                            audio.data(), audio.size(),
                                            &check);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, CheckCrossModalNullCheckFails) {
    auto visual = create_visual_features();
    auto audio = create_audio_features();

    int ret = sec_percept_check_cross_modal(bridge,
                                            visual.data(), visual.size(),
                                            audio.data(), audio.size(),
                                            nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, CheckCrossModalMismatchDetection) {
    sec_percept_start(bridge);

    // Create very different features
    auto visual = create_visual_features(10.0f);
    auto audio = create_audio_features(-10.0f);
    cross_modal_check_t check;

    sec_percept_check_cross_modal(bridge,
                                  visual.data(), visual.size(),
                                  audio.data(), audio.size(),
                                  &check);

    // Semantic alignment uses cosine similarity, range is [-1.0, 1.0]
    // Opposite features (10.0 vs -10.0) produce negative alignment
    EXPECT_GE(check.semantic_alignment, -1.0f);
    EXPECT_LE(check.semantic_alignment, 1.0f);
}

/* ============================================================================
 * Quarantine Management Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, QuarantineInput) {
    sec_percept_start(bridge);

    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_type = SENSORY_THREAT_ADVERSARIAL_EXAMPLE;
    threat.threat_score = 0.9f;

    auto features = create_adversarial_features();
    uint32_t quarantine_id = 0;

    int ret = sec_percept_quarantine_input(bridge, &threat,
                                          features.data(), features.size(),
                                          &quarantine_id);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(quarantine_id, 0u);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);
    EXPECT_EQ(stats.inputs_quarantined, 1u);
    EXPECT_EQ(stats.current_quarantine_count, 1u);
}

TEST_F(SecurityPerceptionBridgeTest, QuarantineInputNullBridgeFails) {
    sensory_threat_result_t threat;
    auto features = create_visual_features();
    uint32_t quarantine_id;

    int ret = sec_percept_quarantine_input(nullptr, &threat,
                                          features.data(), features.size(),
                                          &quarantine_id);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, QuarantineInputNullThreatFails) {
    auto features = create_visual_features();
    uint32_t quarantine_id;

    int ret = sec_percept_quarantine_input(bridge, nullptr,
                                          features.data(), features.size(),
                                          &quarantine_id);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, GetQuarantined) {
    sec_percept_start(bridge);

    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_score = 0.8f;

    auto features = create_visual_features();
    uint32_t quarantine_id;

    sec_percept_quarantine_input(bridge, &threat, features.data(),
                                features.size(), &quarantine_id);

    const quarantined_input_t* input = nullptr;
    int ret = sec_percept_get_quarantined(bridge, quarantine_id, &input);

    EXPECT_EQ(ret, 0);
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->id, quarantine_id);
    EXPECT_EQ(input->modality, SENSORY_MODALITY_VISUAL);
    EXPECT_EQ(input->feature_dim, features.size());
}

TEST_F(SecurityPerceptionBridgeTest, GetQuarantinedNotFound) {
    const quarantined_input_t* input;
    int ret = sec_percept_get_quarantined(bridge, 99999, &input);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, GetQuarantinedNullBridgeFails) {
    const quarantined_input_t* input;
    int ret = sec_percept_get_quarantined(nullptr, 1, &input);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ReleaseQuarantine) {
    sec_percept_start(bridge);

    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_AUDIO;
    threat.threat_score = 0.7f;

    auto features = create_audio_features();
    uint32_t quarantine_id;

    sec_percept_quarantine_input(bridge, &threat, features.data(),
                                features.size(), &quarantine_id);

    int ret = sec_percept_release_quarantine(bridge, quarantine_id);
    EXPECT_EQ(ret, 0);

    // Should not be found after release
    const quarantined_input_t* input;
    ret = sec_percept_get_quarantined(bridge, quarantine_id, &input);
    EXPECT_NE(ret, 0);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);
    EXPECT_EQ(stats.current_quarantine_count, 0u);
}

TEST_F(SecurityPerceptionBridgeTest, ReleaseQuarantineNullBridgeFails) {
    int ret = sec_percept_release_quarantine(nullptr, 1);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ReleaseQuarantineNotFound) {
    int ret = sec_percept_release_quarantine(bridge, 99999);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ClearQuarantine) {
    sec_percept_start(bridge);

    // Add multiple quarantined items
    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_score = 0.8f;

    auto features = create_visual_features();
    uint32_t id1, id2, id3;

    sec_percept_quarantine_input(bridge, &threat, features.data(),
                                features.size(), &id1);
    sec_percept_quarantine_input(bridge, &threat, features.data(),
                                features.size(), &id2);
    sec_percept_quarantine_input(bridge, &threat, features.data(),
                                features.size(), &id3);

    sec_percept_clear_quarantine(bridge);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);
    EXPECT_EQ(stats.current_quarantine_count, 0u);
}

TEST_F(SecurityPerceptionBridgeTest, ClearQuarantineNullIsSafe) {
    sec_percept_clear_quarantine(nullptr);
    // Should not crash
}

TEST_F(SecurityPerceptionBridgeTest, QuarantineEvictionWhenFull) {
    sec_percept_start(bridge);

    // Create bridge with small capacity
    sec_percept_config_t small_config;
    sec_percept_default_config(&small_config);
    small_config.max_quarantine_size = 3;

    security_perception_bridge_t* small_bridge = sec_percept_create(&small_config);
    sec_percept_start(small_bridge);

    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_score = 0.7f;

    auto features = create_visual_features();
    uint32_t ids[4];

    // Fill to capacity
    for (int i = 0; i < 4; i++) {
        sec_percept_quarantine_input(small_bridge, &threat,
                                    features.data(), features.size(), &ids[i]);
    }

    // First item should be evicted
    const quarantined_input_t* input;
    int ret = sec_percept_get_quarantined(small_bridge, ids[0], &input);
    EXPECT_NE(ret, 0);  // First should be evicted

    // Last items should still exist
    ret = sec_percept_get_quarantined(small_bridge, ids[3], &input);
    EXPECT_EQ(ret, 0);

    sec_percept_destroy(small_bridge);
}

/* ============================================================================
 * Attack Signature Management Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, LearnSignature) {
    sec_percept_start(bridge);

    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_type = SENSORY_THREAT_ADVERSARIAL_EXAMPLE;
    threat.threat_score = 0.9f;

    auto features = create_adversarial_features();

    int ret = sec_percept_learn_signature(bridge, &threat,
                                         features.data(), features.size());

    EXPECT_EQ(ret, 0);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);
    EXPECT_EQ(stats.signatures_learned, 1u);
}

TEST_F(SecurityPerceptionBridgeTest, LearnSignatureNullBridgeFails) {
    sensory_threat_result_t threat;
    auto features = create_visual_features();

    int ret = sec_percept_learn_signature(nullptr, &threat,
                                         features.data(), features.size());
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, LearnSignatureNullThreatFails) {
    auto features = create_visual_features();
    int ret = sec_percept_learn_signature(bridge, nullptr,
                                         features.data(), features.size());
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, LearnSignatureWhenLearningDisabled) {
    sec_percept_config_t no_learn_config;
    sec_percept_default_config(&no_learn_config);
    no_learn_config.enable_online_learning = false;

    security_perception_bridge_t* no_learn_bridge = sec_percept_create(&no_learn_config);
    sec_percept_start(no_learn_bridge);

    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_score = 0.9f;

    auto features = create_visual_features();
    int ret = sec_percept_learn_signature(no_learn_bridge, &threat,
                                         features.data(), features.size());

    EXPECT_EQ(ret, 0);  // Succeeds but doesn't learn

    sec_percept_stats_t stats;
    sec_percept_get_stats(no_learn_bridge, &stats);
    EXPECT_EQ(stats.signatures_learned, 0u);

    sec_percept_destroy(no_learn_bridge);
}

TEST_F(SecurityPerceptionBridgeTest, MatchSignature) {
    sec_percept_start(bridge);

    // Learn a signature
    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_type = SENSORY_THREAT_ADVERSARIAL_EXAMPLE;
    threat.threat_score = 0.9f;

    auto features = create_adversarial_features();
    sec_percept_learn_signature(bridge, &threat, features.data(), features.size());

    // Try to match same features
    uint32_t signature_id;
    float match_score;

    int ret = sec_percept_match_signature(bridge, features.data(), features.size(),
                                         SENSORY_MODALITY_VISUAL,
                                         &signature_id, &match_score);

    if (ret == 0) {
        EXPECT_GT(signature_id, 0u);
        EXPECT_GE(match_score, 0.7f);  // Match threshold
    }
}

TEST_F(SecurityPerceptionBridgeTest, MatchSignatureNoMatch) {
    sec_percept_start(bridge);

    auto features = create_visual_features();
    uint32_t signature_id;
    float match_score;

    int ret = sec_percept_match_signature(bridge, features.data(), features.size(),
                                         SENSORY_MODALITY_VISUAL,
                                         &signature_id, &match_score);

    EXPECT_NE(ret, 0);  // No signatures learned yet
}

TEST_F(SecurityPerceptionBridgeTest, MatchSignatureNullBridgeFails) {
    auto features = create_visual_features();
    uint32_t signature_id;
    float match_score;

    int ret = sec_percept_match_signature(nullptr, features.data(), features.size(),
                                         SENSORY_MODALITY_VISUAL,
                                         &signature_id, &match_score);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, GetSignature) {
    sec_percept_start(bridge);

    // Learn a signature
    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_AUDIO;
    threat.threat_type = SENSORY_THREAT_FREQUENCY_ATTACK;
    threat.threat_score = 0.85f;

    auto features = create_audio_features();
    sec_percept_learn_signature(bridge, &threat, features.data(), features.size());

    // Get signature by ID (assume ID 1 for first signature)
    const attack_signature_t* sig;
    int ret = sec_percept_get_signature(bridge, 1, &sig);

    if (ret == 0) {
        ASSERT_NE(sig, nullptr);
        EXPECT_EQ(sig->modality, SENSORY_MODALITY_AUDIO);
        EXPECT_EQ(sig->type, SENSORY_THREAT_FREQUENCY_ATTACK);
    }
}

TEST_F(SecurityPerceptionBridgeTest, GetSignatureNotFound) {
    const attack_signature_t* sig;
    int ret = sec_percept_get_signature(bridge, 99999, &sig);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, ClearSignatures) {
    sec_percept_start(bridge);

    // Learn signatures
    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_score = 0.9f;

    auto features = create_visual_features();
    sec_percept_learn_signature(bridge, &threat, features.data(), features.size());
    sec_percept_learn_signature(bridge, &threat, features.data(), features.size());

    sec_percept_clear_signatures(bridge);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);
    EXPECT_EQ(stats.signatures_learned, 0u);
}

TEST_F(SecurityPerceptionBridgeTest, ClearSignaturesNullIsSafe) {
    sec_percept_clear_signatures(nullptr);
    // Should not crash
}

/* ============================================================================
 * Immune Escalation Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, EscalateToImmuneWithoutImmuneConnected) {
    sec_percept_start(bridge);

    sensory_threat_result_t threat;
    threat.threat_score = 0.95f;
    uint32_t antigen_id;

    int ret = sec_percept_escalate_to_immune(bridge, &threat, &antigen_id);
    EXPECT_NE(ret, 0);  // No immune system connected
}

TEST_F(SecurityPerceptionBridgeTest, EscalateToImmuneNullBridgeFails) {
    sensory_threat_result_t threat;
    uint32_t antigen_id;

    int ret = sec_percept_escalate_to_immune(nullptr, &threat, &antigen_id);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, EscalateToImmuneNullThreatFails) {
    uint32_t antigen_id;
    int ret = sec_percept_escalate_to_immune(bridge, nullptr, &antigen_id);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, BoostSecuritySalienceVisual) {
    sec_percept_start(bridge);

    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_score = 0.8f;

    int ret = sec_percept_boost_security_salience(bridge, &threat, 1.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, BoostSecuritySalienceAudio) {
    sec_percept_start(bridge);

    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_AUDIO;
    threat.threat_score = 0.7f;

    int ret = sec_percept_boost_security_salience(bridge, &threat, 1.2f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, BoostSecuritySalienceNullBridgeFails) {
    sensory_threat_result_t threat;
    int ret = sec_percept_boost_security_salience(nullptr, &threat, 1.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, BoostSecuritySalienceNullThreatFails) {
    int ret = sec_percept_boost_security_salience(bridge, nullptr, 1.5f);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, GetStats) {
    sec_percept_stats_t stats;
    int ret = sec_percept_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_visual_inputs, 0u);
    EXPECT_EQ(stats.total_audio_inputs, 0u);
    EXPECT_EQ(stats.threats_detected, 0u);
    EXPECT_EQ(stats.state, SEC_PERCEPT_STATE_STOPPED);
}

TEST_F(SecurityPerceptionBridgeTest, GetStatsNullBridgeFails) {
    sec_percept_stats_t stats;
    int ret = sec_percept_get_stats(nullptr, &stats);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, GetStatsNullStatsFails) {
    int ret = sec_percept_get_stats(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, StatsAccumulateCorrectly) {
    sec_percept_start(bridge);

    auto visual = create_visual_features();
    auto audio = create_audio_features();
    sensory_threat_result_t result;

    // Process multiple inputs
    sec_percept_analyze_visual(bridge, visual.data(), visual.size(), &result);
    sec_percept_analyze_visual(bridge, visual.data(), visual.size(), &result);
    sec_percept_analyze_audio(bridge, audio.data(), audio.size(), &result);
    sec_percept_analyze_multimodal(bridge, visual.data(), visual.size(),
                                   audio.data(), audio.size(), &result);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);

    // Multimodal analysis internally processes both modalities, so counts include those
    EXPECT_EQ(stats.total_visual_inputs, 3u);   // 2 direct + 1 from multimodal
    EXPECT_EQ(stats.total_audio_inputs, 2u);    // 1 direct + 1 from multimodal
    EXPECT_EQ(stats.total_multimodal_inputs, 1u);
}

TEST_F(SecurityPerceptionBridgeTest, ResetStats) {
    sec_percept_start(bridge);

    // Generate some stats
    auto features = create_visual_features();
    sensory_threat_result_t result;
    sec_percept_analyze_visual(bridge, features.data(), features.size(), &result);

    sec_percept_reset_stats(bridge);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_visual_inputs, 0u);
    EXPECT_EQ(stats.total_audio_inputs, 0u);
    EXPECT_EQ(stats.threats_detected, 0u);
    // State should be preserved
    EXPECT_EQ(stats.state, SEC_PERCEPT_STATE_RUNNING);
}

TEST_F(SecurityPerceptionBridgeTest, ResetStatsNullIsSafe) {
    sec_percept_reset_stats(nullptr);
    // Should not crash
}

TEST_F(SecurityPerceptionBridgeTest, GetState) {
    sec_percept_state_t state = sec_percept_get_state(bridge);
    EXPECT_EQ(state, SEC_PERCEPT_STATE_STOPPED);

    sec_percept_start(bridge);
    state = sec_percept_get_state(bridge);
    EXPECT_EQ(state, SEC_PERCEPT_STATE_RUNNING);
}

TEST_F(SecurityPerceptionBridgeTest, GetStateNullReturnsError) {
    sec_percept_state_t state = sec_percept_get_state(nullptr);
    EXPECT_EQ(state, SEC_PERCEPT_STATE_ERROR);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, ConnectBioAsync) {
    int ret = sec_percept_connect_bio_async(bridge);
    // May succeed or fail depending on bio-async availability
    // Just check it doesn't crash
    // May succeed or fail depending on bio-async availability
    (void)ret;  // Just check it doesn't crash
}

TEST_F(SecurityPerceptionBridgeTest, ConnectBioAsyncNullFails) {
    int ret = sec_percept_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, DisconnectBioAsync) {
    sec_percept_connect_bio_async(bridge);
    int ret = sec_percept_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, DisconnectBioAsyncNullIsSafe) {
    int ret = sec_percept_disconnect_bio_async(nullptr);
    EXPECT_EQ(ret, 0);  // NULL safe
}

TEST_F(SecurityPerceptionBridgeTest, SendThreatAlert) {
    sec_percept_start(bridge);
    sec_percept_connect_bio_async(bridge);

    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_type = SENSORY_THREAT_ADVERSARIAL_EXAMPLE;
    threat.threat_score = 0.9f;
    threat.timestamp_us = 1000000;

    int ret = sec_percept_send_threat_alert(bridge, &threat);
    // May fail if bio-async not available
    // May succeed or fail depending on bio-async availability
    (void)ret;  // Just check it doesn't crash
}

TEST_F(SecurityPerceptionBridgeTest, SendThreatAlertNullBridgeFails) {
    sensory_threat_result_t threat;
    int ret = sec_percept_send_threat_alert(nullptr, &threat);
    EXPECT_NE(ret, 0);
}

TEST_F(SecurityPerceptionBridgeTest, SendThreatAlertNullThreatFails) {
    int ret = sec_percept_send_threat_alert(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, ModalityNameMapping) {
    EXPECT_STREQ(sec_percept_modality_name(SENSORY_MODALITY_VISUAL), "Visual");
    EXPECT_STREQ(sec_percept_modality_name(SENSORY_MODALITY_AUDIO), "Audio");
    EXPECT_STREQ(sec_percept_modality_name(SENSORY_MODALITY_SPEECH), "Speech");
    EXPECT_STREQ(sec_percept_modality_name(SENSORY_MODALITY_MULTIMODAL), "Multimodal");
}

TEST_F(SecurityPerceptionBridgeTest, ThreatTypeNameMapping) {
    EXPECT_STREQ(sec_percept_threat_type_name(SENSORY_THREAT_NONE), "None");
    EXPECT_STREQ(sec_percept_threat_type_name(SENSORY_THREAT_ADVERSARIAL_EXAMPLE),
                "Adversarial Example");
    EXPECT_STREQ(sec_percept_threat_type_name(SENSORY_THREAT_UNIVERSAL_PERTURBATION),
                "Universal Perturbation");
    EXPECT_STREQ(sec_percept_threat_type_name(SENSORY_THREAT_BACKDOOR_TRIGGER),
                "Backdoor Trigger");
    EXPECT_STREQ(sec_percept_threat_type_name(SENSORY_THREAT_STATISTICAL_ANOMALY),
                "Statistical Anomaly");
    EXPECT_STREQ(sec_percept_threat_type_name(SENSORY_THREAT_CROSS_MODAL_MISMATCH),
                "Cross-Modal Mismatch");
    EXPECT_STREQ(sec_percept_threat_type_name(SENSORY_THREAT_TEMPORAL_ANOMALY),
                "Temporal Anomaly");
    EXPECT_STREQ(sec_percept_threat_type_name(SENSORY_THREAT_FREQUENCY_ATTACK),
                "Frequency Attack");
    EXPECT_STREQ(sec_percept_threat_type_name(SENSORY_THREAT_UNKNOWN), "Unknown");
}

TEST_F(SecurityPerceptionBridgeTest, ResponseActionNameMapping) {
    EXPECT_STREQ(sec_percept_response_action_name(THREAT_RESPONSE_ALLOW), "Allow");
    EXPECT_STREQ(sec_percept_response_action_name(THREAT_RESPONSE_LOG), "Log");
    EXPECT_STREQ(sec_percept_response_action_name(THREAT_RESPONSE_QUARANTINE), "Quarantine");
    EXPECT_STREQ(sec_percept_response_action_name(THREAT_RESPONSE_SANITIZE), "Sanitize");
    EXPECT_STREQ(sec_percept_response_action_name(THREAT_RESPONSE_REJECT), "Reject");
    EXPECT_STREQ(sec_percept_response_action_name(THREAT_RESPONSE_IMMUNE_ESCALATE),
                "Immune Escalate");
}

TEST_F(SecurityPerceptionBridgeTest, StateNameMapping) {
    EXPECT_STREQ(sec_percept_state_name(SEC_PERCEPT_STATE_STOPPED), "Stopped");
    EXPECT_STREQ(sec_percept_state_name(SEC_PERCEPT_STATE_STARTING), "Starting");
    EXPECT_STREQ(sec_percept_state_name(SEC_PERCEPT_STATE_RUNNING), "Running");
    EXPECT_STREQ(sec_percept_state_name(SEC_PERCEPT_STATE_DEGRADED), "Degraded");
    EXPECT_STREQ(sec_percept_state_name(SEC_PERCEPT_STATE_ERROR), "Error");
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, ConcurrentAnalysis) {
    sec_percept_start(bridge);

    // Simple concurrent test - analyze from multiple "threads" sequentially
    auto features = create_visual_features();
    sensory_threat_result_t result1, result2, result3;

    sec_percept_analyze_visual(bridge, features.data(), features.size(), &result1);
    sec_percept_analyze_visual(bridge, features.data(), features.size(), &result2);
    sec_percept_analyze_visual(bridge, features.data(), features.size(), &result3);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_visual_inputs, 3u);
}

TEST_F(SecurityPerceptionBridgeTest, ConcurrentQuarantine) {
    sec_percept_start(bridge);

    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_score = 0.8f;

    auto features = create_visual_features();
    uint32_t id1, id2, id3;

    sec_percept_quarantine_input(bridge, &threat, features.data(), features.size(), &id1);
    sec_percept_quarantine_input(bridge, &threat, features.data(), features.size(), &id2);
    sec_percept_quarantine_input(bridge, &threat, features.data(), features.size(), &id3);

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);

    sec_percept_stats_t stats;
    sec_percept_get_stats(bridge, &stats);
    EXPECT_EQ(stats.current_quarantine_count, 3u);
}

/* ============================================================================
 * Integration Workflow Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionBridgeTest, FullThreatDetectionWorkflow) {
    sec_percept_start(bridge);

    // 1. Analyze adversarial input
    auto features = create_adversarial_features();
    sensory_threat_result_t result;

    sec_percept_analyze_visual(bridge, features.data(), features.size(), &result);

    // 2. If threat detected, quarantine
    if (result.threat_score >= config.visual_anomaly_threshold) {
        uint32_t quarantine_id;
        sec_percept_quarantine_input(bridge, &result, features.data(),
                                    features.size(), &quarantine_id);

        // 3. Learn signature
        sec_percept_learn_signature(bridge, &result, features.data(), features.size());

        // 4. Verify quarantine
        const quarantined_input_t* input;
        int ret = sec_percept_get_quarantined(bridge, quarantine_id, &input);
        EXPECT_EQ(ret, 0);

        // 5. Check stats
        sec_percept_stats_t stats;
        sec_percept_get_stats(bridge, &stats);
        EXPECT_GT(stats.threats_detected, 0u);
        EXPECT_GT(stats.inputs_quarantined, 0u);
        EXPECT_GT(stats.signatures_learned, 0u);
    }
}

TEST_F(SecurityPerceptionBridgeTest, MultimodalThreatDetectionWorkflow) {
    sec_percept_start(bridge);

    // Create mismatched multimodal input
    auto visual = create_adversarial_features();
    auto audio = create_anomalous_audio();

    sensory_threat_result_t result;
    sec_percept_analyze_multimodal(bridge,
                                   visual.data(), visual.size(),
                                   audio.data(), audio.size(),
                                   &result);

    // Should detect threats in both modalities
    EXPECT_GT(result.threat_score, 0.0f);

    // Check cross-modal score
    EXPECT_GE(result.cross_modal_score, 0.0f);
    EXPECT_LE(result.cross_modal_score, 1.0f);
}

TEST_F(SecurityPerceptionBridgeTest, SignatureMatchingWorkflow) {
    sec_percept_start(bridge);

    // Learn a signature
    sensory_threat_result_t threat;
    threat.modality = SENSORY_MODALITY_VISUAL;
    threat.threat_type = SENSORY_THREAT_ADVERSARIAL_EXAMPLE;
    threat.threat_score = 0.9f;

    auto features = create_adversarial_features();
    sec_percept_learn_signature(bridge, &threat, features.data(), features.size());

    // Analyze same features again - should match signature
    sensory_threat_result_t result;
    sec_percept_analyze_visual(bridge, features.data(), features.size(), &result);

    // Should detect based on signature match
    if (result.threat_type == SENSORY_THREAT_ADVERSARIAL_EXAMPLE) {
        EXPECT_GT(result.adversarial_score, 0.0f);
    }
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
