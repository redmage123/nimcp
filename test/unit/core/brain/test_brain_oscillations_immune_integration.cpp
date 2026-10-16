/**
 * @file test_brain_oscillations_immune_integration.cpp
 * @brief Unit tests for brain oscillations immune system integration
 *
 * WHAT: Test bidirectional immune-oscillation modulation
 * WHY:  Verify cytokine-induced disruption and abnormality detection
 * HOW:  Test immune effects, oscillation abnormality detection, immune notification
 *
 * TEST COVERAGE:
 * - Connection establishment
 * - Immune → Oscillations: Cytokine-induced disruption
 * - Oscillations → Immune: Abnormality detection and notification
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>
#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/nimcp_brain.h"
#include "utils/spectral/nimcp_fft.h"
#include <cmath>
#include <cstring>

// Test fixture for immune-oscillation integration
class BrainOscillationImmuneTest : public ::testing::Test {
protected:
    /* Shared brain (expensive to create) */
    static brain_t shared_brain;

    /* Per-test analyzer and immune system */
    brain_oscillation_analyzer_t* analyzer;
    brain_immune_system_t* immune;

    /* Convenience alias */
    brain_t brain;

    static void SetUpTestSuite() {
        shared_brain = brain_create("oscillation_immune_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 50, 10);
        ASSERT_NE(shared_brain, nullptr);
    }

    static void TearDownTestSuite() {
        if (shared_brain) {
            brain_destroy(shared_brain);
            shared_brain = nullptr;
        }
    }

    void SetUp() override {
        brain = shared_brain;
        ASSERT_NE(brain, nullptr);

        // Create oscillation analyzer per-test (cheap, avoids stale immune connections)
        analyzer = brain_oscillation_create(brain, 500, 250);
        ASSERT_NE(analyzer, nullptr);

        // Create immune system (per-test — lightweight)
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune = brain_immune_create(&config);
        ASSERT_NE(immune, nullptr);

        // Start immune system
        int ret = brain_immune_start(immune);
        ASSERT_EQ(ret, 0);
    }

    void TearDown() override {
        if (analyzer) {
            brain_oscillation_destroy(analyzer);
        }
        if (immune) {
            brain_immune_destroy(immune);
        }
    }

    // Helper: Simulate oscillation data with specific band powers
    void simulate_oscillation_pattern(float delta_power, float theta_power,
                                     float alpha_power, float beta_power,
                                     float gamma_power) {
        // Record synthetic activity data
        for (uint32_t i = 0; i < 125; i++) {  // 500ms at 250Hz = 125 samples
            // Composite signal from multiple bands
            float t = static_cast<float>(i) / 250.0f;
            float signal =
                delta_power * std::sin(2.0f * M_PI * 2.0f * t) +
                theta_power * std::sin(2.0f * M_PI * 6.0f * t) +
                alpha_power * std::sin(2.0f * M_PI * 10.0f * t) +
                beta_power * std::sin(2.0f * M_PI * 20.0f * t) +
                gamma_power * std::sin(2.0f * M_PI * 40.0f * t);

            brain_oscillation_record_value(analyzer, signal);
        }
    }
};

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(BrainOscillationImmuneTest, ConnectImmuneSystem) {
    // Connect oscillation analyzer to immune system
    bool result = brain_oscillation_connect_immune(analyzer, immune);
    EXPECT_TRUE(result);
}

TEST_F(BrainOscillationImmuneTest, ConnectNullAnalyzer) {
    // Attempt to connect with null analyzer
    bool result = brain_oscillation_connect_immune(nullptr, immune);
    EXPECT_FALSE(result);
}

