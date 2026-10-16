/**
 * @file test_heartbeat_B08_ethics_reasoning_regression.cpp
 * @brief Regression tests for B08 (cognitive/ethics + cognitive/reasoning) heartbeat API contract
 *
 * WHAT: API stability tests for all 30 ethics and reasoning module setters
 * WHY:  Ensure setter contract never breaks: NULL always accepted, valid always accepted
 * HOW:  Edge cases, boundary conditions, rapid cycling
 *
 * NOTE: core_directives excluded — not compiled into library
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

/* B08: cognitive/ethics + cognitive/reasoning — 30 setter declarations */
void backward_chaining_set_health_agent(nimcp_health_agent_t* agent);
void combinatorial_harm_set_health_agent(nimcp_health_agent_t* agent);
void ethics_asimov_set_health_agent(nimcp_health_agent_t* agent);
void ethics_evaluation_set_health_agent(nimcp_health_agent_t* agent);
void ethics_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_hyperbolic_set_health_agent(nimcp_health_agent_t* agent);
void ethics_immune_set_health_agent(nimcp_health_agent_t* agent);
void ethics_incidents_set_health_agent(nimcp_health_agent_t* agent);
void ethics_learning_set_health_agent(nimcp_health_agent_t* agent);
void ethics_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_policies_set_health_agent(nimcp_health_agent_t* agent);
void ethics_set_health_agent(nimcp_health_agent_t* agent);
void ethics_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_warfare_set_health_agent(nimcp_health_agent_t* agent);
void forward_chaining_set_health_agent(nimcp_health_agent_t* agent);
void health_ethics_bridge_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_base_interface_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_factory_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_integration_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_attachment_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_brain_integration_set_health_agent(nimcp_health_agent_t* agent);
void unification_engine_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B08ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B08ModuleEntry B08_MODULES[] = {
    {"backward_chaining",                backward_chaining_set_health_agent},
    {"combinatorial_harm",               combinatorial_harm_set_health_agent},
    {"ethics",                           ethics_set_health_agent},
    {"ethics_asimov",                    ethics_asimov_set_health_agent},
    {"ethics_evaluation",                ethics_evaluation_set_health_agent},
    {"ethics_fep_bridge",                ethics_fep_bridge_set_health_agent},
    {"ethics_hyperbolic",                ethics_hyperbolic_set_health_agent},
    {"ethics_immune",                    ethics_immune_set_health_agent},
    {"ethics_incidents",                 ethics_incidents_set_health_agent},
    {"ethics_learning",                  ethics_learning_set_health_agent},
    {"ethics_plasticity_bridge",         ethics_plasticity_bridge_set_health_agent},
    {"ethics_policies",                  ethics_policies_set_health_agent},
    {"ethics_snn_bridge",                ethics_snn_bridge_set_health_agent},
    {"ethics_substrate_bridge",          ethics_substrate_bridge_set_health_agent},
    {"ethics_thalamic_bridge",           ethics_thalamic_bridge_set_health_agent},
    {"ethics_warfare",                   ethics_warfare_set_health_agent},
    {"forward_chaining",                 forward_chaining_set_health_agent},
    {"health_ethics_bridge",             health_ethics_bridge_set_health_agent},
    {"knowledge_base_interface",         knowledge_base_interface_set_health_agent},
    {"reasoning_factory",                reasoning_factory_set_health_agent},
    {"reasoning_fep_bridge",             reasoning_fep_bridge_set_health_agent},
    {"reasoning_integration",            reasoning_integration_set_health_agent},
    {"reasoning_plasticity_bridge",      reasoning_plasticity_bridge_set_health_agent},
    {"reasoning_sleep_bridge",           reasoning_sleep_bridge_set_health_agent},
    {"reasoning_snn_bridge",             reasoning_snn_bridge_set_health_agent},
    {"reasoning_substrate_bridge",       reasoning_substrate_bridge_set_health_agent},
    {"reasoning_thalamic_bridge",        reasoning_thalamic_bridge_set_health_agent},
    {"symbolic_logic_attachment",        symbolic_logic_attachment_set_health_agent},
    {"symbolic_logic_brain_integration", symbolic_logic_brain_integration_set_health_agent},
    {"unification_engine",               unification_engine_set_health_agent},
};

static constexpr size_t B08_MODULE_COUNT = sizeof(B08_MODULES) / sizeof(B08_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB08RegressionTest : public ::testing::Test {
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

TEST_F(HeartbeatB08RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        SCOPED_TRACE(B08_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B08_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB08RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        SCOPED_TRACE(B08_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B08_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB08RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (int repeat = 0; repeat < 100; repeat++) {
        EXPECT_NO_FATAL_FAILURE(B08_MODULES[0].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB08RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        SCOPED_TRACE(B08_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B08_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB08RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(nullptr);
    }
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        SCOPED_TRACE(B08_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B08_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB08RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "active_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B08_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB08RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "clear_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B08_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB08RegressionTest, RapidSetClearCycleAllModules) {
    const int cycles = 50;
    for (int c = 0; c < cycles; c++) {
        for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
            B08_MODULES[i].set_fn(agent);
        }
        for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
            B08_MODULES[i].set_fn(nullptr);
        }
    }
    SUCCEED();
}

TEST_F(HeartbeatB08RegressionTest, RapidSetClearCycleSingleModule) {
    const int cycles = 1000;
    for (int c = 0; c < cycles; c++) {
        B08_MODULES[0].set_fn(agent);
        B08_MODULES[0].set_fn(nullptr);
    }
    SUCCEED();
}

TEST_F(HeartbeatB08RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int round = 0; round < 5; round++) {
        nimcp_health_agent_t* temp_agent = nimcp_health_agent_create(&config);
        ASSERT_NE(temp_agent, nullptr);

        for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
            B08_MODULES[i].set_fn(temp_agent);
        }

        clear_all_modules();
        nimcp_health_agent_destroy(temp_agent);
    }
}

TEST_F(HeartbeatB08RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent);
    }

    for (int cycle = 0; cycle < 5; cycle++) {
        int result = nimcp_health_agent_start(agent);
        ASSERT_EQ(result, 0);
        nimcp_health_agent_heartbeat_ex(agent, "cycle_test", 0.5f);
        result = nimcp_health_agent_stop(agent);
        ASSERT_EQ(result, 0);
    }
}

TEST_F(HeartbeatB08RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        set_health_agent_fn fn = B08_MODULES[i].set_fn;
        EXPECT_NE(fn, nullptr) << "Module " << B08_MODULES[i].name
                               << " has null function pointer";
    }
}

TEST_F(HeartbeatB08RegressionTest, StatsConsistentAfterModuleSetClear) {
    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent);
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
