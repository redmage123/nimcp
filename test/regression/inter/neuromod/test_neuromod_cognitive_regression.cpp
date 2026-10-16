/**
 * @file test_neuromod_cognitive_regression.cpp
 * @brief Regression tests for Neuromodulatory-Cognitive Bridge Performance/Stability
 *
 * WHAT: Regression test suite for neuromod-cognitive bridges
 * WHY:  Ensure performance benchmarks and stability over repeated operations
 * HOW:  Stress tests, repeated operations, boundary conditions, performance timing
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>
#include <vector>
#include <random>

extern "C" {
#include "integration/inter/neuromod_attention/nimcp_neuromod_attention_bridge.h"
#include "integration/inter/neuromod_wm/nimcp_neuromod_wm_bridge.h"
#include "integration/inter/neuromod_emotion/nimcp_neuromod_emotion_bridge.h"
#include "integration/inter/neuromod_plasticity/nimcp_neuromod_plasticity_bridge.h"
#include "integration/inter/neuromod_gametheory/nimcp_neuromod_gametheory_bridge.h"
#include "integration/inter/neuromod_reasoning/nimcp_neuromod_reasoning_bridge.h"
}

//=============================================================================
// Performance Test Fixture
//=============================================================================

class NeuromodCognitiveRegressionTest : public ::testing::Test {
protected:
    static constexpr int STRESS_ITERATIONS = 1000;
    static constexpr int RAPID_ITERATIONS = 10000;
    static constexpr float EPSILON = 1e-5f;

    neuromod_attention_bridge_t* attention_bridge = nullptr;
    neuromod_wm_bridge_t* wm_bridge = nullptr;
    neuromod_emotion_bridge_t* emotion_bridge = nullptr;
    neuromod_plasticity_bridge_t* plasticity_bridge = nullptr;
    neuromod_gametheory_bridge_t* gametheory_bridge = nullptr;
    neuromod_reasoning_bridge_t* reasoning_bridge = nullptr;

    std::mt19937 rng{42};  /* Fixed seed for reproducibility */

    void SetUp() override {
        attention_bridge = neuromod_attention_create(nullptr);
        wm_bridge = neuromod_wm_create(nullptr);
        emotion_bridge = neuromod_emotion_create(nullptr);
        plasticity_bridge = neuromod_plasticity_create(nullptr);
        gametheory_bridge = neuromod_gametheory_create(nullptr);
        reasoning_bridge = neuromod_reasoning_create(nullptr);

        ASSERT_NE(attention_bridge, nullptr);
        ASSERT_NE(wm_bridge, nullptr);
        ASSERT_NE(emotion_bridge, nullptr);
        ASSERT_NE(plasticity_bridge, nullptr);
        ASSERT_NE(gametheory_bridge, nullptr);
        ASSERT_NE(reasoning_bridge, nullptr);
    }

    void TearDown() override {
        if (attention_bridge) neuromod_attention_destroy(attention_bridge);
        if (wm_bridge) neuromod_wm_destroy(wm_bridge);
        if (emotion_bridge) neuromod_emotion_destroy(emotion_bridge);
        if (plasticity_bridge) neuromod_plasticity_destroy(plasticity_bridge);
        if (gametheory_bridge) neuromod_gametheory_destroy(gametheory_bridge);
        if (reasoning_bridge) neuromod_reasoning_destroy(reasoning_bridge);
    }

    float random_level() {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng);
    }
};

//=============================================================================
// Attention Bridge Regression Tests
//=============================================================================

TEST_F(NeuromodCognitiveRegressionTest, AttentionBridgeStressTest) {
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        float ne = random_level();
        float da = random_level();
        float ht = random_level();

        float gain, salience, patience;
        neuromod_attention_apply_ne_gain(attention_bridge, ne, &gain);
        neuromod_attention_apply_da_salience(attention_bridge, da, &salience);
        neuromod_attention_apply_ht_patience(attention_bridge, ht, &patience);

        EXPECT_GE(gain, 0.0f);
        EXPECT_LE(gain, 10.0f);
        EXPECT_GE(salience, 0.0f);
        EXPECT_LE(salience, 1.0f);
        EXPECT_GE(patience, 0.0f);
        EXPECT_LE(patience, 1.0f);
    }

    EXPECT_TRUE(neuromod_attention_is_connected(attention_bridge));
}

