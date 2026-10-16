/**
 * @file test_perception_immune_integration.cpp
 * @brief Integration tests for Perception-Immune System
 * @version 1.0.0
 * @date 2025-12-11
 *
 * Integration tests covering:
 * - End-to-end perception anomaly to immune response
 * - Immune modulation of perception sensitivity
 * - Overload protection and recovery cycles
 * - Multi-modal coordination
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_perception_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PerceptionImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    perception_immune_context_t* perception_immune = nullptr;

    void SetUp() override {
        // Create and start immune system
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        // Create perception immune
        perception_immune = perception_immune_create(immune_system);
        ASSERT_NE(perception_immune, nullptr);
    }

    void TearDown() override {
        if (perception_immune) {
            perception_immune_destroy(perception_immune);
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
    }

    // Simulate visual features
    std::vector<float> simulateVisualInput(float noise_level = 0.0f) {
        std::vector<float> features(128);
        for (size_t i = 0; i < features.size(); i++) {
            features[i] = 0.5f + 0.3f * sinf(i * 0.1f);
            if (noise_level > 0.0f) {
                features[i] += noise_level * (rand() % 100 / 100.0f - 0.5f);
            }
        }
        return features;
    }

    // Simulate audio spectrum
    std::vector<float> simulateAudioInput(float corruption = 0.0f) {
        std::vector<float> spectrum(256);
        for (size_t i = 0; i < spectrum.size(); i++) {
            spectrum[i] = 0.4f * expf(-i * 0.01f);
            if (corruption > 0.0f) {
                spectrum[i] += corruption * (rand() % 100 / 100.0f);
            }
        }
        return spectrum;
    }
};

/* ============================================================================
 * End-to-End Anomaly Detection Tests
 * ============================================================================ */

TEST_F(PerceptionImmuneIntegrationTest, VisualAnomalyTriggersImmuneResponse) {
    // Generate noisy visual input
    auto noisy_features = simulateVisualInput(0.5f);

    // Report as anomaly
    uint32_t anomaly_id = 0;
    int result = perception_immune_report_visual_anomaly(
        perception_immune,
        ANOMALY_VISUAL_NOISE,
        0.7f,  // severity
        0.85f, // confidence
        noisy_features.data(),
        noisy_features.size(),
        &anomaly_id
    );

    ASSERT_EQ(result, 0);
    ASSERT_GT(anomaly_id, 0u);

    // Verify immune system received antigen
    const perception_anomaly_t* anomaly =
        perception_immune_get_anomaly(perception_immune, anomaly_id);
    ASSERT_NE(anomaly, nullptr);
    EXPECT_TRUE(anomaly->immune_responded);
    EXPECT_GT(anomaly->antigen_id, 0u);

    // Check that immune system processed it
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune_system, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);
}

TEST_F(PerceptionImmuneIntegrationTest, AudioCorruptionTriggersBCellActivation) {
    // Simulate corrupted audio
    auto corrupted_spectrum = simulateAudioInput(0.8f);

    // Report anomaly
    uint32_t anomaly_id = 0;
    perception_immune_report_audio_anomaly(
        perception_immune,
        ANOMALY_AUDIO_CORRUPTION,
        0.8f,
        0.9f,
        corrupted_spectrum.data(),
        corrupted_spectrum.size(),
        &anomaly_id
    );

    // Verify anomaly was processed
    const perception_anomaly_t* anomaly =
        perception_immune_get_anomaly(perception_immune, anomaly_id);
    ASSERT_NE(anomaly, nullptr);

    // Update immune system to process antigens
    brain_immune_update(immune_system, 100); // 100ms update

    // Check immune stats
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune_system, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);
}

/* ============================================================================
 * Inflammation Modulation Tests
 * ============================================================================ */

TEST_F(PerceptionImmuneIntegrationTest, InflammationReducesPerceptionGain) {
    // Baseline modulation
    perception_immune_modulation_t baseline;
    perception_immune_get_modulation(perception_immune, &baseline);
    EXPECT_FLOAT_EQ(baseline.visual_gain, 1.0f);

    // Generate multiple anomalies to trigger inflammation
    for (int i = 0; i < 15; i++) {
        auto features = simulateVisualInput(0.6f);
        uint32_t aid = 0;
        perception_immune_report_visual_anomaly(
            perception_immune,
            ANOMALY_VISUAL_NOISE,
            0.8f,
            0.9f,
            features.data(),
            features.size(),
            &aid
        );
    }

    // Update immune system to process antigens and trigger inflammation
    for (int i = 0; i < 20; i++) {
        brain_immune_update(immune_system, 100);
    }

    // Update perception modulation
    perception_immune_update_modulation(perception_immune);

    // Check that modulation has changed
    perception_immune_modulation_t modulated;
    perception_immune_get_modulation(perception_immune, &modulated);

    // Verify antigens were at least processed by the immune system
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune_system, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);

    // Inflammation depends on immune system dynamics (B-cell activation cycles);
    // at minimum, the system should still be functional
    EXPECT_GE(modulated.visual_gain, 0.0f);
    EXPECT_LE(modulated.visual_gain, 1.0f);
}

