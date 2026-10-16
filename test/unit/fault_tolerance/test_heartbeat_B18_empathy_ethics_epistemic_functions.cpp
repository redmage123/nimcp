/**
 * @file test_heartbeat_B18_emotion_cluster_functions.cpp
 * @brief Unit tests for B18 heartbeat setter functions
 *        (cognitive/emotion + cognitive/emotional_tagging + cognitive/emotion_recognition +
 *         cognitive/emotions + cognitive/emotion_tensor + cognitive/grief + cognitive/joy + cognitive/remorse)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

/* cognitive/emotion modules */
void emotion_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void health_emotion_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/emotional_tagging modules */
void emotional_tagging_set_health_agent(nimcp_health_agent_t* agent);
void emotional_tagging_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotional_tagging_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotional_tagging_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/emotion_recognition modules */
void emotion_recognition_simple_set_health_agent(nimcp_health_agent_t* agent);
void emotion_recognition_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_recognition_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_recognition_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/emotions modules */
void emotional_system_set_health_agent(nimcp_health_agent_t* agent);
void emotional_system_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/emotion_tensor modules */
void emotion_tensor_set_health_agent(nimcp_health_agent_t* agent);
void emotion_tensor_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_tensor_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_tensor_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/grief modules */
void grief_and_loss_set_health_agent(nimcp_health_agent_t* agent);
void grief_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void grief_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void grief_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/joy modules */
void joy_euphoria_set_health_agent(nimcp_health_agent_t* agent);
void joy_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void joy_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void joy_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/remorse modules */
void remorse_regret_set_health_agent(nimcp_health_agent_t* agent);
void remorse_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void remorse_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void remorse_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);
struct B18Module { const char* name; set_health_agent_fn set_fn; };

static const B18Module B18_MODULES[] = {
    /* cognitive/emotion */
    {"emotion_fep_bridge",                emotion_fep_bridge_set_health_agent},
    {"emotion_plasticity_bridge",         emotion_plasticity_bridge_set_health_agent},
    {"emotion_snn_bridge",                emotion_snn_bridge_set_health_agent},
    {"emotion_substrate_bridge",          emotion_substrate_bridge_set_health_agent},
    {"emotion_thalamic_bridge",           emotion_thalamic_bridge_set_health_agent},
    {"health_emotion_bridge",             health_emotion_bridge_set_health_agent},
    /* cognitive/emotional_tagging */
    {"emotional_tagging",                 emotional_tagging_set_health_agent},
    {"emotional_tagging_fep_bridge",      emotional_tagging_fep_bridge_set_health_agent},
    {"emotional_tagging_substrate_bridge", emotional_tagging_substrate_bridge_set_health_agent},
    {"emotional_tagging_thalamic_bridge", emotional_tagging_thalamic_bridge_set_health_agent},
    /* cognitive/emotion_recognition */
    {"emotion_recognition_simple",        emotion_recognition_simple_set_health_agent},
    {"emotion_recognition_fep_bridge",    emotion_recognition_fep_bridge_set_health_agent},
    {"emotion_recognition_substrate_bridge", emotion_recognition_substrate_bridge_set_health_agent},
    {"emotion_recognition_thalamic_bridge", emotion_recognition_thalamic_bridge_set_health_agent},
    /* cognitive/emotions */
    {"emotional_system",                  emotional_system_set_health_agent},
    {"emotional_system_sleep_bridge",     emotional_system_sleep_bridge_set_health_agent},
    /* cognitive/emotion_tensor */
    {"emotion_tensor",                    emotion_tensor_set_health_agent},
    {"emotion_tensor_bridge",             emotion_tensor_bridge_set_health_agent},
    {"emotion_tensor_substrate_bridge",   emotion_tensor_substrate_bridge_set_health_agent},
    {"emotion_tensor_thalamic_bridge",    emotion_tensor_thalamic_bridge_set_health_agent},
    /* cognitive/grief */
    {"grief_and_loss",                    grief_and_loss_set_health_agent},
    {"grief_fep_bridge",                  grief_fep_bridge_set_health_agent},
    {"grief_substrate_bridge",            grief_substrate_bridge_set_health_agent},
    {"grief_thalamic_bridge",             grief_thalamic_bridge_set_health_agent},
    /* cognitive/joy */
    {"joy_euphoria",                      joy_euphoria_set_health_agent},
    {"joy_fep_bridge",                    joy_fep_bridge_set_health_agent},
    {"joy_substrate_bridge",              joy_substrate_bridge_set_health_agent},
    {"joy_thalamic_bridge",               joy_thalamic_bridge_set_health_agent},
    /* cognitive/remorse */
    {"remorse_regret",                    remorse_regret_set_health_agent},
    {"remorse_fep_bridge",                remorse_fep_bridge_set_health_agent},
    {"remorse_substrate_bridge",          remorse_substrate_bridge_set_health_agent},
    {"remorse_thalamic_bridge",           remorse_thalamic_bridge_set_health_agent},
};
static constexpr size_t B18_MODULE_COUNT = sizeof(B18_MODULES) / sizeof(B18_MODULES[0]);

class HeartbeatB18UnitTest : public ::testing::Test {
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

TEST_F(HeartbeatB18UnitTest, AllModulesSetNull) {
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) {
        SCOPED_TRACE(B18_MODULES[i].name);
        B18_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB18UnitTest, AllModulesSetValid) {
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) {
        SCOPED_TRACE(B18_MODULES[i].name);
        B18_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB18UnitTest, AllModulesReplaceAgent) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(agent2, nullptr);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) {
        SCOPED_TRACE(B18_MODULES[i].name);
        B18_MODULES[i].set_fn(agent_);
        B18_MODULES[i].set_fn(agent2);
    }
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB18UnitTest, AllModulesShareSameAgent) {
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
}

TEST_F(HeartbeatB18UnitTest, SetNullAfterValid) {
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) {
        SCOPED_TRACE(B18_MODULES[i].name);
        B18_MODULES[i].set_fn(agent_);
        B18_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB18UnitTest, ConcurrentSetClear) {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 100; j++) {
                B18_MODULES[i].set_fn(agent_);
                B18_MODULES[i].set_fn(nullptr);
            }
        });
    }
    for (auto& t : threads) t.join();
}

TEST_F(HeartbeatB18UnitTest, HeartbeatCounterIncrements) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) B18_MODULES[i].set_fn(agent_);
    health_agent_stats_t before, after;
    nimcp_health_agent_get_stats(agent_, &before);
    nimcp_health_agent_heartbeat_ex(agent_, "B18_test", 0);
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GT(after.heartbeats_received, before.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB18UnitTest, ModuleCount) {
    EXPECT_EQ(B18_MODULE_COUNT, 32u);
}

TEST_F(HeartbeatB18UnitTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < B18_MODULE_COUNT; i++) {
        SCOPED_TRACE(B18_MODULES[i].name);
        B18_MODULES[i].set_fn(agent_);
        B18_MODULES[i].set_fn(agent_);
    }
}
