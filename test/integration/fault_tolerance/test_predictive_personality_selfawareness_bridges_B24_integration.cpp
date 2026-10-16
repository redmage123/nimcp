/**
 * @file test_predictive_personality_selfawareness_bridges_B24_integration.cpp
 * @brief Integration tests for B24 predictive+personality+self_awareness bridge
 *        health agent integration: concurrent health, cross-bridge, shared agents
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

/* Bridge APIs - outside extern "C" */
#include "cognitive/predictive/nimcp_predictive_fep_bridge.h"
#include "cognitive/predictive/nimcp_predictive_plasticity_bridge.h"
#include "cognitive/predictive/nimcp_predictive_snn_bridge.h"
#include "cognitive/predictive/nimcp_predictive_substrate_bridge.h"
#include "cognitive/predictive/nimcp_predictive_thalamic_bridge.h"
#include "cognitive/personality/nimcp_personality_plasticity_bridge.h"
#include "cognitive/personality/nimcp_personality_snn_bridge.h"
#include "cognitive/personality/nimcp_personality_substrate_bridge.h"
#include "cognitive/personality/nimcp_personality_thalamic_bridge.h"
#include "cognitive/self_awareness/nimcp_self_awareness_plasticity_bridge.h"
#include "cognitive/self_awareness/nimcp_self_awareness_snn_bridge.h"
#include "cognitive/self_awareness/nimcp_self_awareness_substrate_bridge.h"
#include "cognitive/self_awareness/nimcp_self_awareness_thalamic_bridge.h"

/* Health agent API */
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    void predictive_fep_bridge_set_health_agent(void* agent);
    void predictive_plasticity_bridge_set_health_agent(void* agent);
    void predictive_snn_bridge_set_health_agent(void* agent);
    void predictive_substrate_bridge_set_health_agent(void* agent);
    void predictive_thalamic_bridge_set_health_agent(void* agent);
    void personality_plasticity_bridge_set_health_agent(void* agent);
    void personality_snn_bridge_set_health_agent(void* agent);
    void personality_substrate_bridge_set_health_agent(void* agent);
    void personality_thalamic_bridge_set_health_agent(void* agent);
    void self_awareness_plasticity_bridge_set_health_agent(void* agent);
    void self_awareness_snn_bridge_set_health_agent(void* agent);
    void self_awareness_substrate_bridge_set_health_agent(void* agent);
    void self_awareness_thalamic_bridge_set_health_agent(void* agent);
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kB24BridgeModules[] = {
    {"predictive_fep_bridge",               predictive_fep_bridge_set_health_agent},
    {"predictive_plasticity_bridge",        predictive_plasticity_bridge_set_health_agent},
    {"predictive_snn_bridge",               predictive_snn_bridge_set_health_agent},
    {"predictive_substrate_bridge",         predictive_substrate_bridge_set_health_agent},
    {"predictive_thalamic_bridge",          predictive_thalamic_bridge_set_health_agent},
    {"personality_plasticity_bridge",       personality_plasticity_bridge_set_health_agent},
    {"personality_snn_bridge",              personality_snn_bridge_set_health_agent},
    {"personality_substrate_bridge",        personality_substrate_bridge_set_health_agent},
    {"personality_thalamic_bridge",         personality_thalamic_bridge_set_health_agent},
    {"self_awareness_plasticity_bridge",    self_awareness_plasticity_bridge_set_health_agent},
    {"self_awareness_snn_bridge",           self_awareness_snn_bridge_set_health_agent},
    {"self_awareness_substrate_bridge",     self_awareness_substrate_bridge_set_health_agent},
    {"self_awareness_thalamic_bridge",      self_awareness_thalamic_bridge_set_health_agent},
};

