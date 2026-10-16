/**
 * @file test_heartbeat_B11_wellbeing_introspection_regression.cpp
 * @brief Regression tests for B11 heartbeat (cognitive/wellbeing + cognitive/introspection)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

void connectivity_health_set_health_agent(nimcp_health_agent_t* agent);
void consciousness_metrics_set_health_agent(nimcp_health_agent_t* agent);
void ensemble_uncertainty_set_health_agent(nimcp_health_agent_t* agent);
void ensemble_uncertainty_pink_noise_bridge_set_health_agent(nimcp_health_agent_t* agent);
void introspection_set_health_agent(nimcp_health_agent_t* agent);
void introspection_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void introspection_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void introspection_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void introspection_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void introspection_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void temporal_patterns_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_enhanced_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_eudaimonic_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_free_energy_bridge_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_prediction_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_resources_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);
struct B11Module { const char* name; set_health_agent_fn set_fn; };

static const B11Module B11_MODULES[] = {
    {"connectivity_health",                    connectivity_health_set_health_agent},
    {"consciousness_metrics",                  consciousness_metrics_set_health_agent},
    {"ensemble_uncertainty",                   ensemble_uncertainty_set_health_agent},
    {"ensemble_uncertainty_pink_noise_bridge",  ensemble_uncertainty_pink_noise_bridge_set_health_agent},
    {"introspection",                          introspection_set_health_agent},
    {"introspection_fep_bridge",               introspection_fep_bridge_set_health_agent},
    {"introspection_plasticity_bridge",        introspection_plasticity_bridge_set_health_agent},
    {"introspection_snn_bridge",               introspection_snn_bridge_set_health_agent},
    {"introspection_substrate_bridge",         introspection_substrate_bridge_set_health_agent},
    {"introspection_thalamic_bridge",          introspection_thalamic_bridge_set_health_agent},
    {"temporal_patterns",                      temporal_patterns_set_health_agent},
    {"wellbeing",                              wellbeing_set_health_agent},
    {"wellbeing_enhanced",                     wellbeing_enhanced_set_health_agent},
    {"wellbeing_eudaimonic",                   wellbeing_eudaimonic_set_health_agent},
    {"wellbeing_fep_bridge",                   wellbeing_fep_bridge_set_health_agent},
    {"wellbeing_free_energy_bridge",           wellbeing_free_energy_bridge_set_health_agent},
    {"wellbeing_plasticity_bridge",            wellbeing_plasticity_bridge_set_health_agent},
    {"wellbeing_prediction",                   wellbeing_prediction_set_health_agent},
    {"wellbeing_resources",                    wellbeing_resources_set_health_agent},
    {"wellbeing_snn_bridge",                   wellbeing_snn_bridge_set_health_agent},
    {"wellbeing_substrate_bridge",             wellbeing_substrate_bridge_set_health_agent},
    {"wellbeing_thalamic_bridge",              wellbeing_thalamic_bridge_set_health_agent},
};
static constexpr size_t B11_MODULE_COUNT = sizeof(B11_MODULES) / sizeof(B11_MODULES[0]);

class HeartbeatB11RegressionTest : public ::testing::Test {
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
        for (size_t i = 0; i < B11_MODULE_COUNT; i++) B11_MODULES[i].set_fn(nullptr);
        if (agent_) { nimcp_health_agent_destroy(agent_); agent_ = nullptr; }
    }
};

TEST_F(HeartbeatB11RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) {
        SCOPED_TRACE(B11_MODULES[i].name);
        B11_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB11RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) {
        SCOPED_TRACE(B11_MODULES[i].name);
        B11_MODULES[i].set_fn(agent_);
        B11_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB11RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) {
        SCOPED_TRACE(B11_MODULES[i].name);
        for (int j = 0; j < 10; j++) B11_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB11RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) {
        SCOPED_TRACE(B11_MODULES[i].name);
        B11_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB11RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) {
        SCOPED_TRACE(B11_MODULES[i].name);
        B11_MODULES[i].set_fn(nullptr);
        B11_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB11RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            B11_MODULES[i].set_fn(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, B11_MODULES[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB11RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) B11_MODULES[i].set_fn(agent_);
    std::thread heartbeat_thread([this]() {
        for (int j = 0; j < 100; j++)
            nimcp_health_agent_heartbeat_ex(agent_, "B11_regression", 0);
    });
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) B11_MODULES[i].set_fn(nullptr);
    heartbeat_thread.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB11RegressionTest, RapidSetClearCycleAllModules) {
    for (int cycle = 0; cycle < 50; cycle++) {
        for (size_t i = 0; i < B11_MODULE_COUNT; i++) B11_MODULES[i].set_fn(agent_);
        for (size_t i = 0; i < B11_MODULE_COUNT; i++) B11_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB11RegressionTest, RapidSetClearCycleSingleModule) {
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) {
        SCOPED_TRACE(B11_MODULES[i].name);
        for (int cycle = 0; cycle < 100; cycle++) {
            B11_MODULES[i].set_fn(agent_);
            B11_MODULES[i].set_fn(nullptr);
        }
    }
}

TEST_F(HeartbeatB11RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int cycle = 0; cycle < 5; cycle++) {
        health_agent_config_t cfg_temp;
        nimcp_health_agent_default_config(&cfg_temp);
        nimcp_health_agent_t* temp = nimcp_health_agent_create(&cfg_temp);
        ASSERT_NE(temp, nullptr);
        for (size_t i = 0; i < B11_MODULE_COUNT; i++) B11_MODULES[i].set_fn(temp);
        for (size_t i = 0; i < B11_MODULE_COUNT; i++) B11_MODULES[i].set_fn(nullptr);
        nimcp_health_agent_destroy(temp);
    }
}

TEST_F(HeartbeatB11RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) B11_MODULES[i].set_fn(agent_);
    for (int cycle = 0; cycle < 5; cycle++) {
        nimcp_health_agent_start(agent_);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        nimcp_health_agent_stop(agent_);
    }
}

TEST_F(HeartbeatB11RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) {
        SCOPED_TRACE(B11_MODULES[i].name);
        EXPECT_NE(B11_MODULES[i].set_fn, nullptr);
        B11_MODULES[i].set_fn(agent_);
        B11_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB11RegressionTest, StatsConsistentAfterModuleSetClear) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B11_MODULE_COUNT; i++) B11_MODULES[i].set_fn(agent_);
    health_agent_stats_t stats1;
    nimcp_health_agent_get_stats(agent_, &stats1);
    nimcp_health_agent_heartbeat_ex(agent_, "B11_consistency", 0);
    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent_, &stats2);
    EXPECT_GE(stats2.heartbeats_received, stats1.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}
