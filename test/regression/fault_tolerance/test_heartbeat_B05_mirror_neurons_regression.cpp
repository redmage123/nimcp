/**
 * @file test_heartbeat_B05_mirror_neurons_regression.cpp
 * @brief Regression tests for B05 (cognitive/mirror_neurons) heartbeat API contract
 *
 * WHAT: API stability tests for all 27 mirror neuron module setters
 * WHY:  Ensure setter contract never breaks: NULL always accepted, valid always accepted
 * HOW:  Edge cases, boundary conditions, rapid cycling
 *
 * @author NIMCP Team
 * @date 2026-01-26
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>

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

class HeartbeatB05RegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        config.enable_auto_recovery = false;
        config.heartbeat_interval_ms = 100;
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

TEST_F(HeartbeatB05RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        SCOPED_TRACE(B05_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB05RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        SCOPED_TRACE(B05_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB05RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (int repeat = 0; repeat < 100; repeat++) {
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[0].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB05RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        SCOPED_TRACE(B05_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB05RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(nullptr);
    }
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        SCOPED_TRACE(B05_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB05RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "active_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB05RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "clear_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB05RegressionTest, RapidSetClearCycleAllModules) {
    const int cycles = 50;
    for (int c = 0; c < cycles; c++) {
        for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
            B05_MODULES[i].set_fn(agent);
        }
        for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
            B05_MODULES[i].set_fn(nullptr);
        }
    }
    SUCCEED();
}

TEST_F(HeartbeatB05RegressionTest, RapidSetClearCycleSingleModule) {
    const int cycles = 1000;
    for (int c = 0; c < cycles; c++) {
        B05_MODULES[0].set_fn(agent);
        B05_MODULES[0].set_fn(nullptr);
    }
    SUCCEED();
}

TEST_F(HeartbeatB05RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int round = 0; round < 5; round++) {
        nimcp_health_agent_t* temp_agent = nimcp_health_agent_create(&config);
        ASSERT_NE(temp_agent, nullptr);

        for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
            B05_MODULES[i].set_fn(temp_agent);
        }

        clear_all_modules();
        nimcp_health_agent_destroy(temp_agent);
    }
}

TEST_F(HeartbeatB05RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }

    for (int cycle = 0; cycle < 5; cycle++) {
        int result = nimcp_health_agent_start(agent);
        ASSERT_EQ(result, 0);
        nimcp_health_agent_heartbeat_ex(agent, "cycle_test", 0.5f);
        result = nimcp_health_agent_stop(agent);
        ASSERT_EQ(result, 0);
    }
}

TEST_F(HeartbeatB05RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        set_health_agent_fn fn = B05_MODULES[i].set_fn;
        EXPECT_NE(fn, nullptr) << "Module " << B05_MODULES[i].name
                               << " has null function pointer";
    }
}

TEST_F(HeartbeatB05RegressionTest, StatsConsistentAfterModuleSetClear) {
    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }
    clear_all_modules();

    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);
    EXPECT_EQ(stats_before.heartbeats_received, stats_after.heartbeats_received);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
