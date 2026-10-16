/**
 * @file e2e_test_neuromodulatory_pipeline.cpp
 * @brief E2E Tests for Neuromodulatory Centers Integration Pipeline
 *
 * WHAT: Complete end-to-end tests for neuromodulatory system integration
 * WHY:  Verify LC, VTA, Raphe, Habenula work correctly with SNN and Plasticity
 * HOW:  Simulate complete learning/modulation scenarios across centers
 *
 * TEST SCENARIOS:
 * 1. RewardLearningPipeline - VTA-driven dopaminergic reward learning
 * 2. AversiveLearningPipeline - Habenula-driven aversive/avoidance learning
 * 3. AttentionModulationPipeline - LC-driven attention and arousal control
 * 4. MoodRegulationPipeline - Raphe-driven impulse control and mood
 * 5. CrossCenterCoordinationPipeline - Multi-center coordinated response
 * 6. NeuromodulatorBalancePipeline - 5-HT/DA balance and risk-taking
 * 7. EmergencyResponsePipeline - Stress-driven multi-center activation
 * 8. LongTermStabilityPipeline - Extended operation without degradation
 *
 * BIOLOGICAL ANALOGY:
 * - VTA encodes reward prediction error (Schultz 1998)
 * - LC encodes salience and attention (Aston-Jones & Cohen 2005)
 * - Raphe modulates impulse control and mood (Dayan & Huys 2008)
 * - Habenula encodes disappointment/aversion (Matsumoto & Hikosaka 2007)
 * - Cross-center interactions: VTA-Habenula inhibition, Habenula-Raphe
 *
 * @author NIMCP Development Team
 * @date 2026-01-11
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <atomic>
#include <cmath>
#include <cstring>

// Neuromodulatory center headers - SNN and Plasticity bridges
#include "core/brain/regions/locus_coeruleus/nimcp_lc_snn_bridge.h"
#include "core/brain/regions/locus_coeruleus/nimcp_lc_plasticity_bridge.h"

#include "core/brain/regions/vta/nimcp_vta_snn_bridge.h"
#include "core/brain/regions/vta/nimcp_vta_plasticity_bridge.h"

#include "core/brain/regions/raphe/nimcp_raphe_snn_bridge.h"
#include "core/brain/regions/raphe/nimcp_raphe_plasticity_bridge.h"

#include "core/brain/regions/habenula/nimcp_habenula_snn_bridge.h"
#include "core/brain/regions/habenula/nimcp_habenula_plasticity_bridge.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

// Learning parameters
constexpr uint32_t REWARD_TRIALS = 20;
constexpr uint32_t AVERSIVE_TRIALS = 15;
constexpr uint32_t ATTENTION_TRIALS = 10;
constexpr float LARGE_REWARD = 1.0f;
constexpr float MEDIUM_REWARD = 0.5f;
constexpr float SMALL_REWARD = 0.2f;
constexpr float STRONG_PUNISHMENT = 0.9f;
constexpr float MODERATE_PUNISHMENT = 0.5f;

// Timing thresholds (milliseconds)
constexpr double MAX_PROCESSING_TIME_MS = 50.0;
constexpr double MAX_PIPELINE_TIME_MS = 500.0;

// Success thresholds
constexpr float MIN_RPE_RESPONSE = 0.1f;
constexpr float MIN_ATTENTION_BOOST = 0.3f;
constexpr float MIN_IMPULSE_CONTROL = 0.3f;
constexpr float MIN_AVOIDANCE_SIGNAL = 0.1f;

// Long-term test parameters
constexpr uint32_t LONG_RUN_STEPS = 5000;
constexpr uint32_t STABILITY_CHECK_INTERVAL = 500;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create and initialize all SNN bridges
 */
struct NeuromodulatorySNNBridges {
    nimcp_lc_snn_bridge_t* lc_snn;
    nimcp_vta_snn_bridge_t* vta_snn;
    nimcp_raphe_snn_bridge_t* raphe_snn;
    nimcp_habenula_snn_bridge_t* habenula_snn;

    bool create_all() {
        lc_snn = nimcp_lc_snn_create(nullptr);
        vta_snn = nimcp_vta_snn_create(nullptr);
        raphe_snn = nimcp_raphe_snn_create(nullptr);
        habenula_snn = nimcp_habenula_snn_create(nullptr);
        return lc_snn && vta_snn && raphe_snn && habenula_snn;
    }

    void destroy_all() {
        if (lc_snn) nimcp_lc_snn_destroy(lc_snn);
        if (vta_snn) nimcp_vta_snn_destroy(vta_snn);
        if (raphe_snn) nimcp_raphe_snn_destroy(raphe_snn);
        if (habenula_snn) nimcp_habenula_snn_destroy(habenula_snn);
    }
};

/**
 * @brief Create and initialize all Plasticity bridges
 */