TEST_F(NeuromodCognitiveRegressionTest, AttentionBridgeRapidUpdates) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < RAPID_ITERATIONS; i++) {
        neuromod_attention_update(attention_bridge, 1.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Should complete 10000 updates in under 1 second */
    EXPECT_LT(duration.count(), 1000);
    EXPECT_TRUE(neuromod_attention_is_connected(attention_bridge));
}

TEST_F(NeuromodCognitiveRegressionTest, AttentionBridgeBoundaryConditions) {
    float out;

    /* Zero inputs */
    EXPECT_EQ(neuromod_attention_apply_ne_gain(attention_bridge, 0.0f, &out), 0);
    EXPECT_GE(out, 0.0f);

    /* Maximum inputs */
    EXPECT_EQ(neuromod_attention_apply_ne_gain(attention_bridge, 1.0f, &out), 0);
    EXPECT_GE(out, 0.0f);

    /* Extreme inputs (clamped internally) */
    EXPECT_EQ(neuromod_attention_apply_ne_gain(attention_bridge, 2.0f, &out), 0);
    EXPECT_GE(out, 0.0f);

    EXPECT_EQ(neuromod_attention_apply_ne_gain(attention_bridge, -0.5f, &out), 0);
    EXPECT_GE(out, 0.0f);
}

//=============================================================================
// Working Memory Bridge Regression Tests
//=============================================================================

TEST_F(NeuromodCognitiveRegressionTest, WMBridgeInvertedUConsistency) {
    /* Verify inverted-U is consistent across many samples */
    std::vector<float> gains_low, gains_mid, gains_high;

    for (int i = 0; i < 100; i++) {
        float gain;
        neuromod_wm_apply_da_gain(wm_bridge, 0.2f, &gain);
        gains_low.push_back(gain);
        neuromod_wm_apply_da_gain(wm_bridge, 0.5f, &gain);
        gains_mid.push_back(gain);
        neuromod_wm_apply_da_gain(wm_bridge, 0.8f, &gain);
        gains_high.push_back(gain);
    }

    /* Check consistency - all samples at same input should give same output */
    for (size_t i = 1; i < gains_low.size(); i++) {
        EXPECT_NEAR(gains_low[i], gains_low[0], EPSILON);
        EXPECT_NEAR(gains_mid[i], gains_mid[0], EPSILON);
        EXPECT_NEAR(gains_high[i], gains_high[0], EPSILON);
    }

    /* Mid should be highest (inverted-U peak) */
    EXPECT_GT(gains_mid[0], gains_low[0]);
    EXPECT_GT(gains_mid[0], gains_high[0]);
}

TEST_F(NeuromodCognitiveRegressionTest, WMBridgeStressTest) {
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        float da = random_level();
        float ne = random_level();
        float ht = random_level();

        neuromod_wm_state_t state;
        int ret = neuromod_wm_compute_modulation(wm_bridge, da, ne, ht, &state);
        EXPECT_EQ(ret, 0);

        EXPECT_GE(state.wm_gain, 0.0f);
        EXPECT_GE(state.bridge_coherence, 0.0f);
        EXPECT_LE(state.bridge_coherence, 1.0f);
    }
}

TEST_F(NeuromodCognitiveRegressionTest, WMBridgeD1D2Balance) {
    /* D1 and D2 should provide complementary effects */
    float stability, flexibility;

    neuromod_wm_apply_d1_stability(wm_bridge, 0.8f, &stability);
    neuromod_wm_apply_d2_flexibility(wm_bridge, 0.8f, &flexibility);

    EXPECT_GT(stability, 0.0f);
    EXPECT_GE(flexibility, 0.0f);
}

//=============================================================================
// Emotion Bridge Regression Tests
//=============================================================================

TEST_F(NeuromodCognitiveRegressionTest, EmotionBridgeValenceRange) {
    /* Valence should always be in [-1, 1] */
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        float da = random_level();
        float valence;
        neuromod_emotion_apply_da_valence(emotion_bridge, da, &valence);

        EXPECT_GE(valence, -1.0f);
        EXPECT_LE(valence, 1.0f);
    }
}

TEST_F(NeuromodCognitiveRegressionTest, EmotionBridgeStateClassificationConsistency) {
    /* Same inputs should always produce same classification */
    neuromod_emotion_compute_modulation(emotion_bridge, 0.9f, 0.1f, 0.3f, 0.2f, nullptr);
    emotional_state_t state1 = neuromod_emotion_classify_state(emotion_bridge);

    for (int i = 0; i < 100; i++) {
        neuromod_emotion_compute_modulation(emotion_bridge, 0.9f, 0.1f, 0.3f, 0.2f, nullptr);
        emotional_state_t state2 = neuromod_emotion_classify_state(emotion_bridge);
        EXPECT_EQ(state1, state2);
    }
}

