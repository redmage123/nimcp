/**
 * @file test_heartbeat_B07_game_theory_regression.cpp
 * @brief Regression tests for B07 (cognitive/game_theory) heartbeat API contract
 *
 * WHAT: API stability tests for all 17 cognitive game theory module setters
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

/* B07: cognitive/game_theory — 17 setter declarations */
void auction_set_health_agent(nimcp_health_agent_t* agent);
void bargaining_set_health_agent(nimcp_health_agent_t* agent);
void credit_assignment_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void gt_auction_ext_set_health_agent(nimcp_health_agent_t* agent);
void gt_coalition_set_health_agent(nimcp_health_agent_t* agent);
void gt_equilibrium_set_health_agent(nimcp_health_agent_t* agent);
void gt_fairness_set_health_agent(nimcp_health_agent_t* agent);
void gt_learning_set_health_agent(nimcp_health_agent_t* agent);
void gt_mechanism_set_health_agent(nimcp_health_agent_t* agent);
void gt_repeated_set_health_agent(nimcp_health_agent_t* agent);
void gt_spatial_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B07ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B07ModuleEntry B07_MODULES[] = {
    {"auction",                       auction_set_health_agent},
    {"bargaining",                    bargaining_set_health_agent},
    {"credit_assignment",             credit_assignment_set_health_agent},
    {"game_theory",                   game_theory_set_health_agent},
    {"game_theory_fep_bridge",        game_theory_fep_bridge_set_health_agent},
    {"game_theory_plasticity_bridge", game_theory_plasticity_bridge_set_health_agent},
    {"game_theory_snn_bridge",        game_theory_snn_bridge_set_health_agent},
    {"game_theory_substrate_bridge",  game_theory_substrate_bridge_set_health_agent},
    {"game_theory_thalamic_bridge",   game_theory_thalamic_bridge_set_health_agent},
    {"gt_auction_ext",                gt_auction_ext_set_health_agent},
    {"gt_coalition",                  gt_coalition_set_health_agent},
    {"gt_equilibrium",                gt_equilibrium_set_health_agent},
    {"gt_fairness",                   gt_fairness_set_health_agent},
    {"gt_learning",                   gt_learning_set_health_agent},
    {"gt_mechanism",                  gt_mechanism_set_health_agent},
    {"gt_repeated",                   gt_repeated_set_health_agent},
    {"gt_spatial",                    gt_spatial_set_health_agent},
};

static constexpr size_t B07_MODULE_COUNT = sizeof(B07_MODULES) / sizeof(B07_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        B07_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB07RegressionTest : public ::testing::Test {
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

TEST_F(HeartbeatB07RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        SCOPED_TRACE(B07_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB07RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        B07_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        SCOPED_TRACE(B07_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB07RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (int repeat = 0; repeat < 100; repeat++) {
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[0].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB07RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        SCOPED_TRACE(B07_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB07RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        B07_MODULES[i].set_fn(nullptr);
    }
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        SCOPED_TRACE(B07_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB07RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "active_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB07RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        B07_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "clear_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB07RegressionTest, RapidSetClearCycleAllModules) {
    const int cycles = 50;
    for (int c = 0; c < cycles; c++) {
        for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
            B07_MODULES[i].set_fn(agent);
        }
        for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
            B07_MODULES[i].set_fn(nullptr);
        }
    }
    SUCCEED();
}

TEST_F(HeartbeatB07RegressionTest, RapidSetClearCycleSingleModule) {
    const int cycles = 1000;
    for (int c = 0; c < cycles; c++) {
        B07_MODULES[0].set_fn(agent);
        B07_MODULES[0].set_fn(nullptr);
    }
    SUCCEED();
}

TEST_F(HeartbeatB07RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int round = 0; round < 5; round++) {
        nimcp_health_agent_t* temp_agent = nimcp_health_agent_create(&config);
        ASSERT_NE(temp_agent, nullptr);

        for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
            B07_MODULES[i].set_fn(temp_agent);
        }

        clear_all_modules();
        nimcp_health_agent_destroy(temp_agent);
    }
}

TEST_F(HeartbeatB07RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        B07_MODULES[i].set_fn(agent);
    }

    for (int cycle = 0; cycle < 5; cycle++) {
        int result = nimcp_health_agent_start(agent);
        ASSERT_EQ(result, 0);
        nimcp_health_agent_heartbeat_ex(agent, "cycle_test", 0.5f);
        result = nimcp_health_agent_stop(agent);
        ASSERT_EQ(result, 0);
    }
}

TEST_F(HeartbeatB07RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        set_health_agent_fn fn = B07_MODULES[i].set_fn;
        EXPECT_NE(fn, nullptr) << "Module " << B07_MODULES[i].name
                               << " has null function pointer";
    }
}

TEST_F(HeartbeatB07RegressionTest, StatsConsistentAfterModuleSetClear) {
    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        B07_MODULES[i].set_fn(agent);
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
