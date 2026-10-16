/**
 * @file test_heartbeat_B12_neuro_symbolic_jepa_e2e.cpp
 * @brief E2E tests for B12 heartbeat (cognitive/neuro_symbolic + cognitive/jepa)
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

class HeartbeatB12E2ETest : public ::testing::Test {
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

TEST_F(HeartbeatB12E2ETest, FullLifecycleAllModules) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++)
        nimcp_health_agent_heartbeat_ex(agent_, B12_MODULES[i].name, 0);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B12_MODULE_COUNT);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB12E2ETest, ConcurrentModulesMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            B12_MODULES[i].set_fn(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, B12_MODULES[i].name, 0);
            B12_MODULES[i].set_fn(nullptr);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB12E2ETest, HighFrequencyBurst1000Heartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(agent_);
    health_agent_stats_t before;
    nimcp_health_agent_get_stats(agent_, &before);
    for (int j = 0; j < 1000; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B12_burst", 0);
    health_agent_stats_t after;
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + 1000);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB12E2ETest, TimeoutDetectionAfterSilence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(agent_);
    nimcp_health_agent_heartbeat_ex(agent_, "B12_timeout_test", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB12E2ETest, MultiPhaseOperation) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT / 2; i++) {
        B12_MODULES[i].set_fn(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, B12_MODULES[i].name, 0);
    }
    for (size_t i = B12_MODULE_COUNT / 2; i < B12_MODULE_COUNT; i++) {
        B12_MODULES[i].set_fn(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, B12_MODULES[i].name, 0);
    }
    for (size_t i = 0; i < B12_MODULE_COUNT / 2; i++) B12_MODULES[i].set_fn(nullptr);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B12_MODULE_COUNT);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB12E2ETest, ModuleHotSwapDuringOperation) {
    nimcp_health_agent_start(agent_);
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(agent2, nullptr);
    nimcp_health_agent_start(agent2);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        B12_MODULES[i].set_fn(agent2);
        nimcp_health_agent_heartbeat_ex(agent2, B12_MODULES[i].name, 0);
    }
    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, B12_MODULE_COUNT);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB12E2ETest, SustainedOperationOverTime) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(agent_);
    auto start = std::chrono::steady_clock::now();
    uint64_t count = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(250)) {
        nimcp_health_agent_heartbeat_ex(agent_, "B12_sustained", 0);
        count++;
    }
    EXPECT_GT(count, 0u);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, count);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB12E2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++)
        nimcp_health_agent_heartbeat_ex(agent_, B12_MODULES[i].name, 0);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) B12_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B12_MODULE_COUNT);
}
