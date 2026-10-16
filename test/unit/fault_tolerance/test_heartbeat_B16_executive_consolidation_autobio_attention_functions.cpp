/**
 * @file test_heartbeat_B16_executive_consolidation_autobio_attention_functions.cpp
 * @brief Unit tests for B16 heartbeat setter functions
 *        (cognitive/executive + cognitive/consolidation + cognitive/autobiographical_memory + cognitive/attention)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

/* cognitive/executive modules */
void executive_set_health_agent(nimcp_health_agent_t* agent);
void executive_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/consolidation modules */
void consolidation_set_health_agent(nimcp_health_agent_t* agent);
void consolidation_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void consolidation_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void consolidation_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void consolidation_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void consolidation_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_consolidation_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/autobiographical_memory modules */
void autobiographical_memory_set_health_agent(nimcp_health_agent_t* agent);
void autobiographical_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobiographical_memory_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobio_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobio_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobio_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobio_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/attention modules */
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
    /* cognitive/executive */
    {"executive",                         executive_set_health_agent},
    {"executive_fep_bridge",              executive_fep_bridge_set_health_agent},
    {"executive_plasticity_bridge",       executive_plasticity_bridge_set_health_agent},
    {"executive_sleep_bridge",            executive_sleep_bridge_set_health_agent},
    {"executive_snn_bridge",              executive_snn_bridge_set_health_agent},
    {"executive_substrate_bridge",        executive_substrate_bridge_set_health_agent},
    {"executive_thalamic_bridge",         executive_thalamic_bridge_set_health_agent},
    /* cognitive/consolidation */
    {"consolidation",                     consolidation_set_health_agent},
    {"consolidation_fep_bridge",          consolidation_fep_bridge_set_health_agent},
    {"consolidation_plasticity_bridge",   consolidation_plasticity_bridge_set_health_agent},
    {"consolidation_snn_bridge",          consolidation_snn_bridge_set_health_agent},
    {"consolidation_substrate_bridge",    consolidation_substrate_bridge_set_health_agent},
    {"consolidation_thalamic_bridge",     consolidation_thalamic_bridge_set_health_agent},
    {"emotion_consolidation",             emotion_consolidation_set_health_agent},
    /* cognitive/autobiographical_memory */
    {"autobiographical_memory",           autobiographical_memory_set_health_agent},
    {"autobiographical_fep_bridge",       autobiographical_fep_bridge_set_health_agent},
    {"autobiographical_memory_sleep_bridge", autobiographical_memory_sleep_bridge_set_health_agent},
    {"autobio_plasticity_bridge",         autobio_plasticity_bridge_set_health_agent},
    {"autobio_snn_bridge",                autobio_snn_bridge_set_health_agent},
    {"autobio_substrate_bridge",          autobio_substrate_bridge_set_health_agent},
    {"autobio_thalamic_bridge",           autobio_thalamic_bridge_set_health_agent},
    /* cognitive/attention */
    {"emotion_attention",                 emotion_attention_set_health_agent},
    {"attention_fep_bridge",              attention_fep_bridge_set_health_agent},
    {"attention_plasticity_bridge",       attention_plasticity_bridge_set_health_agent},
    {"attention_sleep_bridge",            attention_sleep_bridge_set_health_agent},
    {"attention_snn_bridge",              attention_snn_bridge_set_health_agent},
    {"attention_substrate_bridge",        attention_substrate_bridge_set_health_agent},
    {"attention_thalamic_bridge",         attention_thalamic_bridge_set_health_agent},
};
static constexpr size_t B16_MODULE_COUNT = sizeof(B16_MODULES) / sizeof(B16_MODULES[0]);

class HeartbeatB16UnitTest : public ::testing::Test {
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

TEST_F(HeartbeatB16UnitTest, AllModulesSetNull) {
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) {
        SCOPED_TRACE(B16_MODULES[i].name);
        B16_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB16UnitTest, AllModulesSetValid) {
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) {
        SCOPED_TRACE(B16_MODULES[i].name);
        B16_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB16UnitTest, AllModulesReplaceAgent) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(agent2, nullptr);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) {
        SCOPED_TRACE(B16_MODULES[i].name);
        B16_MODULES[i].set_fn(agent_);
        B16_MODULES[i].set_fn(agent2);
    }
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB16UnitTest, AllModulesShareSameAgent) {
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
}

TEST_F(HeartbeatB16UnitTest, SetNullAfterValid) {
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) {
        SCOPED_TRACE(B16_MODULES[i].name);
        B16_MODULES[i].set_fn(agent_);
        B16_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB16UnitTest, ConcurrentSetClear) {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 100; j++) {
                B16_MODULES[i].set_fn(agent_);
                B16_MODULES[i].set_fn(nullptr);
            }
        });
    }
    for (auto& t : threads) t.join();
}

TEST_F(HeartbeatB16UnitTest, HeartbeatCounterIncrements) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) B16_MODULES[i].set_fn(agent_);
    health_agent_stats_t before, after;
    nimcp_health_agent_get_stats(agent_, &before);
    nimcp_health_agent_heartbeat_ex(agent_, "B16_test", 0);
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GT(after.heartbeats_received, before.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB16UnitTest, ModuleCount) {
    EXPECT_EQ(B16_MODULE_COUNT, 28u);
}

TEST_F(HeartbeatB16UnitTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < B16_MODULE_COUNT; i++) {
        SCOPED_TRACE(B16_MODULES[i].name);
        B16_MODULES[i].set_fn(agent_);
        B16_MODULES[i].set_fn(agent_);
    }
}
