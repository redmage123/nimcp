/**
 * @file test_neuromod_reasoning_bridge.cpp
 * @brief Unit tests for Neuromodulatory-Superhuman Reasoning Inter-Layer Bridge
 *
 * WHAT: Test suite for neuromod_reasoning_bridge
 * WHY:  Verify correct DA→confidence, NE→control, 5-HT→deliberation, Hab→error
 * HOW:  Unit tests for lifecycle, modulation, metacognition, mode classification
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "integration/inter/neuromod_reasoning/nimcp_neuromod_reasoning_bridge.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeuromodReasoningBridgeTest : public ::testing::Test {
protected:
    neuromod_reasoning_bridge_t* bridge = nullptr;

    void SetUp() override {
        neuromod_reasoning_config_t config = neuromod_reasoning_default_config();
        bridge = neuromod_reasoning_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            neuromod_reasoning_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(NeuromodReasoningCreateTest, CreateWithDefaultConfig) {
    neuromod_reasoning_bridge_t* br = neuromod_reasoning_create(nullptr);
    ASSERT_NE(br, nullptr);
    EXPECT_TRUE(neuromod_reasoning_is_connected(br));
    neuromod_reasoning_destroy(br);
}

TEST(NeuromodReasoningCreateTest, CreateWithCustomConfig) {
    neuromod_reasoning_config_t config = neuromod_reasoning_default_config();
    config.da_confidence_coupling = 0.9f;
    config.ne_control_coupling = 0.8f;
    config.ht_deliberation_coupling = 0.7f;
    config.enable_mode_classification = true;

    neuromod_reasoning_bridge_t* br = neuromod_reasoning_create(&config);
    ASSERT_NE(br, nullptr);
    neuromod_reasoning_destroy(br);
}

TEST(NeuromodReasoningCreateTest, DestroyNull) {
    neuromod_reasoning_destroy(nullptr);
}

//=============================================================================
// DA Confidence Tests
//=============================================================================

TEST_F(NeuromodReasoningBridgeTest, ApplyDAConfidenceLow) {
    float conf;
    int ret = neuromod_reasoning_apply_da_confidence(bridge, 0.2f, &conf);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(conf, 0.7f);
}

TEST_F(NeuromodReasoningBridgeTest, ApplyDAConfidenceHigh) {
    float conf;
    int ret = neuromod_reasoning_apply_da_confidence(bridge, 0.9f, &conf);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(conf, 0.6f);
}

TEST_F(NeuromodReasoningBridgeTest, ApplyDACuriosity) {
    float curiosity;
    int ret = neuromod_reasoning_apply_da_curiosity(bridge, 0.8f, &curiosity);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(curiosity, 0.5f);
}

//=============================================================================
// NE Control Tests (Inverted-U)
//=============================================================================

TEST_F(NeuromodReasoningBridgeTest, ApplyNEControlLow) {
    float control;
    /* Low NE = poor control (drowsy) */
    int ret = neuromod_reasoning_apply_ne_control(bridge, 0.1f, &control);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(control, 0.7f);
}

TEST_F(NeuromodReasoningBridgeTest, ApplyNEControlOptimal) {
    float control;
    /* Optimal NE ~0.6 should give best control */
    int ret = neuromod_reasoning_apply_ne_control(bridge, 0.6f, &control);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(control, 0.5f);
}

TEST_F(NeuromodReasoningBridgeTest, ApplyNEControlHigh) {
    float control;
    /* High NE = anxiety impairs control */
    int ret = neuromod_reasoning_apply_ne_control(bridge, 0.95f, &control);
    EXPECT_EQ(ret, 0);
    /* Should still have some control but not optimal */
}

TEST_F(NeuromodReasoningBridgeTest, ApplyNEAlertness) {
    float alertness;
    int ret = neuromod_reasoning_apply_ne_alertness(bridge, 0.8f, &alertness);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(alertness, 0.5f);
}

//=============================================================================
// 5-HT Deliberation Tests
//=============================================================================

TEST_F(NeuromodReasoningBridgeTest, ApplyHTDeliberationLow) {
    float delib;
    int ret = neuromod_reasoning_apply_ht_deliberation(bridge, 0.2f, &delib);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(delib, 0.5f);
}

TEST_F(NeuromodReasoningBridgeTest, ApplyHTDeliberationHigh) {
    float delib;
    int ret = neuromod_reasoning_apply_ht_deliberation(bridge, 0.9f, &delib);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(delib, 0.5f);
}

TEST_F(NeuromodReasoningBridgeTest, ApplyHTPatience) {
    float patience;
    int ret = neuromod_reasoning_apply_ht_patience(bridge, 0.7f, &patience);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(patience, 0.5f);
}

//=============================================================================
// Habenula Error Tests
//=============================================================================

TEST_F(NeuromodReasoningBridgeTest, ApplyHabErrorLow) {
    float error_sens;
    int ret = neuromod_reasoning_apply_hab_error(bridge, 0.2f, &error_sens);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(error_sens, 0.5f);
}

