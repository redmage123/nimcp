/**
 * @file test_heartbeat_B04_parietal_regression.cpp
 * @brief Regression tests for B04 (cognitive/parietal) heartbeat API contract
 *
 * WHAT: API stability tests for all 34 parietal module setters
 * WHY:  Ensure setter contract never breaks: NULL always accepted, valid always accepted
 * HOW:  Edge cases, boundary conditions, rapid cycling
 *
 * NOTE: physics_nn excluded — not yet compiled (NOT YET IMPLEMENTED in CMakeLists.txt)
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

/* B04: cognitive/parietal — 34 setter declarations */
void analogical_reasoning_set_health_agent(nimcp_health_agent_t* agent);
void biology_set_health_agent(nimcp_health_agent_t* agent);
void chemistry_set_health_agent(nimcp_health_agent_t* agent);
void civil_engineering_set_health_agent(nimcp_health_agent_t* agent);
void code_generation_set_health_agent(nimcp_health_agent_t* agent);
void conceptual_blending_set_health_agent(nimcp_health_agent_t* agent);
void electrical_engineering_set_health_agent(nimcp_health_agent_t* agent);
void equation_manipulation_set_health_agent(nimcp_health_agent_t* agent);
void fep_parietal_bridge_set_health_agent(nimcp_health_agent_t* agent);
void genius_erdos_set_health_agent(nimcp_health_agent_t* agent);
void genius_gauss_set_health_agent(nimcp_health_agent_t* agent);
void genius_newton_set_health_agent(nimcp_health_agent_t* agent);
void genius_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void genius_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void genius_training_bridge_set_health_agent(nimcp_health_agent_t* agent);
void hypothesis_generation_set_health_agent(nimcp_health_agent_t* agent);
void insight_discovery_set_health_agent(nimcp_health_agent_t* agent);
void intuition_integrations_set_health_agent(nimcp_health_agent_t* agent);
void intuition_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void intuition_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void intuitive_reasoning_set_health_agent(nimcp_health_agent_t* agent);
void mathematical_genius_set_health_agent(nimcp_health_agent_t* agent);
void mathematical_intuition_set_health_agent(nimcp_health_agent_t* agent);
void mechanical_engineering_set_health_agent(nimcp_health_agent_t* agent);
void meta_reasoning_set_health_agent(nimcp_health_agent_t* agent);
void number_sense_set_health_agent(nimcp_health_agent_t* agent);
void parietal_set_health_agent(nimcp_health_agent_t* agent);
void parietal_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void parietal_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void parietal_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void parietal_training_bridge_set_health_agent(nimcp_health_agent_t* agent);
void scientific_reasoning_set_health_agent(nimcp_health_agent_t* agent);
void software_engineering_set_health_agent(nimcp_health_agent_t* agent);
void spatial_reasoning_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B04ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B04ModuleEntry B04_MODULES[] = {
    {"analogical_reasoning",       analogical_reasoning_set_health_agent},
    {"biology",                    biology_set_health_agent},
    {"chemistry",                  chemistry_set_health_agent},
    {"civil_engineering",          civil_engineering_set_health_agent},
    {"code_generation",            code_generation_set_health_agent},
    {"conceptual_blending",        conceptual_blending_set_health_agent},
    {"electrical_engineering",     electrical_engineering_set_health_agent},
    {"equation_manipulation",      equation_manipulation_set_health_agent},
    {"fep_parietal_bridge",        fep_parietal_bridge_set_health_agent},
    {"genius_erdos",               genius_erdos_set_health_agent},
    {"genius_gauss",               genius_gauss_set_health_agent},
    {"genius_newton",              genius_newton_set_health_agent},
    {"genius_plasticity_bridge",   genius_plasticity_bridge_set_health_agent},
    {"genius_snn_bridge",          genius_snn_bridge_set_health_agent},
    {"genius_training_bridge",     genius_training_bridge_set_health_agent},
    {"hypothesis_generation",      hypothesis_generation_set_health_agent},
    {"insight_discovery",          insight_discovery_set_health_agent},
    {"intuition_integrations",     intuition_integrations_set_health_agent},
    {"intuition_substrate_bridge", intuition_substrate_bridge_set_health_agent},
    {"intuition_thalamic_bridge",  intuition_thalamic_bridge_set_health_agent},
    {"intuitive_reasoning",        intuitive_reasoning_set_health_agent},
    {"mathematical_genius",        mathematical_genius_set_health_agent},
    {"mathematical_intuition",     mathematical_intuition_set_health_agent},
    {"mechanical_engineering",     mechanical_engineering_set_health_agent},
    {"meta_reasoning",             meta_reasoning_set_health_agent},
    {"number_sense",               number_sense_set_health_agent},
    {"parietal",                   parietal_set_health_agent},
    {"parietal_fep_bridge",        parietal_fep_bridge_set_health_agent},
    {"parietal_plasticity_bridge", parietal_plasticity_bridge_set_health_agent},
    {"parietal_snn_bridge",        parietal_snn_bridge_set_health_agent},
    {"parietal_training_bridge",   parietal_training_bridge_set_health_agent},
    {"scientific_reasoning",       scientific_reasoning_set_health_agent},
    {"software_engineering",       software_engineering_set_health_agent},
    {"spatial_reasoning",          spatial_reasoning_set_health_agent},
};