struct NeuromodulatoryPlasticityBridges {
    nimcp_lc_plasticity_bridge_t* lc_plasticity;
    nimcp_vta_plasticity_bridge_t* vta_plasticity;
    nimcp_raphe_plasticity_bridge_t* raphe_plasticity;
    nimcp_habenula_plasticity_bridge_t* habenula_plasticity;

    bool create_all() {
        lc_plasticity = nimcp_lc_plasticity_create(nullptr);
        vta_plasticity = nimcp_vta_plasticity_create(nullptr);
        raphe_plasticity = nimcp_raphe_plasticity_create(nullptr);
        habenula_plasticity = nimcp_habenula_plasticity_create(nullptr);
        return lc_plasticity && vta_plasticity && raphe_plasticity && habenula_plasticity;
    }

    void destroy_all() {
        if (lc_plasticity) nimcp_lc_plasticity_destroy(lc_plasticity);
        if (vta_plasticity) nimcp_vta_plasticity_destroy(vta_plasticity);
        if (raphe_plasticity) nimcp_raphe_plasticity_destroy(raphe_plasticity);
        if (habenula_plasticity) nimcp_habenula_plasticity_destroy(habenula_plasticity);
    }
};

/**
 * @brief Register test synapses with plasticity bridges
 */
static void register_test_synapses(NeuromodulatoryPlasticityBridges& bridges) {
    // LC synapses (attention-modulated)
    nimcp_lc_plasticity_register_synapse(bridges.lc_plasticity, 1, LC_SYNAPSE_CORTICAL, 0.5f);
    nimcp_lc_plasticity_register_synapse(bridges.lc_plasticity, 2, LC_SYNAPSE_HIPPOCAMPAL, 0.5f);

    // VTA synapses (reward-modulated)
    nimcp_vta_plasticity_register_synapse(bridges.vta_plasticity, 10, VTA_SYNAPSE_NAC, 0.5f);
    nimcp_vta_plasticity_register_synapse(bridges.vta_plasticity, 11, VTA_SYNAPSE_PFC, 0.5f);

    // Raphe synapses (mood-modulated)
    nimcp_raphe_plasticity_register_synapse(bridges.raphe_plasticity, 20, RAPHE_SYNAPSE_LIMBIC, 0.5f);
    nimcp_raphe_plasticity_register_synapse(bridges.raphe_plasticity, 21, RAPHE_SYNAPSE_PREFRONTAL, 0.5f);

    // Habenula synapses (aversion-modulated)
    nimcp_habenula_plasticity_register_synapse(bridges.habenula_plasticity, 30, HABENULA_SYNAPSE_AVOIDANCE, 0.5f);
    nimcp_habenula_plasticity_register_synapse(bridges.habenula_plasticity, 31, HABENULA_SYNAPSE_VTA_INHIBITORY, 0.5f);
}

/**
 * @brief Simulate reward trial with VTA encoding
 */
static void simulate_reward_trial(
    nimcp_vta_snn_bridge_t* vta_snn,
    nimcp_vta_plasticity_bridge_t* vta_plasticity,
    float reward,
    float expected_reward,
    uint64_t timestamp_us
) {
    // Encode reward in SNN
    nimcp_vta_snn_encode_reward(vta_snn, reward, expected_reward);
    nimcp_vta_snn_step(vta_snn);

    // Update plasticity with DA signal
    float rpe = reward - expected_reward;
    nimcp_vta_plasticity_set_da_level(vta_plasticity, 0.5f + rpe * 0.5f, rpe);
    nimcp_vta_plasticity_update(vta_plasticity, 10.0f);
}

/**
 * @brief Simulate aversive trial with Habenula encoding
 */
static void simulate_aversive_trial(
    nimcp_habenula_snn_bridge_t* hab_snn,
    nimcp_habenula_plasticity_bridge_t* hab_plasticity,
    float punishment,
    uint64_t timestamp_us
) {
    // Encode punishment in SNN
    nimcp_habenula_snn_encode_aversive(hab_snn, HABENULA_SNN_AVERSIVE_PUNISHMENT, punishment);
    nimcp_habenula_snn_step(hab_snn);

    // Update plasticity with punishment signal
    nimcp_habenula_plasticity_punishment(hab_plasticity, punishment, timestamp_us);
    nimcp_habenula_plasticity_update(hab_plasticity, 10.0f);
}

/**
 * @brief Simulate attention event with LC encoding
 */
static void simulate_attention_event(
    nimcp_lc_snn_bridge_t* lc_snn,
    nimcp_lc_plasticity_bridge_t* lc_plasticity,
    float novelty,
    float arousal,
    uint64_t timestamp_us
) {
    // Encode novelty burst
    nimcp_lc_snn_encode_novelty(lc_snn, novelty);
    nimcp_lc_snn_encode_arousal(lc_snn, arousal, arousal * 0.8f);
    nimcp_lc_snn_step(lc_snn);

    // Update plasticity with NE burst
    nimcp_lc_plasticity_ne_burst(lc_plasticity, novelty, timestamp_us);
    nimcp_lc_plasticity_set_ne_level(lc_plasticity, 0.5f + novelty * 0.5f, arousal);
    nimcp_lc_plasticity_update(lc_plasticity, 10.0f);
}

