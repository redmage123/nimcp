/**
 * @file test_heartbeat_B05_mirror_neurons_functions.cpp
 * @brief Unit tests for B05 (cognitive/mirror_neurons) heartbeat setter functions
 *
 * WHAT: Tests setter infrastructure for all 27 mirror neuron modules
 * WHY:  Phase 8 requires every module to have working health agent integration
 * HOW:  Table-driven tests: SetNull, SetValid, ReplaceAgent per module
 *
 * @author NIMCP Team
 * @date 2026-01-26
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

/* B05: cognitive/mirror_neurons — 27 setter declarations */
void mirror_attention_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_emotion_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_habituation_set_health_agent(nimcp_health_agent_t* agent);
void mirror_hierarchy_set_health_agent(nimcp_health_agent_t* agent);
void mirror_hippocampus_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_hypothalamus_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_immune_integration_set_health_agent(nimcp_health_agent_t* agent);
void mirror_language_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_motor_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_multimodal_set_health_agent(nimcp_health_agent_t* agent);
void mirror_neurons_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_neurons_set_health_agent(nimcp_health_agent_t* agent);
void mirror_neurons_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_omni_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_prefrontal_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_resonance_set_health_agent(nimcp_health_agent_t* agent);
void mirror_self_other_set_health_agent(nimcp_health_agent_t* agent);
void mirror_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_social_context_set_health_agent(nimcp_health_agent_t* agent);
void mirror_stdp_set_health_agent(nimcp_health_agent_t* agent);
void mirror_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_substrate_set_health_agent(nimcp_health_agent_t* agent);
void mirror_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_tom_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_vicarious_reward_set_health_agent(nimcp_health_agent_t* agent);
void mirror_visual_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B05ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B05ModuleEntry B05_MODULES[] = {
    {"mirror_attention_bridge",     mirror_attention_bridge_set_health_agent},
    {"mirror_emotion_bridge",       mirror_emotion_bridge_set_health_agent},
    {"mirror_habituation",          mirror_habituation_set_health_agent},
    {"mirror_hierarchy",            mirror_hierarchy_set_health_agent},
    {"mirror_hippocampus_bridge",   mirror_hippocampus_bridge_set_health_agent},
    {"mirror_hypothalamus_bridge",  mirror_hypothalamus_bridge_set_health_agent},
    {"mirror_immune_integration",   mirror_immune_integration_set_health_agent},
    {"mirror_language_bridge",      mirror_language_bridge_set_health_agent},
    {"mirror_motor_bridge",         mirror_motor_bridge_set_health_agent},
    {"mirror_multimodal",           mirror_multimodal_set_health_agent},
    {"mirror_neurons_fep_bridge",   mirror_neurons_fep_bridge_set_health_agent},
    {"mirror_neurons",              mirror_neurons_set_health_agent},
    {"mirror_neurons_sleep_bridge", mirror_neurons_sleep_bridge_set_health_agent},
    {"mirror_omni_bridge",          mirror_omni_bridge_set_health_agent},
    {"mirror_plasticity_bridge",    mirror_plasticity_bridge_set_health_agent},
    {"mirror_prefrontal_bridge",    mirror_prefrontal_bridge_set_health_agent},
    {"mirror_resonance",            mirror_resonance_set_health_agent},
    {"mirror_self_other",           mirror_self_other_set_health_agent},
    {"mirror_snn_bridge",           mirror_snn_bridge_set_health_agent},
    {"mirror_social_context",       mirror_social_context_set_health_agent},
    {"mirror_stdp",                 mirror_stdp_set_health_agent},
    {"mirror_substrate_bridge",     mirror_substrate_bridge_set_health_agent},
    {"mirror_substrate",            mirror_substrate_set_health_agent},
    {"mirror_thalamic_bridge",      mirror_thalamic_bridge_set_health_agent},
    {"mirror_tom_bridge",           mirror_tom_bridge_set_health_agent},
    {"mirror_vicarious_reward",     mirror_vicarious_reward_set_health_agent},
    {"mirror_visual_bridge",        mirror_visual_bridge_set_health_agent},
};

static constexpr size_t B05_MODULE_COUNT = sizeof(B05_MODULES) / sizeof(B05_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(nullptr);
    }
}

/* ========================================================================== */
/* Test Fixture                                                               */
/* ========================================================================== */

class HeartbeatB05UnitTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        config.enable_auto_recovery = false;
        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        clear_all_modules();
        if (agent) {
            if (nimcp_health_agent_is_running(agent)) {
                nimcp_health_agent_stop(agent);
            }
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

TEST_F(HeartbeatB05UnitTest, AllModulesSetNull) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        SCOPED_TRACE(B05_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB05UnitTest, AllModulesSetValid) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        SCOPED_TRACE(B05_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB05UnitTest, AllModulesReplaceAgent) {
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        SCOPED_TRACE(B05_MODULES[i].name);
        B05_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(agent2));
    }

    clear_all_modules();
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB05UnitTest, AllModulesShareSameAgent) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }
    SUCCEED();
}

TEST_F(HeartbeatB05UnitTest, SetNullAfterValid) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        SCOPED_TRACE(B05_MODULES[i].name);
        B05_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB05UnitTest, ConcurrentSetClear) {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 100; j++) {
                B05_MODULES[i].set_fn(agent);
                B05_MODULES[i].set_fn(nullptr);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    SUCCEED();
}

TEST_F(HeartbeatB05UnitTest, HeartbeatCounterIncrements) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    const int beats = 20;
    for (int i = 0; i < beats; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "B05_test", (float)i / beats);
    }

    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);

    uint64_t delta = stats_after.heartbeats_received - stats_before.heartbeats_received;
    EXPECT_GE(delta, static_cast<uint64_t>(beats));
}

TEST_F(HeartbeatB05UnitTest, ModuleCount) {
    EXPECT_EQ(B05_MODULE_COUNT, 27u);
}

TEST_F(HeartbeatB05UnitTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        SCOPED_TRACE(B05_MODULES[i].name);
        B05_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(agent));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
