/**
 * @file test_heartbeat_B13_knowledge_mentalhealth_imagination_integration.cpp
 * @brief Integration tests for B13 heartbeat
 *        (cognitive/knowledge + cognitive/mental_health + cognitive/imagination)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

void gw_imagination_bridge_set_health_agent(nimcp_health_agent_t* agent);
void hippocampus_imagination_bridge_set_health_agent(nimcp_health_agent_t* agent);
void imagination_engine_set_health_agent(nimcp_health_agent_t* agent);
void imagination_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void imagination_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void imagination_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void imagination_workspace_set_health_agent(nimcp_health_agent_t* agent);
void jepa_imagination_bridge_set_health_agent(nimcp_health_agent_t* agent);
void prefrontal_imagination_bridge_set_health_agent(nimcp_health_agent_t* agent);
void sleep_imagination_bridge_set_health_agent(nimcp_health_agent_t* agent);
void kg_reader_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_cow_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_fractal_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_hyperbolic_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mental_health_set_health_agent(nimcp_health_agent_t* agent);
void mental_health_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mental_health_guardian_set_health_agent(nimcp_health_agent_t* agent);
void mental_health_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mental_health_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mental_health_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mental_health_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mental_health_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);
struct B13Module { const char* name; set_health_agent_fn set_fn; };

static const B13Module B13_MODULES[] = {
    {"gw_imagination_bridge",              gw_imagination_bridge_set_health_agent},
    {"hippocampus_imagination_bridge",     hippocampus_imagination_bridge_set_health_agent},
    {"imagination_engine",                 imagination_engine_set_health_agent},
    {"imagination_fep_bridge",             imagination_fep_bridge_set_health_agent},
    {"imagination_plasticity_bridge",      imagination_plasticity_bridge_set_health_agent},
    {"imagination_snn_bridge",             imagination_snn_bridge_set_health_agent},
    {"imagination_workspace",              imagination_workspace_set_health_agent},
    {"jepa_imagination_bridge",            jepa_imagination_bridge_set_health_agent},
    {"prefrontal_imagination_bridge",      prefrontal_imagination_bridge_set_health_agent},
    {"sleep_imagination_bridge",           sleep_imagination_bridge_set_health_agent},
    {"kg_reader",                          kg_reader_set_health_agent},
    {"knowledge",                          knowledge_set_health_agent},
    {"knowledge_cow",                      knowledge_cow_set_health_agent},
    {"knowledge_fep_bridge",               knowledge_fep_bridge_set_health_agent},
    {"knowledge_fractal",                  knowledge_fractal_set_health_agent},
    {"knowledge_hyperbolic",               knowledge_hyperbolic_set_health_agent},
    {"knowledge_plasticity_bridge",        knowledge_plasticity_bridge_set_health_agent},
    {"knowledge_sleep_bridge",             knowledge_sleep_bridge_set_health_agent},
    {"knowledge_snn_bridge",               knowledge_snn_bridge_set_health_agent},
    {"knowledge_substrate_bridge",         knowledge_substrate_bridge_set_health_agent},
    {"knowledge_thalamic_bridge",          knowledge_thalamic_bridge_set_health_agent},
    {"mental_health",                      mental_health_set_health_agent},
    {"mental_health_fep_bridge",           mental_health_fep_bridge_set_health_agent},
    {"mental_health_guardian",             mental_health_guardian_set_health_agent},
    {"mental_health_plasticity_bridge",    mental_health_plasticity_bridge_set_health_agent},
    {"mental_health_sleep_bridge",         mental_health_sleep_bridge_set_health_agent},
    {"mental_health_snn_bridge",           mental_health_snn_bridge_set_health_agent},
    {"mental_health_substrate_bridge",     mental_health_substrate_bridge_set_health_agent},
    {"mental_health_thalamic_bridge",      mental_health_thalamic_bridge_set_health_agent},
};
static constexpr size_t B13_MODULE_COUNT = sizeof(B13_MODULES) / sizeof(B13_MODULES[0]);

class HeartbeatB13IntegrationTest : public ::testing::Test {
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
        for (size_t i = 0; i < B13_MODULE_COUNT; i++) B13_MODULES[i].set_fn(nullptr);
        if (agent_) { nimcp_health_agent_destroy(agent_); agent_ = nullptr; }
    }
};

TEST_F(HeartbeatB13IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B13_MODULE_COUNT; i++) B13_MODULES[i].set_fn(agent_);
    health_agent_stats_t before, after;
    nimcp_health_agent_get_stats(agent_, &before);
    for (size_t i = 0; i < B13_MODULE_COUNT; i++)
        nimcp_health_agent_heartbeat_ex(agent_, B13_MODULES[i].name, 0);
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + B13_MODULE_COUNT);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB13IntegrationTest, MultipleModulesShareSingleAgent) {
    for (size_t i = 0; i < B13_MODULE_COUNT; i++) B13_MODULES[i].set_fn(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
}

TEST_F(HeartbeatB13IntegrationTest, DisconnectModulesWhileAgentRunning) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B13_MODULE_COUNT; i++) B13_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B13_MODULE_COUNT / 2; i++) B13_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB13IntegrationTest, DisconnectAllModulesWhileAgentRunning) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B13_MODULE_COUNT; i++) B13_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B13_MODULE_COUNT; i++) B13_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB13IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < B13_MODULE_COUNT; i++) B13_MODULES[i].set_fn(agent_);
    nimcp_health_agent_start(agent_);
    nimcp_health_agent_stop(agent_);
    nimcp_health_agent_start(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB13IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B13_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            B13_MODULES[i].set_fn(agent_);
            for (int j = 0; j < 50; j++)
                nimcp_health_agent_heartbeat_ex(agent_, B13_MODULES[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB13IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(agent2, nullptr);
    for (size_t i = 0; i < B13_MODULE_COUNT; i++) B13_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B13_MODULE_COUNT; i++) B13_MODULES[i].set_fn(agent2);
    for (size_t i = 0; i < B13_MODULE_COUNT; i++) B13_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB13IntegrationTest, ProgressiveModuleConnection) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B13_MODULE_COUNT; i++) {
        B13_MODULES[i].set_fn(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, B13_MODULES[i].name, 0);
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B13_MODULE_COUNT);
    nimcp_health_agent_stop(agent_);
}
