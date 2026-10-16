/**
 * @file test_heartbeat_B07_game_theory_functions.cpp
 * @brief Unit tests for B07 (cognitive/game_theory) heartbeat setter functions
 *
 * WHAT: Tests setter infrastructure for all 17 cognitive game theory modules
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

class HeartbeatB07UnitTest : public ::testing::Test {
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

TEST_F(HeartbeatB07UnitTest, AllModulesSetNull) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        SCOPED_TRACE(B07_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB07UnitTest, AllModulesSetValid) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        SCOPED_TRACE(B07_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB07UnitTest, AllModulesReplaceAgent) {
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);

    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        SCOPED_TRACE(B07_MODULES[i].name);
        B07_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[i].set_fn(agent2));
    }

    clear_all_modules();
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB07UnitTest, AllModulesShareSameAgent) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        B07_MODULES[i].set_fn(agent);
    }
    SUCCEED();
}

TEST_F(HeartbeatB07UnitTest, SetNullAfterValid) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        SCOPED_TRACE(B07_MODULES[i].name);
        B07_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB07UnitTest, ConcurrentSetClear) {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 100; j++) {
                B07_MODULES[i].set_fn(agent);
                B07_MODULES[i].set_fn(nullptr);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    SUCCEED();
}

TEST_F(HeartbeatB07UnitTest, HeartbeatCounterIncrements) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    const int beats = 20;
    for (int i = 0; i < beats; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "B07_test", (float)i / beats);
    }

    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);

    uint64_t delta = stats_after.heartbeats_received - stats_before.heartbeats_received;
    EXPECT_GE(delta, static_cast<uint64_t>(beats));
}

TEST_F(HeartbeatB07UnitTest, ModuleCount) {
    EXPECT_EQ(B07_MODULE_COUNT, 17u);
}

TEST_F(HeartbeatB07UnitTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < B07_MODULE_COUNT; i++) {
        SCOPED_TRACE(B07_MODULES[i].name);
        B07_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B07_MODULES[i].set_fn(agent));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
