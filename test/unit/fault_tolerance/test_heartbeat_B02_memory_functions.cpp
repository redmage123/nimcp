/**
 * @file test_heartbeat_B02_memory_functions.cpp
 * @brief Unit tests for B02 (cognitive/memory non-core) heartbeat setter functions
 *
 * WHAT: Tests setter infrastructure for all 13 memory (non-core) modules
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

/* Setter function pointer type */
typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

/* Module entry for table-driven tests */
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

class HeartbeatB02UnitTest : public ::testing::Test {
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

/* ========================================================================== */
/* Set NULL (initial state)                                                   */
/* ========================================================================== */

TEST_F(HeartbeatB02UnitTest, AllModulesSetNull) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        SCOPED_TRACE(B02_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(nullptr));
    }
}

/* ========================================================================== */
/* Set Valid Agent                                                            */
/* ========================================================================== */

TEST_F(HeartbeatB02UnitTest, AllModulesSetValid) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        SCOPED_TRACE(B02_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(agent));
    }
}

/* ========================================================================== */
/* Replace Agent                                                              */
/* ========================================================================== */

TEST_F(HeartbeatB02UnitTest, AllModulesReplaceAgent) {
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        SCOPED_TRACE(B02_MODULES[i].name);
        B02_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(agent2));
    }

    clear_all_modules();
    nimcp_health_agent_destroy(agent2);
}

/* ========================================================================== */
/* Share Same Agent Across All Modules                                        */
/* ========================================================================== */

TEST_F(HeartbeatB02UnitTest, AllModulesShareSameAgent) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }
    /* All modules pointing to same agent — no crash */
    SUCCEED();
}

/* ========================================================================== */
/* Set NULL After Valid                                                        */
/* ========================================================================== */

TEST_F(HeartbeatB02UnitTest, SetNullAfterValid) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        SCOPED_TRACE(B02_MODULES[i].name);
        B02_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(nullptr));
    }
}

/* ========================================================================== */
/* Concurrent Set/Clear                                                       */
/* ========================================================================== */

TEST_F(HeartbeatB02UnitTest, ConcurrentSetClear) {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 100; j++) {
                B02_MODULES[i].set_fn(agent);
                B02_MODULES[i].set_fn(nullptr);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    SUCCEED();
}

/* ========================================================================== */
/* Heartbeat Counter Increments                                               */
/* ========================================================================== */

TEST_F(HeartbeatB02UnitTest, HeartbeatCounterIncrements) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    const int beats = 20;
    for (int i = 0; i < beats; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "B02_test", (float)i / beats);
    }

    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);

    uint64_t delta = stats_after.heartbeats_received - stats_before.heartbeats_received;
    EXPECT_GE(delta, static_cast<uint64_t>(beats));
}

/* ========================================================================== */
/* Module Count Validation                                                    */
/* ========================================================================== */

TEST_F(HeartbeatB02UnitTest, ModuleCount) {
    EXPECT_EQ(B02_MODULE_COUNT, 12u);
}

/* ========================================================================== */
/* Double Set Same Agent (Idempotent)                                         */
/* ========================================================================== */

TEST_F(HeartbeatB02UnitTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        SCOPED_TRACE(B02_MODULES[i].name);
        B02_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(agent));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