TEST_F(BrainOscillationImmuneTest, ConnectNullImmune) {
    // Attempt to connect with null immune system
    bool result = brain_oscillation_connect_immune(analyzer, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Immune Effects Computation Tests (Immune → Oscillations)
//=============================================================================

TEST_F(BrainOscillationImmuneTest, ComputeEffectsNoInflammation) {
    // Connect systems
    brain_oscillation_connect_immune(analyzer, immune);

    // Compute effects with no inflammation
    immune_oscillation_effects_t effects =
        brain_oscillation_compute_immune_effects(analyzer, 0, 0.0f);

    // Expect baseline (no disruption)
    EXPECT_FLOAT_EQ(effects.delta_amplification, 1.0f);
    EXPECT_FLOAT_EQ(effects.theta_suppression, 1.0f);
    EXPECT_FLOAT_EQ(effects.gamma_suppression, 1.0f);
    EXPECT_FLOAT_EQ(effects.beta_suppression, 1.0f);
    EXPECT_FLOAT_EQ(effects.coherence_disruption, 0.0f);
    EXPECT_FLOAT_EQ(effects.synchrony_disruption, 0.0f);
}

TEST_F(BrainOscillationImmuneTest, ComputeEffectsLocalInflammation) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Local inflammation, moderate cytokine concentration
    immune_oscillation_effects_t effects =
        brain_oscillation_compute_immune_effects(analyzer, 1, 0.5f);

    // Expect minor disruption
    EXPECT_GT(effects.delta_amplification, 1.0f);
    EXPECT_LT(effects.delta_amplification, 1.3f);
    EXPECT_LT(effects.gamma_suppression, 1.0f);
    EXPECT_GT(effects.gamma_suppression, 0.9f);
    EXPECT_LT(effects.coherence_disruption, 0.1f);
}

TEST_F(BrainOscillationImmuneTest, ComputeEffectsSystemicInflammation) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Systemic inflammation, high cytokine concentration
    immune_oscillation_effects_t effects =
        brain_oscillation_compute_immune_effects(analyzer, 3, 0.8f);

    // Expect strong disruption
    EXPECT_GT(effects.delta_amplification, 1.5f);
    EXPECT_LT(effects.gamma_suppression, 0.6f);
    EXPECT_GT(effects.coherence_disruption, 0.3f);
}

TEST_F(BrainOscillationImmuneTest, ComputeEffectsCytokineStorm) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Cytokine storm, maximum concentration
    immune_oscillation_effects_t effects =
        brain_oscillation_compute_immune_effects(analyzer, 4, 1.0f);

    // Expect severe disruption
    EXPECT_GT(effects.delta_amplification, 2.5f);
    EXPECT_LT(effects.gamma_suppression, 0.4f);
    EXPECT_GT(effects.coherence_disruption, 0.6f);
    EXPECT_GT(effects.synchrony_disruption, 0.6f);
}

TEST_F(BrainOscillationImmuneTest, ComputeEffectsCytokineClamp) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Test cytokine concentration clamping
    immune_oscillation_effects_t effects_neg =
        brain_oscillation_compute_immune_effects(analyzer, 1, -0.5f);
    immune_oscillation_effects_t effects_over =
        brain_oscillation_compute_immune_effects(analyzer, 1, 1.5f);

    // Negative should be clamped to 0
    EXPECT_FLOAT_EQ(effects_neg.delta_amplification, 1.0f);

    // Over 1.0 should be clamped to 1.0 (delta_amp = 1.0 + 0.32*1.0 = 1.32)
    EXPECT_NEAR(effects_over.delta_amplification, 1.32f, 0.01f);
}

//=============================================================================
// Immune Effects Application Tests
//=============================================================================

TEST_F(BrainOscillationImmuneTest, ApplyImmuneEffects) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Simulate normal oscillation pattern
    simulate_oscillation_pattern(1.0f, 1.0f, 2.0f, 1.5f, 1.0f);

    // Analyze to populate wave power
    oscillation_analysis_t analysis;
    brain_oscillation_analyze(analyzer, &analysis);

    // Store original gamma power
    float original_gamma = analysis.wave_power.gamma_power;

    // Compute and apply systemic inflammation effects
    immune_oscillation_effects_t effects =
        brain_oscillation_compute_immune_effects(analyzer, 3, 0.8f);
    bool result = brain_oscillation_apply_immune_effects(analyzer, &effects);
    EXPECT_TRUE(result);

    // Get updated wave power
    brain_wave_power_t wave_power;
    brain_oscillation_get_wave_power(analyzer, &wave_power);

    // Gamma should be suppressed
    EXPECT_LT(wave_power.gamma_power, original_gamma);

    // Delta should be amplified (proportionally)
    EXPECT_GT(effects.delta_amplification, 1.0f);
}