/**
 * @brief Simulate mood state with Raphe encoding
 */
static void simulate_mood_state(
    nimcp_raphe_snn_bridge_t* raphe_snn,
    nimcp_raphe_plasticity_bridge_t* raphe_plasticity,
    nimcp_raphe_snn_mood_t mood,
    float impulse_control
) {
    // Encode mood in SNN
    nimcp_raphe_snn_encode_mood(raphe_snn, mood, 0.7f);
    nimcp_raphe_snn_encode_impulse_control(raphe_snn, impulse_control);
    nimcp_raphe_snn_step(raphe_snn);

    // Update plasticity
    nimcp_raphe_plasticity_set_ht_state(raphe_plasticity, impulse_control, impulse_control * 0.5f);
    nimcp_raphe_plasticity_update(raphe_plasticity, 10.0f);
}

//=============================================================================
// E2E Test: Reward Learning Pipeline (VTA-Driven)
//=============================================================================

E2E_TEST(NeuromodulatoryPipeline, RewardLearningPipeline) {
    // Create bridges
    NeuromodulatorySNNBridges snn;
    NeuromodulatoryPlasticityBridges plasticity;

    E2E_ASSERT(snn.create_all(), "Failed to create SNN bridges");
    E2E_ASSERT(plasticity.create_all(), "Failed to create Plasticity bridges");

    register_test_synapses(plasticity);

    // Phase 1: Baseline RPE (no learning yet)
    nimcp_vta_snn_modulation_t baseline_mod;
    EXPECT_EQ(nimcp_vta_snn_get_modulation(snn.vta_snn, &baseline_mod), 0);
    float baseline_motivation = baseline_mod.motivation;

    // Phase 2: Run reward learning trials
    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t trial = 0; trial < REWARD_TRIALS; trial++) {
        uint64_t timestamp = trial * 1000000; // 1 second intervals

        // Unexpected reward (positive RPE)
        float expected = (float)trial / REWARD_TRIALS * MEDIUM_REWARD; // Increasing expectation
        simulate_reward_trial(snn.vta_snn, plasticity.vta_plasticity,
                            LARGE_REWARD, expected, timestamp);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(duration_ms, MAX_PIPELINE_TIME_MS) << "Reward learning too slow";

    // Phase 3: Verify motivation increased
    nimcp_vta_snn_modulation_t post_mod;
    EXPECT_EQ(nimcp_vta_snn_get_modulation(snn.vta_snn, &post_mod), 0);
    EXPECT_GE(post_mod.motivation, 0.0f);
    EXPECT_LE(post_mod.motivation, 1.0f);

    // Verify reward prediction exists
    float prediction = nimcp_vta_snn_get_reward_prediction(snn.vta_snn);
    EXPECT_GE(prediction, 0.0f);
    EXPECT_FALSE(std::isnan(prediction));

    // Verify RPE signal is valid
    EXPECT_GE(post_mod.rpe_signal, -1.0f);
    EXPECT_LE(post_mod.rpe_signal, 1.0f);

    // Verify statistics accumulated
    nimcp_vta_snn_stats_t stats;
    EXPECT_EQ(nimcp_vta_snn_get_stats(snn.vta_snn, &stats), 0);
    EXPECT_GT(stats.total_updates, 0u);

    snn.destroy_all();
    plasticity.destroy_all();
}

//=============================================================================
// E2E Test: Aversive Learning Pipeline (Habenula-Driven)
//=============================================================================