static constexpr size_t B04_MODULE_COUNT = sizeof(B04_MODULES) / sizeof(B04_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB04RegressionTest : public ::testing::Test {
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

TEST_F(HeartbeatB04RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        SCOPED_TRACE(B04_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B04_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB04RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        SCOPED_TRACE(B04_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B04_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB04RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (int repeat = 0; repeat < 100; repeat++) {
        EXPECT_NO_FATAL_FAILURE(B04_MODULES[0].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB04RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        SCOPED_TRACE(B04_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B04_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB04RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(nullptr);
    }
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        SCOPED_TRACE(B04_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B04_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB04RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "active_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B04_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB04RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "clear_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B04_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB04RegressionTest, RapidSetClearCycleAllModules) {
    const int cycles = 50;
    for (int c = 0; c < cycles; c++) {
        for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
            B04_MODULES[i].set_fn(agent);
        }
        for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
            B04_MODULES[i].set_fn(nullptr);
        }
    }
    SUCCEED();
}

TEST_F(HeartbeatB04RegressionTest, RapidSetClearCycleSingleModule) {
    const int cycles = 1000;
    for (int c = 0; c < cycles; c++) {
        B04_MODULES[0].set_fn(agent);
        B04_MODULES[0].set_fn(nullptr);
    }
    SUCCEED();
}

TEST_F(HeartbeatB04RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int round = 0; round < 5; round++) {
        nimcp_health_agent_t* temp_agent = nimcp_health_agent_create(&config);
        ASSERT_NE(temp_agent, nullptr);

        for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
            B04_MODULES[i].set_fn(temp_agent);
        }

        clear_all_modules();
        nimcp_health_agent_destroy(temp_agent);
    }
}

TEST_F(HeartbeatB04RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent);
    }

    for (int cycle = 0; cycle < 5; cycle++) {
        int result = nimcp_health_agent_start(agent);
        ASSERT_EQ(result, 0);
        nimcp_health_agent_heartbeat_ex(agent, "cycle_test", 0.5f);
        result = nimcp_health_agent_stop(agent);
        ASSERT_EQ(result, 0);
    }
}

TEST_F(HeartbeatB04RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        set_health_agent_fn fn = B04_MODULES[i].set_fn;
        EXPECT_NE(fn, nullptr) << "Module " << B04_MODULES[i].name
                               << " has null function pointer";
    }
}

TEST_F(HeartbeatB04RegressionTest, StatsConsistentAfterModuleSetClear) {
    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent);
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
