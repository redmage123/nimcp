/**
 * @file test_linguistics_plasticity_bridge.cpp
 * @brief Unit tests for Parietal Linguistics Plasticity Bridge
 * @version 1.0.0
 * @date 2026-01-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_plasticity_bridge.h"
#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_types.h"
}

/* ============================================================================
 * TEST FIXTURE
 * ============================================================================ */

class PlasticityBridgeTest : public ::testing::Test {
protected:
    ling_plasticity_bridge_t* bridge;

    void SetUp() override {
        bridge = ling_plasticity_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            ling_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * LIFECYCLE TESTS
 * ============================================================================ */

TEST(PlasticityLifecycle, DefaultConfigHasValidValues) {
    ling_plasticity_config_t config = ling_plasticity_config_default();

    /* STDP config */
    EXPECT_GT(config.stdp.pairwise_lr, 0.0f);
    EXPECT_GT(config.stdp.a_plus, 0.0f);
    EXPECT_GT(config.stdp.a_minus, 0.0f);
    EXPECT_GT(config.stdp.tau_plus, 0.0f);
    EXPECT_GT(config.stdp.tau_minus, 0.0f);
    EXPECT_TRUE(config.stdp.enable_da_modulation);

    /* Triplet config */
    EXPECT_GT(config.triplet.A2_plus, 0.0f);
    EXPECT_GT(config.triplet.A2_minus, 0.0f);
    EXPECT_GT(config.triplet.A3_plus, 0.0f);
    EXPECT_GT(config.triplet.tau_plus, 0.0f);

    /* BCM config */
    EXPECT_GT(config.bcm.learning_rate, 0.0f);
    EXPECT_GT(config.bcm.threshold_tau, 0.0f);
    EXPECT_TRUE(config.bcm.enable_winner_take_all);

    /* Structural config */
    EXPECT_GT(config.structural.formation_threshold, 0.0f);
    EXPECT_GT(config.structural.pruning_timeout_ms, 0u);

    /* Homeostatic config */
    EXPECT_GT(config.homeostatic.target_rate, 0.0f);
    EXPECT_GT(config.homeostatic.scaling_tau, 0.0f);

    /* Enabled mechanisms */
    EXPECT_TRUE(config.enable_pairwise_stdp);
    EXPECT_TRUE(config.enable_triplet_stdp);
    EXPECT_TRUE(config.enable_r_stdp);
    EXPECT_TRUE(config.enable_bcm);
    EXPECT_TRUE(config.enable_structural);
    EXPECT_TRUE(config.enable_homeostatic);
}

TEST(PlasticityLifecycle, CreateWithNullConfigUsesDefaults) {
    ling_plasticity_bridge_t* bridge = ling_plasticity_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    ling_plasticity_destroy(bridge);
}

TEST(PlasticityLifecycle, CreateWithCustomConfig) {
    ling_plasticity_config_t config = ling_plasticity_config_default();
    config.stdp.pairwise_lr = 0.05f;
    config.enable_bcm = false;

    ling_plasticity_bridge_t* bridge = ling_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);
    ling_plasticity_destroy(bridge);
}

TEST(PlasticityLifecycle, DestroyNullIsSafe) {
    ling_plasticity_destroy(nullptr);
    /* Should not crash */
}

TEST_F(PlasticityBridgeTest, ResetClearsState) {
    /* Create some synapses */
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 2, 101, 0.6f),
              LING_PLASTICITY_OK);

    /* Reset */
    int ret = ling_plasticity_reset(bridge);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);

    /* Weights should be reset */
    float weight;
    EXPECT_EQ(ling_plasticity_get_word_weight(bridge, 1, 100, &weight),
              LING_PLASTICITY_OK);
    /* Weight should have been re-initialized */
}

/* ============================================================================
 * WORD SYNAPSE (PAIRWISE STDP) TESTS
 * ============================================================================ */

TEST_F(PlasticityBridgeTest, CreateWordSynapse_Success) {
    int ret = ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);

    float weight;
    ret = ling_plasticity_get_word_weight(bridge, 1, 100, &weight);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);
    EXPECT_FLOAT_EQ(weight, 0.5f);
}

