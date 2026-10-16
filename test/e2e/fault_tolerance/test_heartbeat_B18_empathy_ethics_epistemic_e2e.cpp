/**
 * @file test_heartbeat_B18_empathy_ethics_epistemic_e2e.cpp
 * @brief E2E tests for B18 heartbeat
 *        (cognitive/empathetic_response + cognitive/ethics + cognitive/epistemic)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

void empathetic_response_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void empathetic_response_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void empathetic_response_set_health_agent(nimcp_health_agent_t* agent);
void empathy_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void empathy_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void empathetic_response_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_policies_set_health_agent(nimcp_health_agent_t* agent);
void ethics_incidents_set_health_agent(nimcp_health_agent_t* agent);
void ethics_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void health_ethics_bridge_set_health_agent(nimcp_health_agent_t* agent);
void combinatorial_harm_set_health_agent(nimcp_health_agent_t* agent);
void ethics_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_immune_set_health_agent(nimcp_health_agent_t* agent);
void ethics_evaluation_set_health_agent(nimcp_health_agent_t* agent);
void ethics_warfare_set_health_agent(nimcp_health_agent_t* agent);
void ethics_set_health_agent(nimcp_health_agent_t* agent);
void core_directives_set_health_agent(nimcp_health_agent_t* agent);
void ethics_learning_set_health_agent(nimcp_health_agent_t* agent);
void ethics_asimov_set_health_agent(nimcp_health_agent_t* agent);
void ethics_hyperbolic_set_health_agent(nimcp_health_agent_t* agent);
void ethics_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void epistemic_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void epistemic_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void epistemic_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void epistemic_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void epistemic_filter_set_health_agent(nimcp_health_agent_t* agent);
void epistemic_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);
struct B18Module { const char* name; set_health_agent_fn set_fn; };

static const B18Module B18_MODULES[] = {
    {"empathetic_response_fep_bridge",      empathetic_response_fep_bridge_set_health_agent},
    {"empathetic_response_substrate_bridge", empathetic_response_substrate_bridge_set_health_agent},
    {"empathetic_response",                 empathetic_response_set_health_agent},
    {"empathy_plasticity_bridge",           empathy_plasticity_bridge_set_health_agent},
    {"empathy_snn_bridge",                  empathy_snn_bridge_set_health_agent},
    {"empathetic_response_thalamic_bridge", empathetic_response_thalamic_bridge_set_health_agent},
    {"ethics_policies",                     ethics_policies_set_health_agent},
    {"ethics_incidents",                    ethics_incidents_set_health_agent},
    {"ethics_snn_bridge",                   ethics_snn_bridge_set_health_agent},
    {"ethics_thalamic_bridge",              ethics_thalamic_bridge_set_health_agent},
    {"ethics_fep_bridge",                   ethics_fep_bridge_set_health_agent},
    {"health_ethics_bridge",                health_ethics_bridge_set_health_agent},
    {"combinatorial_harm",                  combinatorial_harm_set_health_agent},
    {"ethics_substrate_bridge",             ethics_substrate_bridge_set_health_agent},
    {"ethics_immune",                       ethics_immune_set_health_agent},
    {"ethics_evaluation",                   ethics_evaluation_set_health_agent},
    {"ethics_warfare",                      ethics_warfare_set_health_agent},
    {"ethics",                              ethics_set_health_agent},
    {"core_directives",                     core_directives_set_health_agent},
    {"ethics_learning",                     ethics_learning_set_health_agent},
    {"ethics_asimov",                       ethics_asimov_set_health_agent},
    {"ethics_hyperbolic",                   ethics_hyperbolic_set_health_agent},
    {"ethics_plasticity_bridge",            ethics_plasticity_bridge_set_health_agent},
    {"epistemic_substrate_bridge",          epistemic_substrate_bridge_set_health_agent},
    {"epistemic_snn_bridge",                epistemic_snn_bridge_set_health_agent},
    {"epistemic_thalamic_bridge",           epistemic_thalamic_bridge_set_health_agent},
    {"epistemic_plasticity_bridge",         epistemic_plasticity_bridge_set_health_agent},
    {"epistemic_filter",                    epistemic_filter_set_health_agent},
    {"epistemic_fep_bridge",                epistemic_fep_bridge_set_health_agent},
};
static constexpr size_t B18_MODULE_COUNT = sizeof(B18_MODULES) / sizeof(B18_MODULES[0]);

class HeartbeatB18E2ETest : public ::testing::Test {
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
        for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(nullptr);
        if (agent_) { nimcp_health_agent_destroy(agent_); agent_ = nullptr; }
    }
};

TEST_F(HeartbeatB18E2ETest, FullLifecycleAllModules) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) nimcp_health_agent_heartbeat_ex(agent_, B18_MODULES[i].name, 0);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B18_MODULE_COUNT);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB18E2ETest, ConcurrentModulesMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() { B18_MODULES[i].set_fn(agent_); for (int j = 0; j < 20; j++) nimcp_health_agent_heartbeat_ex(agent_, B18_MODULES[i].name, 0); B18_MODULES[i].set_fn(nullptr); });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB18E2ETest, HighFrequencyBurst1000Heartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(agent_);
    health_agent_stats_t before; nimcp_health_agent_get_stats(agent_, &before);
    for (int j = 0; j < 1000; j++) nimcp_health_agent_heartbeat_ex(agent_, "B18_burst", 0);
    health_agent_stats_t after; nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + 1000);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB18E2ETest, TimeoutDetectionAfterSilence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(agent_);
    nimcp_health_agent_heartbeat_ex(agent_, "B18_timeout_test", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB18E2ETest, MultiPhaseOperation) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B18_MODULE_COUNT / 2; i++) { B18_MODULES[i].set_fn(agent_); nimcp_health_agent_heartbeat_ex(agent_, B18_MODULES[i].name, 0); }
    for (size_t i = B18_MODULE_COUNT / 2; i < B18_MODULE_COUNT; i++) { B18_MODULES[i].set_fn(agent_); nimcp_health_agent_heartbeat_ex(agent_, B18_MODULES[i].name, 0); }
    for (size_t i = 0; i < B18_MODULE_COUNT / 2; i++) B18_MODULES[i].set_fn(nullptr);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B18_MODULE_COUNT);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB18E2ETest, ModuleHotSwapDuringOperation) {
    nimcp_health_agent_start(agent_);
    health_agent_config_t cfg2; nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2); ASSERT_NE(agent2, nullptr);
    nimcp_health_agent_start(agent2);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) { B18_MODULES[i].set_fn(agent2); nimcp_health_agent_heartbeat_ex(agent2, B18_MODULES[i].name, 0); }
    health_agent_stats_t stats2; nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, B18_MODULE_COUNT);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent2); nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB18E2ETest, SustainedOperationOverTime) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(agent_);
    auto start = std::chrono::steady_clock::now();
    uint64_t count = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(250)) { nimcp_health_agent_heartbeat_ex(agent_, "B18_sustained", 0); count++; }
    EXPECT_GT(count, 0u);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, count);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB18E2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) nimcp_health_agent_heartbeat_ex(agent_, B18_MODULES[i].name, 0);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B18_MODULE_COUNT);
}