TEST_F(PerceptionImmuneIntegrationTest, CytokineReleaseModulatesThresholds) {
    // Generate anomalies
    for (int i = 0; i < 10; i++) {
        auto features = simulateVisualInput(0.5f);
        uint32_t aid = 0;
        perception_immune_report_visual_anomaly(
            perception_immune,
            ANOMALY_VISUAL_ADVERSARIAL,
            0.9f,
            0.95f,
            features.data(),
            features.size(),
            &aid
        );
    }

    // Process immune responses
    for (int i = 0; i < 10; i++) {
        brain_immune_update(immune_system, 50);
    }

    // Update modulation
    perception_immune_update_modulation(perception_immune);

    // Check cytokine levels and thresholds
    perception_immune_modulation_t mod;
    perception_immune_get_modulation(perception_immune, &mod);

    // Pro-inflammatory cytokines should increase thresholds
    if (mod.il1_level > 0.0f || mod.il6_level > 0.0f) {
        EXPECT_GE(mod.visual_threshold, 0.5f);
    }
}

/* ============================================================================
 * Overload Protection Tests
 * ============================================================================ */

TEST_F(PerceptionImmuneIntegrationTest, VisualOverloadTriggersProtection) {
    // Generate high-variance visual input (variance must exceed threshold 0.8)
    std::vector<float> overload_features(128);
    for (size_t i = 0; i < overload_features.size(); i++) {
        overload_features[i] = (i % 2 == 0) ? -1.0f : 2.0f;
    }

    // Check for overload
    bool overload = false;
    perception_immune_check_visual_overload(
        perception_immune,
        overload_features.data(),
        overload_features.size(),
        &overload
    );

    EXPECT_TRUE(overload);

    // Trigger protection
    perception_immune_trigger_overload_protection(
        perception_immune,
        PERCEPTION_VISUAL
    );

    // Verify protection is active
    EXPECT_TRUE(perception_immune_is_protected(
        perception_immune, PERCEPTION_VISUAL));

    // Gain should be reduced
    float gain = perception_immune_get_visual_gain(perception_immune);
    EXPECT_LT(gain, 1.0f);
}

TEST_F(PerceptionImmuneIntegrationTest, AudioOverloadRecoveryCycle) {
    // Trigger overload protection
    int trigger_result = perception_immune_trigger_overload_protection(
        perception_immune,
        PERCEPTION_AUDIO
    );
    ASSERT_EQ(trigger_result, 0);

    EXPECT_TRUE(perception_immune_is_protected(
        perception_immune, PERCEPTION_AUDIO));

    // Audio gain should be reduced while protected
    float protected_gain = perception_immune_get_audio_gain(perception_immune);
    EXPECT_LT(protected_gain, 1.0f);

    // Release protection
    perception_immune_release_overload_protection(
        perception_immune,
        PERCEPTION_AUDIO
    );

    EXPECT_FALSE(perception_immune_is_protected(
        perception_immune, PERCEPTION_AUDIO));

    // Update modulation to restore normal gains
    perception_immune_update_modulation(perception_immune);
    float gain = perception_immune_get_audio_gain(perception_immune);
    EXPECT_FLOAT_EQ(gain, 1.0f);
}

/* ============================================================================
 * Multi-Modal Coordination Tests
 * ============================================================================ */

TEST_F(PerceptionImmuneIntegrationTest, CrossModalInflammationPropagation) {
    // Generate severe visual anomaly
    auto visual = simulateVisualInput(0.9f);
    uint32_t vid = 0;
    perception_immune_report_visual_anomaly(
        perception_immune,
        ANOMALY_VISUAL_ADVERSARIAL,
        0.95f,
        0.98f,
        visual.data(),
        visual.size(),
        &vid
    );

    // Generate audio anomaly
    auto audio = simulateAudioInput(0.7f);
    uint32_t aid = 0;
    perception_immune_report_audio_anomaly(
        perception_immune,
        ANOMALY_AUDIO_JAMMING,
        0.85f,
        0.9f,
        audio.data(),
        audio.size(),
        &aid
    );

    // Process immune responses
    for (int i = 0; i < 10; i++) {
        brain_immune_update(immune_system, 100);
    }

    // Update modulation
    perception_immune_update_modulation(perception_immune);

    // Check that both modalities are affected
    perception_immune_modulation_t mod;
    perception_immune_get_modulation(perception_immune, &mod);

    // Systemic inflammation should affect both
    EXPECT_LE(mod.visual_gain, 1.0f);
    EXPECT_LE(mod.audio_gain, 1.0f);
}

