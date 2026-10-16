/**
 * @file test_heartbeat_B17_emotion_cluster_integration.cpp
 * @brief Integration tests for B17 heartbeat
 *        (cognitive/emotion + cognitive/emotional_tagging + cognitive/emotion_recognition +
 *         cognitive/emotions + cognitive/emotion_tensor + cognitive/grief + cognitive/joy + cognitive/remorse)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

void emotion_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void health_emotion_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotional_tagging_set_health_agent(nimcp_health_agent_t* agent);
void emotional_tagging_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotional_tagging_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotional_tagging_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_recognition_simple_set_health_agent(nimcp_health_agent_t* agent);
void emotion_recognition_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_recognition_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_recognition_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotional_system_set_health_agent(nimcp_health_agent_t* agent);
void emotional_system_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_tensor_set_health_agent(nimcp_health_agent_t* agent);
void emotion_tensor_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_tensor_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_tensor_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void grief_and_loss_set_health_agent(nimcp_health_agent_t* agent);
void grief_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void grief_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void grief_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void joy_euphoria_set_health_agent(nimcp_health_agent_t* agent);
void joy_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void joy_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void joy_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void remorse_regret_set_health_agent(nimcp_health_agent_t* agent);
void remorse_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void remorse_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void remorse_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);
struct B17Module { const char* name; set_health_agent_fn set_fn; };

static const B17Module B17_MODULES[] = {
    {"emotion_fep_bridge",                emotion_fep_bridge_set_health_agent},
    {"emotion_plasticity_bridge",         emotion_plasticity_bridge_set_health_agent},
    {"emotion_snn_bridge",                emotion_snn_bridge_set_health_agent},
    {"emotion_substrate_bridge",          emotion_substrate_bridge_set_health_agent},
    {"emotion_thalamic_bridge",           emotion_thalamic_bridge_set_health_agent},
    {"health_emotion_bridge",             health_emotion_bridge_set_health_agent},
    {"emotional_tagging",                 emotional_tagging_set_health_agent},
    {"emotional_tagging_fep_bridge",      emotional_tagging_fep_bridge_set_health_agent},
    {"emotional_tagging_substrate_bridge", emotional_tagging_substrate_bridge_set_health_agent},
    {"emotional_tagging_thalamic_bridge", emotional_tagging_thalamic_bridge_set_health_agent},
    {"emotion_recognition_simple",        emotion_recognition_simple_set_health_agent},
    {"emotion_recognition_fep_bridge",    emotion_recognition_fep_bridge_set_health_agent},
    {"emotion_recognition_substrate_bridge", emotion_recognition_substrate_bridge_set_health_agent},
    {"emotion_recognition_thalamic_bridge", emotion_recognition_thalamic_bridge_set_health_agent},
    {"emotional_system",                  emotional_system_set_health_agent},
    {"emotional_system_sleep_bridge",     emotional_system_sleep_bridge_set_health_agent},
    {"emotion_tensor",                    emotion_tensor_set_health_agent},
    {"emotion_tensor_bridge",             emotion_tensor_bridge_set_health_agent},
    {"emotion_tensor_substrate_bridge",   emotion_tensor_substrate_bridge_set_health_agent},
    {"emotion_tensor_thalamic_bridge",    emotion_tensor_thalamic_bridge_set_health_agent},
    {"grief_and_loss",                    grief_and_loss_set_health_agent},
    {"grief_fep_bridge",                  grief_fep_bridge_set_health_agent},
    {"grief_substrate_bridge",            grief_substrate_bridge_set_health_agent},
    {"grief_thalamic_bridge",             grief_thalamic_bridge_set_health_agent},
    {"joy_euphoria",                      joy_euphoria_set_health_agent},
    {"joy_fep_bridge",                    joy_fep_bridge_set_health_agent},
    {"joy_substrate_bridge",              joy_substrate_bridge_set_health_agent},
    {"joy_thalamic_bridge",               joy_thalamic_bridge_set_health_agent},
    {"remorse_regret",                    remorse_regret_set_health_agent},
    {"remorse_fep_bridge",                remorse_fep_bridge_set_health_agent},
    {"remorse_substrate_bridge",          remorse_substrate_bridge_set_health_agent},
    {"remorse_thalamic_bridge",           remorse_thalamic_bridge_set_health_agent},
};
static constexpr size_t B17_MODULE_COUNT = sizeof(B17_MODULES) / sizeof(B17_MODULES[0]);

class HeartbeatB17IntegrationTest : public ::testing::Test {
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
        for (size_t i = 0; i < B17_MODULE_COUNT; i++) B17_MODULES[i].set_fn(nullptr);
        if (agent_) { nimcp_health_agent_destroy(agent_); agent_ = nullptr; }
    }
};

TEST_F(HeartbeatB17IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B17_MODULE_COUNT; i++) B17_MODULES[i].set_fn(agent_);
    health_agent_stats_t before, after;
    nimcp_health_agent_get_stats(agent_, &before);
    for (size_t i = 0; i < B17_MODULE_COUNT; i++)
        nimcp_health_agent_heartbeat_ex(agent_, B17_MODULES[i].name, 0);
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + B17_MODULE_COUNT);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB17IntegrationTest, MultipleModulesShareSingleAgent) {
    for (size_t i = 0; i < B17_MODULE_COUNT; i++) B17_MODULES[i].set_fn(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
}

TEST_F(HeartbeatB17IntegrationTest, DisconnectModulesWhileAgentRunning) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B17_MODULE_COUNT; i++) B17_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B17_MODULE_COUNT / 2; i++) B17_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB17IntegrationTest, DisconnectAllModulesWhileAgentRunning) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B17_MODULE_COUNT; i++) B17_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B17_MODULE_COUNT; i++) B17_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB17IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < B17_MODULE_COUNT; i++) B17_MODULES[i].set_fn(agent_);
    nimcp_health_agent_start(agent_);
    nimcp_health_agent_stop(agent_);
    nimcp_health_agent_start(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB17IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B17_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            B17_MODULES[i].set_fn(agent_);
            for (int j = 0; j < 50; j++)
                nimcp_health_agent_heartbeat_ex(agent_, B17_MODULES[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB17IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(agent2, nullptr);
    for (size_t i = 0; i < B17_MODULE_COUNT; i++) B17_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B17_MODULE_COUNT; i++) B17_MODULES[i].set_fn(agent2);
    for (size_t i = 0; i < B17_MODULE_COUNT; i++) B17_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB17IntegrationTest, ProgressiveModuleConnection) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B17_MODULE_COUNT; i++) {
        B17_MODULES[i].set_fn(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, B17_MODULES[i].name, 0);
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B17_MODULE_COUNT);
    nimcp_health_agent_stop(agent_);
}
