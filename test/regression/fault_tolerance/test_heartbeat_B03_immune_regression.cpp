/**
 * @file test_heartbeat_B03_immune_regression.cpp
 * @brief Regression tests for B03 (cognitive/immune) heartbeat API contract
 *
 * WHAT: API stability tests for all 39 immune module setters
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

/* B03: cognitive/immune — 39 setter declarations */
void attention_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobiographical_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_integration_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_plasticity_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_tick_set_health_agent(nimcp_health_agent_t* agent);
void claude_healer_set_health_agent(nimcp_health_agent_t* agent);
void code_immune_set_health_agent(nimcp_health_agent_t* agent);
void code_immune_self_repair_set_health_agent(nimcp_health_agent_t* agent);
void complement_system_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void heal_bridge_set_health_agent(nimcp_health_agent_t* agent);
void immune_bridge_coordinator_set_health_agent(nimcp_health_agent_t* agent);
void immune_exhaustion_set_health_agent(nimcp_health_agent_t* agent);
void immune_metrics_set_health_agent(nimcp_health_agent_t* agent);
void immune_persistence_set_health_agent(nimcp_health_agent_t* agent);
void immune_tolerance_set_health_agent(nimcp_health_agent_t* agent);
void immune_vaccine_set_health_agent(nimcp_health_agent_t* agent);
void introspection_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void memory_immune_integration_set_health_agent(nimcp_health_agent_t* agent);
void mental_health_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mucosal_immunity_set_health_agent(nimcp_health_agent_t* agent);
void omni_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void perception_immune_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_immune_set_health_agent(nimcp_health_agent_t* agent);
void regulatory_tcells_set_health_agent(nimcp_health_agent_t* agent);
void self_heal_set_health_agent(nimcp_health_agent_t* agent);
void self_model_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void sleep_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void surface_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void tom_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void trained_immunity_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B03ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B03ModuleEntry B03_MODULES[] = {
    {"attention_immune_bridge",          attention_immune_bridge_set_health_agent},
    {"autobiographical_immune_bridge",   autobiographical_immune_bridge_set_health_agent},
    {"brain_immune",                     brain_immune_set_health_agent},
    {"brain_immune_fep_bridge",          brain_immune_fep_bridge_set_health_agent},
    {"brain_immune_integration",         brain_immune_integration_set_health_agent},
    {"brain_immune_plasticity",          brain_immune_plasticity_set_health_agent},
    {"brain_immune_substrate_bridge",    brain_immune_substrate_bridge_set_health_agent},
    {"brain_immune_thalamic_bridge",     brain_immune_thalamic_bridge_set_health_agent},
    {"brain_immune_tick",                brain_immune_tick_set_health_agent},
    {"claude_healer",                    claude_healer_set_health_agent},
    {"code_immune",                      code_immune_set_health_agent},
    {"code_immune_self_repair",          code_immune_self_repair_set_health_agent},
    {"complement_system",                complement_system_set_health_agent},
    {"curiosity_immune_bridge",          curiosity_immune_bridge_set_health_agent},
    {"emotion_immune_bridge",            emotion_immune_bridge_set_health_agent},
    {"executive_immune_bridge",          executive_immune_bridge_set_health_agent},
    {"heal_bridge",                      heal_bridge_set_health_agent},
    {"immune_bridge_coordinator",        immune_bridge_coordinator_set_health_agent},
    {"immune_exhaustion",                immune_exhaustion_set_health_agent},
    {"immune_metrics",                   immune_metrics_set_health_agent},
    {"immune_persistence",               immune_persistence_set_health_agent},
    {"immune_tolerance",                 immune_tolerance_set_health_agent},
    {"immune_vaccine",                   immune_vaccine_set_health_agent},
    {"introspection_immune_bridge",      introspection_immune_bridge_set_health_agent},
    {"knowledge_immune_bridge",          knowledge_immune_bridge_set_health_agent},
    {"memory_immune_integration",        memory_immune_integration_set_health_agent},
    {"mental_health_immune_bridge",      mental_health_immune_bridge_set_health_agent},
    {"mucosal_immunity",                 mucosal_immunity_set_health_agent},
    {"omni_immune_bridge",               omni_immune_bridge_set_health_agent},
    {"perception_immune",                perception_immune_set_health_agent},
    {"reasoning_immune",                 reasoning_immune_set_health_agent},
    {"regulatory_tcells",                regulatory_tcells_set_health_agent},
    {"self_heal",                        self_heal_set_health_agent},
    {"self_model_immune_bridge",         self_model_immune_bridge_set_health_agent},
    {"sleep_immune_bridge",              sleep_immune_bridge_set_health_agent},
    {"surface_immune_bridge",            surface_immune_bridge_set_health_agent},
    {"tom_immune_bridge",                tom_immune_bridge_set_health_agent},
    {"trained_immunity",                 trained_immunity_set_health_agent},
    {"wellbeing_immune_bridge",          wellbeing_immune_bridge_set_health_agent},
};

static constexpr size_t B03_MODULE_COUNT = sizeof(B03_MODULES) / sizeof(B03_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB03RegressionTest : public ::testing::Test {
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

TEST_F(HeartbeatB03RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        SCOPED_TRACE(B03_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B03_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB03RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        SCOPED_TRACE(B03_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B03_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB03RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (int repeat = 0; repeat < 100; repeat++) {
        EXPECT_NO_FATAL_FAILURE(B03_MODULES[0].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB03RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        SCOPED_TRACE(B03_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B03_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB03RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(nullptr);
    }
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        SCOPED_TRACE(B03_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B03_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB03RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "active_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B03_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB03RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "clear_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B03_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB03RegressionTest, RapidSetClearCycleAllModules) {
    const int cycles = 50;
    for (int c = 0; c < cycles; c++) {
        for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
            B03_MODULES[i].set_fn(agent);
        }
        for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
            B03_MODULES[i].set_fn(nullptr);
        }
    }
    SUCCEED();
}

TEST_F(HeartbeatB03RegressionTest, RapidSetClearCycleSingleModule) {
    const int cycles = 1000;
    for (int c = 0; c < cycles; c++) {
        B03_MODULES[0].set_fn(agent);
        B03_MODULES[0].set_fn(nullptr);
    }
    SUCCEED();
}

TEST_F(HeartbeatB03RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int round = 0; round < 5; round++) {
        nimcp_health_agent_t* temp_agent = nimcp_health_agent_create(&config);
        ASSERT_NE(temp_agent, nullptr);

        for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
            B03_MODULES[i].set_fn(temp_agent);
        }

        clear_all_modules();
        nimcp_health_agent_destroy(temp_agent);
    }
}

TEST_F(HeartbeatB03RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }

    for (int cycle = 0; cycle < 5; cycle++) {
        int result = nimcp_health_agent_start(agent);
        ASSERT_EQ(result, 0);
        nimcp_health_agent_heartbeat_ex(agent, "cycle_test", 0.5f);
        result = nimcp_health_agent_stop(agent);
        ASSERT_EQ(result, 0);
    }
}

TEST_F(HeartbeatB03RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        set_health_agent_fn fn = B03_MODULES[i].set_fn;
        EXPECT_NE(fn, nullptr) << "Module " << B03_MODULES[i].name
                               << " has null function pointer";
    }
}

TEST_F(HeartbeatB03RegressionTest, StatsConsistentAfterModuleSetClear) {
    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
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