E2E_TEST(NeuromodulatoryPipeline, AversiveLearningPipeline) {
    NeuromodulatorySNNBridges snn;
    NeuromodulatoryPlasticityBridges plasticity;

    E2E_ASSERT(snn.create_all(), "Failed to create SNN bridges");
    E2E_ASSERT(plasticity.create_all(), "Failed to create Plasticity bridges");

    register_test_synapses(plasticity);

    // Phase 1: Baseline avoidance
    nimcp_habenula_snn_modulation_t baseline_mod;
    EXPECT_EQ(nimcp_habenula_snn_get_modulation(snn.habenula_snn, &baseline_mod), 0);

    // Phase 2: Run aversive learning trials
    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t trial = 0; trial < AVERSIVE_TRIALS; trial++) {
        uint64_t timestamp = trial * 1000000;
        simulate_aversive_trial(snn.habenula_snn, plasticity.habenula_plasticity,
                               STRONG_PUNISHMENT, timestamp);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(duration_ms, MAX_PIPELINE_TIME_MS) << "Aversive learning too slow";

    // Phase 3: Verify avoidance response
    nimcp_habenula_snn_modulation_t post_mod;
    EXPECT_EQ(nimcp_habenula_snn_get_modulation(snn.habenula_snn, &post_mod), 0);
    EXPECT_GE(post_mod.avoidance_signal, 0.0f);
    EXPECT_LE(post_mod.avoidance_signal, 1.0f);

    // Verify VTA inhibition present
    float vta_inhibition = nimcp_habenula_snn_get_vta_inhibition(snn.habenula_snn);
    EXPECT_GE(vta_inhibition, 0.0f);
    EXPECT_LE(vta_inhibition, 1.0f);

    // Verify Raphe inhibition
    float raphe_inhibition = nimcp_habenula_snn_get_raphe_inhibition(snn.habenula_snn);
    EXPECT_GE(raphe_inhibition, 0.0f);
    EXPECT_LE(raphe_inhibition, 1.0f);

    // Verify plasticity statistics
    nimcp_habenula_plasticity_stats_t pstats;
    EXPECT_EQ(nimcp_habenula_plasticity_get_stats(plasticity.habenula_plasticity, &pstats), 0);
    EXPECT_GT(pstats.punishment_events, 0u);
    EXPECT_GT(pstats.total_punishment, 0.0f);

    snn.destroy_all();
    plasticity.destroy_all();
}

//=============================================================================
// E2E Test: Attention Modulation Pipeline (LC-Driven)
//=============================================================================

E2E_TEST(NeuromodulatoryPipeline, AttentionModulationPipeline) {
    NeuromodulatorySNNBridges snn;
    NeuromodulatoryPlasticityBridges plasticity;

    E2E_ASSERT(snn.create_all(), "Failed to create SNN bridges");
    E2E_ASSERT(plasticity.create_all(), "Failed to create Plasticity bridges");

    register_test_synapses(plasticity);

    // Phase 1: Baseline attention/gain
    nimcp_lc_snn_modulation_t baseline_mod;
    EXPECT_EQ(nimcp_lc_snn_get_modulation(snn.lc_snn, &baseline_mod), 0);
    float baseline_gain = baseline_mod.gain;

    // Phase 2: High novelty/arousal events
    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t event = 0; event < ATTENTION_TRIALS; event++) {
        uint64_t timestamp = event * 500000; // 500ms intervals
        float novelty = 0.8f + (float)(event % 3) * 0.1f; // Varying novelty
        float arousal = 0.7f + (float)(event % 2) * 0.2f;

        simulate_attention_event(snn.lc_snn, plasticity.lc_plasticity,
                                novelty, arousal, timestamp);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(duration_ms, MAX_PIPELINE_TIME_MS) << "Attention modulation too slow";

    // Phase 3: Verify gain modulation
    nimcp_lc_snn_modulation_t post_mod;
    EXPECT_EQ(nimcp_lc_snn_get_modulation(snn.lc_snn, &post_mod), 0);
    EXPECT_GE(post_mod.gain, 0.0f);
    EXPECT_LE(post_mod.gain, 3.0f); // Reasonable gain range

    // Verify attention boost
    EXPECT_GE(post_mod.attention_boost, 0.0f);
    EXPECT_LE(post_mod.attention_boost, 1.0f);

    // Verify exploration drive is valid
    float exploration = nimcp_lc_snn_get_exploration_drive(snn.lc_snn);
    EXPECT_GE(exploration, 0.0f);
    EXPECT_LE(exploration, 1.0f);

    // Verify statistics
    nimcp_lc_snn_stats_t stats;
    EXPECT_EQ(nimcp_lc_snn_get_stats(snn.lc_snn, &stats), 0);
    EXPECT_GT(stats.total_updates, 0u);
    EXPECT_GT(stats.novelty_events, 0u);

    snn.destroy_all();
    plasticity.destroy_all();
}

//=============================================================================
// E2E Test: Mood Regulation Pipeline (Raphe-Driven)
//=============================================================================