TEST_F(NeuromodCognitiveRegressionTest, EmotionBridgeStabilityTracking) {
    float stability = neuromod_emotion_get_stability(emotion_bridge);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);

    /* Stability should remain bounded after many operations */
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        neuromod_emotion_apply_ne_arousal(emotion_bridge, random_level(), nullptr);
        neuromod_emotion_apply_da_valence(emotion_bridge, random_level(), nullptr);
    }

    stability = neuromod_emotion_get_stability(emotion_bridge);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

//=============================================================================
// Plasticity Bridge Regression Tests
//=============================================================================

TEST_F(NeuromodCognitiveRegressionTest, PlasticityBridgeRPEConsistency) {
    /* RPE should have consistent sign with reward - expected
     * Implementation may scale RPE by coupling factors */
    for (int i = 0; i < 100; i++) {
        float reward = random_level();
        float expected = random_level();
        float rpe;

        neuromod_plasticity_apply_reward_pe(plasticity_bridge, reward, expected, &rpe);

        float raw_rpe = reward - expected;
        /* RPE sign should match raw difference sign */
        if (fabsf(raw_rpe) > 0.1f) {  /* Only check significant differences */
            EXPECT_EQ(rpe > 0, raw_rpe > 0) << "RPE sign mismatch for reward=" << reward
                                           << " expected=" << expected;
        }
    }
}

TEST_F(NeuromodCognitiveRegressionTest, PlasticityBridgeEligibilityDecay) {
    /* Set high eligibility */
    neuromod_plasticity_set_eligibility(plasticity_bridge, 1.0f);

    neuromod_plasticity_state_t state_before;
    neuromod_plasticity_get_state(plasticity_bridge, &state_before);

    /* Decay over time */
    for (int i = 0; i < 100; i++) {
        neuromod_plasticity_decay_eligibility(plasticity_bridge, 10.0f);
    }

    neuromod_plasticity_state_t state_after;
    neuromod_plasticity_get_state(plasticity_bridge, &state_after);

    /* Should have decayed */
    EXPECT_LT(state_after.eligibility_level, state_before.eligibility_level);
    /* But never negative */
    EXPECT_GE(state_after.eligibility_level, 0.0f);
}

TEST_F(NeuromodCognitiveRegressionTest, PlasticityBridgeModeTransitions) {
    /* Test all mode transitions */
    neuromod_plasticity_apply_ne_boost(plasticity_bridge, 0.9f, nullptr);
    EXPECT_EQ(neuromod_plasticity_get_mode(plasticity_bridge), PLASTICITY_MODE_BOOSTED);

    neuromod_plasticity_apply_da_gating(plasticity_bridge, 0.05f, nullptr);
    EXPECT_EQ(neuromod_plasticity_get_mode(plasticity_bridge), PLASTICITY_MODE_GATED);

    neuromod_plasticity_apply_hab_avoidance(plasticity_bridge, 0.9f, nullptr);
    EXPECT_EQ(neuromod_plasticity_get_mode(plasticity_bridge), PLASTICITY_MODE_SUPPRESSED);

    /* Reset and test consolidation */
    neuromod_plasticity_bridge_t* fresh = neuromod_plasticity_create(nullptr);
    neuromod_plasticity_apply_ne_boost(fresh, 0.2f, nullptr);
    neuromod_plasticity_apply_ht_consolidation(fresh, 0.9f, nullptr);
    EXPECT_EQ(neuromod_plasticity_get_mode(fresh), PLASTICITY_MODE_CONSOLIDATING);
    neuromod_plasticity_destroy(fresh);
}

//=============================================================================
// Game Theory Bridge Regression Tests
//=============================================================================

TEST_F(NeuromodCognitiveRegressionTest, GameTheoryBridgeDecisionConsistency) {
    /* Same inputs should always produce same decision */
    neuromod_gametheory_apply_ht_cooperation(gametheory_bridge, 0.8f, nullptr);
    neuromod_gametheory_apply_da_risk(gametheory_bridge, 0.5f, nullptr);

    bool decision1 = neuromod_gametheory_should_cooperate(gametheory_bridge, 0.7f);

    for (int i = 0; i < 100; i++) {
        bool decision2 = neuromod_gametheory_should_cooperate(gametheory_bridge, 0.7f);
        EXPECT_EQ(decision1, decision2);
    }
}