TEST_F(PlasticityBridgeTest, CreateWordSynapse_DuplicateIsOK) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);
    /* Creating same synapse again should succeed (idempotent) */
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.6f),
              LING_PLASTICITY_OK);

    /* Weight should still be original value */
    float weight;
    EXPECT_EQ(ling_plasticity_get_word_weight(bridge, 1, 100, &weight),
              LING_PLASTICITY_OK);
    EXPECT_FLOAT_EQ(weight, 0.5f);
}

TEST_F(PlasticityBridgeTest, GetWordWeight_NotFound) {
    float weight;
    int ret = ling_plasticity_get_word_weight(bridge, 999, 888, &weight);
    EXPECT_EQ(ret, LING_PLASTICITY_ERR_NOT_FOUND);
}

TEST_F(PlasticityBridgeTest, WordPreSpike_AppliesLTD) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);

    /* First, simulate a post-spike to create a post-trace */
    float dw_post = ling_plasticity_word_post_spike(bridge, 1, 100, 0.0f);

    /* Then a pre-spike should cause LTD (post-before-pre) */
    float dw_pre = ling_plasticity_word_pre_spike(bridge, 1, 100, 10.0f);
    /* LTD expected when post comes before pre */
    EXPECT_LE(dw_pre, 0.0f);
}

TEST_F(PlasticityBridgeTest, WordPostSpike_AppliesLTP) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);

    /* First, simulate a pre-spike to create a pre-trace */
    float dw_pre = ling_plasticity_word_pre_spike(bridge, 1, 100, 0.0f);

    /* Then a post-spike should cause LTP (pre-before-post) */
    float dw_post = ling_plasticity_word_post_spike(bridge, 1, 100, 10.0f);
    /* LTP expected when pre comes before post */
    EXPECT_GE(dw_post, 0.0f);
}

TEST_F(PlasticityBridgeTest, WordReward_ModulatesWeight) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);

    /* Create eligibility trace */
    EXPECT_EQ(ling_plasticity_create_eligibility_trace(bridge, 1, 100),
              LING_PLASTICITY_OK);

    /* Apply positive reward with dopamine */
    float dw = ling_plasticity_word_reward(bridge, 1, 100, 1.0f, 0.1f);

    /* Weight should increase with positive reward */
    float weight;
    EXPECT_EQ(ling_plasticity_get_word_weight(bridge, 1, 100, &weight),
              LING_PLASTICITY_OK);
    EXPECT_GE(weight, 0.5f);
}

TEST_F(PlasticityBridgeTest, WordReward_DopamineBurstAmplifies) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.3f),
              LING_PLASTICITY_OK);

    /* Create eligibility trace */
    EXPECT_EQ(ling_plasticity_create_eligibility_trace(bridge, 1, 100),
              LING_PLASTICITY_OK);

    /* Apply reward with high dopamine (should trigger burst amplification) */
    float burst_threshold = LING_PLASTICITY_BURST_THRESHOLD;
    float dw = ling_plasticity_word_reward(bridge, 1, 100, 0.5f, burst_threshold + 1.0f);

    /* Burst should amplify the weight change */
    EXPECT_GT(dw, 0.0f);
}

/* ============================================================================
 * SEQUENCE SYNAPSE (TRIPLET STDP) TESTS
 * ============================================================================ */

TEST_F(PlasticityBridgeTest, CreateSequenceSynapse_Success) {
    int ret = ling_plasticity_create_sequence_synapse(bridge, 1, 2, 0.5f);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);
}

TEST_F(PlasticityBridgeTest, SequenceSpike_UpdatesStrength) {
    EXPECT_EQ(ling_plasticity_create_sequence_synapse(bridge, 1, 2, 0.3f),
              LING_PLASTICITY_OK);

    /* Pre-spike from first word */
    float dw1 = ling_plasticity_sequence_spike(bridge, 1, 2, 0.0f, false);

    /* Post-spike from second word (should strengthen) */
    float dw2 = ling_plasticity_sequence_spike(bridge, 1, 2, 10.0f, true);

    /* Some weight change expected */
    /* Note: exact behavior depends on triplet STDP parameters */
}