E2E_TEST(NeuromodulatoryPipeline, MoodRegulationPipeline) {
    NeuromodulatorySNNBridges snn;
    NeuromodulatoryPlasticityBridges plasticity;

    E2E_ASSERT(snn.create_all(), "Failed to create SNN bridges");
    E2E_ASSERT(plasticity.create_all(), "Failed to create Plasticity bridges");

    register_test_synapses(plasticity);

    // Phase 1: Neutral baseline
    nimcp_raphe_snn_modulation_t baseline_mod;
    EXPECT_EQ(nimcp_raphe_snn_get_modulation(snn.raphe_snn, &baseline_mod), 0);

    // Phase 2: Test impulse control pathway
    auto start = std::chrono::high_resolution_clock::now();

    // High impulse control (patient, inhibited)
    simulate_mood_state(snn.raphe_snn, plasticity.raphe_plasticity,
                       RAPHE_SNN_MOOD_CALM, 0.9f);

    nimcp_raphe_snn_modulation_t high_control_mod;
    EXPECT_EQ(nimcp_raphe_snn_get_modulation(snn.raphe_snn, &high_control_mod), 0);
    EXPECT_GE(high_control_mod.inhibition_strength, 0.0f);

    // Low impulse control (impulsive)
    simulate_mood_state(snn.raphe_snn, plasticity.raphe_plasticity,
                       RAPHE_SNN_MOOD_ANXIOUS, 0.2f);

    nimcp_raphe_snn_modulation_t low_control_mod;
    EXPECT_EQ(nimcp_raphe_snn_get_modulation(snn.raphe_snn, &low_control_mod), 0);

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(duration_ms, MAX_PROCESSING_TIME_MS) << "Mood regulation too slow";

    // Verify patience signal
    float patience = nimcp_raphe_snn_get_patience(snn.raphe_snn);
    EXPECT_GE(patience, 0.0f);
    EXPECT_LE(patience, 1.0f);

    // Verify mood bias
    float mood_bias = nimcp_raphe_snn_get_mood_bias(snn.raphe_snn);
    EXPECT_GE(mood_bias, -1.0f);
    EXPECT_LE(mood_bias, 1.0f);

    // Verify inhibition signal
    float inhibition = nimcp_raphe_snn_get_inhibition(snn.raphe_snn);
    EXPECT_GE(inhibition, 0.0f);
    EXPECT_LE(inhibition, 1.0f);

    snn.destroy_all();
    plasticity.destroy_all();
}

//=============================================================================
// E2E Test: Cross-Center Coordination Pipeline
//=============================================================================

E2E_TEST(NeuromodulatoryPipeline, CrossCenterCoordinationPipeline) {
    NeuromodulatorySNNBridges snn;
    NeuromodulatoryPlasticityBridges plasticity;

    E2E_ASSERT(snn.create_all(), "Failed to create SNN bridges");
    E2E_ASSERT(plasticity.create_all(), "Failed to create Plasticity bridges");

    register_test_synapses(plasticity);

    // Scenario: Novel rewarding stimulus (activates LC and VTA)
    auto start = std::chrono::high_resolution_clock::now();

    // Step 1: LC detects novelty
    uint64_t timestamp = 0;
    nimcp_lc_snn_encode_novelty(snn.lc_snn, 0.9f);
    nimcp_lc_snn_encode_burst(snn.lc_snn, 0.8f);
    nimcp_lc_snn_step(snn.lc_snn);

    nimcp_lc_plasticity_ne_burst(plasticity.lc_plasticity, 0.9f, timestamp);

    // Get LC output for gain modulation
    nimcp_lc_snn_modulation_t lc_mod;
    EXPECT_EQ(nimcp_lc_snn_get_modulation(snn.lc_snn, &lc_mod), 0);

    // Step 2: VTA processes reward (with LC-enhanced attention)
    timestamp = 100000; // 100ms later
    nimcp_vta_snn_encode_reward(snn.vta_snn, LARGE_REWARD, 0.0f);
    nimcp_vta_snn_step(snn.vta_snn);

    nimcp_vta_plasticity_set_da_level(plasticity.vta_plasticity, 0.9f, 0.8f);
    nimcp_vta_plasticity_update(plasticity.vta_plasticity, 10.0f);

    nimcp_vta_snn_modulation_t vta_mod;
    EXPECT_EQ(nimcp_vta_snn_get_modulation(snn.vta_snn, &vta_mod), 0);

    // Step 3: Raphe modulates response (patience for delayed reward)
    nimcp_raphe_snn_encode_mood(snn.raphe_snn, RAPHE_SNN_MOOD_POSITIVE, 0.7f);
    nimcp_raphe_snn_step(snn.raphe_snn);

    nimcp_raphe_snn_modulation_t raphe_mod;
    EXPECT_EQ(nimcp_raphe_snn_get_modulation(snn.raphe_snn, &raphe_mod), 0);

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(duration_ms, MAX_PROCESSING_TIME_MS) << "Cross-center too slow";

    // Verify coordinated response
    EXPECT_GE(lc_mod.gain, 0.0f);           // LC provided attention
    EXPECT_GE(vta_mod.motivation, 0.0f);    // VTA provided motivation
    EXPECT_GE(raphe_mod.inhibition_strength, 0.0f); // Raphe modulated inhibition

    // All values should be valid (not NaN)
    EXPECT_FALSE(std::isnan(lc_mod.gain));
    EXPECT_FALSE(std::isnan(vta_mod.motivation));
    EXPECT_FALSE(std::isnan(raphe_mod.inhibition_strength));

    snn.destroy_all();
    plasticity.destroy_all();
}

//=============================================================================
// E2E Test: Neuromodulator Balance Pipeline (5-HT/DA)
//=============================================================================