TEST_F(BrainOscillationImmuneTest, ApplyEffectsNullInputs) {
    // Test null analyzer
    immune_oscillation_effects_t effects = {0};
    bool result = brain_oscillation_apply_immune_effects(nullptr, &effects);
    EXPECT_FALSE(result);

    // Test null effects
    result = brain_oscillation_apply_immune_effects(analyzer, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Abnormality Detection Tests (Oscillations → Immune)
//=============================================================================

TEST_F(BrainOscillationImmuneTest, DetectNormalPattern) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Simulate normal oscillation pattern (balanced bands)
    simulate_oscillation_pattern(1.0f, 1.0f, 2.0f, 1.5f, 1.0f);

    // Analyze
    oscillation_analysis_t analysis;
    brain_oscillation_analyze(analyzer, &analysis);

    // Detect abnormality
    oscillation_abnormality_t abnormality;
    bool is_abnormal = brain_oscillation_detect_abnormality(analyzer, &abnormality);

    // Should be normal
    EXPECT_FALSE(is_abnormal);
    EXPECT_FALSE(abnormality.excessive_delta);
    EXPECT_FALSE(abnormality.suppressed_gamma);
    EXPECT_LT(abnormality.abnormality_score, 0.5f);
}

TEST_F(BrainOscillationImmuneTest, DetectExcessiveDelta) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Simulate excessive delta pattern (infection marker)
    simulate_oscillation_pattern(5.0f, 0.5f, 0.5f, 0.5f, 0.2f);

    // Analyze
    oscillation_analysis_t analysis;
    brain_oscillation_analyze(analyzer, &analysis);

    // Detect abnormality
    oscillation_abnormality_t abnormality;
    bool is_abnormal = brain_oscillation_detect_abnormality(analyzer, &abnormality);

    // Should detect excessive delta
    EXPECT_TRUE(is_abnormal);
    EXPECT_TRUE(abnormality.excessive_delta);
    EXPECT_GT(abnormality.abnormality_score, 0.5f);
}

TEST_F(BrainOscillationImmuneTest, DetectSuppressedGamma) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Simulate suppressed gamma pattern (cognitive impairment)
    simulate_oscillation_pattern(1.0f, 1.0f, 2.0f, 1.0f, 0.05f);

    // Analyze
    oscillation_analysis_t analysis;
    brain_oscillation_analyze(analyzer, &analysis);

    // Detect abnormality
    oscillation_abnormality_t abnormality;
    bool is_abnormal = brain_oscillation_detect_abnormality(analyzer, &abnormality);

    // Should detect suppressed gamma
    EXPECT_TRUE(is_abnormal);
    EXPECT_TRUE(abnormality.suppressed_gamma);
    EXPECT_GT(abnormality.abnormality_score, 0.2f);
}

TEST_F(BrainOscillationImmuneTest, DetectMultipleAbnormalities) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Simulate severe dysfunction (high delta, low gamma)
    simulate_oscillation_pattern(10.0f, 0.3f, 0.3f, 0.3f, 0.01f);

    // Analyze
    oscillation_analysis_t analysis;
    brain_oscillation_analyze(analyzer, &analysis);

    // Detect abnormality
    oscillation_abnormality_t abnormality;
    bool is_abnormal = brain_oscillation_detect_abnormality(analyzer, &abnormality);

    // Should detect multiple abnormalities
    EXPECT_TRUE(is_abnormal);
    EXPECT_TRUE(abnormality.excessive_delta);
    EXPECT_TRUE(abnormality.suppressed_gamma);
    EXPECT_GT(abnormality.abnormality_score, 0.5f);
}

TEST_F(BrainOscillationImmuneTest, TrackConsecutiveAbnormal) {
    brain_oscillation_connect_immune(analyzer, immune);

    // First abnormal reading
    simulate_oscillation_pattern(5.0f, 0.5f, 0.5f, 0.5f, 0.2f);
    oscillation_analysis_t analysis1;
    brain_oscillation_analyze(analyzer, &analysis1);
    oscillation_abnormality_t abnormality1;
    brain_oscillation_detect_abnormality(analyzer, &abnormality1);
    EXPECT_EQ(abnormality1.consecutive_abnormal, 1u);

    // Second abnormal reading
    simulate_oscillation_pattern(5.0f, 0.5f, 0.5f, 0.5f, 0.2f);
    oscillation_analysis_t analysis2;
    brain_oscillation_analyze(analyzer, &analysis2);
    oscillation_abnormality_t abnormality2;
    brain_oscillation_detect_abnormality(analyzer, &abnormality2);
    EXPECT_EQ(abnormality2.consecutive_abnormal, 2u);

    // Normal reading resets counter
    simulate_oscillation_pattern(1.0f, 1.0f, 2.0f, 1.5f, 1.0f);
    oscillation_analysis_t analysis3;
    brain_oscillation_analyze(analyzer, &analysis3);
    oscillation_abnormality_t abnormality3;
    brain_oscillation_detect_abnormality(analyzer, &abnormality3);
    EXPECT_EQ(abnormality3.consecutive_abnormal, 0u);
}