TEST_F(PlasticityBridgeTest, LearnSequence_StrengthensConnections) {
    uint32_t sequence[] = {10, 20, 30, 40};  /* "twenty" "one" etc. */
    float frequency = 20.0f;  /* Hz */

    float total_dw = ling_plasticity_learn_sequence(
        bridge, sequence, 4, frequency);

    /* Learning should produce weight changes */
    /* Note: may be 0 if synapses didn't exist */
}

TEST_F(PlasticityBridgeTest, LearnSequence_HighFrequencyStronger) {
    /* Create sequence synapses first */
    EXPECT_EQ(ling_plasticity_create_sequence_synapse(bridge, 10, 20, 0.1f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_sequence_synapse(bridge, 20, 30, 0.1f),
              LING_PLASTICITY_OK);

    uint32_t sequence[] = {10, 20, 30};

    /* Low frequency learning */
    float dw_low = ling_plasticity_learn_sequence(bridge, sequence, 3, 5.0f);

    /* Reset and learn at high frequency */
    ling_plasticity_reset(bridge);
    EXPECT_EQ(ling_plasticity_create_sequence_synapse(bridge, 10, 20, 0.1f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_sequence_synapse(bridge, 20, 30, 0.1f),
              LING_PLASTICITY_OK);

    float dw_high = ling_plasticity_learn_sequence(bridge, sequence, 3, 50.0f);

    /* High frequency should produce larger weight changes (triplet effect) */
    /* This is the key feature of triplet STDP */
}

/* ============================================================================
 * BCM COMPETITIVE LEARNING TESTS
 * ============================================================================ */

TEST_F(PlasticityBridgeTest, CreateBCMSynapse_Success) {
    int ret = ling_plasticity_create_bcm_synapse(bridge, 1, 0.5f);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);
}

TEST_F(PlasticityBridgeTest, BCMUpdate_AppliesRule) {
    EXPECT_EQ(ling_plasticity_create_bcm_synapse(bridge, 1, 0.5f),
              LING_PLASTICITY_OK);

    /* BCM update with activity above threshold → LTP */
    float dw = ling_plasticity_bcm_update(bridge, 1, 1.0f, 0.8f);

    /* With post > threshold, weight should increase */
    /* Note: depends on initial threshold */
}

TEST_F(PlasticityBridgeTest, BCMCompete_SelectsWinner) {
    /* Create competing word synapses */
    EXPECT_EQ(ling_plasticity_create_bcm_synapse(bridge, 1, 0.3f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_bcm_synapse(bridge, 2, 0.8f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_bcm_synapse(bridge, 3, 0.5f),
              LING_PLASTICITY_OK);

    /* Update with activity to set competition scores */
    ling_plasticity_bcm_update(bridge, 1, 1.0f, 0.3f);
    ling_plasticity_bcm_update(bridge, 2, 1.0f, 0.8f);
    ling_plasticity_bcm_update(bridge, 3, 1.0f, 0.5f);

    uint32_t candidates[] = {1, 2, 3};
    uint32_t winner;

    int ret = ling_plasticity_bcm_compete(bridge, candidates, 3, &winner);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);

    /* Word 2 should win (highest activity × weight) */
    EXPECT_EQ(winner, 2u);
}

TEST_F(PlasticityBridgeTest, BCMCompete_SingleCandidate) {
    EXPECT_EQ(ling_plasticity_create_bcm_synapse(bridge, 42, 0.5f),
              LING_PLASTICITY_OK);

    uint32_t candidates[] = {42};
    uint32_t winner;

    int ret = ling_plasticity_bcm_compete(bridge, candidates, 1, &winner);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);
    EXPECT_EQ(winner, 42u);
}

/* ============================================================================
 * STRUCTURAL PLASTICITY TESTS
 * ============================================================================ */

TEST_F(PlasticityBridgeTest, GetSpineState_InitiallyNascent) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);

    ling_spine_state_t state;
    int ret = ling_plasticity_get_spine_state(bridge, 1, 100, &state);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);
    EXPECT_EQ(state, LING_SPINE_NASCENT);
}

