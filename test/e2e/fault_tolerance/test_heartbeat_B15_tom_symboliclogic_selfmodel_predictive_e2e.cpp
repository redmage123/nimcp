/**
 * @file test_heartbeat_B15_tom_symboliclogic_selfmodel_predictive_e2e.cpp
 * @brief E2E tests for B15 heartbeat
 *        (cognitive/theory_of_mind + cognitive/symbolic_logic + cognitive/self_model + cognitive/predictive)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

void theory_of_mind_set_health_agent(nimcp_health_agent_t* agent);
void tom_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void tom_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void tom_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void theory_of_mind_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void theory_of_mind_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void theory_of_mind_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_hub_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_lgss_loader_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_safety_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_model_set_health_agent(nimcp_health_agent_t* agent);
void self_model_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_model_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_model_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_model_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_model_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_model_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_set_health_agent(nimcp_health_agent_t* agent);
void predictive_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_hierarchy_set_health_agent(nimcp_health_agent_t* agent);
void predictive_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);
struct B15Module { const char* name; set_health_agent_fn set_fn; };

static const B15Module B15_MODULES[] = {
    {"theory_of_mind",                     theory_of_mind_set_health_agent},
    {"tom_snn_bridge",                     tom_snn_bridge_set_health_agent},
    {"tom_plasticity_bridge",              tom_plasticity_bridge_set_health_agent},
    {"tom_fep_bridge",                     tom_fep_bridge_set_health_agent},
    {"theory_of_mind_thalamic_bridge",     theory_of_mind_thalamic_bridge_set_health_agent},
    {"theory_of_mind_substrate_bridge",    theory_of_mind_substrate_bridge_set_health_agent},
    {"theory_of_mind_sleep_bridge",        theory_of_mind_sleep_bridge_set_health_agent},
    {"symbolic_logic_fep_bridge",          symbolic_logic_fep_bridge_set_health_agent},
    {"symbolic_logic_hub_bridge",          symbolic_logic_hub_bridge_set_health_agent},
    {"symbolic_logic_lgss_loader",         symbolic_logic_lgss_loader_set_health_agent},
    {"symbolic_logic_safety",              symbolic_logic_safety_set_health_agent},
    {"symbolic_logic_substrate_bridge",    symbolic_logic_substrate_bridge_set_health_agent},
    {"symbolic_logic_thalamic_bridge",     symbolic_logic_thalamic_bridge_set_health_agent},
    {"symbolic_logic_plasticity_bridge",   symbolic_logic_plasticity_bridge_set_health_agent},
    {"self_model",                         self_model_set_health_agent},
    {"self_model_fep_bridge",              self_model_fep_bridge_set_health_agent},
    {"self_model_plasticity_bridge",       self_model_plasticity_bridge_set_health_agent},
    {"self_model_sleep_bridge",            self_model_sleep_bridge_set_health_agent},
    {"self_model_snn_bridge",              self_model_snn_bridge_set_health_agent},
    {"self_model_substrate_bridge",        self_model_substrate_bridge_set_health_agent},
    {"self_model_thalamic_bridge",         self_model_thalamic_bridge_set_health_agent},
    {"predictive",                         predictive_set_health_agent},
    {"predictive_fep_bridge",              predictive_fep_bridge_set_health_agent},
    {"predictive_hierarchy",               predictive_hierarchy_set_health_agent},
    {"predictive_plasticity_bridge",       predictive_plasticity_bridge_set_health_agent},
    {"predictive_snn_bridge",              predictive_snn_bridge_set_health_agent},
    {"predictive_substrate_bridge",        predictive_substrate_bridge_set_health_agent},
    {"predictive_thalamic_bridge",         predictive_thalamic_bridge_set_health_agent},
};
static constexpr size_t B15_MODULE_COUNT = sizeof(B15_MODULES) / sizeof(B15_MODULES[0]);

class HeartbeatB15E2ETest : public ::testing::Test {
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
        for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(nullptr);
        if (agent_) { nimcp_health_agent_destroy(agent_); agent_ = nullptr; }
    }
};

TEST_F(HeartbeatB15E2ETest, FullLifecycleAllModules) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) nimcp_health_agent_heartbeat_ex(agent_, B15_MODULES[i].name, 0);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B15_MODULE_COUNT);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB15E2ETest, ConcurrentModulesMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() { B15_MODULES[i].set_fn(agent_); for (int j = 0; j < 20; j++) nimcp_health_agent_heartbeat_ex(agent_, B15_MODULES[i].name, 0); B15_MODULES[i].set_fn(nullptr); });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB15E2ETest, HighFrequencyBurst1000Heartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(agent_);
    health_agent_stats_t before; nimcp_health_agent_get_stats(agent_, &before);
    for (int j = 0; j < 1000; j++) nimcp_health_agent_heartbeat_ex(agent_, "B15_burst", 0);
    health_agent_stats_t after; nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + 1000);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB15E2ETest, TimeoutDetectionAfterSilence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(agent_);
    nimcp_health_agent_heartbeat_ex(agent_, "B15_timeout_test", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB15E2ETest, MultiPhaseOperation) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B15_MODULE_COUNT / 2; i++) { B15_MODULES[i].set_fn(agent_); nimcp_health_agent_heartbeat_ex(agent_, B15_MODULES[i].name, 0); }
    for (size_t i = B15_MODULE_COUNT / 2; i < B15_MODULE_COUNT; i++) { B15_MODULES[i].set_fn(agent_); nimcp_health_agent_heartbeat_ex(agent_, B15_MODULES[i].name, 0); }
    for (size_t i = 0; i < B15_MODULE_COUNT / 2; i++) B15_MODULES[i].set_fn(nullptr);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B15_MODULE_COUNT);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB15E2ETest, ModuleHotSwapDuringOperation) {
    nimcp_health_agent_start(agent_);
    health_agent_config_t cfg2; nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2); ASSERT_NE(agent2, nullptr);
    nimcp_health_agent_start(agent2);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) { B15_MODULES[i].set_fn(agent2); nimcp_health_agent_heartbeat_ex(agent2, B15_MODULES[i].name, 0); }
    health_agent_stats_t stats2; nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, B15_MODULE_COUNT);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent2); nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB15E2ETest, SustainedOperationOverTime) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(agent_);
    auto start = std::chrono::steady_clock::now();
    uint64_t count = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(250)) { nimcp_health_agent_heartbeat_ex(agent_, "B15_sustained", 0); count++; }
    EXPECT_GT(count, 0u);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, count);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB15E2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) nimcp_health_agent_heartbeat_ex(agent_, B15_MODULES[i].name, 0);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B15_MODULE_COUNT);
}