TEST_F(NeuromodCognitiveRegressionTest, GameTheoryBridgeOfferEvaluation) {
    neuromod_gametheory_apply_ht_cooperation(gametheory_bridge, 0.8f, nullptr);

    /* Fair offers should be more acceptable than unfair */
    float accept_fair = neuromod_gametheory_evaluate_offer(gametheory_bridge, 0.5f);
    float accept_unfair = neuromod_gametheory_evaluate_offer(gametheory_bridge, 0.1f);

    EXPECT_GT(accept_fair, accept_unfair);
}

TEST_F(NeuromodCognitiveRegressionTest, GameTheoryBridgeTrustAfterBetrayal) {
    neuromod_gametheory_state_t state_before;
    neuromod_gametheory_get_state(gametheory_bridge, &state_before);

    /* Multiple betrayals should reduce trust */
    for (int i = 0; i < 5; i++) {
        neuromod_gametheory_report_betrayal(gametheory_bridge, 0.8f, nullptr);
    }

    neuromod_gametheory_state_t state_after;
    neuromod_gametheory_get_state(gametheory_bridge, &state_after);

    EXPECT_LT(state_after.trust_level, state_before.trust_level);
    EXPECT_GE(state_after.trust_level, 0.0f);  /* Never negative */
}

TEST_F(NeuromodCognitiveRegressionTest, GameTheoryBridgeStrategyStability) {
    /* Strategy classification should be stable for given state */
    neuromod_gametheory_compute_modulation(gametheory_bridge, 0.9f, 0.3f, 0.4f, 0.2f, nullptr);
    gt_strategy_t strategy1 = neuromod_gametheory_classify_strategy(gametheory_bridge);

    for (int i = 0; i < 100; i++) {
        gt_strategy_t strategy2 = neuromod_gametheory_classify_strategy(gametheory_bridge);
        EXPECT_EQ(strategy1, strategy2);
    }
}

//=============================================================================
// Reasoning Bridge Regression Tests
//=============================================================================

TEST_F(NeuromodCognitiveRegressionTest, ReasoningBridgeInvertedUControl) {
    /* NE should follow inverted-U for cognitive control */
    float control_low, control_mid, control_high;

    neuromod_reasoning_apply_ne_control(reasoning_bridge, 0.1f, &control_low);
    neuromod_reasoning_apply_ne_control(reasoning_bridge, 0.6f, &control_mid);
    neuromod_reasoning_apply_ne_control(reasoning_bridge, 0.95f, &control_high);

    /* Mid should be highest */
    EXPECT_GT(control_mid, control_low);
    EXPECT_GE(control_mid, control_high);
}

TEST_F(NeuromodCognitiveRegressionTest, ReasoningBridgeModeClassificationCoverage) {
    /* Test all mode classifications are reachable */
    std::vector<reasoning_mode_t> modes_seen;

    /* Intuitive: low NE */
    neuromod_reasoning_compute_modulation(reasoning_bridge, 0.5f, 0.3f, 0.5f, 0.2f, nullptr);
    modes_seen.push_back(neuromod_reasoning_classify_mode(reasoning_bridge));

    /* Analytical: high NE */
    neuromod_reasoning_compute_modulation(reasoning_bridge, 0.5f, 0.8f, 0.5f, 0.2f, nullptr);
    modes_seen.push_back(neuromod_reasoning_classify_mode(reasoning_bridge));

    /* Creative: high DA */
    neuromod_reasoning_compute_modulation(reasoning_bridge, 0.9f, 0.5f, 0.5f, 0.2f, nullptr);
    modes_seen.push_back(neuromod_reasoning_classify_mode(reasoning_bridge));

    /* Cautious: high 5-HT */
    neuromod_reasoning_compute_modulation(reasoning_bridge, 0.5f, 0.5f, 0.9f, 0.2f, nullptr);
    modes_seen.push_back(neuromod_reasoning_classify_mode(reasoning_bridge));

    /* Impaired: high Hab */
    neuromod_reasoning_compute_modulation(reasoning_bridge, 0.5f, 0.5f, 0.5f, 0.9f, nullptr);
    modes_seen.push_back(neuromod_reasoning_classify_mode(reasoning_bridge));

    /* Verify we got distinct modes */
    EXPECT_EQ(modes_seen[0], REASONING_MODE_INTUITIVE);
    EXPECT_EQ(modes_seen[1], REASONING_MODE_ANALYTICAL);
    EXPECT_EQ(modes_seen[2], REASONING_MODE_CREATIVE);
    EXPECT_EQ(modes_seen[3], REASONING_MODE_CAUTIOUS);
    EXPECT_EQ(modes_seen[4], REASONING_MODE_IMPAIRED);
}