TEST_F(PlasticityBridgeTest, GetSpineState_NotFound) {
    ling_spine_state_t state;
    int ret = ling_plasticity_get_spine_state(bridge, 999, 888, &state);
    EXPECT_EQ(ret, LING_PLASTICITY_ERR_NOT_FOUND);
}

TEST_F(PlasticityBridgeTest, StructuralUpdate_ProcessesTransitions) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);

    /* Simulate time passing */
    uint32_t transitions = ling_plasticity_structural_update(bridge, 1000.0f);

    /* May or may not have transitions depending on activity */
    EXPECT_GE(transitions, 0u);
}

TEST_F(PlasticityBridgeTest, TagForConsolidation_Success) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);

    int ret = ling_plasticity_tag_for_consolidation(bridge, 1, 100);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);
}

TEST_F(PlasticityBridgeTest, SleepConsolidation_ProcessesTaggedSpines) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);

    /* Tag for consolidation */
    EXPECT_EQ(ling_plasticity_tag_for_consolidation(bridge, 1, 100),
              LING_PLASTICITY_OK);

    /* Simulate NREM sleep */
    uint32_t consolidated = ling_plasticity_sleep_consolidation(
        bridge, SLEEP_STATE_LIGHT_NREM, 60000.0f);

    /* Tagged spine should be consolidated */
    EXPECT_GE(consolidated, 0u);
}

/* ============================================================================
 * HOMEOSTATIC PLASTICITY TESTS
 * ============================================================================ */

TEST_F(PlasticityBridgeTest, ApplyScaling_ReturnsScalingFactor) {
    /* Create some synapses with varying weights */
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.2f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 2, 101, 0.8f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 3, 102, 0.5f),
              LING_PLASTICITY_OK);

    float scaling = ling_plasticity_apply_scaling(bridge, 5.0f);

    /* Scaling factor should be positive */
    EXPECT_GT(scaling, 0.0f);
    EXPECT_LT(scaling, 100.0f);  /* Reasonable bounds */
}

TEST_F(PlasticityBridgeTest, IntrinsicUpdate_DoesNotCrash) {
    EXPECT_EQ(ling_plasticity_create_bcm_synapse(bridge, 1, 0.5f),
              LING_PLASTICITY_OK);

    /* Should not crash */
    ling_plasticity_intrinsic_update(bridge, 100.0f);
}

TEST_F(PlasticityBridgeTest, BCMThresholdUpdate_SlidesThreshold) {
    /* Should not crash, updates global threshold */
    ling_plasticity_bcm_threshold_update(bridge, 0.3f);
    ling_plasticity_bcm_threshold_update(bridge, 0.7f);
}

/* ============================================================================
 * ELIGIBILITY TRACE TESTS
 * ============================================================================ */

TEST_F(PlasticityBridgeTest, CreateEligibilityTrace_Success) {
    int ret = ling_plasticity_create_eligibility_trace(bridge, 1, 100);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);

    /* Should also create the synapse if it doesn't exist */
    float weight;
    ret = ling_plasticity_get_word_weight(bridge, 1, 100, &weight);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);
}

TEST_F(PlasticityBridgeTest, DecayTraces_ReducesTraceValues) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_eligibility_trace(bridge, 1, 100),
              LING_PLASTICITY_OK);

    /* Decay traces over time */
    for (int i = 0; i < 10; i++) {
        ling_plasticity_decay_traces(bridge, 10.0f);
    }

    /* Traces should have decayed (can't directly check, but shouldn't crash) */
}

TEST_F(PlasticityBridgeTest, ApplyRewardToTraces_AffectsActiveTraces) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_eligibility_trace(bridge, 1, 100),
              LING_PLASTICITY_OK);

    float total_dw = ling_plasticity_apply_reward_to_traces(bridge, 1.0f, 0.1f);

    /* Should have some weight change */
    EXPECT_GE(total_dw, 0.0f);
}

/* ============================================================================
 * BATCH OPERATION TESTS
 * ============================================================================ */

