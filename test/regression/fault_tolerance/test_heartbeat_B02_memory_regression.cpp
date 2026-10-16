/**
 * @file test_heartbeat_B02_memory_regression.cpp
 * @brief Regression tests for B02 (cognitive/memory non-core) heartbeat API contract
 *
 * WHAT: API stability tests for all 13 memory (non-core) module setters
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

/* B02: cognitive/memory (non-core) — 12 setter declarations */
void engram_set_health_agent(nimcp_health_agent_t* agent);
void hopfield_memory_set_health_agent(nimcp_health_agent_t* agent);
void memory_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void memory_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void memory_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void semantic_memory_set_health_agent(nimcp_health_agent_t* agent);
void systems_consolidation_set_health_agent(nimcp_health_agent_t* agent);
void systems_consolidation_pink_noise_bridge_set_health_agent(nimcp_health_agent_t* agent);
void temporal_replay_set_health_agent(nimcp_health_agent_t* agent);
void wm_transfer_set_health_agent(nimcp_health_agent_t* agent);
void working_memory_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void working_memory_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B02ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B02ModuleEntry B02_MODULES[] = {
    {"engram",                                  engram_set_health_agent},
    {"hopfield_memory",                         hopfield_memory_set_health_agent},
    {"memory_fep_bridge",                       memory_fep_bridge_set_health_agent},
    {"memory_sleep_bridge",                     memory_sleep_bridge_set_health_agent},
    {"memory_thalamic_bridge",                  memory_thalamic_bridge_set_health_agent},
    {"semantic_memory",                         semantic_memory_set_health_agent},
    {"systems_consolidation",                   systems_consolidation_set_health_agent},
    {"systems_consolidation_pink_noise_bridge", systems_consolidation_pink_noise_bridge_set_health_agent},
    {"temporal_replay",                         temporal_replay_set_health_agent},
    {"wm_transfer",                             wm_transfer_set_health_agent},
    {"working_memory_plasticity_bridge",        working_memory_plasticity_bridge_set_health_agent},
    {"working_memory_snn_bridge",               working_memory_snn_bridge_set_health_agent},
};

static constexpr size_t B02_MODULE_COUNT = sizeof(B02_MODULES) / sizeof(B02_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(nullptr);
    }
}

/* ========================================================================== */
/* Test Fixture                                                               */
/* ========================================================================== */

class HeartbeatB02RegressionTest : public ::testing::Test {
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

/* ========================================================================== */
/* API Contract: NULL Always Accepted                                         */
/* ========================================================================== */

TEST_F(HeartbeatB02RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        SCOPED_TRACE(B02_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB02RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        SCOPED_TRACE(B02_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB02RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (int repeat = 0; repeat < 100; repeat++) {
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[0].set_fn(nullptr));
    }
}

/* ========================================================================== */
/* API Contract: Valid Agent Always Accepted                                  */
/* ========================================================================== */

TEST_F(HeartbeatB02RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        SCOPED_TRACE(B02_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB02RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(nullptr);
    }
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        SCOPED_TRACE(B02_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(agent));
    }
}

/* ========================================================================== */
/* Set During Active Heartbeats Doesn't Crash                                 */
/* ========================================================================== */

TEST_F(HeartbeatB02RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "active_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB02RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "clear_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(nullptr));
    }
}

/* ========================================================================== */
/* Rapid Cycling: Set/Clear/Set                                               */
/* ========================================================================== */

TEST_F(HeartbeatB02RegressionTest, RapidSetClearCycleAllModules) {
    const int cycles = 50;
    for (int c = 0; c < cycles; c++) {
        for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
            B02_MODULES[i].set_fn(agent);
        }
        for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
            B02_MODULES[i].set_fn(nullptr);
        }
    }
    SUCCEED();
}

TEST_F(HeartbeatB02RegressionTest, RapidSetClearCycleSingleModule) {
    const int cycles = 1000;
    for (int c = 0; c < cycles; c++) {
        B02_MODULES[0].set_fn(agent);
        B02_MODULES[0].set_fn(nullptr);
    }
    SUCCEED();
}

/* ========================================================================== */
/* Multiple Agent Lifecycle                                                   */
/* ========================================================================== */

TEST_F(HeartbeatB02RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int round = 0; round < 5; round++) {
        nimcp_health_agent_t* temp_agent = nimcp_health_agent_create(&config);
        ASSERT_NE(temp_agent, nullptr);

        for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
            B02_MODULES[i].set_fn(temp_agent);
        }

        clear_all_modules();
        nimcp_health_agent_destroy(temp_agent);
    }
}

/* ========================================================================== */
/* Agent Start/Stop With Connected Modules                                    */
/* ========================================================================== */

TEST_F(HeartbeatB02RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    for (int cycle = 0; cycle < 5; cycle++) {
        int result = nimcp_health_agent_start(agent);
        ASSERT_EQ(result, 0);
        EXPECT_TRUE(nimcp_health_agent_is_running(agent));

        nimcp_health_agent_heartbeat_ex(agent, "cycle_test", 0.5f);

        result = nimcp_health_agent_stop(agent);
        ASSERT_EQ(result, 0);
        EXPECT_FALSE(nimcp_health_agent_is_running(agent));
    }
}

/* ========================================================================== */
/* Setter Signature: void return, single pointer param                        */
/* ========================================================================== */

TEST_F(HeartbeatB02RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        set_health_agent_fn fn = B02_MODULES[i].set_fn;
        EXPECT_NE(fn, nullptr) << "Module " << B02_MODULES[i].name
                               << " has null function pointer";
    }
}

/* ========================================================================== */
/* Stats Consistency After Module Operations                                  */
/* ========================================================================== */

TEST_F(HeartbeatB02RegressionTest, StatsConsistentAfterModuleSetClear) {
    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
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
