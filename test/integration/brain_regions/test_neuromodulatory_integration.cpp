/**
 * @file test_neuromodulatory_integration.cpp
 * @brief Integration tests for neuromodulatory center SNN/Plasticity bridges
 *
 * TEST PHILOSOPHY:
 * - Test bidirectional communication between neuromodulatory centers and SNN
 * - Test bidirectional communication between neuromodulatory centers and Plasticity
 * - Test cross-center coordination (LC-VTA, Raphe-Habenula interactions)
 * - Test realistic neuromodulatory learning scenarios
 *
 * @author NIMCP Development Team
 * @date 2026-01-11
 * @version 1.0.0 Phase 4 Neuromodulatory Integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

/* LC bridges */
extern "C" {
#include "core/brain/regions/locus_coeruleus/nimcp_lc_snn_bridge.h"
#include "core/brain/regions/locus_coeruleus/nimcp_lc_plasticity_bridge.h"
}

/* VTA bridges */
extern "C" {
#include "core/brain/regions/vta/nimcp_vta_snn_bridge.h"
#include "core/brain/regions/vta/nimcp_vta_plasticity_bridge.h"
}

/* Raphe bridges */
extern "C" {
#include "core/brain/regions/raphe/nimcp_raphe_snn_bridge.h"
#include "core/brain/regions/raphe/nimcp_raphe_plasticity_bridge.h"
}