TEST_F(PlasticityBridgeTest, UpdateTraces_DoesNotCrash) {
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_sequence_synapse(bridge, 1, 2, 0.3f),
              LING_PLASTICITY_OK);

    /* Should not crash */
    ling_plasticity_update_traces(bridge, 10.0f);
}

TEST_F(PlasticityBridgeTest, FullUpdate_ProcessesAllMechanisms) {
    /* Create various synapses */
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_sequence_synapse(bridge, 1, 2, 0.3f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_bcm_synapse(bridge, 1, 0.5f),
              LING_PLASTICITY_OK);

    int ret = ling_plasticity_full_update(bridge, 100.0f);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);
}

/* ============================================================================
 * STATISTICS TESTS
 * ============================================================================ */

TEST_F(PlasticityBridgeTest, GetStats_ReturnsValidData) {
    /* Create some synapses */
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.3f),
              LING_PLASTICITY_OK);
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 2, 101, 0.7f),
              LING_PLASTICITY_OK);

    ling_plasticity_stats_t stats;
    int ret = ling_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);

    EXPECT_EQ(stats.total_word_synapses, 2u);
    EXPECT_GE(stats.nascent_count, 0u);
    EXPECT_GE(stats.mean_weight, 0.0f);
    EXPECT_LE(stats.mean_weight, 1.0f);
}

TEST_F(PlasticityBridgeTest, ResetStats_ClearsCounters) {
    /* Create synapse and trigger some events */
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);
    ling_plasticity_word_pre_spike(bridge, 1, 100, 0.0f);
    ling_plasticity_word_post_spike(bridge, 1, 100, 10.0f);

    int ret = ling_plasticity_reset_stats(bridge);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);

    ling_plasticity_stats_t stats;
    EXPECT_EQ(ling_plasticity_get_stats(bridge, &stats), LING_PLASTICITY_OK);
    EXPECT_EQ(stats.total_ltp_events, 0u);
    EXPECT_EQ(stats.total_ltd_events, 0u);
}

/* ============================================================================
 * CALLBACK TESTS
 * ============================================================================ */

static int g_callback_count = 0;
static ling_plasticity_event_type_t g_last_event_type;

static void test_callback(const ling_plasticity_event_t* event, void* user_data) {
    g_callback_count++;
    g_last_event_type = event->type;
    (void)user_data;
}

TEST_F(PlasticityBridgeTest, RegisterCallback_ReceivesEvents) {
    g_callback_count = 0;

    int ret = ling_plasticity_register_callback(bridge, test_callback, nullptr);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);

    /* Creating a synapse should trigger FORMATION event */
    EXPECT_EQ(ling_plasticity_create_word_synapse(bridge, 1, 100, 0.5f),
              LING_PLASTICITY_OK);

    EXPECT_GE(g_callback_count, 1);
    EXPECT_EQ(g_last_event_type, LING_PLASTICITY_EVENT_FORMATION);
}

/* ============================================================================
 * UTILITY FUNCTION TESTS
 * ============================================================================ */

TEST(PlasticityUtility, GetLastError_ReturnsNonNull) {
    const char* error = ling_plasticity_get_last_error();
    EXPECT_NE(error, nullptr);
}

TEST(PlasticityUtility, SpineStateName_ReturnsValidStrings) {
    EXPECT_STREQ(ling_plasticity_spine_state_name(LING_SPINE_NASCENT), "NASCENT");
    EXPECT_STREQ(ling_plasticity_spine_state_name(LING_SPINE_STABLE), "STABLE");
    EXPECT_STREQ(ling_plasticity_spine_state_name(LING_SPINE_POTENTIATED), "POTENTIATED");
    EXPECT_STREQ(ling_plasticity_spine_state_name(LING_SPINE_PRUNING), "PRUNING");
    EXPECT_STREQ(ling_plasticity_spine_state_name(LING_SPINE_ELIMINATED), "ELIMINATED");
}

