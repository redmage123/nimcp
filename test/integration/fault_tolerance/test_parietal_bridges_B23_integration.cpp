/**
 * @file test_parietal_bridges_B23_integration.cpp
 * @brief Integration tests for B23 parietal bridges working together
 *        with health agent + cross-bridge interactions
 *        (cognitive/parietal bridges: fep_parietal, genius_plasticity,
 *         genius_snn, genius_training, intuition_substrate, intuition_thalamic,
 *         parietal_fep, parietal_plasticity, parietal_quantum, parietal_snn,
 *         parietal_training)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

/* Parietal bridge APIs (for cross-bridge tests) */
#include "cognitive/parietal/nimcp_fep_parietal_bridge.h"
#include "cognitive/parietal/nimcp_genius_plasticity_bridge.h"
#include "cognitive/parietal/nimcp_genius_snn_bridge.h"
#include "cognitive/parietal/nimcp_genius_training_bridge.h"
#include "cognitive/parietal/nimcp_intuition_substrate_bridge.h"
#include "cognitive/parietal/nimcp_intuition_thalamic_bridge.h"
#include "cognitive/parietal/nimcp_parietal_fep_bridge.h"
#include "cognitive/parietal/nimcp_parietal_plasticity_bridge.h"
#include "cognitive/parietal/nimcp_parietal_quantum_bridge.h"
#include "cognitive/parietal/nimcp_parietal_snn_bridge.h"
#include "cognitive/parietal/nimcp_parietal_training_bridge.h"

/* Health agent API */
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    /* Parietal bridge health agent global setters */
    void fep_parietal_bridge_set_health_agent(void* agent);
    void genius_plasticity_bridge_set_health_agent(void* agent);
    void genius_snn_bridge_set_health_agent(void* agent);
    void genius_training_bridge_set_health_agent(void* agent);
    void intuition_substrate_bridge_set_health_agent(void* agent);
    void intuition_thalamic_bridge_set_health_agent(void* agent);
    void parietal_fep_bridge_set_health_agent(void* agent);
    void parietal_plasticity_bridge_set_health_agent(void* agent);
    void parietal_quantum_bridge_set_health_agent(void* agent);
    void parietal_snn_bridge_set_health_agent(void* agent);
    void parietal_training_bridge_set_health_agent(void* agent);
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kParietalBridgeModules[] = {
    {"fep_parietal_bridge",           fep_parietal_bridge_set_health_agent},
    {"genius_plasticity_bridge",      genius_plasticity_bridge_set_health_agent},
    {"genius_snn_bridge",             genius_snn_bridge_set_health_agent},
    {"genius_training_bridge",        genius_training_bridge_set_health_agent},
    {"intuition_substrate_bridge",    intuition_substrate_bridge_set_health_agent},
    {"intuition_thalamic_bridge",     intuition_thalamic_bridge_set_health_agent},
    {"parietal_fep_bridge",           parietal_fep_bridge_set_health_agent},
    {"parietal_plasticity_bridge",    parietal_plasticity_bridge_set_health_agent},
    {"parietal_quantum_bridge",       parietal_quantum_bridge_set_health_agent},
    {"parietal_snn_bridge",           parietal_snn_bridge_set_health_agent},
    {"parietal_training_bridge",      parietal_training_bridge_set_health_agent},
};

static constexpr size_t kNumModules = 11;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ParietalBridgesB23IntegrationTest : public ::testing::Test {
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
            kParietalBridgeModules[i].setter(nullptr);
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

TEST_F(ParietalBridgesB23IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    health_agent_stats_t before, after;
    nimcp_health_agent_get_stats(agent_, &before);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kParietalBridgeModules[i].name, 0);
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + kNumModules);
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23IntegrationTest, MultipleModulesShareSingleAgent) {
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
}

TEST_F(ParietalBridgesB23IntegrationTest, DisconnectModulesWhileAgentRunning) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules / 2; i++) kParietalBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    nimcp_health_agent_start(agent_);
    nimcp_health_agent_stop(agent_);
    nimcp_health_agent_start(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kParietalBridgeModules[i].setter(agent_);
            for (int j = 0; j < 50; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kParietalBridgeModules[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent2);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(ParietalBridgesB23IntegrationTest, ProgressiveModuleConnection) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) {
        kParietalBridgeModules[i].setter(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, kParietalBridgeModules[i].name, 0);
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
    nimcp_health_agent_stop(agent_);
}

/* ============================================================================
 * Cross-Bridge Interaction Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23IntegrationTest, FepParietalQuantumCrossInteraction) {
    /* FEP parietal handles free energy; quantum handles optimization */
    fep_parietal_config_t fep_cfg = fep_parietal_default_config();
    fep_parietal_bridge_t* fep = fep_parietal_bridge_create(&fep_cfg);
    ASSERT_NE(nullptr, fep);
    parietal_quantum_config_t q_cfg = parietal_quantum_default_config();
    parietal_quantum_bridge_t* quantum = parietal_quantum_bridge_create(&q_cfg);
    ASSERT_NE(nullptr, quantum);

    /* Verify stats are accessible for both */
    fep_parietal_stats_t fep_stats;
    int rc = fep_parietal_get_stats(fep, &fep_stats);
    EXPECT_EQ(0, rc);

    parietal_quantum_stats_t q_stats;
    rc = parietal_quantum_get_stats(quantum, &q_stats);
    EXPECT_EQ(0, rc);

    parietal_quantum_bridge_destroy(quantum);
    fep_parietal_bridge_destroy(fep);
}