static constexpr size_t kNumModules = 13;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class B24BridgesIntegrationTest : public ::testing::Test {
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
            kB24BridgeModules[i].setter(nullptr);
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

TEST_F(B24BridgesIntegrationTest, ConnectAll13Modules) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kB24BridgeModules[i].name);
        kB24BridgeModules[i].setter(agent_);
    }
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kB24BridgeModules[i].name, 0);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesIntegrationTest, SharedAgentAcrossAllModules) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);
    for (int j = 0; j < 5; j++) {
        for (size_t i = 0; i < kNumModules; i++)
            nimcp_health_agent_heartbeat_ex(agent_, kB24BridgeModules[i].name, 0);
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules * 5);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesIntegrationTest, ConcurrentModuleSetters) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kB24BridgeModules[i].setter(agent_);
            for (int j = 0; j < 10; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kB24BridgeModules[i].name, 0);
            kB24BridgeModules[i].setter(nullptr);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesIntegrationTest, CrossBridgePredictive) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);

    /* Create predictive plasticity + snn bridges, operate, destroy */
    predictive_plasticity_config_t pp_cfg = predictive_plasticity_config_default();
    predictive_plasticity_bridge_t* pp = predictive_plasticity_create(&pp_cfg);
    ASSERT_NE(nullptr, pp);

    predictive_snn_config_t ps_cfg = predictive_snn_config_default();
    predictive_snn_bridge_t* ps = predictive_snn_create(&ps_cfg);
    ASSERT_NE(nullptr, ps);

    predictive_plasticity_reset(pp);
    predictive_snn_reset(ps);

    nimcp_health_agent_heartbeat_ex(agent_, "B24_cross_predictive", 1.0f);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);

    predictive_snn_destroy(ps);
    predictive_plasticity_destroy(pp);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesIntegrationTest, CrossBridgePersonality) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);

    personality_plasticity_config_t pp_cfg = personality_plasticity_config_default();
    personality_plasticity_bridge_t* pp = personality_plasticity_create(&pp_cfg);
    ASSERT_NE(nullptr, pp);

    personality_snn_config_t ps_cfg = personality_snn_config_default();
    personality_snn_bridge_t* ps = personality_snn_create(&ps_cfg);
    ASSERT_NE(nullptr, ps);

    personality_plasticity_reset(pp);
    personality_snn_reset(ps);

    nimcp_health_agent_heartbeat_ex(agent_, "B24_cross_personality", 1.0f);

    personality_snn_destroy(ps);
    personality_plasticity_destroy(pp);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesIntegrationTest, CrossBridgeSelfAwareness) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);

    self_awareness_plasticity_config_t sp_cfg = self_awareness_plasticity_config_default();
    self_awareness_plasticity_bridge_t* sp = self_awareness_plasticity_create(&sp_cfg);
    ASSERT_NE(nullptr, sp);

    self_awareness_snn_config_t ss_cfg = self_awareness_snn_config_default();
    self_awareness_snn_bridge_t* ss = self_awareness_snn_create(&ss_cfg);
    ASSERT_NE(nullptr, ss);

    self_awareness_plasticity_reset(sp);
    self_awareness_snn_reset(ss);

    nimcp_health_agent_heartbeat_ex(agent_, "B24_cross_self_awareness", 1.0f);

    self_awareness_snn_destroy(ss);
    self_awareness_plasticity_destroy(sp);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesIntegrationTest, CrossDomainPredictivePersonalitySelfAwareness) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);

    /* Create bridges from all 3 domains */
    predictive_plasticity_config_t pr_cfg = predictive_plasticity_config_default();
    predictive_plasticity_bridge_t* pr = predictive_plasticity_create(&pr_cfg);
    ASSERT_NE(nullptr, pr);

    personality_plasticity_config_t pe_cfg = personality_plasticity_config_default();
    personality_plasticity_bridge_t* pe = personality_plasticity_create(&pe_cfg);
    ASSERT_NE(nullptr, pe);

    self_awareness_plasticity_config_t sa_cfg = self_awareness_plasticity_config_default();
    self_awareness_plasticity_bridge_t* sa = self_awareness_plasticity_create(&sa_cfg);
    ASSERT_NE(nullptr, sa);

    /* Operate all */
    predictive_plasticity_reset(pr);
    personality_plasticity_reset(pe);
    self_awareness_plasticity_reset(sa);

    nimcp_health_agent_heartbeat_ex(agent_, "B24_cross_domain", 1.0f);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);

    self_awareness_plasticity_destroy(sa);
    personality_plasticity_destroy(pe);
    predictive_plasticity_destroy(pr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesIntegrationTest, TwoAgentsSequentialHandoff) {
    nimcp_health_agent_start(agent_);
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    nimcp_health_agent_start(agent2);

    /* Connect to agent 1 */
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kB24BridgeModules[i].name, 0);

    /* Hand off to agent 2 */
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent2);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent2, kB24BridgeModules[i].name, 0);

    health_agent_stats_t stats1, stats2;
    nimcp_health_agent_get_stats(agent_, &stats1);
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats1.heartbeats_received, kNumModules);
    EXPECT_GE(stats2.heartbeats_received, kNumModules);

    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesIntegrationTest, DisconnectReconnectCycles) {
    nimcp_health_agent_start(agent_);
    for (int cycle = 0; cycle < 3; cycle++) {
        for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);
        for (size_t i = 0; i < kNumModules; i++)
            nimcp_health_agent_heartbeat_ex(agent_, kB24BridgeModules[i].name, 0);
        for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(nullptr);
    }
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesIntegrationTest, PartialConnectionSubset) {
    nimcp_health_agent_start(agent_);
    /* Connect only first 5 modules (predictive domain) */
    for (size_t i = 0; i < 5; i++) kB24BridgeModules[i].setter(agent_);
    for (size_t i = 0; i < 5; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kB24BridgeModules[i].name, 0);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 5u);
    for (size_t i = 0; i < 5; i++) kB24BridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesIntegrationTest, InterleavedOperations) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);

    /* Create a predictive FEP bridge and operate while heartbeating */
    predictive_fep_config_t cfg;
    predictive_fep_bridge_default_config(&cfg);
    predictive_fep_bridge_t* bridge = predictive_fep_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    for (int j = 0; j < 10; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B24_interleaved", (float)j / 10.0f);

    predictive_fep_bridge_destroy(bridge);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesIntegrationTest, AllFepBridgesCoexist) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);

    predictive_fep_config_t cfg;
    predictive_fep_bridge_default_config(&cfg);
    predictive_fep_bridge_t* fep = predictive_fep_bridge_create(&cfg);
    ASSERT_NE(nullptr, fep);

    predictive_plasticity_config_t pp_cfg = predictive_plasticity_config_default();
    predictive_plasticity_bridge_t* pp = predictive_plasticity_create(&pp_cfg);
    ASSERT_NE(nullptr, pp);

    nimcp_health_agent_heartbeat_ex(agent_, "B24_coexist", 0.5f);

    predictive_plasticity_destroy(pp);
    predictive_fep_bridge_destroy(fep);
    nimcp_health_agent_stop(agent_);
}