TEST_F(NeuromodCognitiveRegressionTest, ReasoningBridgeConfidenceCalibration) {
    /* Build up calibration data */
    for (int i = 0; i < 20; i++) {
        neuromod_reasoning_apply_da_confidence(reasoning_bridge, 0.7f, nullptr);
        neuromod_reasoning_report_success(reasoning_bridge, 0.7f, nullptr);
    }

    float calibration = neuromod_reasoning_get_confidence_calibration(reasoning_bridge, 0.7f);
    EXPECT_GE(calibration, 0.0f);
    EXPECT_LE(calibration, 1.0f);
}

//=============================================================================
// Cross-Bridge Performance Tests
//=============================================================================

TEST_F(NeuromodCognitiveRegressionTest, AllBridgesRapidCycle) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < RAPID_ITERATIONS / 10; i++) {
        float ne = random_level();
        float da = random_level();
        float ht = random_level();
        float hab = random_level();

        neuromod_attention_compute_modulation(attention_bridge, ne, da, ht, hab, nullptr);
        neuromod_wm_compute_modulation(wm_bridge, da, ne, ht, nullptr);
        neuromod_emotion_compute_modulation(emotion_bridge, ne, da, ht, hab, nullptr);
        neuromod_plasticity_compute_modulation(plasticity_bridge, da, ne, ht, hab, nullptr);
        neuromod_gametheory_compute_modulation(gametheory_bridge, ht, da, ne, hab, nullptr);
        neuromod_reasoning_compute_modulation(reasoning_bridge, da, ne, ht, hab, nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* 1000 cycles of all bridges should complete in under 2 seconds */
    EXPECT_LT(duration.count(), 2000);
}

TEST_F(NeuromodCognitiveRegressionTest, AllBridgesCoherenceBounds) {
    /* Coherence should always be in [0, 1] */
    for (int i = 0; i < 100; i++) {
        float ne = random_level();
        float da = random_level();
        float ht = random_level();
        float hab = random_level();

        neuromod_attention_compute_modulation(attention_bridge, ne, da, ht, hab, nullptr);
        neuromod_wm_compute_modulation(wm_bridge, da, ne, ht, nullptr);
        neuromod_emotion_compute_modulation(emotion_bridge, ne, da, ht, hab, nullptr);
        neuromod_plasticity_compute_modulation(plasticity_bridge, da, ne, ht, hab, nullptr);
        neuromod_gametheory_compute_modulation(gametheory_bridge, ht, da, ne, hab, nullptr);
        neuromod_reasoning_compute_modulation(reasoning_bridge, da, ne, ht, hab, nullptr);

        float coh1 = neuromod_attention_get_coherence(attention_bridge);
        float coh2 = neuromod_wm_get_coherence(wm_bridge);
        float coh3 = neuromod_emotion_get_coherence(emotion_bridge);
        float coh4 = neuromod_plasticity_get_coherence(plasticity_bridge);
        float coh5 = neuromod_gametheory_get_coherence(gametheory_bridge);
        float coh6 = neuromod_reasoning_get_coherence(reasoning_bridge);

        EXPECT_GE(coh1, 0.0f); EXPECT_LE(coh1, 1.0f);
        EXPECT_GE(coh2, 0.0f); EXPECT_LE(coh2, 1.0f);
        EXPECT_GE(coh3, 0.0f); EXPECT_LE(coh3, 1.0f);
        EXPECT_GE(coh4, 0.0f); EXPECT_LE(coh4, 1.0f);
        EXPECT_GE(coh5, 0.0f); EXPECT_LE(coh5, 1.0f);
        EXPECT_GE(coh6, 0.0f); EXPECT_LE(coh6, 1.0f);
    }
}

//=============================================================================
// Memory Leak and Resource Tests
//=============================================================================

TEST_F(NeuromodCognitiveRegressionTest, RepeatedCreateDestroy) {
    /* Create and destroy many times - should not leak */
    for (int i = 0; i < 100; i++) {
        neuromod_attention_bridge_t* att = neuromod_attention_create(nullptr);
        neuromod_wm_bridge_t* wm = neuromod_wm_create(nullptr);
        neuromod_emotion_bridge_t* emo = neuromod_emotion_create(nullptr);
        neuromod_plasticity_bridge_t* plas = neuromod_plasticity_create(nullptr);
        neuromod_gametheory_bridge_t* gt = neuromod_gametheory_create(nullptr);
        neuromod_reasoning_bridge_t* reason = neuromod_reasoning_create(nullptr);

        ASSERT_NE(att, nullptr);
        ASSERT_NE(wm, nullptr);
        ASSERT_NE(emo, nullptr);
        ASSERT_NE(plas, nullptr);
        ASSERT_NE(gt, nullptr);
        ASSERT_NE(reason, nullptr);

        neuromod_attention_destroy(att);
        neuromod_wm_destroy(wm);
        neuromod_emotion_destroy(emo);
        neuromod_plasticity_destroy(plas);
        neuromod_gametheory_destroy(gt);
        neuromod_reasoning_destroy(reason);
    }
}

TEST_F(NeuromodCognitiveRegressionTest, NullHandling) {
    /* All bridges should handle null gracefully */
    neuromod_attention_destroy(nullptr);
    neuromod_wm_destroy(nullptr);
    neuromod_emotion_destroy(nullptr);
    neuromod_plasticity_destroy(nullptr);
    neuromod_gametheory_destroy(nullptr);
    neuromod_reasoning_destroy(nullptr);

    EXPECT_FALSE(neuromod_attention_is_connected(nullptr));
    EXPECT_FALSE(neuromod_wm_is_connected(nullptr));
    EXPECT_FALSE(neuromod_emotion_is_connected(nullptr));
    EXPECT_FALSE(neuromod_plasticity_is_connected(nullptr));
    EXPECT_FALSE(neuromod_gametheory_is_connected(nullptr));
    EXPECT_FALSE(neuromod_reasoning_is_connected(nullptr));
}

//=============================================================================
// Stats Overflow Tests
//=============================================================================

TEST_F(NeuromodCognitiveRegressionTest, StatsNoOverflowAfterManyOperations) {
    /* Perform many operations and verify stats don't overflow */
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        neuromod_attention_apply_ne_gain(attention_bridge, random_level(), nullptr);
        neuromod_wm_apply_da_gain(wm_bridge, random_level(), nullptr);
        neuromod_emotion_apply_ne_arousal(emotion_bridge, random_level(), nullptr);
        neuromod_plasticity_apply_da_gating(plasticity_bridge, random_level(), nullptr);
        neuromod_gametheory_apply_ht_cooperation(gametheory_bridge, random_level(), nullptr);
        neuromod_reasoning_apply_da_confidence(reasoning_bridge, random_level(), nullptr);
    }

    /* All stats should be reasonable positive values */
    neuromod_attention_stats_t att_stats;
    neuromod_attention_get_stats(attention_bridge, &att_stats);
    EXPECT_GT(att_stats.gain_modulations, 0u);
    EXPECT_LE(att_stats.gain_modulations, (uint32_t)STRESS_ITERATIONS);

    neuromod_wm_stats_t wm_stats;
    neuromod_wm_get_stats(wm_bridge, &wm_stats);
    EXPECT_GT(wm_stats.gain_modulations, 0u);

    neuromod_emotion_stats_t emo_stats;
    neuromod_emotion_get_stats(emotion_bridge, &emo_stats);
    EXPECT_GT(emo_stats.arousal_modulations, 0u);

    neuromod_plasticity_stats_t plas_stats;
    neuromod_plasticity_get_stats(plasticity_bridge, &plas_stats);
    EXPECT_GT(plas_stats.ltp_gate_openings + plas_stats.ltd_gate_openings, 0u);

    neuromod_gametheory_stats_t gt_stats;
    neuromod_gametheory_get_stats(gametheory_bridge, &gt_stats);
    EXPECT_GT(gt_stats.cooperation_modulations, 0u);

    neuromod_reasoning_stats_t reason_stats;
    neuromod_reasoning_get_stats(reasoning_bridge, &reason_stats);
    EXPECT_GT(reason_stats.confidence_modulations, 0u);
}

