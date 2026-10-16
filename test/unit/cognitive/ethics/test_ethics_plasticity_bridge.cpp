/**
 * @file test_ethics_plasticity_bridge.cpp
 * @brief Unit tests for Ethics-Plasticity Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/ethics/nimcp_ethics_plasticity_bridge.h"

class EthicsPlasticityBridgeTest : public ::testing::Test {
protected:
    ethics_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = ethics_plasticity_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            ethics_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }

    void register_test_synapses() {
        ethics_plasticity_register_synapse(bridge, 0, ETHICS_SYNAPSE_HARM_DETECTION, 0.5f);
        ethics_plasticity_register_synapse(bridge, 1, ETHICS_SYNAPSE_FAIRNESS, 0.5f);
        ethics_plasticity_register_synapse(bridge, 2, ETHICS_SYNAPSE_GOLDEN_RULE, 0.5f);
        ethics_plasticity_register_synapse(bridge, 3, ETHICS_SYNAPSE_FIRST_LAW, 0.5f);
        ethics_plasticity_register_synapse(bridge, 4, ETHICS_SYNAPSE_EMPATHY, 0.5f);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EthicsPlasticityBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(EthicsPlasticityBridgeTest, CreateWithConfig) {
    ethics_plasticity_config_t config = ethics_plasticity_config_default();
    config.base_learning_rate = 0.05f;
    config.max_synapses = 128;

    ethics_plasticity_bridge_t* custom = ethics_plasticity_create(&config);
    ASSERT_NE(custom, nullptr);
    ethics_plasticity_destroy(custom);
}

TEST_F(EthicsPlasticityBridgeTest, DefaultConfigValues) {
    ethics_plasticity_config_t config = ethics_plasticity_config_default();

    EXPECT_GT(config.base_learning_rate, 0.0f);
    EXPECT_GT(config.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_minus_ms, 0.0f);
    EXPECT_EQ(config.protect_first_law, true);
    EXPECT_EQ(config.protect_golden_rule, true);
}

TEST_F(EthicsPlasticityBridgeTest, Reset) {
    register_test_synapses();
    ethics_plasticity_learn(bridge, ETHICS_LEARN_POSITIVE_OUTCOME, 0.8f, 0.5f, 0);

    int ret = ethics_plasticity_reset(bridge);
    EXPECT_EQ(ret, 0);

    ethics_plasticity_bridge_state_t state;
    ethics_plasticity_get_state(bridge, &state);
    EXPECT_EQ(state.state, ETHICS_PLASTICITY_STATE_IDLE);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(EthicsPlasticityBridgeTest, RegisterSynapse) {
    int ret = ethics_plasticity_register_synapse(bridge, 100, ETHICS_SYNAPSE_HARM_DETECTION, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsPlasticityBridgeTest, RegisterDuplicateSynapse) {
    ethics_plasticity_register_synapse(bridge, 100, ETHICS_SYNAPSE_HARM_DETECTION, 0.5f);
    int ret = ethics_plasticity_register_synapse(bridge, 100, ETHICS_SYNAPSE_HARM_DETECTION, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(EthicsPlasticityBridgeTest, UnregisterSynapse) {
    ethics_plasticity_register_synapse(bridge, 100, ETHICS_SYNAPSE_HARM_DETECTION, 0.5f);
    int ret = ethics_plasticity_unregister_synapse(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsPlasticityBridgeTest, GetSynapse) {
    ethics_plasticity_register_synapse(bridge, 100, ETHICS_SYNAPSE_FAIRNESS, 0.6f);

    ethics_plasticity_synapse_t synapse;
    int ret = ethics_plasticity_get_synapse(bridge, 100, &synapse);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(synapse.synapse_id, 100u);
    EXPECT_EQ(synapse.type, ETHICS_SYNAPSE_FAIRNESS);
    EXPECT_NEAR(synapse.weight, 0.6f, 0.01f);
}

TEST_F(EthicsPlasticityBridgeTest, GetNonexistentSynapse) {
    ethics_plasticity_synapse_t synapse;
    int ret = ethics_plasticity_get_synapse(bridge, 9999, &synapse);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(EthicsPlasticityBridgeTest, LearnPositiveOutcome) {
    register_test_synapses();

    int ret = ethics_plasticity_learn(bridge, ETHICS_LEARN_POSITIVE_OUTCOME, 0.8f, 0.5f, 0);
    EXPECT_EQ(ret, 0);

    ethics_plasticity_stats_t stats;
    ethics_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.positive_outcomes, 1u);
}

TEST_F(EthicsPlasticityBridgeTest, LearnNegativeOutcome) {
    register_test_synapses();

    int ret = ethics_plasticity_learn(bridge, ETHICS_LEARN_NEGATIVE_OUTCOME, 0.8f, -0.5f, 0);
    EXPECT_EQ(ret, 0);

    ethics_plasticity_stats_t stats;
    ethics_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.negative_outcomes, 1u);
}

TEST_F(EthicsPlasticityBridgeTest, LearnHarmAvoided) {
    register_test_synapses();

    int ret = ethics_plasticity_learn(bridge, ETHICS_LEARN_HARM_AVOIDED, 0.9f, 0.8f, 0);
    EXPECT_EQ(ret, 0);

    ethics_plasticity_stats_t stats;
    ethics_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.harm_avoidance_events, 0u);
}

TEST_F(EthicsPlasticityBridgeTest, LearnFirstLawActivation) {
    register_test_synapses();

    int ret = ethics_plasticity_learn(bridge, ETHICS_LEARN_FIRST_LAW_ACTIVATED, 1.0f, 1.0f, 0);
    EXPECT_EQ(ret, 0);

    ethics_plasticity_stats_t stats;
    ethics_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.first_law_activations, 0u);
}

TEST_F(EthicsPlasticityBridgeTest, ApplySTDP) {
    ethics_plasticity_register_synapse(bridge, 100, ETHICS_SYNAPSE_EMPATHY, 0.5f);

    uint64_t pre_time = 1000000;
    uint64_t post_time = 1010000; // 10ms later

    float delta = ethics_plasticity_apply_stdp(bridge, 100, pre_time, post_time);
    EXPECT_GT(delta, 0.0f); // Post after pre = potentiation
}

TEST_F(EthicsPlasticityBridgeTest, ApplySTDPDepression) {
    ethics_plasticity_register_synapse(bridge, 100, ETHICS_SYNAPSE_EMPATHY, 0.5f);

    uint64_t pre_time = 1010000;
    uint64_t post_time = 1000000; // 10ms earlier

    float delta = ethics_plasticity_apply_stdp(bridge, 100, pre_time, post_time);
    EXPECT_LT(delta, 0.0f); // Pre after post = depression
}

TEST_F(EthicsPlasticityBridgeTest, ApplyReward) {
    register_test_synapses();

    // Set up eligibility traces first
    ethics_plasticity_learn(bridge, ETHICS_LEARN_POSITIVE_OUTCOME, 0.8f, 0.1f, 0);

    int ret = ethics_plasticity_apply_reward(bridge, 0.9f);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsPlasticityBridgeTest, UpdateTraces) {
    register_test_synapses();
    ethics_plasticity_learn(bridge, ETHICS_LEARN_POSITIVE_OUTCOME, 0.8f, 0.5f, 0);

    int ret = ethics_plasticity_update_traces(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsPlasticityBridgeTest, UpdateBCM) {
    register_test_synapses();

    int ret = ethics_plasticity_update_bcm(bridge, 100.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsPlasticityBridgeTest, HomeostaticUpdate) {
    register_test_synapses();

    int ret = ethics_plasticity_homeostatic_update(bridge, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsPlasticityBridgeTest, Consolidate) {
    register_test_synapses();
    ethics_plasticity_learn(bridge, ETHICS_LEARN_POSITIVE_OUTCOME, 0.8f, 0.5f, 0);

    int ret = ethics_plasticity_consolidate(bridge);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Protection Tests
//=============================================================================

TEST_F(EthicsPlasticityBridgeTest, ProtectSynapse) {
    ethics_plasticity_register_synapse(bridge, 100, ETHICS_SYNAPSE_EMPATHY, 0.5f);

    int ret = ethics_plasticity_protect_synapse(bridge, 100);
    EXPECT_EQ(ret, 0);

    ethics_plasticity_synapse_t synapse;
    ethics_plasticity_get_synapse(bridge, 100, &synapse);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(EthicsPlasticityBridgeTest, UnprotectSynapse) {
    ethics_plasticity_register_synapse(bridge, 100, ETHICS_SYNAPSE_EMPATHY, 0.5f);
    ethics_plasticity_protect_synapse(bridge, 100);

    int ret = ethics_plasticity_unprotect_synapse(bridge, 100);
    EXPECT_EQ(ret, 0);

    ethics_plasticity_synapse_t synapse;
    ethics_plasticity_get_synapse(bridge, 100, &synapse);
    EXPECT_FALSE(synapse.is_protected);
}

TEST_F(EthicsPlasticityBridgeTest, ProtectFirstLaw) {
    register_test_synapses();

    int count = ethics_plasticity_protect_first_law(bridge);
    EXPECT_GE(count, 1);
}

TEST_F(EthicsPlasticityBridgeTest, ProtectGoldenRule) {
    register_test_synapses();

    int count = ethics_plasticity_protect_golden_rule(bridge);
    EXPECT_GE(count, 1);
}

TEST_F(EthicsPlasticityBridgeTest, AutoProtectFirstLawSynapse) {
    // First Law synapses should be auto-protected
    ethics_plasticity_register_synapse(bridge, 100, ETHICS_SYNAPSE_FIRST_LAW, 0.5f);

    ethics_plasticity_synapse_t synapse;
    ethics_plasticity_get_synapse(bridge, 100, &synapse);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(EthicsPlasticityBridgeTest, ProtectedSynapseBlocksSTDP) {
    ethics_plasticity_register_synapse(bridge, 100, ETHICS_SYNAPSE_FIRST_LAW, 0.5f);

    float delta = ethics_plasticity_apply_stdp(bridge, 100, 1000000, 1010000);
    EXPECT_NEAR(delta, 0.0f, 0.001f); // Protected, no change
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(EthicsPlasticityBridgeTest, GetPrincipleState) {
    ethics_principle_state_t state;
    int ret = ethics_plasticity_get_principle_state(bridge, &state);
    EXPECT_EQ(ret, 0);

    EXPECT_GE(state.harm_sensitivity, 0.0f);
    EXPECT_GE(state.first_law_strength, 0.0f);
}

TEST_F(EthicsPlasticityBridgeTest, GetBridgeState) {
    ethics_plasticity_bridge_state_t state;
    int ret = ethics_plasticity_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.state, ETHICS_PLASTICITY_STATE_IDLE);
}

TEST_F(EthicsPlasticityBridgeTest, GetStats) {
    ethics_plasticity_stats_t stats;
    int ret = ethics_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsPlasticityBridgeTest, ResetStats) {
    register_test_synapses();
    ethics_plasticity_learn(bridge, ETHICS_LEARN_POSITIVE_OUTCOME, 0.8f, 0.5f, 0);

    int ret = ethics_plasticity_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    ethics_plasticity_stats_t stats;
    ethics_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int weight_callback_count = 0;
static void weight_callback(uint32_t, ethics_synapse_type_t, float, float,
                           ethics_learn_event_t, void*) {
    weight_callback_count++;
}

TEST_F(EthicsPlasticityBridgeTest, SetWeightCallback) {
    int ret = ethics_plasticity_set_weight_callback(bridge, weight_callback, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsPlasticityBridgeTest, WeightCallbackFires) {
    weight_callback_count = 0;
    ethics_plasticity_set_weight_callback(bridge, weight_callback, nullptr);
    register_test_synapses();

    ethics_plasticity_learn(bridge, ETHICS_LEARN_POSITIVE_OUTCOME, 0.8f, 0.5f, 0);
    EXPECT_GT(weight_callback_count, 0);
}

TEST_F(EthicsPlasticityBridgeTest, SetPrincipleCallback) {
    int ret = ethics_plasticity_set_principle_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(EthicsPlasticityBridgeTest, BioAsyncNotConnectedInitially) {
    bool connected = ethics_plasticity_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(EthicsPlasticityBridgeTest, BioAsyncDisconnect) {
    int ret = ethics_plasticity_bio_async_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Null Pointer Tests
//=============================================================================

TEST_F(EthicsPlasticityBridgeTest, NullBridgeHandling) {
    EXPECT_EQ(ethics_plasticity_reset(nullptr), -1);
    EXPECT_EQ(ethics_plasticity_register_synapse(nullptr, 0, ETHICS_SYNAPSE_HARM_DETECTION, 0.5f), -1);
    EXPECT_EQ(ethics_plasticity_learn(nullptr, ETHICS_LEARN_POSITIVE_OUTCOME, 0.5f, 0.5f, 0), -1);
    EXPECT_FALSE(ethics_plasticity_is_bio_async_connected(nullptr));
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(EthicsPlasticityBridgeTest, FullLearningCycle) {
    register_test_synapses();

    // Learning phase
    for (int i = 0; i < 10; i++) {
        ethics_plasticity_learn(bridge, ETHICS_LEARN_POSITIVE_OUTCOME, 0.8f, 0.5f, 0);
        ethics_plasticity_update_traces(bridge, 10.0f);
    }

    // Apply reward
    ethics_plasticity_apply_reward(bridge, 0.9f);

    // Update BCM
    ethics_plasticity_update_bcm(bridge, 100.0f);

    // Consolidate
    ethics_plasticity_consolidate(bridge);

    ethics_plasticity_stats_t stats;
    ethics_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 10u);
    EXPECT_GT(stats.weight_updates, 0u);
}

TEST_F(EthicsPlasticityBridgeTest, PrincipleEvolution) {
    register_test_synapses();

    ethics_principle_state_t initial_state;
    ethics_plasticity_get_principle_state(bridge, &initial_state);

    // Multiple harm avoidance events
    for (int i = 0; i < 5; i++) {
        ethics_plasticity_learn(bridge, ETHICS_LEARN_HARM_AVOIDED, 0.9f, 0.8f, 0);
    }

    ethics_principle_state_t final_state;
    ethics_plasticity_get_principle_state(bridge, &final_state);

    // Harm sensitivity should increase
    EXPECT_GE(final_state.harm_sensitivity, initial_state.harm_sensitivity);
}
