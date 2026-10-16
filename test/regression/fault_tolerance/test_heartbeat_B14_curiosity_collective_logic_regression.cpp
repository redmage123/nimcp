/**
 * @file test_heartbeat_B14_curiosity_collective_logic_regression.cpp
 * @brief Regression tests for B14 heartbeat
 *        (cognitive/curiosity + cognitive/collective_cognition + cognitive/logic)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

void curiosity_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_enhanced_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_fractal_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_hyperbolic_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void collective_cognition_set_health_agent(nimcp_health_agent_t* agent);
void collective_cognition_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void collective_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void collective_phi_set_health_agent(nimcp_health_agent_t* agent);
void collective_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void collective_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void extended_mind_set_health_agent(nimcp_health_agent_t* agent);
void hyperscanning_set_health_agent(nimcp_health_agent_t* agent);
void shared_intentionality_set_health_agent(nimcp_health_agent_t* agent);
void audio_logic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void logic_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void logic_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void logic_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_logic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_set_health_agent(nimcp_health_agent_t* agent);
void visual_logic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void somatosensory_logic_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);
struct B14Module { const char* name; set_health_agent_fn set_fn; };

static const B14Module B14_MODULES[] = {
    {"curiosity",                          curiosity_set_health_agent},
    {"curiosity_enhanced",                 curiosity_enhanced_set_health_agent},
    {"curiosity_fep_bridge",               curiosity_fep_bridge_set_health_agent},
    {"curiosity_fractal",                  curiosity_fractal_set_health_agent},
    {"curiosity_hyperbolic",               curiosity_hyperbolic_set_health_agent},
    {"curiosity_plasticity_bridge",        curiosity_plasticity_bridge_set_health_agent},
    {"curiosity_sleep_bridge",             curiosity_sleep_bridge_set_health_agent},
    {"curiosity_snn_bridge",               curiosity_snn_bridge_set_health_agent},
    {"curiosity_substrate_bridge",         curiosity_substrate_bridge_set_health_agent},
    {"curiosity_thalamic_bridge",          curiosity_thalamic_bridge_set_health_agent},
    {"collective_cognition",               collective_cognition_set_health_agent},
    {"collective_cognition_immune_bridge", collective_cognition_immune_bridge_set_health_agent},
    {"collective_fep_bridge",              collective_fep_bridge_set_health_agent},
    {"collective_phi",                     collective_phi_set_health_agent},
    {"collective_plasticity_bridge",       collective_plasticity_bridge_set_health_agent},
    {"collective_snn_bridge",              collective_snn_bridge_set_health_agent},
    {"extended_mind",                      extended_mind_set_health_agent},
    {"hyperscanning",                      hyperscanning_set_health_agent},
    {"shared_intentionality",              shared_intentionality_set_health_agent},
    {"audio_logic_bridge",                 audio_logic_bridge_set_health_agent},
    {"logic_sleep_bridge",                 logic_sleep_bridge_set_health_agent},
    {"logic_substrate_bridge",             logic_substrate_bridge_set_health_agent},
    {"logic_thalamic_bridge",              logic_thalamic_bridge_set_health_agent},
    {"omni_logic_bridge",                  omni_logic_bridge_set_health_agent},
    {"symbolic_logic",                     symbolic_logic_set_health_agent},
    {"visual_logic_bridge",                visual_logic_bridge_set_health_agent},
    {"somatosensory_logic_bridge",         somatosensory_logic_bridge_set_health_agent},
};
static constexpr size_t B14_MODULE_COUNT = sizeof(B14_MODULES) / sizeof(B14_MODULES[0]);

class HeartbeatB14RegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent_ = nullptr;
    void SetUp() override {
        health_agent_config_t cfg;
        nimcp_health_agent_default_config(&cfg);
        cfg.check_interval_ms = 50;
        cfg.enable_auto_recovery = false;
        agent_ = nimcp_health_agent_create(&cfg);
        ASSERT_NE(agent_, nullptr);
    }
    void TearDown() override {
        for (size_t i = 0; i < B14_MODULE_COUNT; i++) B14_MODULES[i].set_fn(nullptr);
        if (agent_) { nimcp_health_agent_destroy(agent_); agent_ = nullptr; }
    }
};

TEST_F(HeartbeatB14RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) {
        SCOPED_TRACE(B14_MODULES[i].name);
        B14_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB14RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) {
        SCOPED_TRACE(B14_MODULES[i].name);
        B14_MODULES[i].set_fn(agent_);
        B14_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB14RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) {
        SCOPED_TRACE(B14_MODULES[i].name);
        for (int j = 0; j < 10; j++) B14_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB14RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) {
        SCOPED_TRACE(B14_MODULES[i].name);
        B14_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB14RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) {
        SCOPED_TRACE(B14_MODULES[i].name);
        B14_MODULES[i].set_fn(nullptr);
        B14_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB14RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            B14_MODULES[i].set_fn(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, B14_MODULES[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB14RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) B14_MODULES[i].set_fn(agent_);
    std::thread heartbeat_thread([this]() {
        for (int j = 0; j < 100; j++)
            nimcp_health_agent_heartbeat_ex(agent_, "B14_regression", 0);
    });
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) B14_MODULES[i].set_fn(nullptr);
    heartbeat_thread.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB14RegressionTest, RapidSetClearCycleAllModules) {
    for (int cycle = 0; cycle < 50; cycle++) {
        for (size_t i = 0; i < B14_MODULE_COUNT; i++) B14_MODULES[i].set_fn(agent_);
        for (size_t i = 0; i < B14_MODULE_COUNT; i++) B14_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB14RegressionTest, RapidSetClearCycleSingleModule) {
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) {
        SCOPED_TRACE(B14_MODULES[i].name);
        for (int cycle = 0; cycle < 100; cycle++) {
            B14_MODULES[i].set_fn(agent_);
            B14_MODULES[i].set_fn(nullptr);
        }
    }
}

TEST_F(HeartbeatB14RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int cycle = 0; cycle < 5; cycle++) {
        health_agent_config_t cfg_temp;
        nimcp_health_agent_default_config(&cfg_temp);
        nimcp_health_agent_t* temp = nimcp_health_agent_create(&cfg_temp);
        ASSERT_NE(temp, nullptr);
        for (size_t i = 0; i < B14_MODULE_COUNT; i++) B14_MODULES[i].set_fn(temp);
        for (size_t i = 0; i < B14_MODULE_COUNT; i++) B14_MODULES[i].set_fn(nullptr);
        nimcp_health_agent_destroy(temp);
    }
}

TEST_F(HeartbeatB14RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) B14_MODULES[i].set_fn(agent_);
    for (int cycle = 0; cycle < 5; cycle++) {
        nimcp_health_agent_start(agent_);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        nimcp_health_agent_stop(agent_);
    }
}

TEST_F(HeartbeatB14RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) {
        SCOPED_TRACE(B14_MODULES[i].name);
        EXPECT_NE(B14_MODULES[i].set_fn, nullptr);
        B14_MODULES[i].set_fn(agent_);
        B14_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB14RegressionTest, StatsConsistentAfterModuleSetClear) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B14_MODULE_COUNT; i++) B14_MODULES[i].set_fn(agent_);
    health_agent_stats_t stats1;
    nimcp_health_agent_get_stats(agent_, &stats1);
    nimcp_health_agent_heartbeat_ex(agent_, "B14_consistency", 0);
    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent_, &stats2);
    EXPECT_GE(stats2.heartbeats_received, stats1.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}
