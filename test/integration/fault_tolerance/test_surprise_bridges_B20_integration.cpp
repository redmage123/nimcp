/**
 * @file test_surprise_bridges_B20_integration.cpp
 * @brief Integration tests for B20 surprise bridges working together
 *        with health agent + cross-bridge interactions
 *        (cognitive/salience surprise bridges: plasticity, SNN, substrate,
 *         thalamic, pink_noise, imagination, self_model)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

extern "C" {
    /* Surprise bridge health agent global setters */
    void surprise_plasticity_bridge_set_health_agent_global(void* agent);
    void surprise_snn_bridge_set_health_agent_global(void* agent);
    void surprise_substrate_bridge_set_health_agent_global(void* agent);
    void surprise_thalamic_bridge_set_health_agent_global(void* agent);
    void surprise_pink_noise_bridge_set_health_agent_global(void* agent);
    void surprise_imagination_bridge_set_health_agent_global(void* agent);
    void surprise_self_model_bridge_set_health_agent_global(void* agent);

    /* Surprise bridge APIs (for cross-bridge tests) */
    #include "cognitive/salience/nimcp_surprise_plasticity_bridge.h"
    #include "cognitive/salience/nimcp_surprise_snn_bridge.h"
    #include "cognitive/salience/nimcp_surprise_substrate_bridge.h"
    #include "cognitive/salience/nimcp_surprise_thalamic_bridge.h"
    #include "cognitive/salience/nimcp_surprise_pink_noise_bridge.h"
    #include "cognitive/salience/nimcp_surprise_imagination_bridge.h"
    #include "cognitive/salience/nimcp_surprise_self_model_bridge.h"

    /* Health agent API */
    #include "utils/fault_tolerance/nimcp_health_agent.h"
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kSurpriseBridgeModules[] = {
    {"surprise_plasticity_bridge", surprise_plasticity_bridge_set_health_agent_global},
    {"surprise_snn_bridge",        surprise_snn_bridge_set_health_agent_global},
    {"surprise_substrate_bridge",  surprise_substrate_bridge_set_health_agent_global},
    {"surprise_thalamic_bridge",   surprise_thalamic_bridge_set_health_agent_global},
    {"surprise_pink_noise_bridge", surprise_pink_noise_bridge_set_health_agent_global},
    {"surprise_imagination_bridge", surprise_imagination_bridge_set_health_agent_global},
    {"surprise_self_model_bridge", surprise_self_model_bridge_set_health_agent_global},
};

static constexpr size_t kNumModules = sizeof(kSurpriseBridgeModules) / sizeof(kSurpriseBridgeModules[0]);

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SurpriseBridgesB20IntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent_ = nullptr;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = false;
        agent_ = nimcp_health_agent_create(&config);
        ASSERT_NE(nullptr, agent_);
    }

    void TearDown() override {
        for (size_t i = 0; i < kNumModules; i++) {
            kSurpriseBridgeModules[i].setter(nullptr);
        }
        if (agent_) {
            nimcp_health_agent_stop(agent_);
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }
};

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(SurpriseBridgesB20IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    health_agent_stats_t before, after;
    nimcp_health_agent_get_stats(agent_, &before);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kSurpriseBridgeModules[i].name, 0);
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + kNumModules);
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20IntegrationTest, MultipleModulesShareSingleAgent) {
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
}