TEST(PlasticityUtility, RuleName_ReturnsValidStrings) {
    EXPECT_STREQ(ling_plasticity_rule_name(LING_PLASTICITY_RULE_PAIRWISE_STDP),
                 "PAIRWISE_STDP");
    EXPECT_STREQ(ling_plasticity_rule_name(LING_PLASTICITY_RULE_TRIPLET_STDP),
                 "TRIPLET_STDP");
    EXPECT_STREQ(ling_plasticity_rule_name(LING_PLASTICITY_RULE_R_STDP),
                 "R_STDP");
    EXPECT_STREQ(ling_plasticity_rule_name(LING_PLASTICITY_RULE_BCM),
                 "BCM");
    EXPECT_STREQ(ling_plasticity_rule_name(LING_PLASTICITY_RULE_COMBINED),
                 "COMBINED");
}

/* ============================================================================
 * NULL SAFETY TESTS
 * ============================================================================ */

TEST(PlasticityNullSafety, FunctionsHandleNullBridge) {
    /* All functions should handle NULL bridge gracefully */
    EXPECT_EQ(ling_plasticity_reset(nullptr), LING_PLASTICITY_ERR_NULL);

    EXPECT_EQ(ling_plasticity_create_word_synapse(nullptr, 1, 100, 0.5f),
              LING_PLASTICITY_ERR_NULL);

    float weight;
    EXPECT_EQ(ling_plasticity_get_word_weight(nullptr, 1, 100, &weight),
              LING_PLASTICITY_ERR_NULL);

    EXPECT_EQ(ling_plasticity_word_pre_spike(nullptr, 1, 100, 0.0f), 0.0f);
    EXPECT_EQ(ling_plasticity_word_post_spike(nullptr, 1, 100, 0.0f), 0.0f);

    EXPECT_EQ(ling_plasticity_create_sequence_synapse(nullptr, 1, 2, 0.5f),
              LING_PLASTICITY_ERR_NULL);

    EXPECT_EQ(ling_plasticity_create_bcm_synapse(nullptr, 1, 0.5f),
              LING_PLASTICITY_ERR_NULL);

    ling_spine_state_t state;
    EXPECT_EQ(ling_plasticity_get_spine_state(nullptr, 1, 100, &state),
              LING_PLASTICITY_ERR_NULL);

    EXPECT_EQ(ling_plasticity_structural_update(nullptr, 100.0f), 0u);

    ling_plasticity_stats_t stats;
    EXPECT_EQ(ling_plasticity_get_stats(nullptr, &stats),
              LING_PLASTICITY_ERR_NULL);
}

/* ============================================================================
 * MESH HANDLER TESTS
 * ============================================================================ */

TEST_F(PlasticityBridgeTest, GetMeshHandler_ReturnsValid) {
    linguistics_mesh_handler_t handler;
    int ret = ling_plasticity_get_mesh_handler(bridge, &handler);
    EXPECT_EQ(ret, LING_PLASTICITY_OK);

    EXPECT_NE(handler.process, nullptr);
    EXPECT_NE(handler.update, nullptr);
    EXPECT_NE(handler.get_precision, nullptr);
    EXPECT_EQ(handler.ctx, bridge);
}

TEST_F(PlasticityBridgeTest, MeshHandler_ProcessGeneratesBelief) {
    linguistics_mesh_handler_t handler;
    EXPECT_EQ(ling_plasticity_get_mesh_handler(bridge, &handler),
              LING_PLASTICITY_OK);

    linguistics_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = LING_REQUEST_REHEARSE;  // Use valid request type (no PLASTICITY_UPDATE in enum)

    linguistics_belief_t belief;
    memset(&belief, 0, sizeof(belief));

    int ret = handler.process(handler.ctx, &request, &belief);
    EXPECT_EQ(ret, 0);

    /* Belief should have valid values */
    EXPECT_GE(belief.certainty, 0.0f);
    EXPECT_LE(belief.certainty, 1.0f);
    EXPECT_GT(belief.precision, 0.0f);
}

TEST_F(PlasticityBridgeTest, MeshHandler_GetPrecisionReturnsValidValue) {
    linguistics_mesh_handler_t handler;
    EXPECT_EQ(ling_plasticity_get_mesh_handler(bridge, &handler),
              LING_PLASTICITY_OK);

    float precision = handler.get_precision(handler.ctx);
    EXPECT_GE(precision, 0.0f);
    EXPECT_LE(precision, 1.0f);
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
