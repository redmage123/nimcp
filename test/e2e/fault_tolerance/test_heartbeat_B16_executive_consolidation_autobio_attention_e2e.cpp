/**
 * @file test_heartbeat_B16_executive_consolidation_autobio_attention_e2e.cpp
 * @brief E2E tests for B16 heartbeat
 *        (cognitive/executive + cognitive/consolidation + cognitive/autobiographical_memory + cognitive/attention)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

void executive_set_health_agent(nimcp_health_agent_t* agent);
void executive_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void consolidation_set_health_agent(nimcp_health_agent_t* agent);
void consolidation_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void consolidation_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void consolidation_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void consolidation_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void consolidation_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_consolidation_set_health_agent(nimcp_health_agent_t* agent);
void autobiographical_memory_set_health_agent(nimcp_health_agent_t* agent);
void autobiographical_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobiographical_memory_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobio_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobio_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobio_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobio_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_attention_set_health_agent(nimcp_health_agent_t* agent);
void attention_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void attention_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void attention_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void attention_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void attention_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void attention_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);
struct B16Module { const char* name; set_health_agent_fn set_fn; };

static const B16Module B16_MODULES[] = {
    {"executive",                         executive_set_health_agent},
    {"executive_fep_bridge",              executive_fep_bridge_set_health_agent},
    {"executive_plasticity_bridge",       executive_plasticity_bridge_set_health_agent},
    {"executive_sleep_bridge",            executive_sleep_bridge_set_health_agent},
    {"executive_snn_bridge",              executive_snn_bridge_set_health_agent},
    {"executive_substrate_bridge",        executive_substrate_bridge_set_health_agent},
    {"executive_thalamic_bridge",         executive_thalamic_bridge_set_health_agent},
    {"consolidation",                     consolidation_set_health_agent},
    {"consolidation_fep_bridge",          consolidation_fep_bridge_set_health_agent},
    {"consolidation_plasticity_bridge",   consolidation_plasticity_bridge_set_health_agent},
    {"consolidation_snn_bridge",          consolidation_snn_bridge_set_health_agent},
    {"consolidation_substrate_bridge",    consolidation_substrate_bridge_set_health_agent},
    {"consolidation_thalamic_bridge",     consolidation_thalamic_bridge_set_health_agent},
    {"emotion_consolidation",             emotion_consolidation_set_health_agent},
    {"autobiographical_memory",           autobiographical_memory_set_health_agent},
    {"autobiographical_fep_bridge",       autobiographical_fep_bridge_set_health_agent},
    {"autobiographical_memory_sleep_bridge", autobiographical_memory_sleep_bridge_set_health_agent},
    {"autobio_plasticity_bridge",         autobio_plasticity_bridge_set_health_agent},
    {"autobio_snn_bridge",                autobio_snn_bridge_set_health_agent},
    {"autobio_substrate_bridge",          autobio_substrate_bridge_set_health_agent},
    {"autobio_thalamic_bridge",           autobio_thalamic_bridge_set_health_agent},
    {"emotion_attention",                 emotion_attention_set_health_agent},
    {"attention_fep_bridge",              attention_fep_bridge_set_health_agent},
    {"attention_plasticity_bridge",       attention_plasticity_bridge_set_health_agent},
    {"attention_sleep_bridge",            attention_sleep_bridge_set_health_agent},
    {"attention_snn_bridge",              attention_snn_bridge_set_health_agent},
    {"attention_substrate_bridge",        attention_substrate_bridge_set_health_agent},
    {"attention_thalamic_bridge",         attention_thalamic_bridge_set_health_agent},
};
static constexpr size_t B16_MODULE_COUNT = sizeof(B16_MODULES) / sizeof(B16_MODULES[0]);

class HeartbeatB16E2ETest : public ::testing::Test {
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
        for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(nullptr);
        if (agent_) { nimcp_health_agent_destroy(agent_); agent_ = nullptr; }
    }
};

TEST_F(HeartbeatB16E2ETest, FullLifecycleAllModules) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) nimcp_health_agent_heartbeat_ex(agent_, B16_MODULES[i].name, 0);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B16_MODULE_COUNT);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB16E2ETest, ConcurrentModulesMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() { B16_MODULES[i].set_fn(agent_); for (int j = 0; j < 20; j++) nimcp_health_agent_heartbeat_ex(agent_, B16_MODULES[i].name, 0); B16_MODULES[i].set_fn(nullptr); });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB16E2ETest, HighFrequencyBurst1000Heartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(agent_);
    health_agent_stats_t before; nimcp_health_agent_get_stats(agent_, &before);
    for (int j = 0; j < 1000; j++) nimcp_health_agent_heartbeat_ex(agent_, "B16_burst", 0);
    health_agent_stats_t after; nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + 1000);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB16E2ETest, TimeoutDetectionAfterSilence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(agent_);
    nimcp_health_agent_heartbeat_ex(agent_, "B16_timeout_test", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB16E2ETest, MultiPhaseOperation) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B16_MODULE_COUNT / 2; i++) { B16_MODULES[i].set_fn(agent_); nimcp_health_agent_heartbeat_ex(agent_, B16_MODULES[i].name, 0); }
    for (size_t i = B16_MODULE_COUNT / 2; i < B16_MODULE_COUNT; i++) { B16_MODULES[i].set_fn(agent_); nimcp_health_agent_heartbeat_ex(agent_, B16_MODULES[i].name, 0); }
    for (size_t i = 0; i < B16_MODULE_COUNT / 2; i++) B16_MODULES[i].set_fn(nullptr);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B16_MODULE_COUNT);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB16E2ETest, ModuleHotSwapDuringOperation) {
    nimcp_health_agent_start(agent_);
    health_agent_config_t cfg2; nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2); ASSERT_NE(agent2, nullptr);
    nimcp_health_agent_start(agent2);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) { B16_MODULES[i].set_fn(agent2); nimcp_health_agent_heartbeat_ex(agent2, B16_MODULES[i].name, 0); }
    health_agent_stats_t stats2; nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, B16_MODULE_COUNT);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent2); nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB16E2ETest, SustainedOperationOverTime) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(agent_);
    auto start = std::chrono::steady_clock::now();
    uint64_t count = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(250)) { nimcp_health_agent_heartbeat_ex(agent_, "B16_sustained", 0); count++; }
    EXPECT_GT(count, 0u);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, count);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB16E2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) nimcp_health_agent_heartbeat_ex(agent_, B16_MODULES[i].name, 0);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B16_MODULE_COUNT);
}
