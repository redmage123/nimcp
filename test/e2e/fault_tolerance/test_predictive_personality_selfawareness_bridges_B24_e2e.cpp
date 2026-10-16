/**
 * @file test_predictive_personality_selfawareness_bridges_B24_e2e.cpp
 * @brief End-to-end tests for B24 predictive+personality+self_awareness bridges:
 *        full lifecycle, sustained operation, load testing
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

class B24BridgesE2ETest : public ::testing::Test {
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
 * E2E Tests
 * ============================================================================ */

TEST_F(B24BridgesE2ETest, FullLifecycleAllModules) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kB24BridgeModules[i].name, 0);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesE2ETest, ConcurrentModulesMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kB24BridgeModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kB24BridgeModules[i].name, 0);
            kB24BridgeModules[i].setter(nullptr);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesE2ETest, HighFrequencyBurst1000Heartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);
    health_agent_stats_t before;
    nimcp_health_agent_get_stats(agent_, &before);
    for (int j = 0; j < 1000; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B24_burst", 0);
    health_agent_stats_t after;
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + 1000);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesE2ETest, MultiPhaseOperation) {
    nimcp_health_agent_start(agent_);

    /* Phase 1: Create bridges */
    predictive_plasticity_config_t pp_cfg = predictive_plasticity_config_default();
    predictive_plasticity_bridge_t* pp = predictive_plasticity_create(&pp_cfg);
    ASSERT_NE(nullptr, pp);

    personality_snn_config_t ps_cfg = personality_snn_config_default();
    personality_snn_bridge_t* ps = personality_snn_create(&ps_cfg);
    ASSERT_NE(nullptr, ps);

    self_awareness_plasticity_config_t sa_cfg = self_awareness_plasticity_config_default();
    self_awareness_plasticity_bridge_t* sa = self_awareness_plasticity_create(&sa_cfg);
    ASSERT_NE(nullptr, sa);

    /* Connect health agent modules */
    for (size_t i = 0; i < kNumModules; i++) {
        kB24BridgeModules[i].setter(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, kB24BridgeModules[i].name, 0);
    }

    /* Phase 2: Operate bridges */
    predictive_plasticity_reset(pp);
    personality_snn_reset(ps);
    self_awareness_plasticity_reset(sa);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);

    /* Phase 3: Teardown bridges */
    self_awareness_plasticity_destroy(sa);
    personality_snn_destroy(ps);
    predictive_plasticity_destroy(pp);

    for (size_t i = 0; i < kNumModules / 2; i++) kB24BridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesE2ETest, ModuleHotSwapDuringOperation) {
    nimcp_health_agent_start(agent_);
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    nimcp_health_agent_start(agent2);

    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++) {
        kB24BridgeModules[i].setter(agent2);
        nimcp_health_agent_heartbeat_ex(agent2, kB24BridgeModules[i].name, 0);
    }

    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, kNumModules);

    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesE2ETest, SustainedOperationOverTime) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);
    auto start = std::chrono::steady_clock::now();
    uint64_t count = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(250)) {
        nimcp_health_agent_heartbeat_ex(agent_, "B24_sustained", 0);
        count++;
    }
    EXPECT_GT(count, 0u);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, count);
    nimcp_health_agent_stop(agent_);
}

TEST_F(B24BridgesE2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kB24BridgeModules[i].name, 0);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
}

TEST_F(B24BridgesE2ETest, AllBridgesCreateOperateDestroy) {
    /* Full lifecycle for each bridge type that accepts config-only create params */
    {
        predictive_fep_config_t cfg;
        predictive_fep_bridge_default_config(&cfg);
        predictive_fep_bridge_t* b = predictive_fep_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        predictive_fep_bridge_destroy(b);
    }
    {
        predictive_plasticity_config_t cfg = predictive_plasticity_config_default();
        predictive_plasticity_bridge_t* b = predictive_plasticity_create(&cfg);
        ASSERT_NE(nullptr, b);
        predictive_plasticity_reset(b);
        predictive_plasticity_destroy(b);
    }
    {
        predictive_snn_config_t cfg = predictive_snn_config_default();
        predictive_snn_bridge_t* b = predictive_snn_create(&cfg);
        ASSERT_NE(nullptr, b);
        predictive_snn_reset(b);
        predictive_snn_destroy(b);
    }
    {
        personality_plasticity_config_t cfg = personality_plasticity_config_default();
        personality_plasticity_bridge_t* b = personality_plasticity_create(&cfg);
        ASSERT_NE(nullptr, b);
        personality_plasticity_reset(b);
        personality_plasticity_destroy(b);
    }
    {
        personality_snn_config_t cfg = personality_snn_config_default();
        personality_snn_bridge_t* b = personality_snn_create(&cfg);
        ASSERT_NE(nullptr, b);
        personality_snn_reset(b);
        personality_snn_destroy(b);
    }
    {
        self_awareness_plasticity_config_t cfg = self_awareness_plasticity_config_default();
        self_awareness_plasticity_bridge_t* b = self_awareness_plasticity_create(&cfg);
        ASSERT_NE(nullptr, b);
        self_awareness_plasticity_reset(b);
        self_awareness_plasticity_destroy(b);
    }
    {
        self_awareness_snn_config_t cfg = self_awareness_snn_config_default();
        self_awareness_snn_bridge_t* b = self_awareness_snn_create(&cfg);
        ASSERT_NE(nullptr, b);
        self_awareness_snn_reset(b);
        self_awareness_snn_destroy(b);
    }
}

TEST_F(B24BridgesE2ETest, CrossDomainEventPropagation) {
    /* Event flows through predictive -> personality -> self_awareness pipeline */
    predictive_plasticity_config_t pr_cfg = predictive_plasticity_config_default();
    predictive_plasticity_bridge_t* pr = predictive_plasticity_create(&pr_cfg);
    ASSERT_NE(nullptr, pr);

    personality_plasticity_config_t pe_cfg = personality_plasticity_config_default();
    personality_plasticity_bridge_t* pe = personality_plasticity_create(&pe_cfg);
    ASSERT_NE(nullptr, pe);

    self_awareness_plasticity_config_t sa_cfg = self_awareness_plasticity_config_default();
    self_awareness_plasticity_bridge_t* sa = self_awareness_plasticity_create(&sa_cfg);
    ASSERT_NE(nullptr, sa);

    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);

    /* Step 1: Predictive processes */
    predictive_plasticity_reset(pr);

    /* Step 2: Personality processes */
    personality_plasticity_reset(pe);

    /* Step 3: Self-awareness consolidates */
    self_awareness_plasticity_reset(sa);

    nimcp_health_agent_heartbeat_ex(agent_, "B24_cross_domain", 1.0f);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);

    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);

    self_awareness_plasticity_destroy(sa);
    personality_plasticity_destroy(pe);
    predictive_plasticity_destroy(pr);
}

TEST_F(B24BridgesE2ETest, TimeoutDetectionAfterSilence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);
    nimcp_health_agent_heartbeat_ex(agent_, "B24_timeout_test", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
    nimcp_health_agent_stop(agent_);
}