TEST_F(BrainOscillationImmuneTest, DetectAbnormalityNullInputs) {
    oscillation_abnormality_t abnormality;

    // Null analyzer
    bool result = brain_oscillation_detect_abnormality(nullptr, &abnormality);
    EXPECT_FALSE(result);

    // Null abnormality
    result = brain_oscillation_detect_abnormality(analyzer, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Immune Notification Tests
//=============================================================================

TEST_F(BrainOscillationImmuneTest, NotifyImmuneAbnormality) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Simulate abnormal pattern
    simulate_oscillation_pattern(10.0f, 0.3f, 0.3f, 0.3f, 0.01f);
    oscillation_analysis_t analysis;
    brain_oscillation_analyze(analyzer, &analysis);

    // Detect abnormality
    oscillation_abnormality_t abnormality;
    bool is_abnormal = brain_oscillation_detect_abnormality(analyzer, &abnormality);
    ASSERT_TRUE(is_abnormal);

    // Notify immune system
    bool result = brain_oscillation_notify_immune_abnormality(analyzer, &abnormality);
    EXPECT_TRUE(result);

    // Verify antigen was created in immune system
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);
}

TEST_F(BrainOscillationImmuneTest, NotifyImmuneNotConnected) {
    // Do NOT connect immune system

    // Create dummy abnormality
    oscillation_abnormality_t abnormality;
    memset(&abnormality, 0, sizeof(abnormality));
    abnormality.excessive_delta = true;
    abnormality.abnormality_score = 0.8f;

    // Attempt to notify (should fail - not connected)
    bool result = brain_oscillation_notify_immune_abnormality(analyzer, &abnormality);
    EXPECT_FALSE(result);
}

TEST_F(BrainOscillationImmuneTest, NotifyImmuneNullInputs) {
    brain_oscillation_connect_immune(analyzer, immune);
    oscillation_abnormality_t abnormality;

    // Null analyzer
    bool result = brain_oscillation_notify_immune_abnormality(nullptr, &abnormality);
    EXPECT_FALSE(result);

    // Null abnormality
    result = brain_oscillation_notify_immune_abnormality(analyzer, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// End-to-End Integration Tests
//=============================================================================

TEST_F(BrainOscillationImmuneTest, E2E_ImmuneDisruptsOscillations) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Simulate normal oscillations
    simulate_oscillation_pattern(1.0f, 1.0f, 2.0f, 1.5f, 1.0f);
    oscillation_analysis_t baseline;
    brain_oscillation_analyze(analyzer, &baseline);
    float baseline_gamma = baseline.wave_power.gamma_power;

    // Trigger immune inflammation
    immune_oscillation_effects_t effects =
        brain_oscillation_compute_immune_effects(analyzer, 3, 0.9f);
    brain_oscillation_apply_immune_effects(analyzer, &effects);

    // Re-analyze with immune effects
    brain_wave_power_t disrupted_power;
    brain_oscillation_get_wave_power(analyzer, &disrupted_power);

    // Verify gamma suppression
    EXPECT_LT(disrupted_power.gamma_power, baseline_gamma);
}

TEST_F(BrainOscillationImmuneTest, E2E_AbnormalOscillationsTriggersImmune) {
    brain_oscillation_connect_immune(analyzer, immune);

    // Simulate severe abnormality (infection-like pattern)
    simulate_oscillation_pattern(10.0f, 0.2f, 0.2f, 0.2f, 0.01f);
    oscillation_analysis_t analysis;
    brain_oscillation_analyze(analyzer, &analysis);

    // Detect and notify
    oscillation_abnormality_t abnormality;
    bool is_abnormal = brain_oscillation_detect_abnormality(analyzer, &abnormality);
    ASSERT_TRUE(is_abnormal);

    brain_oscillation_notify_immune_abnormality(analyzer, &abnormality);

    // Verify immune system responded
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);
}

TEST_F(BrainOscillationImmuneTest, E2E_BidirectionalFeedback) {
    brain_oscillation_connect_immune(analyzer, immune);

    // 1. Normal state
    simulate_oscillation_pattern(1.0f, 1.0f, 2.0f, 1.5f, 1.0f);
    oscillation_analysis_t normal_analysis;
    brain_oscillation_analyze(analyzer, &normal_analysis);

    // 2. Immune activation causes oscillation disruption
    immune_oscillation_effects_t effects =
        brain_oscillation_compute_immune_effects(analyzer, 3, 0.8f);
    brain_oscillation_apply_immune_effects(analyzer, &effects);

    // 3. Disrupted oscillations detected as abnormal
    oscillation_abnormality_t abnormality;
    bool is_abnormal = brain_oscillation_detect_abnormality(analyzer, &abnormality);

    // Should detect abnormality due to immune-induced disruption
    // Note: This test validates the feedback loop concept
    EXPECT_TRUE(is_abnormal || abnormality.abnormality_score > 0.3f);
}

//=============================================================================
// Main
//=============================================================================

brain_t BrainOscillationImmuneTest::shared_brain = nullptr;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