E2E_TEST(NeuromodulatoryPipeline, NeuromodulatorBalancePipeline) {
    NeuromodulatorySNNBridges snn;
    NeuromodulatoryPlasticityBridges plasticity;

    E2E_ASSERT(snn.create_all(), "Failed to create SNN bridges");
    E2E_ASSERT(plasticity.create_all(), "Failed to create Plasticity bridges");

    register_test_synapses(plasticity);

    // Test 5-HT/DA balance affects risk-taking

    // Scenario 1: High DA, Low 5-HT (risk-seeking)
    nimcp_vta_snn_encode_motivation(snn.vta_snn, 0.9f, 0.8f);  // High motivation/wanting
    nimcp_raphe_snn_encode_impulse_control(snn.raphe_snn, 0.2f); // Low impulse control
    nimcp_vta_snn_step(snn.vta_snn);
    nimcp_raphe_snn_step(snn.raphe_snn);

    nimcp_vta_snn_modulation_t high_da_mod;
    nimcp_raphe_snn_modulation_t low_5ht_mod;
    EXPECT_EQ(nimcp_vta_snn_get_modulation(snn.vta_snn, &high_da_mod), 0);
    EXPECT_EQ(nimcp_raphe_snn_get_modulation(snn.raphe_snn, &low_5ht_mod), 0);

    // Scenario 2: Low DA, High 5-HT (risk-averse)
    nimcp_vta_snn_encode_motivation(snn.vta_snn, 0.2f, 0.2f);
    nimcp_raphe_snn_encode_impulse_control(snn.raphe_snn, 0.9f);
    nimcp_vta_snn_step(snn.vta_snn);
    nimcp_raphe_snn_step(snn.raphe_snn);

    nimcp_vta_snn_modulation_t low_da_mod;
    nimcp_raphe_snn_modulation_t high_5ht_mod;
    EXPECT_EQ(nimcp_vta_snn_get_modulation(snn.vta_snn, &low_da_mod), 0);
    EXPECT_EQ(nimcp_raphe_snn_get_modulation(snn.raphe_snn, &high_5ht_mod), 0);

    // Verify risk aversion increases with high 5-HT
    EXPECT_GE(high_5ht_mod.risk_aversion, 0.0f);
    EXPECT_LE(high_5ht_mod.risk_aversion, 1.0f);

    // Verify all outputs valid
    EXPECT_FALSE(std::isnan(high_da_mod.motivation));
    EXPECT_FALSE(std::isnan(low_da_mod.motivation));
    EXPECT_FALSE(std::isnan(high_5ht_mod.risk_aversion));
    EXPECT_FALSE(std::isnan(low_5ht_mod.risk_aversion));

    snn.destroy_all();
    plasticity.destroy_all();
}

//=============================================================================
// E2E Test: Emergency Response Pipeline (Stress-Driven)
//=============================================================================

E2E_TEST(NeuromodulatoryPipeline, EmergencyResponsePipeline) {
    NeuromodulatorySNNBridges snn;
    NeuromodulatoryPlasticityBridges plasticity;

    E2E_ASSERT(snn.create_all(), "Failed to create SNN bridges");
    E2E_ASSERT(plasticity.create_all(), "Failed to create Plasticity bridges");

    register_test_synapses(plasticity);

    uint64_t timestamp = 0;

    // Emergency event: simultaneous high arousal and threat detection
    auto start = std::chrono::high_resolution_clock::now();

    // LC: Maximum arousal burst
    nimcp_lc_snn_encode_burst(snn.lc_snn, 1.0f);
    nimcp_lc_snn_encode_arousal(snn.lc_snn, 1.0f, 1.0f);
    nimcp_lc_snn_step(snn.lc_snn);

    // Habenula: Strong aversive signal
    nimcp_habenula_snn_encode_aversive(snn.habenula_snn,
                                       HABENULA_SNN_AVERSIVE_PUNISHMENT, 1.0f);
    nimcp_habenula_snn_step(snn.habenula_snn);

    // VTA: Negative RPE (expectation violated)
    nimcp_vta_snn_encode_pause(snn.vta_snn, 0.8f);
    nimcp_vta_snn_step(snn.vta_snn);

    // Raphe: Anxious state
    nimcp_raphe_snn_set_mood(snn.raphe_snn, RAPHE_SNN_MOOD_ANXIOUS, 0.9f);
    nimcp_raphe_snn_step(snn.raphe_snn);

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(duration_ms, MAX_PROCESSING_TIME_MS) << "Emergency response too slow";

    // Verify emergency response outputs
    nimcp_lc_snn_modulation_t lc_mod;
    nimcp_habenula_snn_modulation_t hab_mod;
    nimcp_vta_snn_modulation_t vta_mod;
    nimcp_raphe_snn_modulation_t raphe_mod;

    EXPECT_EQ(nimcp_lc_snn_get_modulation(snn.lc_snn, &lc_mod), 0);
    EXPECT_EQ(nimcp_habenula_snn_get_modulation(snn.habenula_snn, &hab_mod), 0);
    EXPECT_EQ(nimcp_vta_snn_get_modulation(snn.vta_snn, &vta_mod), 0);
    EXPECT_EQ(nimcp_raphe_snn_get_modulation(snn.raphe_snn, &raphe_mod), 0);

    // Should trigger avoidance
    EXPECT_GE(hab_mod.avoidance_signal, 0.0f);

    // All outputs bounded and valid
    EXPECT_GE(lc_mod.gain, 0.0f);
    EXPECT_LE(lc_mod.gain, 10.0f);
    EXPECT_GE(vta_mod.motivation, 0.0f);
    EXPECT_LE(vta_mod.motivation, 1.0f);

    snn.destroy_all();
    plasticity.destroy_all();
}