TEST_F(NeuromodReasoningBridgeTest, ApplyHabErrorHigh) {
    float error_sens;
    int ret = neuromod_reasoning_apply_hab_error(bridge, 0.9f, &error_sens);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(error_sens, 0.5f);
}

TEST_F(NeuromodReasoningBridgeTest, HighHabReducesConfidence) {
    /* Set high confidence first */
    neuromod_reasoning_apply_da_confidence(bridge, 0.9f, nullptr);

    neuromod_reasoning_state_t state_before;
    neuromod_reasoning_get_state(bridge, &state_before);

    /* Apply high habenula */
    neuromod_reasoning_apply_hab_error(bridge, 0.9f, nullptr);

    neuromod_reasoning_state_t state_after;
    neuromod_reasoning_get_state(bridge, &state_after);

    EXPECT_LT(state_after.confidence_level, state_before.confidence_level);
}

//=============================================================================
// Metacognitive Query Tests
//=============================================================================

TEST_F(NeuromodReasoningBridgeTest, ConfidenceCalibrationInsufficientData) {
    /* Not enough samples for calibration */
    float calibration = neuromod_reasoning_get_confidence_calibration(bridge, 0.5f);
    EXPECT_EQ(calibration, 0.5f);
}

TEST_F(NeuromodReasoningBridgeTest, ConfidenceCalibrationAfterSamples) {
    /* Report several successes to build calibration data */
    for (int i = 0; i < 15; i++) {
        neuromod_reasoning_apply_da_confidence(bridge, 0.6f, nullptr);
        neuromod_reasoning_report_success(bridge, 0.6f, nullptr);
    }

    float calibration = neuromod_reasoning_get_confidence_calibration(bridge, 0.6f);
    EXPECT_GE(calibration, 0.0f);
    EXPECT_LE(calibration, 1.0f);
}

TEST_F(NeuromodReasoningBridgeTest, ShouldSwitchModeFromIntuitive) {
    /* Set low NE for intuitive mode */
    neuromod_reasoning_apply_ne_control(bridge, 0.3f, nullptr);
    neuromod_reasoning_compute_modulation(bridge, 0.5f, 0.3f, 0.5f, 0.2f, nullptr);

    /* Then spike NE */
    neuromod_reasoning_apply_ne_control(bridge, 0.9f, nullptr);

    bool should_switch = neuromod_reasoning_should_switch_mode(bridge);
    EXPECT_TRUE(should_switch);
}

TEST_F(NeuromodReasoningBridgeTest, EstimateEffortNeeded) {
    /* Low resources should require high effort */
    neuromod_reasoning_apply_ne_control(bridge, 0.2f, nullptr);
    neuromod_reasoning_apply_ht_deliberation(bridge, 0.2f, nullptr);

    float effort = neuromod_reasoning_estimate_effort_needed(bridge, 0.8f);
    EXPECT_GT(effort, 0.5f);
}

//=============================================================================
// Mode Classification Tests
//=============================================================================

TEST_F(NeuromodReasoningBridgeTest, ClassifyModeIntuitive) {
    neuromod_reasoning_compute_modulation(bridge, 0.5f, 0.3f, 0.5f, 0.2f, nullptr);
    reasoning_mode_t mode = neuromod_reasoning_classify_mode(bridge);
    EXPECT_EQ(mode, REASONING_MODE_INTUITIVE);
}

TEST_F(NeuromodReasoningBridgeTest, ClassifyModeAnalytical) {
    neuromod_reasoning_compute_modulation(bridge, 0.5f, 0.8f, 0.5f, 0.2f, nullptr);
    reasoning_mode_t mode = neuromod_reasoning_classify_mode(bridge);
    EXPECT_EQ(mode, REASONING_MODE_ANALYTICAL);
}

TEST_F(NeuromodReasoningBridgeTest, ClassifyModeCreative) {
    neuromod_reasoning_compute_modulation(bridge, 0.9f, 0.5f, 0.5f, 0.2f, nullptr);
    reasoning_mode_t mode = neuromod_reasoning_classify_mode(bridge);
    EXPECT_EQ(mode, REASONING_MODE_CREATIVE);
}

TEST_F(NeuromodReasoningBridgeTest, ClassifyModeCautious) {
    neuromod_reasoning_compute_modulation(bridge, 0.5f, 0.5f, 0.9f, 0.2f, nullptr);
    reasoning_mode_t mode = neuromod_reasoning_classify_mode(bridge);
    EXPECT_EQ(mode, REASONING_MODE_CAUTIOUS);
}

TEST_F(NeuromodReasoningBridgeTest, ClassifyModeImpaired) {
    neuromod_reasoning_compute_modulation(bridge, 0.5f, 0.5f, 0.5f, 0.9f, nullptr);
    reasoning_mode_t mode = neuromod_reasoning_classify_mode(bridge);
    EXPECT_EQ(mode, REASONING_MODE_IMPAIRED);
}