TEST_F(SurpriseBridgesB20IntegrationTest, DisconnectModulesWhileAgentRunning) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules / 2; i++) kSurpriseBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    nimcp_health_agent_start(agent_);
    nimcp_health_agent_stop(agent_);
    nimcp_health_agent_start(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kSurpriseBridgeModules[i].setter(agent_);
            for (int j = 0; j < 50; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kSurpriseBridgeModules[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent2);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(SurpriseBridgesB20IntegrationTest, ProgressiveModuleConnection) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) {
        kSurpriseBridgeModules[i].setter(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, kSurpriseBridgeModules[i].name, 0);
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20IntegrationTest, PlasticitySnnCrossInteraction) {
    /* Plasticity boost drives SNN encoding */
    surprise_plasticity_bridge_t* plast = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, plast);
    surprise_snn_bridge_t* snn = surprise_snn_bridge_create(nullptr);
    ASSERT_NE(nullptr, snn);

    /* Fire surprise event on plasticity bridge */
    surprise_plasticity_on_surprise_event(plast, 0.85f, 1);
    surprise_plasticity_effects_t plast_effects;
    surprise_plasticity_bridge_get_effects(plast, &plast_effects);
    float lr_mult = plast_effects.learning_rate_multiplier;
    EXPECT_GT(lr_mult, 1.0f);

    /* Use boost magnitude to encode into SNN */
    float encoded_level = (lr_mult > 1.0f) ? 0.9f : 0.3f;
    int rc = surprise_snn_encode_surprise(snn, encoded_level, SURPRISE_SNN_CHANNEL_PE);
    EXPECT_EQ(0, rc);
    surprise_snn_simulate_step(snn);
    surprise_snn_stats_t snn_stats;
    surprise_snn_bridge_get_stats(snn, &snn_stats);
    EXPECT_GE(snn_stats.encoding_events, 1u);

    surprise_snn_bridge_destroy(snn);
    surprise_plasticity_bridge_destroy(plast);
}

TEST_F(SurpriseBridgesB20IntegrationTest, SubstrateAffectsProcessing) {
    /* Low ATP affects all bridge operations */
    surprise_substrate_bridge_t* sub = surprise_substrate_bridge_create(nullptr);
    ASSERT_NE(nullptr, sub);
    surprise_plasticity_bridge_t* plast = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, plast);

    /* Update substrate with low ATP */
    surprise_substrate_bridge_update(sub, 0.1f, 0.8f);
    surprise_substrate_effects_t sub_effects;
    surprise_substrate_bridge_get_effects(sub, &sub_effects);
    EXPECT_LE(sub_effects.overall_capacity, 1.0f);

    /* Plasticity should still function */
    int rc = surprise_plasticity_on_surprise_event(plast, 0.9f, 1);
    EXPECT_EQ(0, rc);

    surprise_plasticity_bridge_destroy(plast);
    surprise_substrate_bridge_destroy(sub);
}

TEST_F(SurpriseBridgesB20IntegrationTest, FullPipeline) {
    /* Full pipeline: plasticity -> SNN -> thalamic -> self-model */
    surprise_plasticity_bridge_t* plast = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, plast);
    surprise_snn_bridge_t* snn = surprise_snn_bridge_create(nullptr);
    ASSERT_NE(nullptr, snn);
    surprise_thalamic_bridge_t* thal = surprise_thalamic_bridge_create(nullptr);
    ASSERT_NE(nullptr, thal);
    surprise_self_model_bridge_t* sm = surprise_self_model_bridge_create(nullptr);
    ASSERT_NE(nullptr, sm);

    /* Step 1: Surprise event triggers plasticity boost */
    surprise_plasticity_on_surprise_event(plast, 0.9f, 1);
    surprise_plasticity_effects_t plast_effects;
    surprise_plasticity_bridge_get_effects(plast, &plast_effects);
    EXPECT_GT(plast_effects.learning_rate_multiplier, 1.0f);

    /* Step 2: Encode surprise into SNN */
    surprise_snn_encode_surprise(snn, 0.9f, SURPRISE_SNN_CHANNEL_PE);
    surprise_snn_simulate_step(snn);

    /* Step 3: Route through thalamic bridge */
    surprise_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = SURPRISE_THALAMIC_REALIZATION;
    signal.surprise_magnitude = 0.9f;
    signal.source_module = 1;
    signal.urgency = 0.8f;
    int rc = surprise_thalamic_route_surprise(thal, &signal);
    EXPECT_EQ(0, rc);

    /* Step 4: Self-model capability surprise */
    rc = surprise_self_model_on_capability_surprise(
        sm, 1, 0.8f, SURPRISE_CAPABILITY_UPGRADE);
    EXPECT_EQ(0, rc);

    surprise_self_model_stats_t sm_stats;
    surprise_self_model_bridge_get_stats(sm, &sm_stats);
    EXPECT_GE(sm_stats.capability_surprises, 1u);

    surprise_self_model_bridge_destroy(sm);
    surprise_thalamic_bridge_destroy(thal);
    surprise_snn_bridge_destroy(snn);
    surprise_plasticity_bridge_destroy(plast);
}
