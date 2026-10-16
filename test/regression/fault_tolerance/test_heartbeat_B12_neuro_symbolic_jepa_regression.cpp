/**
 * @file test_heartbeat_B12_neuro_symbolic_jepa_regression.cpp
 * @brief Regression tests for B12 heartbeat (cognitive/neuro_symbolic + cognitive/jepa)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

void energy_consistency_set_health_agent(nimcp_health_agent_t* agent);
void evolutionary_proof_set_health_agent(nimcp_health_agent_t* agent);
void genius_math_orchestrator_set_health_agent(nimcp_health_agent_t* agent);
void hypergraph_set_health_agent(nimcp_health_agent_t* agent);
void quantum_math_engine_set_health_agent(nimcp_health_agent_t* agent);
void quantum_mcts_set_health_agent(nimcp_health_agent_t* agent);
void jepa_bidirectional_set_health_agent(nimcp_health_agent_t* agent);
void jepa_context_set_health_agent(nimcp_health_agent_t* agent);
void jepa_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void jepa_latent_set_health_agent(nimcp_health_agent_t* agent);
void jepa_masking_set_health_agent(nimcp_health_agent_t* agent);
void jepa_multimodal_set_health_agent(nimcp_health_agent_t* agent);
void jepa_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void jepa_predictor_set_health_agent(nimcp_health_agent_t* agent);
void jepa_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void jepa_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void jepa_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void jepa_weights_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);
struct B12Module { const char* name; set_health_agent_fn set_fn; };

static const B12Module B12_MODULES[] = {
    {"energy_consistency",        energy_consistency_set_health_agent},
    {"evolutionary_proof",        evolutionary_proof_set_health_agent},
    {"genius_math_orchestrator",  genius_math_orchestrator_set_health_agent},
    {"hypergraph",                hypergraph_set_health_agent},
    {"quantum_math_engine",       quantum_math_engine_set_health_agent},
    {"quantum_mcts",              quantum_mcts_set_health_agent},
    {"jepa_bidirectional",        jepa_bidirectional_set_health_agent},
    {"jepa_context",              jepa_context_set_health_agent},
    {"jepa_fep_bridge",           jepa_fep_bridge_set_health_agent},
    {"jepa_latent",               jepa_latent_set_health_agent},
    {"jepa_masking",              jepa_masking_set_health_agent},
    {"jepa_multimodal",           jepa_multimodal_set_health_agent},
    {"jepa_plasticity_bridge",    jepa_plasticity_bridge_set_health_agent},
    {"jepa_predictor",            jepa_predictor_set_health_agent},
    {"jepa_snn_bridge",           jepa_snn_bridge_set_health_agent},
    {"jepa_substrate_bridge",     jepa_substrate_bridge_set_health_agent},
    {"jepa_thalamic_bridge",      jepa_thalamic_bridge_set_health_agent},
    {"jepa_weights",              jepa_weights_set_health_agent},
};
static constexpr size_t B12_MODULE_COUNT = sizeof(B12_MODULES) / sizeof(B12_MODULES[0]);

class HeartbeatB12RegressionTest : public ::testing::Test {
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
        for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(nullptr);
        if (agent_) { nimcp_health_agent_destroy(agent_); agent_ = nullptr; }
    }
};

TEST_F(HeartbeatB12RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        B12_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB12RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        B12_MODULES[i].set_fn(agent_);
        B12_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB12RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        for (int j = 0; j < 10; j++) B12_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB12RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        B12_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB12RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        B12_MODULES[i].set_fn(nullptr);
        B12_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB12RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            B12_MODULES[i].set_fn(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, B12_MODULES[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB12RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(agent_);
    std::thread heartbeat_thread([this]() {
        for (int j = 0; j < 100; j++)
            nimcp_health_agent_heartbeat_ex(agent_, "B12_regression", 0);
    });
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(nullptr);
    heartbeat_thread.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB12RegressionTest, RapidSetClearCycleAllModules) {
    for (int cycle = 0; cycle < 50; cycle++) {
        for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(agent_);
        for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB12RegressionTest, RapidSetClearCycleSingleModule) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        for (int cycle = 0; cycle < 100; cycle++) {
            B12_MODULES[i].set_fn(agent_);
            B12_MODULES[i].set_fn(nullptr);
        }
    }
}

TEST_F(HeartbeatB12RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int cycle = 0; cycle < 5; cycle++) {
        health_agent_config_t cfg_temp;
        nimcp_health_agent_default_config(&cfg_temp);
        nimcp_health_agent_t* temp = nimcp_health_agent_create(&cfg_temp);
        ASSERT_NE(temp, nullptr);
        for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(temp);
        for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(nullptr);
        nimcp_health_agent_destroy(temp);
    }
}

TEST_F(HeartbeatB12RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(agent_);
    for (int cycle = 0; cycle < 5; cycle++) {
        nimcp_health_agent_start(agent_);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        nimcp_health_agent_stop(agent_);
    }
}

TEST_F(HeartbeatB12RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        EXPECT_NE(B12_MODULES[i].set_fn, nullptr);
        B12_MODULES[i].set_fn(agent_);
        B12_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB12RegressionTest, StatsConsistentAfterModuleSetClear) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(agent_);
    health_agent_stats_t stats1;
    nimcp_health_agent_get_stats(agent_, &stats1);
    nimcp_health_agent_heartbeat_ex(agent_, "B12_consistency", 0);
    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent_, &stats2);
    EXPECT_GE(stats2.heartbeats_received, stats1.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}