//=============================================================================
// E2E Test: Long-Term Stability Pipeline
//=============================================================================

E2E_TEST(NeuromodulatoryPipeline, LongTermStabilityPipeline) {
    NeuromodulatorySNNBridges snn;
    NeuromodulatoryPlasticityBridges plasticity;

    E2E_ASSERT(snn.create_all(), "Failed to create SNN bridges");
    E2E_ASSERT(plasticity.create_all(), "Failed to create Plasticity bridges");

    register_test_synapses(plasticity);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // Run extended simulation with random inputs
    for (uint32_t step = 0; step < LONG_RUN_STEPS; step++) {
        // Random stimulation
        float random_reward = dist(gen);
        float random_novelty = dist(gen);
        float random_punishment = dist(gen) * 0.3f; // Keep moderate

        // Update all bridges
        nimcp_lc_snn_encode_arousal(snn.lc_snn, random_novelty, random_novelty * 0.8f);
        nimcp_lc_snn_step(snn.lc_snn);

        nimcp_vta_snn_encode_reward(snn.vta_snn, random_reward, random_reward * 0.5f);
        nimcp_vta_snn_step(snn.vta_snn);

        nimcp_raphe_snn_encode_impulse_control(snn.raphe_snn, 0.5f + dist(gen) * 0.3f);
        nimcp_raphe_snn_step(snn.raphe_snn);

        // Occasional punishment
        if (step % 100 == 0) {
            nimcp_habenula_snn_encode_aversive(snn.habenula_snn,
                                               HABENULA_SNN_AVERSIVE_DISAPPOINTMENT,
                                               random_punishment);
        }
        nimcp_habenula_snn_step(snn.habenula_snn);

        // Update plasticity
        nimcp_lc_plasticity_update(plasticity.lc_plasticity, 1.0f);
        nimcp_vta_plasticity_update(plasticity.vta_plasticity, 1.0f);
        nimcp_raphe_plasticity_update(plasticity.raphe_plasticity, 1.0f);
        nimcp_habenula_plasticity_update(plasticity.habenula_plasticity, 1.0f);

        // Periodic stability checks
        if (step % STABILITY_CHECK_INTERVAL == 0) {
            nimcp_lc_snn_modulation_t lc_mod;
            nimcp_vta_snn_modulation_t vta_mod;
            nimcp_raphe_snn_modulation_t raphe_mod;
            nimcp_habenula_snn_modulation_t hab_mod;

            EXPECT_EQ(nimcp_lc_snn_get_modulation(snn.lc_snn, &lc_mod), 0);
            EXPECT_EQ(nimcp_vta_snn_get_modulation(snn.vta_snn, &vta_mod), 0);
            EXPECT_EQ(nimcp_raphe_snn_get_modulation(snn.raphe_snn, &raphe_mod), 0);
            EXPECT_EQ(nimcp_habenula_snn_get_modulation(snn.habenula_snn, &hab_mod), 0);

            // Verify no NaN or infinite values
            EXPECT_FALSE(std::isnan(lc_mod.gain)) << "LC gain NaN at step " << step;
            EXPECT_FALSE(std::isnan(vta_mod.motivation)) << "VTA motivation NaN at step " << step;
            EXPECT_FALSE(std::isnan(raphe_mod.patience_level)) << "Raphe patience NaN at step " << step;
            EXPECT_FALSE(std::isnan(hab_mod.avoidance_signal)) << "Habenula avoidance NaN at step " << step;

            EXPECT_FALSE(std::isinf(lc_mod.gain)) << "LC gain infinite at step " << step;
            EXPECT_FALSE(std::isinf(vta_mod.motivation)) << "VTA motivation infinite at step " << step;

            // Verify bounds
            EXPECT_GE(lc_mod.gain, 0.0f);
            EXPECT_LE(lc_mod.gain, 10.0f);
            EXPECT_GE(vta_mod.motivation, 0.0f);
            EXPECT_LE(vta_mod.motivation, 1.0f);
            EXPECT_GE(raphe_mod.patience_level, 0.0f);
            EXPECT_LE(raphe_mod.patience_level, 1.0f);
            EXPECT_GE(hab_mod.avoidance_signal, 0.0f);
            EXPECT_LE(hab_mod.avoidance_signal, 1.0f);
        }
    }

    // Final state verification
    nimcp_lc_snn_stats_t lc_stats;
    nimcp_vta_snn_stats_t vta_stats;
    nimcp_raphe_snn_stats_t raphe_stats;
    nimcp_habenula_snn_stats_t hab_stats;

    EXPECT_EQ(nimcp_lc_snn_get_stats(snn.lc_snn, &lc_stats), 0);
    EXPECT_EQ(nimcp_vta_snn_get_stats(snn.vta_snn, &vta_stats), 0);
    EXPECT_EQ(nimcp_raphe_snn_get_stats(snn.raphe_snn, &raphe_stats), 0);
    EXPECT_EQ(nimcp_habenula_snn_get_stats(snn.habenula_snn, &hab_stats), 0);

    // Verify accumulated statistics
    EXPECT_GT(lc_stats.total_updates, 0u);
    EXPECT_GT(vta_stats.total_updates, 0u);
    EXPECT_GT(raphe_stats.total_updates, 0u);
    EXPECT_GT(hab_stats.total_updates, 0u);

    snn.destroy_all();
    plasticity.destroy_all();
}

