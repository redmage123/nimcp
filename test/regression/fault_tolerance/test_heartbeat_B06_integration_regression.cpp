/**
 * @file test_heartbeat_B06_integration_regression.cpp
 * @brief Regression tests for B06 (cognitive/integration) heartbeat API contract
 *
 * WHAT: API stability tests for all 24 cognitive integration module setters
 * WHY:  Ensure setter contract never breaks: NULL always accepted, valid always accepted
 * HOW:  Edge cases, boundary conditions, rapid cycling
 *
 * NOTE: cognitive_bio_async_bridge now included (mutex bug fixed, added to build)
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

/* B06: cognitive/integration — 24 setter declarations */
void attention_wm_bridge_set_health_agent(nimcp_health_agent_t* agent);
void cognitive_bio_async_bridge_set_health_agent(nimcp_health_agent_t* agent);
void cognitive_integration_fep_set_health_agent(nimcp_health_agent_t* agent);
void cognitive_integration_hub_set_health_agent(nimcp_health_agent_t* agent);
void collective_hub_bridge_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_reasoning_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_executive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_memory_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_executive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_executive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_executive_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void gw_cognitive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void imagination_reasoning_bridge_set_health_agent(nimcp_health_agent_t* agent);
void imagination_reasoning_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_empathy_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_empathy_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_attention_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_attention_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_hub_bridge_set_health_agent(nimcp_health_agent_t* agent);
void salience_attention_bridge_set_health_agent(nimcp_health_agent_t* agent);
void salience_attention_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void security_cognitive_hub_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_introspection_bridge_set_health_agent(nimcp_health_agent_t* agent);
void tom_social_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B06ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B06ModuleEntry B06_MODULES[] = {
    {"attention_wm_bridge",                  attention_wm_bridge_set_health_agent},
    {"cognitive_bio_async_bridge",           cognitive_bio_async_bridge_set_health_agent},
    {"cognitive_integration_fep",            cognitive_integration_fep_set_health_agent},
    {"cognitive_integration_hub",            cognitive_integration_hub_set_health_agent},
    {"collective_hub_bridge",                collective_hub_bridge_set_health_agent},
    {"curiosity_reasoning_bridge",           curiosity_reasoning_bridge_set_health_agent},
    {"emotion_executive_bridge",             emotion_executive_bridge_set_health_agent},
    {"emotion_memory_bridge",                emotion_memory_bridge_set_health_agent},
    {"ethics_executive_bridge",              ethics_executive_bridge_set_health_agent},
    {"game_theory_executive_bridge",         game_theory_executive_bridge_set_health_agent},
    {"game_theory_executive_fep_bridge",     game_theory_executive_fep_bridge_set_health_agent},
    {"gw_cognitive_bridge",                  gw_cognitive_bridge_set_health_agent},
    {"imagination_reasoning_bridge",         imagination_reasoning_bridge_set_health_agent},
    {"imagination_reasoning_fep_bridge",     imagination_reasoning_fep_bridge_set_health_agent},
    {"mirror_empathy_bridge",                mirror_empathy_bridge_set_health_agent},
    {"mirror_empathy_fep_bridge",            mirror_empathy_fep_bridge_set_health_agent},
    {"predictive_attention_bridge",          predictive_attention_bridge_set_health_agent},
    {"predictive_attention_fep_bridge",      predictive_attention_fep_bridge_set_health_agent},
    {"rcog_hub_bridge",                      rcog_hub_bridge_set_health_agent},
    {"salience_attention_bridge",            salience_attention_bridge_set_health_agent},
    {"salience_attention_fep_bridge",        salience_attention_fep_bridge_set_health_agent},
    {"security_cognitive_hub_bridge",        security_cognitive_hub_bridge_set_health_agent},
    {"self_introspection_bridge",            self_introspection_bridge_set_health_agent},
    {"tom_social_bridge",                    tom_social_bridge_set_health_agent},
};

static constexpr size_t B06_MODULE_COUNT = sizeof(B06_MODULES) / sizeof(B06_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB06RegressionTest : public ::testing::Test {
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

TEST_F(HeartbeatB06RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        SCOPED_TRACE(B06_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB06RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        SCOPED_TRACE(B06_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB06RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (int repeat = 0; repeat < 100; repeat++) {
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[0].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB06RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        SCOPED_TRACE(B06_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB06RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(nullptr);
    }
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        SCOPED_TRACE(B06_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB06RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "active_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB06RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "clear_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB06RegressionTest, RapidSetClearCycleAllModules) {
    const int cycles = 50;
    for (int c = 0; c < cycles; c++) {
        for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
            B06_MODULES[i].set_fn(agent);
        }
        for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
            B06_MODULES[i].set_fn(nullptr);
        }
    }
    SUCCEED();
}

TEST_F(HeartbeatB06RegressionTest, RapidSetClearCycleSingleModule) {
    const int cycles = 1000;
    for (int c = 0; c < cycles; c++) {
        B06_MODULES[0].set_fn(agent);
        B06_MODULES[0].set_fn(nullptr);
    }
    SUCCEED();
}

TEST_F(HeartbeatB06RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int round = 0; round < 5; round++) {
        nimcp_health_agent_t* temp_agent = nimcp_health_agent_create(&config);
        ASSERT_NE(temp_agent, nullptr);

        for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
            B06_MODULES[i].set_fn(temp_agent);
        }

        clear_all_modules();
        nimcp_health_agent_destroy(temp_agent);
    }
}

TEST_F(HeartbeatB06RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }

    for (int cycle = 0; cycle < 5; cycle++) {
        int result = nimcp_health_agent_start(agent);
        ASSERT_EQ(result, 0);
        nimcp_health_agent_heartbeat_ex(agent, "cycle_test", 0.5f);
        result = nimcp_health_agent_stop(agent);
        ASSERT_EQ(result, 0);
    }
}

TEST_F(HeartbeatB06RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        set_health_agent_fn fn = B06_MODULES[i].set_fn;
        EXPECT_NE(fn, nullptr) << "Module " << B06_MODULES[i].name
                               << " has null function pointer";
    }
}

TEST_F(HeartbeatB06RegressionTest, StatsConsistentAfterModuleSetClear) {
    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
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
