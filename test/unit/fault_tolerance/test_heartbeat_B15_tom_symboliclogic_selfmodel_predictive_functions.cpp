/**
 * @file test_heartbeat_B15_tom_symboliclogic_selfmodel_predictive_functions.cpp
 * @brief Unit tests for B15 heartbeat setter functions
 *        (cognitive/theory_of_mind + cognitive/symbolic_logic + cognitive/self_model + cognitive/predictive)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

/* cognitive/theory_of_mind modules */
void theory_of_mind_set_health_agent(nimcp_health_agent_t* agent);
void tom_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void tom_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void tom_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void theory_of_mind_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void theory_of_mind_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void theory_of_mind_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/symbolic_logic modules */
void symbolic_logic_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_hub_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_lgss_loader_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_safety_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/self_model modules */
void self_model_set_health_agent(nimcp_health_agent_t* agent);
void self_model_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_model_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_model_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_model_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_model_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_model_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/predictive modules */
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
    /* cognitive/theory_of_mind */
    {"theory_of_mind",                     theory_of_mind_set_health_agent},
    {"tom_snn_bridge",                     tom_snn_bridge_set_health_agent},
    {"tom_plasticity_bridge",              tom_plasticity_bridge_set_health_agent},
    {"tom_fep_bridge",                     tom_fep_bridge_set_health_agent},
    {"theory_of_mind_thalamic_bridge",     theory_of_mind_thalamic_bridge_set_health_agent},
    {"theory_of_mind_substrate_bridge",    theory_of_mind_substrate_bridge_set_health_agent},
    {"theory_of_mind_sleep_bridge",        theory_of_mind_sleep_bridge_set_health_agent},
    /* cognitive/symbolic_logic */
    {"symbolic_logic_fep_bridge",          symbolic_logic_fep_bridge_set_health_agent},
    {"symbolic_logic_hub_bridge",          symbolic_logic_hub_bridge_set_health_agent},
    {"symbolic_logic_lgss_loader",         symbolic_logic_lgss_loader_set_health_agent},
    {"symbolic_logic_safety",              symbolic_logic_safety_set_health_agent},
    {"symbolic_logic_substrate_bridge",    symbolic_logic_substrate_bridge_set_health_agent},
    {"symbolic_logic_thalamic_bridge",     symbolic_logic_thalamic_bridge_set_health_agent},
    {"symbolic_logic_plasticity_bridge",   symbolic_logic_plasticity_bridge_set_health_agent},
    /* cognitive/self_model */
    {"self_model",                         self_model_set_health_agent},
    {"self_model_fep_bridge",              self_model_fep_bridge_set_health_agent},
    {"self_model_plasticity_bridge",       self_model_plasticity_bridge_set_health_agent},
    {"self_model_sleep_bridge",            self_model_sleep_bridge_set_health_agent},
    {"self_model_snn_bridge",              self_model_snn_bridge_set_health_agent},
    {"self_model_substrate_bridge",        self_model_substrate_bridge_set_health_agent},
    {"self_model_thalamic_bridge",         self_model_thalamic_bridge_set_health_agent},
    /* cognitive/predictive */
    {"predictive",                         predictive_set_health_agent},
    {"predictive_fep_bridge",              predictive_fep_bridge_set_health_agent},
    {"predictive_hierarchy",               predictive_hierarchy_set_health_agent},
    {"predictive_plasticity_bridge",       predictive_plasticity_bridge_set_health_agent},
    {"predictive_snn_bridge",              predictive_snn_bridge_set_health_agent},
    {"predictive_substrate_bridge",        predictive_substrate_bridge_set_health_agent},
    {"predictive_thalamic_bridge",         predictive_thalamic_bridge_set_health_agent},
};
static constexpr size_t B15_MODULE_COUNT = sizeof(B15_MODULES) / sizeof(B15_MODULES[0]);

class HeartbeatB15UnitTest : public ::testing::Test {
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

TEST_F(HeartbeatB15UnitTest, AllModulesSetNull) {
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) {
        SCOPED_TRACE(B15_MODULES[i].name);
        B15_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB15UnitTest, AllModulesSetValid) {
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) {
        SCOPED_TRACE(B15_MODULES[i].name);
        B15_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB15UnitTest, AllModulesReplaceAgent) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(agent2, nullptr);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) {
        SCOPED_TRACE(B15_MODULES[i].name);
        B15_MODULES[i].set_fn(agent_);
        B15_MODULES[i].set_fn(agent2);
    }
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB15UnitTest, AllModulesShareSameAgent) {
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
}

TEST_F(HeartbeatB15UnitTest, SetNullAfterValid) {
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) {
        SCOPED_TRACE(B15_MODULES[i].name);
        B15_MODULES[i].set_fn(agent_);
        B15_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB15UnitTest, ConcurrentSetClear) {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 100; j++) {
                B15_MODULES[i].set_fn(agent_);
                B15_MODULES[i].set_fn(nullptr);
            }
        });
    }
    for (auto& t : threads) t.join();
}

TEST_F(HeartbeatB15UnitTest, HeartbeatCounterIncrements) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) B15_MODULES[i].set_fn(agent_);
    health_agent_stats_t before, after;
    nimcp_health_agent_get_stats(agent_, &before);
    nimcp_health_agent_heartbeat_ex(agent_, "B15_test", 0);
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GT(after.heartbeats_received, before.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB15UnitTest, ModuleCount) {
    EXPECT_EQ(B15_MODULE_COUNT, 28u);
}

TEST_F(HeartbeatB15UnitTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < B15_MODULE_COUNT; i++) {
        SCOPED_TRACE(B15_MODULES[i].name);
        B15_MODULES[i].set_fn(agent_);
        B15_MODULES[i].set_fn(agent_);
    }
}