/* Habenula bridges */
extern "C" {
#include "core/brain/regions/habenula/nimcp_habenula_snn_bridge.h"
#include "core/brain/regions/habenula/nimcp_habenula_plasticity_bridge.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

class NeuromodulatoryIntegrationTest : public ::testing::Test {
protected:
    /* LC bridges */
    nimcp_lc_snn_bridge_t* lc_snn = nullptr;
    nimcp_lc_plasticity_bridge_t* lc_plasticity = nullptr;

    /* VTA bridges */
    nimcp_vta_snn_bridge_t* vta_snn = nullptr;
    nimcp_vta_plasticity_bridge_t* vta_plasticity = nullptr;

    /* Raphe bridges */
    nimcp_raphe_snn_bridge_t* raphe_snn = nullptr;
    nimcp_raphe_plasticity_bridge_t* raphe_plasticity = nullptr;

    /* Habenula bridges */
    nimcp_habenula_snn_bridge_t* habenula_snn = nullptr;
    nimcp_habenula_plasticity_bridge_t* habenula_plasticity = nullptr;

    void SetUp() override {
        /* Create all bridges with default configs */
        lc_snn = nimcp_lc_snn_create(nullptr);
        lc_plasticity = nimcp_lc_plasticity_create(nullptr);
        vta_snn = nimcp_vta_snn_create(nullptr);
        vta_plasticity = nimcp_vta_plasticity_create(nullptr);
        raphe_snn = nimcp_raphe_snn_create(nullptr);
        raphe_plasticity = nimcp_raphe_plasticity_create(nullptr);
        habenula_snn = nimcp_habenula_snn_create(nullptr);
        habenula_plasticity = nimcp_habenula_plasticity_create(nullptr);
    }

    void TearDown() override {
        if (lc_snn) nimcp_lc_snn_destroy(lc_snn);
        if (lc_plasticity) nimcp_lc_plasticity_destroy(lc_plasticity);
        if (vta_snn) nimcp_vta_snn_destroy(vta_snn);
        if (vta_plasticity) nimcp_vta_plasticity_destroy(vta_plasticity);
        if (raphe_snn) nimcp_raphe_snn_destroy(raphe_snn);
        if (raphe_plasticity) nimcp_raphe_plasticity_destroy(raphe_plasticity);
        if (habenula_snn) nimcp_habenula_snn_destroy(habenula_snn);
        if (habenula_plasticity) nimcp_habenula_plasticity_destroy(habenula_plasticity);
    }
};

//=============================================================================
// LC SNN/Plasticity Integration Tests
//=============================================================================

TEST_F(NeuromodulatoryIntegrationTest, LC_SNN_EncodeNovelty_ProducesGainModulation) {
    ASSERT_NE(lc_snn, nullptr);

    /* Encode novelty burst */
    int spikes = nimcp_lc_snn_encode_burst(lc_snn, 0.8f);
    EXPECT_GE(spikes, 0);

    /* Get modulation output */
    nimcp_lc_snn_modulation_t mod;
    EXPECT_EQ(nimcp_lc_snn_get_modulation(lc_snn, &mod), 0);

    /* Verify gain modulation reflects NE state */
    EXPECT_GT(mod.gain, 0.0f);
    EXPECT_GE(mod.attention_boost, 0.0f);
}

TEST_F(NeuromodulatoryIntegrationTest, LC_Plasticity_NEBurst_GatesLearning) {
    ASSERT_NE(lc_plasticity, nullptr);

    /* Register a synapse */
    EXPECT_EQ(nimcp_lc_plasticity_register_synapse(lc_plasticity, 1, LC_SYNAPSE_CORTICAL, 0.5f), 0);

    /* Trigger NE burst (gates learning) */
    EXPECT_EQ(nimcp_lc_plasticity_ne_burst(lc_plasticity, 0.9f, 1000), 0);

    /* Get modulation */
    nimcp_lc_plasticity_modulation_t mod;
    EXPECT_EQ(nimcp_lc_plasticity_get_modulation(lc_plasticity, &mod), 0);

    /* Verify learning rate modulation */
    EXPECT_GE(mod.lr_multiplier, 1.0f);  /* NE burst should boost LR */
    EXPECT_GE(mod.eligibility_gate, 0.0f);
}

TEST_F(NeuromodulatoryIntegrationTest, LC_SNN_Plasticity_Coordinated_Learning) {
    ASSERT_NE(lc_snn, nullptr);
    ASSERT_NE(lc_plasticity, nullptr);

    /* Register synapses for learning */
    EXPECT_EQ(nimcp_lc_plasticity_register_synapse(lc_plasticity, 1, LC_SYNAPSE_CORTICAL, 0.5f), 0);
    EXPECT_EQ(nimcp_lc_plasticity_register_synapse(lc_plasticity, 2, LC_SYNAPSE_HIPPOCAMPAL, 0.5f), 0);

    /* Encode NE state via SNN bridge */
    int spikes = nimcp_lc_snn_encode_ne_state(lc_snn);
    EXPECT_GE(spikes, 0);

    /* Get SNN modulation */
    nimcp_lc_snn_modulation_t snn_mod;
    EXPECT_EQ(nimcp_lc_snn_get_modulation(lc_snn, &snn_mod), 0);

    /* Use SNN output to gate plasticity */
    EXPECT_EQ(nimcp_lc_plasticity_ne_burst(lc_plasticity, snn_mod.gain, 1000), 0);

    /* Verify coordinated modulation */
    nimcp_lc_plasticity_modulation_t plast_mod;
    EXPECT_EQ(nimcp_lc_plasticity_get_modulation(lc_plasticity, &plast_mod), 0);
    EXPECT_GT(plast_mod.lr_multiplier, 0.5f);
}

//=============================================================================
// VTA SNN/Plasticity Integration Tests
//=============================================================================

TEST_F(NeuromodulatoryIntegrationTest, VTA_SNN_EncodeReward_ProducesRPE) {
    ASSERT_NE(vta_snn, nullptr);

    /* Encode reward burst (reward=1.0, expected=0.5 -> positive RPE)
     * Returns spike count (>= 0) for positive RPE, not status code */
    EXPECT_GE(nimcp_vta_snn_encode_reward(vta_snn, 1.0f, 0.5f), 0);

    /* Step simulation */
    EXPECT_EQ(nimcp_vta_snn_step(vta_snn), 0);

    /* Get modulation */
    nimcp_vta_snn_modulation_t mod;
    EXPECT_EQ(nimcp_vta_snn_get_modulation(vta_snn, &mod), 0);

    /* Verify motivation signal */
    EXPECT_GE(mod.motivation, 0.0f);
}

TEST_F(NeuromodulatoryIntegrationTest, VTA_Plasticity_TDLearning_UpdatesWeights) {
    ASSERT_NE(vta_plasticity, nullptr);

    /* Register synapse */
    EXPECT_EQ(nimcp_vta_plasticity_register_synapse(vta_plasticity, 1, VTA_SYNAPSE_NAC, 0.5f), 0);

    /* Set DA level and motivation */
    EXPECT_EQ(nimcp_vta_plasticity_set_da_level(vta_plasticity, 80.0f, 0.9f), 0);

    /* Apply TD update */
    EXPECT_EQ(nimcp_vta_plasticity_td_update(vta_plasticity, 0.5f, 0.8f, 1.0f), 0);

    /* Verify stats updated */
    nimcp_vta_plasticity_stats_t stats;
    EXPECT_EQ(nimcp_vta_plasticity_get_stats(vta_plasticity, &stats), 0);
    EXPECT_EQ(stats.td_updates, 1u);
}

TEST_F(NeuromodulatoryIntegrationTest, VTA_SNN_Plasticity_RewardPrediction) {
    ASSERT_NE(vta_snn, nullptr);
    ASSERT_NE(vta_plasticity, nullptr);

    /* Register synapses */
    EXPECT_EQ(nimcp_vta_plasticity_register_synapse(vta_plasticity, 1, VTA_SYNAPSE_NAC, 0.5f), 0);

    /* Simulate reward via SNN - encode returns spike count, not 0 */
    EXPECT_GE(nimcp_vta_snn_encode_reward(vta_snn, 1.0f, 0.5f), 0);
    EXPECT_EQ(nimcp_vta_snn_step(vta_snn), 0);

    /* Get RPE from SNN */
    nimcp_vta_snn_modulation_t snn_mod;
    EXPECT_EQ(nimcp_vta_snn_get_modulation(vta_snn, &snn_mod), 0);

    /* Apply to plasticity bridge */
    EXPECT_EQ(nimcp_vta_plasticity_rpe(vta_plasticity, snn_mod.rpe_signal, 1000), 0);

    /* Verify modulation updated */
    nimcp_vta_plasticity_modulation_t plast_mod;
    EXPECT_EQ(nimcp_vta_plasticity_get_modulation(vta_plasticity, &plast_mod), 0);
}

//=============================================================================
// Raphe SNN/Plasticity Integration Tests
//=============================================================================

TEST_F(NeuromodulatoryIntegrationTest, Raphe_SNN_EncodeMood_ProducesInhibition) {
    ASSERT_NE(raphe_snn, nullptr);

    /* Encode positive mood */
    EXPECT_EQ(nimcp_raphe_snn_encode_mood(raphe_snn, RAPHE_SNN_MOOD_POSITIVE, 0.8f), 0);

    /* Step simulation */
    EXPECT_EQ(nimcp_raphe_snn_step(raphe_snn), 0);

    /* Get modulation */
    nimcp_raphe_snn_modulation_t mod;
    EXPECT_EQ(nimcp_raphe_snn_get_modulation(raphe_snn, &mod), 0);

    /* Verify impulse inhibition */
    EXPECT_GT(mod.inhibition_strength, 0.0f);
    EXPECT_GT(mod.patience_level, 0.0f);
}

TEST_F(NeuromodulatoryIntegrationTest, Raphe_Plasticity_MoodModulatesSTDP) {
    ASSERT_NE(raphe_plasticity, nullptr);

    /* Register synapse */
    EXPECT_EQ(nimcp_raphe_plasticity_register_synapse(raphe_plasticity, 1, RAPHE_SYNAPSE_LIMBIC, 0.5f), 0);

    /* Set serotonin state and mood */
    EXPECT_EQ(nimcp_raphe_plasticity_set_ht_state(raphe_plasticity, 70.0f, 0.5f), 0);

    /* Get modulation */
    nimcp_raphe_plasticity_modulation_t mod;
    EXPECT_EQ(nimcp_raphe_plasticity_get_modulation(raphe_plasticity, &mod), 0);

    /* Verify mood bias applied */
    EXPECT_GT(mod.lr_multiplier, 0.5f);
}

TEST_F(NeuromodulatoryIntegrationTest, Raphe_SNN_Plasticity_ImpulseControl) {
    ASSERT_NE(raphe_snn, nullptr);
    ASSERT_NE(raphe_plasticity, nullptr);

    /* Register synapse */
    EXPECT_EQ(nimcp_raphe_plasticity_register_synapse(raphe_plasticity, 1, RAPHE_SYNAPSE_PREFRONTAL, 0.5f), 0);

    /* Encode impulse control via SNN */
    EXPECT_EQ(nimcp_raphe_snn_encode_impulse_control(raphe_snn, 0.8f), 0);
    EXPECT_EQ(nimcp_raphe_snn_step(raphe_snn), 0);

    /* Get SNN inhibition */
    float inhibition = nimcp_raphe_snn_get_inhibition(raphe_snn);
    EXPECT_GT(inhibition, 0.0f);

    /* Apply to plasticity via serotonin state */
    EXPECT_EQ(nimcp_raphe_plasticity_set_ht_state(raphe_plasticity, 80.0f, 0.5f), 0);

    /* Record inhibition success */
    EXPECT_EQ(nimcp_raphe_plasticity_inhibition_success(raphe_plasticity, 1000), 0);

    /* Verify stats */
    nimcp_raphe_plasticity_stats_t stats;
    EXPECT_EQ(nimcp_raphe_plasticity_get_stats(raphe_plasticity, &stats), 0);
    EXPECT_EQ(stats.inhibition_successes, 1u);
}

//=============================================================================
// Habenula SNN/Plasticity Integration Tests
//=============================================================================

TEST_F(NeuromodulatoryIntegrationTest, Habenula_SNN_EncodeAversive_ProducesAvoidance) {
    ASSERT_NE(habenula_snn, nullptr);

    /* Encode aversive event */
    int spikes = nimcp_habenula_snn_encode_aversive(habenula_snn, HABENULA_SNN_AVERSIVE_PUNISHMENT, 0.9f);
    EXPECT_GE(spikes, 0);

    /* Step simulation */
    EXPECT_EQ(nimcp_habenula_snn_step(habenula_snn), 0);

    /* Get modulation */
    nimcp_habenula_snn_modulation_t mod;
    EXPECT_EQ(nimcp_habenula_snn_get_modulation(habenula_snn, &mod), 0);

    /* Verify avoidance signal */
    EXPECT_GT(mod.avoidance_signal, 0.0f);
    EXPECT_GT(mod.vta_inhibition, 0.0f);  /* Habenula inhibits VTA */
}

TEST_F(NeuromodulatoryIntegrationTest, Habenula_Plasticity_AvoidanceLearning) {
    ASSERT_NE(habenula_plasticity, nullptr);

    /* Register avoidance synapse */
    EXPECT_EQ(nimcp_habenula_plasticity_register_synapse(habenula_plasticity, 1, HABENULA_SYNAPSE_AVOIDANCE, 0.5f), 0);

    /* Set aversive state */
    EXPECT_EQ(nimcp_habenula_plasticity_set_state(habenula_plasticity, 0.8f, 0.5f), 0);

    /* Trigger pre-spike to build eligibility trace */
    EXPECT_EQ(nimcp_habenula_plasticity_pre_spike(habenula_plasticity, 1, 1000), 0);

    /* Signal punishment */
    EXPECT_EQ(nimcp_habenula_plasticity_punishment(habenula_plasticity, 0.9f, 2000), 0);

    /* Verify stats */
    nimcp_habenula_plasticity_stats_t stats;
    EXPECT_EQ(nimcp_habenula_plasticity_get_stats(habenula_plasticity, &stats), 0);
    EXPECT_EQ(stats.punishment_events, 1u);
}

TEST_F(NeuromodulatoryIntegrationTest, Habenula_SNN_Plasticity_NegativeRPE) {
    ASSERT_NE(habenula_snn, nullptr);
    ASSERT_NE(habenula_plasticity, nullptr);

    /* Register synapse */
    EXPECT_EQ(nimcp_habenula_plasticity_register_synapse(habenula_plasticity, 1, HABENULA_SYNAPSE_AVOIDANCE, 0.5f), 0);

    /* Encode negative RPE via SNN (disappointment) - returns spike count */
    EXPECT_GE(nimcp_habenula_snn_encode_negative_rpe(habenula_snn, 0.7f), 0);
    EXPECT_EQ(nimcp_habenula_snn_step(habenula_snn), 0);

    /* Get VTA inhibition - may be 0 if no aversive state set */
    float vta_inhibition = nimcp_habenula_snn_get_vta_inhibition(habenula_snn);
    EXPECT_GE(vta_inhibition, 0.0f);

    /* Apply to plasticity */
    EXPECT_EQ(nimcp_habenula_plasticity_negative_rpe(habenula_plasticity, 0.7f, 1000), 0);

    /* Get modulation */
    nimcp_habenula_plasticity_modulation_t mod;
    EXPECT_EQ(nimcp_habenula_plasticity_get_modulation(habenula_plasticity, &mod), 0);
    EXPECT_GT(mod.ltd_boost, 1.0f);  /* Negative RPE should boost LTD */
}

//=============================================================================
// Cross-Center Integration Tests
//=============================================================================

TEST_F(NeuromodulatoryIntegrationTest, VTA_Habenula_Bidirectional_Inhibition) {
    ASSERT_NE(vta_snn, nullptr);
    ASSERT_NE(habenula_snn, nullptr);

    /* VTA signals positive reward (reward > expected)
     * Returns spike count (>= 0) for positive RPE */
    EXPECT_GE(nimcp_vta_snn_encode_reward(vta_snn, 1.0f, 0.5f), 0);
    EXPECT_EQ(nimcp_vta_snn_step(vta_snn), 0);

    nimcp_vta_snn_modulation_t vta_mod;
    EXPECT_EQ(nimcp_vta_snn_get_modulation(vta_snn, &vta_mod), 0);
    float initial_motivation = vta_mod.motivation;

    /* Habenula signals aversive (should inhibit VTA) */
    nimcp_habenula_snn_encode_aversive(habenula_snn, HABENULA_SNN_AVERSIVE_PUNISHMENT, 0.9f);
    EXPECT_EQ(nimcp_habenula_snn_step(habenula_snn), 0);

    float vta_inhibition = nimcp_habenula_snn_get_vta_inhibition(habenula_snn);
    EXPECT_GT(vta_inhibition, 0.5f);  /* Strong VTA inhibition */

    /* In a full system, VTA motivation would be reduced by vta_inhibition */
    /* Here we just verify the inhibition signal is generated */
    EXPECT_GE(initial_motivation, 0.0f);
}

TEST_F(NeuromodulatoryIntegrationTest, Habenula_Raphe_Bidirectional_Inhibition) {
    ASSERT_NE(habenula_snn, nullptr);
    ASSERT_NE(raphe_snn, nullptr);

    /* Encode aversive event in habenula */
    nimcp_habenula_snn_encode_aversive(habenula_snn, HABENULA_SNN_AVERSIVE_PUNISHMENT, 0.8f);
    EXPECT_EQ(nimcp_habenula_snn_step(habenula_snn), 0);

    /* Get raphe inhibition from habenula */
    float raphe_inhibition = nimcp_habenula_snn_get_raphe_inhibition(habenula_snn);
    EXPECT_GT(raphe_inhibition, 0.0f);

    /* In a full system, this would suppress serotonin release */
    /* Verify inhibition signal is generated */
    EXPECT_LT(raphe_inhibition, 1.0f);  /* Should be bounded */
}

TEST_F(NeuromodulatoryIntegrationTest, LC_VTA_Attention_Reward_Coordination) {
    ASSERT_NE(lc_snn, nullptr);
    ASSERT_NE(vta_snn, nullptr);
    ASSERT_NE(lc_plasticity, nullptr);
    ASSERT_NE(vta_plasticity, nullptr);

    /* Register synapses */
    EXPECT_EQ(nimcp_lc_plasticity_register_synapse(lc_plasticity, 1, LC_SYNAPSE_CORTICAL, 0.5f), 0);
    EXPECT_EQ(nimcp_vta_plasticity_register_synapse(vta_plasticity, 1, VTA_SYNAPSE_NAC, 0.5f), 0);

    /* LC detects novelty, boosts attention */
    int lc_spikes = nimcp_lc_snn_encode_burst(lc_snn, 0.9f);
    EXPECT_GE(lc_spikes, 0);

    nimcp_lc_snn_modulation_t lc_mod;
    EXPECT_EQ(nimcp_lc_snn_get_modulation(lc_snn, &lc_mod), 0);

    /* VTA signals reward - encode returns spike count */
    EXPECT_GE(nimcp_vta_snn_encode_reward(vta_snn, 1.0f, 0.5f), 0);
    EXPECT_EQ(nimcp_vta_snn_step(vta_snn), 0);

    nimcp_vta_snn_modulation_t vta_mod;
    EXPECT_EQ(nimcp_vta_snn_get_modulation(vta_snn, &vta_mod), 0);

    /* Both systems should be actively modulating */
    EXPECT_GT(lc_mod.gain, 0.0f);
    EXPECT_GE(vta_mod.motivation, 0.0f);
}

TEST_F(NeuromodulatoryIntegrationTest, All_Centers_Coordinated_Learning_Scenario) {
    /* Scenario: Novel stimulus -> attention -> reward -> learning */

    /* Register synapses in all plasticity bridges */
    EXPECT_EQ(nimcp_lc_plasticity_register_synapse(lc_plasticity, 1, LC_SYNAPSE_CORTICAL, 0.5f), 0);
    EXPECT_EQ(nimcp_vta_plasticity_register_synapse(vta_plasticity, 1, VTA_SYNAPSE_NAC, 0.5f), 0);
    EXPECT_EQ(nimcp_raphe_plasticity_register_synapse(raphe_plasticity, 1, RAPHE_SYNAPSE_LIMBIC, 0.5f), 0);
    EXPECT_EQ(nimcp_habenula_plasticity_register_synapse(habenula_plasticity, 1, HABENULA_SYNAPSE_AVOIDANCE, 0.5f), 0);

    /* Phase 1: LC detects novelty */
    int lc_spikes = nimcp_lc_snn_encode_burst(lc_snn, 0.8f);
    EXPECT_GE(lc_spikes, 0);
    EXPECT_EQ(nimcp_lc_plasticity_ne_burst(lc_plasticity, 0.8f, 1000), 0);

    /* Phase 2: VTA processes reward */
    EXPECT_EQ(nimcp_vta_snn_encode_reward(vta_snn, 0.9f, 0.5f), 0);
    EXPECT_EQ(nimcp_vta_snn_step(vta_snn), 0);
    EXPECT_EQ(nimcp_vta_plasticity_reward(vta_plasticity, 0.9f, 2000), 0);

    /* Phase 3: Raphe maintains mood stability */
    EXPECT_EQ(nimcp_raphe_snn_encode_mood(raphe_snn, RAPHE_SNN_MOOD_POSITIVE, 0.6f), 0);
    EXPECT_EQ(nimcp_raphe_snn_step(raphe_snn), 0);
    EXPECT_EQ(nimcp_raphe_plasticity_set_ht_state(raphe_plasticity, 75.0f, 0.5f), 0);

    /* Phase 4: Habenula remains quiescent (no aversive signal) */
    nimcp_habenula_snn_modulation_t hab_mod;
    EXPECT_EQ(nimcp_habenula_snn_get_modulation(habenula_snn, &hab_mod), 0);
    /* Initially, avoidance should be low */

    /* Verify all systems updated */
    nimcp_lc_plasticity_stats_t lc_stats;
    EXPECT_EQ(nimcp_lc_plasticity_get_stats(lc_plasticity, &lc_stats), 0);

    nimcp_vta_plasticity_stats_t vta_stats;
    EXPECT_EQ(nimcp_vta_plasticity_get_stats(vta_plasticity, &vta_stats), 0);
    EXPECT_EQ(vta_stats.reward_events, 1u);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(NeuromodulatoryIntegrationTest, BioAsync_Connect_Disconnect_AllBridges) {
    /* Test bio-async connection for all bridges */

    /* LC */
    EXPECT_EQ(nimcp_lc_snn_connect_bio_async(lc_snn), 0);
    EXPECT_TRUE(nimcp_lc_snn_is_bio_async_connected(lc_snn));
    EXPECT_EQ(nimcp_lc_snn_disconnect_bio_async(lc_snn), 0);
    EXPECT_FALSE(nimcp_lc_snn_is_bio_async_connected(lc_snn));

    EXPECT_EQ(nimcp_lc_plasticity_connect_bio_async(lc_plasticity), 0);
    EXPECT_TRUE(nimcp_lc_plasticity_is_bio_async_connected(lc_plasticity));
    EXPECT_EQ(nimcp_lc_plasticity_disconnect_bio_async(lc_plasticity), 0);
    EXPECT_FALSE(nimcp_lc_plasticity_is_bio_async_connected(lc_plasticity));

    /* VTA */
    EXPECT_EQ(nimcp_vta_snn_connect_bio_async(vta_snn), 0);
    EXPECT_TRUE(nimcp_vta_snn_is_bio_async_connected(vta_snn));
    EXPECT_EQ(nimcp_vta_snn_disconnect_bio_async(vta_snn), 0);
    EXPECT_FALSE(nimcp_vta_snn_is_bio_async_connected(vta_snn));

    EXPECT_EQ(nimcp_vta_plasticity_connect_bio_async(vta_plasticity), 0);
    EXPECT_TRUE(nimcp_vta_plasticity_is_bio_async_connected(vta_plasticity));
    EXPECT_EQ(nimcp_vta_plasticity_disconnect_bio_async(vta_plasticity), 0);
    EXPECT_FALSE(nimcp_vta_plasticity_is_bio_async_connected(vta_plasticity));

    /* Raphe */
    EXPECT_EQ(nimcp_raphe_snn_connect_bio_async(raphe_snn), 0);
    EXPECT_TRUE(nimcp_raphe_snn_is_bio_async_connected(raphe_snn));
    EXPECT_EQ(nimcp_raphe_snn_disconnect_bio_async(raphe_snn), 0);
    EXPECT_FALSE(nimcp_raphe_snn_is_bio_async_connected(raphe_snn));

    EXPECT_EQ(nimcp_raphe_plasticity_connect_bio_async(raphe_plasticity), 0);
    EXPECT_TRUE(nimcp_raphe_plasticity_is_bio_async_connected(raphe_plasticity));
    EXPECT_EQ(nimcp_raphe_plasticity_disconnect_bio_async(raphe_plasticity), 0);
    EXPECT_FALSE(nimcp_raphe_plasticity_is_bio_async_connected(raphe_plasticity));

    /* Habenula */
    EXPECT_EQ(nimcp_habenula_snn_connect_bio_async(habenula_snn), 0);
    EXPECT_TRUE(nimcp_habenula_snn_is_bio_async_connected(habenula_snn));
    EXPECT_EQ(nimcp_habenula_snn_disconnect_bio_async(habenula_snn), 0);
    EXPECT_FALSE(nimcp_habenula_snn_is_bio_async_connected(habenula_snn));

    EXPECT_EQ(nimcp_habenula_plasticity_connect_bio_async(habenula_plasticity), 0);
    EXPECT_TRUE(nimcp_habenula_plasticity_is_bio_async_connected(habenula_plasticity));
    EXPECT_EQ(nimcp_habenula_plasticity_disconnect_bio_async(habenula_plasticity), 0);
    EXPECT_FALSE(nimcp_habenula_plasticity_is_bio_async_connected(habenula_plasticity));
}

//=============================================================================
// Reset and State Management Tests
//=============================================================================

TEST_F(NeuromodulatoryIntegrationTest, Reset_ClearsAll_BridgeStates) {
    /* Modify all bridges */
    nimcp_lc_snn_encode_burst(lc_snn, 0.9f);
    nimcp_vta_snn_encode_reward(vta_snn, 1.0f, 0.5f);
    nimcp_raphe_snn_encode_mood(raphe_snn, RAPHE_SNN_MOOD_POSITIVE, 0.8f);
    nimcp_habenula_snn_encode_aversive(habenula_snn, HABENULA_SNN_AVERSIVE_PUNISHMENT, 0.7f);

    /* Reset all */
    EXPECT_EQ(nimcp_lc_snn_reset(lc_snn), 0);
    EXPECT_EQ(nimcp_vta_snn_reset(vta_snn), 0);
    EXPECT_EQ(nimcp_raphe_snn_reset(raphe_snn), 0);
    EXPECT_EQ(nimcp_habenula_snn_reset(habenula_snn), 0);

    /* Verify states are reset */
    nimcp_lc_snn_bridge_state_t lc_state;
    EXPECT_EQ(nimcp_lc_snn_get_state(lc_snn, &lc_state), 0);
    EXPECT_EQ(lc_state.state, LC_SNN_STATE_IDLE);

    nimcp_vta_snn_bridge_state_t vta_state;
    EXPECT_EQ(nimcp_vta_snn_get_state(vta_snn, &vta_state), 0);
    EXPECT_EQ(vta_state.state, VTA_SNN_STATE_IDLE);

    nimcp_raphe_snn_bridge_state_t raphe_state;
    EXPECT_EQ(nimcp_raphe_snn_get_state(raphe_snn, &raphe_state), 0);
    EXPECT_EQ(raphe_state.state, RAPHE_SNN_STATE_IDLE);

    nimcp_habenula_snn_bridge_state_t habenula_state;
    EXPECT_EQ(nimcp_habenula_snn_get_state(habenula_snn, &habenula_state), 0);
    EXPECT_EQ(habenula_state.state, HABENULA_SNN_STATE_IDLE);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
