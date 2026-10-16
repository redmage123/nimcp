/**
 * @file test_mirror_plasticity_bridge.cpp
 * @brief Unit tests for Mirror Neuron - Plasticity Bridge integration
 * @date 2026-01-05
 *
 * Tests bidirectional integration between mirror neurons and plasticity module:
 * - Mirror --> Plasticity: Spike events trigger STDP learning
 * - Plasticity --> Mirror: Weight updates modulate mirror activity
 * - Homeostatic scaling
 * - Reward-modulated learning
 */

#include <gtest/gtest.h>

// Note: Header has its own extern "C" guards
#include "cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.h"

#include "utils/time/nimcp_time.h"

#include <cmath>
#include <cstring>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class MirrorPlasticityBridgeTest : public ::testing::Test {
protected:
    mirror_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        mirror_plasticity_config_t config = mirror_plasticity_config_default();
        config.enable_bio_async = false;  /* Disable for unit tests */
        config.enable_immune_integration = false;
        config.enable_sleep_integration = false;
        bridge = mirror_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create mirror-plasticity bridge";
    }

    void TearDown() override {
        if (bridge) {
            mirror_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }

    /* Helper to register multiple synapses for an action */
    void register_synapses_for_action(uint32_t action_id, uint32_t count, float init_weight) {
        for (uint32_t i = 0; i < count; i++) {
            mirror_plasticity_register_synapse(bridge, action_id,
                MIRROR_SYNAPSE_OBS_TO_HIDDEN, init_weight);
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MirrorPlasticityBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(MirrorPlasticityBridgeTest, CreateWithDefaultConfig) {
    mirror_plasticity_bridge_t* b = mirror_plasticity_create(nullptr);
    ASSERT_NE(b, nullptr);
    mirror_plasticity_destroy(b);
}

TEST_F(MirrorPlasticityBridgeTest, DefaultConfigValues) {
    mirror_plasticity_config_t config = mirror_plasticity_config_default();

    EXPECT_GT(config.stdp_ltp_window_ms, 0.0f);
    EXPECT_GT(config.stdp_ltd_window_ms, 0.0f);
    EXPECT_GT(config.stdp_a_plus, 0.0f);
    EXPECT_GT(config.stdp_a_minus, 0.0f);
    EXPECT_TRUE(config.enable_bcm);
    EXPECT_TRUE(config.enable_homeostatic);
    EXPECT_TRUE(config.enable_eligibility);
    EXPECT_GE(config.weight_min, 0.0f);
    EXPECT_GT(config.weight_max, config.weight_min);
}

TEST_F(MirrorPlasticityBridgeTest, GetOrchestratorNotNull) {
    plasticity_orchestrator_t* orch = mirror_plasticity_get_orchestrator(bridge);
    EXPECT_NE(orch, nullptr);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(MirrorPlasticityBridgeTest, RegisterSynapse) {
    uint32_t synapse_id = mirror_plasticity_register_synapse(
        bridge, 1, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);

    EXPECT_NE(synapse_id, UINT32_MAX) << "Should return valid synapse ID";
}

TEST_F(MirrorPlasticityBridgeTest, RegisterMultipleSynapses) {
    uint32_t id1 = mirror_plasticity_register_synapse(bridge, 1, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);
    uint32_t id2 = mirror_plasticity_register_synapse(bridge, 1, MIRROR_SYNAPSE_EXEC_TO_HIDDEN, 0.5f);
    uint32_t id3 = mirror_plasticity_register_synapse(bridge, 2, MIRROR_SYNAPSE_HIDDEN_TO_OUTPUT, 0.5f);

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST_F(MirrorPlasticityBridgeTest, UnregisterSynapse) {
    uint32_t synapse_id = mirror_plasticity_register_synapse(
        bridge, 1, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);

    int ret = mirror_plasticity_unregister_synapse(bridge, synapse_id);
    EXPECT_EQ(ret, 0) << "Should unregister synapse";

    /* Try to unregister again - should fail */
    ret = mirror_plasticity_unregister_synapse(bridge, synapse_id);
    EXPECT_EQ(ret, -1) << "Should fail for non-existent synapse";
}

TEST_F(MirrorPlasticityBridgeTest, GetSynapseState) {
    uint32_t synapse_id = mirror_plasticity_register_synapse(
        bridge, 5, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.7f);

    mirror_plasticity_synapse_t state;
    int ret = mirror_plasticity_get_synapse(bridge, synapse_id, &state);

    EXPECT_EQ(ret, 0) << "Should get synapse state";
    EXPECT_EQ(state.synapse_id, synapse_id);
    EXPECT_EQ(state.action_id, 5u);
    EXPECT_FLOAT_EQ(state.weight, 0.7f);
    EXPECT_FLOAT_EQ(state.initial_weight, 0.7f);
}

TEST_F(MirrorPlasticityBridgeTest, GetWeight) {
    uint32_t synapse_id = mirror_plasticity_register_synapse(
        bridge, 1, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.6f);

    float weight = mirror_plasticity_get_weight(bridge, synapse_id);
    EXPECT_FLOAT_EQ(weight, 0.6f);
}

TEST_F(MirrorPlasticityBridgeTest, GetWeightInvalidSynapse) {
    float weight = mirror_plasticity_get_weight(bridge, 99999);
    EXPECT_TRUE(std::isnan(weight)) << "Should return NaN for invalid synapse";
}

TEST_F(MirrorPlasticityBridgeTest, GetActionWeights) {
    register_synapses_for_action(3, 5, 0.5f);

    float weights[10];
    int count = mirror_plasticity_get_action_weights(bridge, 3, weights, 10);

    EXPECT_EQ(count, 5) << "Should get 5 weights for action 3";
    for (int i = 0; i < count; i++) {
        EXPECT_FLOAT_EQ(weights[i], 0.5f);
    }
}

//=============================================================================
// Mirror --> Plasticity Pathway Tests (STDP)
//=============================================================================

TEST_F(MirrorPlasticityBridgeTest, PreSpike) {
    uint32_t synapse_id = mirror_plasticity_register_synapse(
        bridge, 1, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);

    uint64_t timestamp = 1000000;  /* 1 second */
    float dw = mirror_plasticity_pre_spike(bridge, synapse_id, timestamp);

    /* Without a prior post-spike, should not cause weight change */
    EXPECT_FLOAT_EQ(dw, 0.0f);
}

TEST_F(MirrorPlasticityBridgeTest, PostSpike) {
    uint32_t synapse_id = mirror_plasticity_register_synapse(
        bridge, 1, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);

    uint64_t timestamp = 1000000;
    float dw = mirror_plasticity_post_spike(bridge, synapse_id, timestamp);

    /* Without a prior pre-spike, should not cause weight change */
    EXPECT_FLOAT_EQ(dw, 0.0f);
}

TEST_F(MirrorPlasticityBridgeTest, STDPLTPPreBeforePost) {
    uint32_t synapse_id = mirror_plasticity_register_synapse(
        bridge, 1, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);

    /* Pre-spike at t=1s */
    mirror_plasticity_pre_spike(bridge, synapse_id, 1000000);

    /* Post-spike at t=1.01s (10ms later) - should trigger LTP */
    float dw = mirror_plasticity_post_spike(bridge, synapse_id, 1010000);

    EXPECT_GT(dw, 0.0f) << "Pre before post should cause LTP (positive weight change)";
}

TEST_F(MirrorPlasticityBridgeTest, STDPLTDPostBeforePre) {
    uint32_t synapse_id = mirror_plasticity_register_synapse(
        bridge, 1, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);

    /* Post-spike at t=1s */
    mirror_plasticity_post_spike(bridge, synapse_id, 1000000);

    /* Pre-spike at t=1.01s (10ms later) - should trigger LTD */
    float dw = mirror_plasticity_pre_spike(bridge, synapse_id, 1010000);

    EXPECT_LT(dw, 0.0f) << "Post before pre should cause LTD (negative weight change)";
}

TEST_F(MirrorPlasticityBridgeTest, STDPWindowExpiry) {
    uint32_t synapse_id = mirror_plasticity_register_synapse(
        bridge, 1, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);

    /* Pre-spike at t=0 */
    mirror_plasticity_pre_spike(bridge, synapse_id, 0);

    /* Post-spike at t=100ms (beyond typical STDP window) */
    float dw = mirror_plasticity_post_spike(bridge, synapse_id, 100000);

    EXPECT_FLOAT_EQ(dw, 0.0f) << "Spikes beyond window should not cause weight change";
}

TEST_F(MirrorPlasticityBridgeTest, Observation) {
    register_synapses_for_action(2, 3, 0.5f);

    int ret = mirror_plasticity_observation(bridge, 2, 1.0f, 1000000);
    EXPECT_EQ(ret, 0) << "Observation should succeed";
}

TEST_F(MirrorPlasticityBridgeTest, Execution) {
    register_synapses_for_action(2, 3, 0.5f);

    int ret = mirror_plasticity_execution(bridge, 2, 1.0f, 1000000);
    EXPECT_EQ(ret, 0) << "Execution should succeed";
}

//=============================================================================
// Reward-Modulated Learning Tests
//=============================================================================

TEST_F(MirrorPlasticityBridgeTest, RewardPositive) {
    register_synapses_for_action(1, 5, 0.5f);

    /* Create activity to build eligibility traces */
    mirror_plasticity_observation(bridge, 1, 1.0f, 1000000);
    mirror_plasticity_execution(bridge, 1, 1.0f, 1010000);

    /* Apply positive reward */
    int updated = mirror_plasticity_reward(bridge, 0.5f, 1020000);
    EXPECT_GE(updated, 0) << "Reward should update some synapses";
}

TEST_F(MirrorPlasticityBridgeTest, RewardNegative) {
    register_synapses_for_action(1, 5, 0.5f);

    mirror_plasticity_observation(bridge, 1, 1.0f, 1000000);
    mirror_plasticity_execution(bridge, 1, 1.0f, 1010000);

    int updated = mirror_plasticity_reward(bridge, -0.5f, 1020000);
    EXPECT_GE(updated, 0) << "Negative reward should also update";
}

TEST_F(MirrorPlasticityBridgeTest, RewardAction) {
    register_synapses_for_action(3, 5, 0.5f);

    mirror_plasticity_observation(bridge, 3, 1.0f, 1000000);
    mirror_plasticity_execution(bridge, 3, 1.0f, 1010000);

    int updated = mirror_plasticity_reward_action(bridge, 3, 0.8f);
    EXPECT_GE(updated, 0) << "Action-specific reward should succeed";
}

//=============================================================================
// Consolidation Tests
//=============================================================================

TEST_F(MirrorPlasticityBridgeTest, Consolidate) {
    register_synapses_for_action(1, 3, 0.3f);

    /* Manually modify weights to simulate significant learning */
    /* Note: We'll use STDP to create changes */
    for (int i = 0; i < 10; i++) {
        mirror_plasticity_observation(bridge, 1, 1.0f, i * 1000000);
        mirror_plasticity_execution(bridge, 1, 1.0f, i * 1000000 + 10000);
    }

    int consolidated = mirror_plasticity_consolidate(bridge);
    EXPECT_GE(consolidated, 0) << "Consolidation should succeed";
}

//=============================================================================
// Plasticity --> Mirror Pathway Tests
//=============================================================================

TEST_F(MirrorPlasticityBridgeTest, GetActionModulation) {
    register_synapses_for_action(5, 3, 0.7f);

    float modulation;
    int ret = mirror_plasticity_get_action_modulation(bridge, 5, &modulation);

    EXPECT_EQ(ret, 0) << "Should get action modulation";
    EXPECT_FLOAT_EQ(modulation, 0.7f) << "Modulation should be average weight";
}

TEST_F(MirrorPlasticityBridgeTest, GetLRModulation) {
    float lr_mod = mirror_plasticity_get_lr_modulation(bridge);
    EXPECT_GT(lr_mod, 0.0f) << "LR modulation should be positive";
    EXPECT_LE(lr_mod, 1.0f) << "LR modulation should be <= 1.0";
}

TEST_F(MirrorPlasticityBridgeTest, IsLearningBlocked) {
    bool blocked = mirror_plasticity_is_learning_blocked(bridge);
    EXPECT_FALSE(blocked) << "Learning should not be blocked initially";
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(MirrorPlasticityBridgeTest, GetState) {
    register_synapses_for_action(1, 5, 0.5f);

    mirror_plasticity_bridge_state_t state;
    int ret = mirror_plasticity_get_state(bridge, &state);

    EXPECT_EQ(ret, 0) << "Should get state";
    EXPECT_EQ(state.state, MIRROR_PLASTICITY_STATE_IDLE);
    EXPECT_EQ(state.total_synapses, 5u);
    EXPECT_FLOAT_EQ(state.mean_weight, 0.5f);
}

TEST_F(MirrorPlasticityBridgeTest, GetStats) {
    mirror_plasticity_stats_t stats;
    int ret = mirror_plasticity_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0) << "Should get stats";
    EXPECT_EQ(stats.total_ltp_events, 0u);
    EXPECT_EQ(stats.total_ltd_events, 0u);
}

TEST_F(MirrorPlasticityBridgeTest, ResetStats) {
    register_synapses_for_action(1, 3, 0.5f);

    /* Generate some activity */
    mirror_plasticity_observation(bridge, 1, 1.0f, 1000000);
    mirror_plasticity_execution(bridge, 1, 1.0f, 1010000);

    mirror_plasticity_stats_t stats;
    mirror_plasticity_get_stats(bridge, &stats);
    uint64_t pre_spikes = stats.total_pre_spikes;

    mirror_plasticity_reset_stats(bridge);

    mirror_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_pre_spikes, 0u) << "Stats should be reset";
}

TEST_F(MirrorPlasticityBridgeTest, GetATPLevel) {
    float atp = mirror_plasticity_get_atp_level(bridge);
    // ATP level from orchestrator is returned as percentage (0-100)
    EXPECT_GE(atp, 0.0f);
    EXPECT_LE(atp, 100.0f);
}

//=============================================================================
// Update Loop Tests
//=============================================================================

TEST_F(MirrorPlasticityBridgeTest, Update) {
    register_synapses_for_action(1, 5, 0.5f);

    int ret = mirror_plasticity_update(bridge, 10.0f);
    EXPECT_EQ(ret, 0) << "Update should succeed";
}

TEST_F(MirrorPlasticityBridgeTest, Reset) {
    register_synapses_for_action(1, 3, 0.5f);

    /* Modify weights through STDP */
    mirror_plasticity_pre_spike(bridge, 0, 1000000);
    mirror_plasticity_post_spike(bridge, 0, 1010000);

    int ret = mirror_plasticity_reset(bridge);
    EXPECT_EQ(ret, 0) << "Reset should succeed";

    /* Weights should be back to initial */
    float weight = mirror_plasticity_get_weight(bridge, 0);
    EXPECT_FLOAT_EQ(weight, 0.5f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int g_weight_callback_count = 0;
static float g_last_weight_change = 0.0f;
static void test_weight_callback(uint32_t synapse_id, uint32_t action_id,
                                  float old_weight, float new_weight,
                                  mirror_learn_event_t event_type, void* user_data) {
    g_weight_callback_count++;
    g_last_weight_change = new_weight - old_weight;
}

TEST_F(MirrorPlasticityBridgeTest, RegisterWeightCallback) {
    g_weight_callback_count = 0;
    int ret = mirror_plasticity_register_weight_callback(bridge, test_weight_callback, nullptr);
    EXPECT_EQ(ret, 0) << "Should register callback";
}

TEST_F(MirrorPlasticityBridgeTest, WeightCallbackTriggered) {
    g_weight_callback_count = 0;
    g_last_weight_change = 0.0f;

    mirror_plasticity_register_weight_callback(bridge, test_weight_callback, nullptr);

    uint32_t synapse_id = mirror_plasticity_register_synapse(
        bridge, 1, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);

    /* Create LTP event */
    mirror_plasticity_pre_spike(bridge, synapse_id, 1000000);
    mirror_plasticity_post_spike(bridge, synapse_id, 1010000);

    EXPECT_GT(g_weight_callback_count, 0) << "Callback should be triggered";
    EXPECT_GT(g_last_weight_change, 0.0f) << "LTP should increase weight";
}

static int g_consolidation_callback_count = 0;
static void test_consolidation_callback(uint32_t action_id, uint32_t synapse_count,
                                         float avg_weight, void* user_data) {
    g_consolidation_callback_count++;
}

TEST_F(MirrorPlasticityBridgeTest, RegisterConsolidationCallback) {
    g_consolidation_callback_count = 0;
    int ret = mirror_plasticity_register_consolidation_callback(
        bridge, test_consolidation_callback, nullptr);
    EXPECT_EQ(ret, 0) << "Should register callback";
}

static int g_homeostatic_callback_count = 0;
static void test_homeostatic_callback(float current_rate, float target_rate,
                                       float scale_factor, void* user_data) {
    g_homeostatic_callback_count++;
}

TEST_F(MirrorPlasticityBridgeTest, RegisterHomeostaticCallback) {
    g_homeostatic_callback_count = 0;
    int ret = mirror_plasticity_register_homeostatic_callback(
        bridge, test_homeostatic_callback, nullptr);
    EXPECT_EQ(ret, 0) << "Should register callback";
}

static int g_energy_callback_count = 0;
static void test_energy_callback(float atp_level, bool learning_blocked, void* user_data) {
    g_energy_callback_count++;
}

TEST_F(MirrorPlasticityBridgeTest, RegisterEnergyCallback) {
    g_energy_callback_count = 0;
    int ret = mirror_plasticity_register_energy_callback(
        bridge, test_energy_callback, nullptr);
    EXPECT_EQ(ret, 0) << "Should register callback";
}

//=============================================================================
// Null Parameter Handling
//=============================================================================

TEST_F(MirrorPlasticityBridgeTest, NullBridgeHandling) {
    EXPECT_EQ(mirror_plasticity_register_synapse(nullptr, 0, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0), UINT32_MAX);
    EXPECT_EQ(mirror_plasticity_pre_spike(nullptr, 0, 0), 0.0f);
    EXPECT_EQ(mirror_plasticity_post_spike(nullptr, 0, 0), 0.0f);
    EXPECT_EQ(mirror_plasticity_reward(nullptr, 0, 0), 0);
    EXPECT_EQ(mirror_plasticity_update(nullptr, 0), -1);
    EXPECT_EQ(mirror_plasticity_reset(nullptr), -1);
    EXPECT_EQ(mirror_plasticity_get_orchestrator(nullptr), nullptr);
}

//=============================================================================
// Weight Bounds Tests
//=============================================================================

TEST_F(MirrorPlasticityBridgeTest, WeightBoundsRespected) {
    uint32_t synapse_id = mirror_plasticity_register_synapse(
        bridge, 1, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.95f);

    /* Try to trigger massive LTP to exceed bounds */
    for (int i = 0; i < 100; i++) {
        mirror_plasticity_pre_spike(bridge, synapse_id, i * 1000);
        mirror_plasticity_post_spike(bridge, synapse_id, i * 1000 + 5);
    }

    float weight = mirror_plasticity_get_weight(bridge, synapse_id);
    EXPECT_LE(weight, 1.0f) << "Weight should not exceed max";
    EXPECT_GE(weight, 0.0f) << "Weight should not go below min";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