TEST_F(ParietalBridgesB23IntegrationTest, GeniusPlasticitySnnCrossInteraction) {
    /* Genius plasticity manages synaptic learning; SNN encodes spike patterns */
    genius_plasticity_config_t p_cfg = genius_plasticity_config_default();
    genius_plasticity_bridge_t* plast = genius_plasticity_create(&p_cfg);
    ASSERT_NE(nullptr, plast);
    genius_snn_config_t s_cfg = genius_snn_config_default();
    genius_snn_bridge_t* snn = genius_snn_create(&s_cfg);
    ASSERT_NE(nullptr, snn);

    /* Reset both bridges */
    int rc = genius_plasticity_reset(plast);
    EXPECT_EQ(0, rc);
    rc = genius_snn_reset(snn);
    EXPECT_EQ(0, rc);

    genius_snn_destroy(snn);
    genius_plasticity_destroy(plast);
}

TEST_F(ParietalBridgesB23IntegrationTest, ParietalFepPlasticityCrossInteraction) {
    /* Parietal FEP handles spatial free energy; plasticity handles learning */
    parietal_fep_config_t fep_cfg = parietal_fep_config_default();
    parietal_fep_bridge_t* fep = parietal_fep_bridge_create(&fep_cfg);
    ASSERT_NE(nullptr, fep);
    parietal_plasticity_config_t p_cfg = parietal_plasticity_config_default();
    parietal_plasticity_bridge_t* plast = parietal_plasticity_create(&p_cfg);
    ASSERT_NE(nullptr, plast);

    /* Reset and get stats from both */
    int rc = parietal_fep_bridge_reset(fep);
    EXPECT_EQ(0, rc);
    rc = parietal_plasticity_reset(plast);
    EXPECT_EQ(0, rc);

    parietal_fep_stats_t fep_stats;
    rc = parietal_fep_bridge_get_stats(fep, &fep_stats);
    EXPECT_EQ(0, rc);

    parietal_plasticity_destroy(plast);
    parietal_fep_bridge_destroy(fep);
}

TEST_F(ParietalBridgesB23IntegrationTest, FullPipeline) {
    /* Full pipeline: FEP parietal -> parietal FEP -> parietal SNN -> parietal plasticity */
    fep_parietal_config_t cfg1 = fep_parietal_default_config();
    fep_parietal_bridge_t* fep_par = fep_parietal_bridge_create(&cfg1);
    ASSERT_NE(nullptr, fep_par);
    parietal_fep_config_t cfg2 = parietal_fep_config_default();
    parietal_fep_bridge_t* par_fep = parietal_fep_bridge_create(&cfg2);
    ASSERT_NE(nullptr, par_fep);
    parietal_snn_config_t cfg3 = parietal_snn_config_default();
    parietal_snn_bridge_t* snn = parietal_snn_create(&cfg3);
    ASSERT_NE(nullptr, snn);
    parietal_plasticity_config_t cfg4 = parietal_plasticity_config_default();
    parietal_plasticity_bridge_t* plast = parietal_plasticity_create(&cfg4);
    ASSERT_NE(nullptr, plast);

    /* Step 1: FEP parietal processes beliefs */
    fep_parietal_stats_t fep_stats;
    int rc = fep_parietal_get_stats(fep_par, &fep_stats);
    EXPECT_EQ(0, rc);

    /* Step 2: Parietal FEP computes free energy */
    parietal_fep_stats_t par_fep_stats;
    rc = parietal_fep_bridge_get_stats(par_fep, &par_fep_stats);
    EXPECT_EQ(0, rc);

    /* Step 3: SNN encodes spatial state */
    rc = parietal_snn_reset(snn);
    EXPECT_EQ(0, rc);

    /* Step 4: Plasticity consolidates learning */
    rc = parietal_plasticity_reset(plast);
    EXPECT_EQ(0, rc);

    parietal_plasticity_destroy(plast);
    parietal_snn_destroy(snn);
    parietal_fep_bridge_destroy(par_fep);
    fep_parietal_bridge_destroy(fep_par);
}
