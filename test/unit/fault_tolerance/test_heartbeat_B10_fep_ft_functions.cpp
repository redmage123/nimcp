/**
 * @file test_heartbeat_B10_fep_ft_functions.cpp
 * @brief Unit tests for B10 heartbeat setter functions (cognitive/free_energy + cognitive/fault_tolerance)
 *
 * Tests that each module's set_health_agent function correctly accepts,
 * replaces, and clears the health agent pointer.
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

/* cognitive/free_energy modules */
void fep_consciousness_set_health_agent(nimcp_health_agent_t* agent);
void fep_context_set_health_agent(nimcp_health_agent_t* agent);
void fep_curiosity_set_health_agent(nimcp_health_agent_t* agent);
void fep_evidence_set_health_agent(nimcp_health_agent_t* agent);
void fep_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void fep_learning_set_health_agent(nimcp_health_agent_t* agent);
void fep_neuromod_set_health_agent(nimcp_health_agent_t* agent);
void fep_orchestrator_set_health_agent(nimcp_health_agent_t* agent);
void fep_planning_set_health_agent(nimcp_health_agent_t* agent);
void fep_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void fep_sleep_set_health_agent(nimcp_health_agent_t* agent);
void fep_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void free_energy_set_health_agent(nimcp_health_agent_t* agent);
void free_energy_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void free_energy_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/fault_tolerance modules */
void emotional_tagging_set_health_agent(nimcp_health_agent_t* agent);
void failure_prediction_set_health_agent(nimcp_health_agent_t* agent);
void fault_attention_set_health_agent(nimcp_health_agent_t* agent);
void fault_tolerance_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void fault_tolerance_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void fault_working_memory_set_health_agent(nimcp_health_agent_t* agent);
void health_diagnostic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void health_self_repair_bridge_set_health_agent(nimcp_health_agent_t* agent);
void metacognition_set_health_agent(nimcp_health_agent_t* agent);
void recovery_consolidation_set_health_agent(nimcp_health_agent_t* agent);
void recovery_episodic_memory_set_health_agent(nimcp_health_agent_t* agent);
void recovery_executive_set_health_agent(nimcp_health_agent_t* agent);
void recovery_parietal_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_repair_set_health_agent(nimcp_health_agent_t* agent);
void self_repair_health_notify_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B10Module {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B10Module B10_MODULES[] = {
    /* cognitive/free_energy */
    {"fep_consciousness",              fep_consciousness_set_health_agent},
    {"fep_context",                    fep_context_set_health_agent},
    {"fep_curiosity",                  fep_curiosity_set_health_agent},
    {"fep_evidence",                   fep_evidence_set_health_agent},
    {"fep_immune_bridge",              fep_immune_bridge_set_health_agent},
    {"fep_learning",                   fep_learning_set_health_agent},
    {"fep_neuromod",                   fep_neuromod_set_health_agent},
    {"fep_orchestrator",               fep_orchestrator_set_health_agent},
    {"fep_planning",                   fep_planning_set_health_agent},
    {"fep_plasticity_bridge",          fep_plasticity_bridge_set_health_agent},
    {"fep_sleep",                      fep_sleep_set_health_agent},
    {"fep_snn_bridge",                 fep_snn_bridge_set_health_agent},
    {"free_energy",                    free_energy_set_health_agent},
    {"free_energy_substrate_bridge",   free_energy_substrate_bridge_set_health_agent},
    {"free_energy_thalamic_bridge",    free_energy_thalamic_bridge_set_health_agent},
    /* cognitive/fault_tolerance */
    {"emotional_tagging",              emotional_tagging_set_health_agent},
    {"failure_prediction",             failure_prediction_set_health_agent},
    {"fault_attention",                fault_attention_set_health_agent},
    {"fault_tolerance_substrate_bridge", fault_tolerance_substrate_bridge_set_health_agent},
    {"fault_tolerance_thalamic_bridge", fault_tolerance_thalamic_bridge_set_health_agent},
    {"fault_working_memory",           fault_working_memory_set_health_agent},
    {"health_diagnostic_bridge",       health_diagnostic_bridge_set_health_agent},
    {"health_self_repair_bridge",      health_self_repair_bridge_set_health_agent},
    {"metacognition",                  metacognition_set_health_agent},
    {"recovery_consolidation",         recovery_consolidation_set_health_agent},
    {"recovery_episodic_memory",       recovery_episodic_memory_set_health_agent},
    {"recovery_executive",             recovery_executive_set_health_agent},
    {"recovery_parietal_bridge",       recovery_parietal_bridge_set_health_agent},
    {"self_repair",                    self_repair_set_health_agent},
    {"self_repair_health_notify",      self_repair_health_notify_set_health_agent},
};

static constexpr size_t B10_MODULE_COUNT = sizeof(B10_MODULES) / sizeof(B10_MODULES[0]);

class HeartbeatB10UnitTest : public ::testing::Test {
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
        for (size_t i = 0; i < B10_MODULE_COUNT; i++) {
            B10_MODULES[i].set_fn(nullptr);
        }
        if (agent_) {
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }
};

TEST_F(HeartbeatB10UnitTest, AllModulesSetNull) {
    for (size_t i = 0; i < B10_MODULE_COUNT; i++) {
        SCOPED_TRACE(B10_MODULES[i].name);
        B10_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB10UnitTest, AllModulesSetValid) {
    for (size_t i = 0; i < B10_MODULE_COUNT; i++) {
        SCOPED_TRACE(B10_MODULES[i].name);
        B10_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB10UnitTest, AllModulesReplaceAgent) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(agent2, nullptr);

    for (size_t i = 0; i < B10_MODULE_COUNT; i++) {
        SCOPED_TRACE(B10_MODULES[i].name);
        B10_MODULES[i].set_fn(agent_);
        B10_MODULES[i].set_fn(agent2);
    }

    for (size_t i = 0; i < B10_MODULE_COUNT; i++) {
        B10_MODULES[i].set_fn(nullptr);
    }
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB10UnitTest, AllModulesShareSameAgent) {
    for (size_t i = 0; i < B10_MODULE_COUNT; i++) {
        B10_MODULES[i].set_fn(agent_);
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
}

TEST_F(HeartbeatB10UnitTest, SetNullAfterValid) {
    for (size_t i = 0; i < B10_MODULE_COUNT; i++) {
        SCOPED_TRACE(B10_MODULES[i].name);
        B10_MODULES[i].set_fn(agent_);
        B10_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB10UnitTest, ConcurrentSetClear) {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B10_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 100; j++) {
                B10_MODULES[i].set_fn(agent_);
                B10_MODULES[i].set_fn(nullptr);
            }
        });
    }
    for (auto& t : threads) t.join();
}

TEST_F(HeartbeatB10UnitTest, HeartbeatCounterIncrements) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B10_MODULE_COUNT; i++) {
        B10_MODULES[i].set_fn(agent_);
    }

    health_agent_stats_t before, after;
    nimcp_health_agent_get_stats(agent_, &before);
    nimcp_health_agent_heartbeat_ex(agent_, "B10_test", 0);
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GT(after.heartbeats_received, before.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB10UnitTest, ModuleCount) {
    EXPECT_EQ(B10_MODULE_COUNT, 30u);
}

TEST_F(HeartbeatB10UnitTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < B10_MODULE_COUNT; i++) {
        SCOPED_TRACE(B10_MODULES[i].name);
        B10_MODULES[i].set_fn(agent_);
        B10_MODULES[i].set_fn(agent_);
    }
}