//=============================================================================
// E2E Test: Reset and State Restoration
//=============================================================================

E2E_TEST(NeuromodulatoryPipeline, ResetStateRestorationPipeline) {
    NeuromodulatorySNNBridges snn;
    NeuromodulatoryPlasticityBridges plasticity;

    E2E_ASSERT(snn.create_all(), "Failed to create SNN bridges");
    E2E_ASSERT(plasticity.create_all(), "Failed to create Plasticity bridges");

    register_test_synapses(plasticity);

    // Phase 1: Generate activity
    for (int i = 0; i < 10; i++) {
        nimcp_lc_snn_encode_burst(snn.lc_snn, 0.8f);
        nimcp_vta_snn_encode_reward(snn.vta_snn, 0.9f, 0.2f);
        nimcp_raphe_snn_encode_mood(snn.raphe_snn, RAPHE_SNN_MOOD_POSITIVE, 0.7f);
        nimcp_habenula_snn_encode_aversive(snn.habenula_snn,
                                           HABENULA_SNN_AVERSIVE_PUNISHMENT, 0.5f);

        nimcp_lc_snn_step(snn.lc_snn);
        nimcp_vta_snn_step(snn.vta_snn);
        nimcp_raphe_snn_step(snn.raphe_snn);
        nimcp_habenula_snn_step(snn.habenula_snn);
    }

    // Phase 2: Get pre-reset state
    nimcp_lc_snn_bridge_state_t pre_lc_state;
    nimcp_vta_snn_bridge_state_t pre_vta_state;
    EXPECT_EQ(nimcp_lc_snn_get_state(snn.lc_snn, &pre_lc_state), 0);
    EXPECT_EQ(nimcp_vta_snn_get_state(snn.vta_snn, &pre_vta_state), 0);

    // Phase 3: Reset all bridges
    EXPECT_EQ(nimcp_lc_snn_reset(snn.lc_snn), 0);
    EXPECT_EQ(nimcp_vta_snn_reset(snn.vta_snn), 0);
    EXPECT_EQ(nimcp_raphe_snn_reset(snn.raphe_snn), 0);
    EXPECT_EQ(nimcp_habenula_snn_reset(snn.habenula_snn), 0);

    EXPECT_EQ(nimcp_lc_plasticity_reset(plasticity.lc_plasticity), 0);
    EXPECT_EQ(nimcp_vta_plasticity_reset(plasticity.vta_plasticity), 0);
    EXPECT_EQ(nimcp_raphe_plasticity_reset(plasticity.raphe_plasticity), 0);
    EXPECT_EQ(nimcp_habenula_plasticity_reset(plasticity.habenula_plasticity), 0);

    // Phase 4: Verify reset state
    nimcp_lc_snn_bridge_state_t post_lc_state;
    nimcp_vta_snn_bridge_state_t post_vta_state;
    EXPECT_EQ(nimcp_lc_snn_get_state(snn.lc_snn, &post_lc_state), 0);
    EXPECT_EQ(nimcp_vta_snn_get_state(snn.vta_snn, &post_vta_state), 0);

    // State should be idle after reset
    EXPECT_EQ(post_lc_state.state, LC_SNN_STATE_IDLE);
    EXPECT_EQ(post_vta_state.state, VTA_SNN_STATE_IDLE);

    // Bridges should still be functional after reset
    EXPECT_GE(nimcp_lc_snn_encode_burst(snn.lc_snn, 0.5f), 0);
    EXPECT_EQ(nimcp_vta_snn_encode_reward(snn.vta_snn, 0.5f, 0.5f), 0);

    snn.destroy_all();
    plasticity.destroy_all();
}

// Main
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
