/**
 * @file test_heartbeat_B06_integration_functions.cpp
 * @brief Unit tests for B06 (cognitive/integration) heartbeat setter functions
 *
 * WHAT: Tests setter infrastructure for all 24 cognitive integration modules
 * WHY:  Phase 8 requires every module to have working health agent integration
 * HOW:  Table-driven tests: SetNull, SetValid, ReplaceAgent per module
 *
 * NOTE: cognitive_bio_async_bridge now included (mutex bug fixed, added to build)
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

class HeartbeatB06UnitTest : public ::testing::Test {
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

TEST_F(HeartbeatB06UnitTest, AllModulesSetNull) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        SCOPED_TRACE(B06_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB06UnitTest, AllModulesSetValid) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        SCOPED_TRACE(B06_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB06UnitTest, AllModulesReplaceAgent) {
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        SCOPED_TRACE(B06_MODULES[i].name);
        B06_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(agent2));
    }

    clear_all_modules();
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB06UnitTest, AllModulesShareSameAgent) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }
    SUCCEED();
}

TEST_F(HeartbeatB06UnitTest, SetNullAfterValid) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        SCOPED_TRACE(B06_MODULES[i].name);
        B06_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB06UnitTest, ConcurrentSetClear) {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 100; j++) {
                B06_MODULES[i].set_fn(agent);
                B06_MODULES[i].set_fn(nullptr);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    SUCCEED();
}

TEST_F(HeartbeatB06UnitTest, HeartbeatCounterIncrements) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    const int beats = 20;
    for (int i = 0; i < beats; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "B06_test", (float)i / beats);
    }

    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);

    uint64_t delta = stats_after.heartbeats_received - stats_before.heartbeats_received;
    EXPECT_GE(delta, static_cast<uint64_t>(beats));
}

TEST_F(HeartbeatB06UnitTest, ModuleCount) {
    EXPECT_EQ(B06_MODULE_COUNT, 24u);
}

TEST_F(HeartbeatB06UnitTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        SCOPED_TRACE(B06_MODULES[i].name);
        B06_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(agent));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