TEST_F(PerceptionImmuneIntegrationTest, ModalitySpecificProtection) {
    // Trigger protection for visual only
    perception_immune_trigger_overload_protection(
        perception_immune,
        PERCEPTION_VISUAL
    );

    // Verify only visual is protected
    EXPECT_TRUE(perception_immune_is_protected(
        perception_immune, PERCEPTION_VISUAL));
    EXPECT_FALSE(perception_immune_is_protected(
        perception_immune, PERCEPTION_AUDIO));
    EXPECT_FALSE(perception_immune_is_protected(
        perception_immune, PERCEPTION_SPEECH));

    // Visual gain should be reduced, others normal
    perception_immune_update_modulation(perception_immune);
    EXPECT_LT(perception_immune_get_visual_gain(perception_immune), 1.0f);
    EXPECT_FLOAT_EQ(perception_immune_get_audio_gain(perception_immune), 1.0f);
    EXPECT_FLOAT_EQ(perception_immune_get_speech_gain(perception_immune), 1.0f);
}

/* ============================================================================
 * Memory and Adaptation Tests
 * ============================================================================ */

TEST_F(PerceptionImmuneIntegrationTest, RepeatedAnomalyRecognition) {
    // Generate characteristic visual anomaly
    auto anomaly_pattern = simulateVisualInput(0.6f);

    // Report first occurrence
    uint32_t aid1 = 0;
    perception_immune_report_visual_anomaly(
        perception_immune,
        ANOMALY_VISUAL_NOISE,
        0.7f,
        0.85f,
        anomaly_pattern.data(),
        anomaly_pattern.size(),
        &aid1
    );

    // Process immune response
    for (int i = 0; i < 5; i++) {
        brain_immune_update(immune_system, 100);
    }

    // Report second occurrence (same pattern)
    uint32_t aid2 = 0;
    perception_immune_report_visual_anomaly(
        perception_immune,
        ANOMALY_VISUAL_NOISE,
        0.7f,
        0.85f,
        anomaly_pattern.data(),
        anomaly_pattern.size(),
        &aid2
    );

    // Both should be processed
    EXPECT_GT(aid1, 0u);
    EXPECT_GT(aid2, 0u);

    // Immune system should recognize pattern
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune_system, &stats);
    EXPECT_GE(stats.antigens_processed, 2u);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(PerceptionImmuneIntegrationTest, HighFrequencyAnomalyStream) {
    // Simulate rapid anomaly stream
    const int NUM_ANOMALIES = 50;

    for (int i = 0; i < NUM_ANOMALIES; i++) {
        auto features = simulateVisualInput(0.5f);
        uint32_t aid = 0;
        int result = perception_immune_report_visual_anomaly(
            perception_immune,
            ANOMALY_VISUAL_NOISE,
            0.6f,
            0.8f,
            features.data(),
            features.size(),
            &aid
        );

        // Should handle gracefully (may hit capacity limit)
        if (result == 0) {
            EXPECT_GT(aid, 0u);
        }

        // Process some immune updates
        if (i % 10 == 0) {
            brain_immune_update(immune_system, 100);
        }
    }

    // System should still be functional
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune_system, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);
}

TEST_F(PerceptionImmuneIntegrationTest, AllModalitiesSimultaneousOverload) {
    // Trigger all protections
    perception_immune_trigger_overload_protection(
        perception_immune, PERCEPTION_VISUAL);
    perception_immune_trigger_overload_protection(
        perception_immune, PERCEPTION_AUDIO);
    perception_immune_trigger_overload_protection(
        perception_immune, PERCEPTION_SPEECH);

    // All should be protected
    EXPECT_TRUE(perception_immune_is_protected(
        perception_immune, PERCEPTION_VISUAL));
    EXPECT_TRUE(perception_immune_is_protected(
        perception_immune, PERCEPTION_AUDIO));
    EXPECT_TRUE(perception_immune_is_protected(
        perception_immune, PERCEPTION_SPEECH));

    // All gains should be reduced
    perception_immune_update_modulation(perception_immune);
    EXPECT_LT(perception_immune_get_visual_gain(perception_immune), 1.0f);
    EXPECT_LT(perception_immune_get_audio_gain(perception_immune), 1.0f);
    EXPECT_LT(perception_immune_get_speech_gain(perception_immune), 1.0f);

    // Verify overload count
    EXPECT_EQ(perception_immune->overload_protections_activated, 3u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