TEST_F(NeuromodReasoningBridgeTest, ModeNameMapping) {
    EXPECT_STREQ(neuromod_reasoning_mode_name(REASONING_MODE_BALANCED), "Balanced");
    EXPECT_STREQ(neuromod_reasoning_mode_name(REASONING_MODE_INTUITIVE), "Intuitive");
    EXPECT_STREQ(neuromod_reasoning_mode_name(REASONING_MODE_ANALYTICAL), "Analytical");
    EXPECT_STREQ(neuromod_reasoning_mode_name(REASONING_MODE_CREATIVE), "Creative");
    EXPECT_STREQ(neuromod_reasoning_mode_name(REASONING_MODE_CAUTIOUS), "Cautious");
    EXPECT_STREQ(neuromod_reasoning_mode_name(REASONING_MODE_IMPAIRED), "Impaired");
}

//=============================================================================
// Top-Down Feedback Tests
//=============================================================================

TEST_F(NeuromodReasoningBridgeTest, ReportSuccess) {
    float vta_trigger;
    int ret = neuromod_reasoning_report_success(bridge, 0.8f, &vta_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(vta_trigger, 0.0f);
}

TEST_F(NeuromodReasoningBridgeTest, ReportNovelty) {
    float lc_trigger;
    int ret = neuromod_reasoning_report_novelty(bridge, 0.7f, &lc_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(lc_trigger, 0.0f);
}

TEST_F(NeuromodReasoningBridgeTest, ReportDepthNeed) {
    float ht_demand;
    int ret = neuromod_reasoning_report_depth_need(bridge, 0.8f, &ht_demand);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(ht_demand, 0.0f);
}

TEST_F(NeuromodReasoningBridgeTest, ReportError) {
    float hab_trigger;
    int ret = neuromod_reasoning_report_error(bridge, 0.6f, &hab_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(hab_trigger, 0.0f);
}

//=============================================================================
// Unified Modulation Tests
//=============================================================================

TEST_F(NeuromodReasoningBridgeTest, ComputeModulation) {
    neuromod_reasoning_state_t state;
    int ret = neuromod_reasoning_compute_modulation(bridge, 0.5f, 0.6f, 0.5f, 0.2f, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.confidence_level, 0.0f);
    EXPECT_LE(state.confidence_level, 1.0f);
    EXPECT_GE(state.cognitive_control, 0.0f);
    EXPECT_LE(state.cognitive_control, 1.0f);
    EXPECT_GE(state.reasoning_quality, 0.0f);
    EXPECT_LE(state.reasoning_quality, 1.0f);
    EXPECT_GE(state.metacognitive_awareness, 0.0f);
    EXPECT_LE(state.metacognitive_awareness, 1.0f);
}

TEST_F(NeuromodReasoningBridgeTest, ComputeModulationCoherenceReduction) {
    /* High confidence + high error should reduce coherence */
    neuromod_reasoning_apply_da_confidence(bridge, 0.9f, nullptr);
    neuromod_reasoning_report_error(bridge, 0.8f, nullptr);

    neuromod_reasoning_state_t state;
    neuromod_reasoning_compute_modulation(bridge, 0.9f, 0.5f, 0.5f, 0.8f, &state);

    EXPECT_LT(state.bridge_coherence, 1.0f);
}

//=============================================================================
// Update and Stats Tests
//=============================================================================

TEST_F(NeuromodReasoningBridgeTest, Update) {
    int ret = neuromod_reasoning_update(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(NeuromodReasoningBridgeTest, GetStats) {
    neuromod_reasoning_apply_da_confidence(bridge, 0.8f, nullptr);
    neuromod_reasoning_report_success(bridge, 0.7f, nullptr);

    neuromod_reasoning_stats_t stats;
    int ret = neuromod_reasoning_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.confidence_modulations, 0u);
    EXPECT_GT(stats.successful_reasoning, 0u);
}

TEST_F(NeuromodReasoningBridgeTest, ResetStats) {
    neuromod_reasoning_apply_da_confidence(bridge, 0.8f, nullptr);

    int ret = neuromod_reasoning_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    neuromod_reasoning_stats_t stats;
    neuromod_reasoning_get_stats(bridge, &stats);
    EXPECT_EQ(stats.confidence_modulations, 0u);
}

TEST_F(NeuromodReasoningBridgeTest, GetQuality) {
    float quality = neuromod_reasoning_get_quality(bridge);
    EXPECT_GE(quality, 0.0f);
    EXPECT_LE(quality, 1.0f);
}

TEST_F(NeuromodReasoningBridgeTest, GetCoherence) {
    float coherence = neuromod_reasoning_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(NeuromodReasoningBridgeTest, PrintSummary) {
    neuromod_reasoning_print_summary(bridge);
    neuromod_reasoning_print_summary(nullptr);
}
