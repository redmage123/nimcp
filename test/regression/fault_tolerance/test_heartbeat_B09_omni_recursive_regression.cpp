/**
 * @file test_heartbeat_B09_omni_recursive_regression.cpp
 * @brief Regression tests for B09 heartbeat (cognitive/omni + cognitive/recursive)
 *
 * Tests edge cases, boundary conditions, and stability of health agent integration.
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

void omni_active_inference_set_health_agent(nimcp_health_agent_t* agent);
void omni_kg_sync_set_health_agent(nimcp_health_agent_t* agent);
void omni_metacognition_set_health_agent(nimcp_health_agent_t* agent);
void omni_precision_set_health_agent(nimcp_health_agent_t* agent);
void omni_wm_cognitive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_wm_hypothalamus_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_wm_kg_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_wm_logging_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_wm_memory_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_wm_parietal_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_wm_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_wm_security_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_wm_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_wm_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_wm_tom_bridge_set_health_agent(nimcp_health_agent_t* agent);
void omni_world_model_set_health_agent(nimcp_health_agent_t* agent);
void omni_rcog_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_answer_set_health_agent(nimcp_health_agent_t* agent);
void rcog_bio_async_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_brain_kg_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_collective_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_context_store_set_health_agent(nimcp_health_agent_t* agent);
void rcog_delegation_pool_set_health_agent(nimcp_health_agent_t* agent);
void rcog_engine_set_health_agent(nimcp_health_agent_t* agent);
void rcog_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_imagination_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_orchestrator_set_health_agent(nimcp_health_agent_t* agent);
void rcog_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_tool_router_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B09Module {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B09Module B09_MODULES[] = {
    {"omni_active_inference",          omni_active_inference_set_health_agent},
    {"omni_kg_sync",                   omni_kg_sync_set_health_agent},
    {"omni_metacognition",             omni_metacognition_set_health_agent},
    {"omni_precision",                 omni_precision_set_health_agent},
    {"omni_wm_cognitive_bridge",       omni_wm_cognitive_bridge_set_health_agent},
    {"omni_wm_hypothalamus_bridge",    omni_wm_hypothalamus_bridge_set_health_agent},
    {"omni_wm_kg_bridge",              omni_wm_kg_bridge_set_health_agent},
    {"omni_wm_logging_bridge",         omni_wm_logging_bridge_set_health_agent},
    {"omni_wm_memory_bridge",          omni_wm_memory_bridge_set_health_agent},
    {"omni_wm_parietal_bridge",        omni_wm_parietal_bridge_set_health_agent},
    {"omni_wm_plasticity_bridge",      omni_wm_plasticity_bridge_set_health_agent},
    {"omni_wm_security_immune_bridge", omni_wm_security_immune_bridge_set_health_agent},
    {"omni_wm_substrate_bridge",       omni_wm_substrate_bridge_set_health_agent},
    {"omni_wm_thalamic_bridge",        omni_wm_thalamic_bridge_set_health_agent},
    {"omni_wm_tom_bridge",             omni_wm_tom_bridge_set_health_agent},
    {"omni_world_model",               omni_world_model_set_health_agent},
    {"omni_rcog_bridge",               omni_rcog_bridge_set_health_agent},
    {"rcog_answer",                    rcog_answer_set_health_agent},
    {"rcog_bio_async_bridge",          rcog_bio_async_bridge_set_health_agent},
    {"rcog_brain_kg_bridge",           rcog_brain_kg_bridge_set_health_agent},
    {"rcog_collective_bridge",         rcog_collective_bridge_set_health_agent},
    {"rcog_context_store",             rcog_context_store_set_health_agent},
    {"rcog_delegation_pool",           rcog_delegation_pool_set_health_agent},
    {"rcog_engine",                    rcog_engine_set_health_agent},
    {"rcog_fep_bridge",                rcog_fep_bridge_set_health_agent},
    {"rcog_imagination_bridge",        rcog_imagination_bridge_set_health_agent},
    {"rcog_immune_bridge",             rcog_immune_bridge_set_health_agent},
    {"rcog_orchestrator",              rcog_orchestrator_set_health_agent},
    {"rcog_plasticity_bridge",         rcog_plasticity_bridge_set_health_agent},
    {"rcog_snn_bridge",                rcog_snn_bridge_set_health_agent},
    {"rcog_tool_router",               rcog_tool_router_set_health_agent},
};

static constexpr size_t B09_MODULE_COUNT = sizeof(B09_MODULES) / sizeof(B09_MODULES[0]);

class HeartbeatB09RegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent_ = nullptr;

    void SetUp() override {
        health_agent_config_t cfg;
        nimcp_health_agent_default_config(&cfg);
        cfg.check_interval_ms = 50;
        cfg.enable_auto_recovery = false;
        agent_ = nimcp_health_agent_create(&cfg);
        ASSERT_NE(agent_, nullptr);
    }

    void TearDown() override {
        for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
            B09_MODULES[i].set_fn(nullptr);
        }
        if (agent_) {
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }
};

TEST_F(HeartbeatB09RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        SCOPED_TRACE(B09_MODULES[i].name);
        B09_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB09RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        SCOPED_TRACE(B09_MODULES[i].name);
        B09_MODULES[i].set_fn(agent_);
        B09_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB09RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        SCOPED_TRACE(B09_MODULES[i].name);
        for (int j = 0; j < 10; j++) {
            B09_MODULES[i].set_fn(nullptr);
        }
    }
}

TEST_F(HeartbeatB09RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        SCOPED_TRACE(B09_MODULES[i].name);
        B09_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB09RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        SCOPED_TRACE(B09_MODULES[i].name);
        B09_MODULES[i].set_fn(nullptr);
        B09_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB09RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            B09_MODULES[i].set_fn(agent_);
            for (int j = 0; j < 20; j++) {
                nimcp_health_agent_heartbeat_ex(agent_, B09_MODULES[i].name, 0);
            }
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB09RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(agent_);
    }

    std::thread heartbeat_thread([this]() {
        for (int j = 0; j < 100; j++) {
            nimcp_health_agent_heartbeat_ex(agent_, "B09_regression", 0);
        }
    });

    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(nullptr);
    }

    heartbeat_thread.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB09RegressionTest, RapidSetClearCycleAllModules) {
    for (int cycle = 0; cycle < 50; cycle++) {
        for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
            B09_MODULES[i].set_fn(agent_);
        }
        for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
            B09_MODULES[i].set_fn(nullptr);
        }
    }
}

TEST_F(HeartbeatB09RegressionTest, RapidSetClearCycleSingleModule) {
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        SCOPED_TRACE(B09_MODULES[i].name);
        for (int cycle = 0; cycle < 100; cycle++) {
            B09_MODULES[i].set_fn(agent_);
            B09_MODULES[i].set_fn(nullptr);
        }
    }
}

TEST_F(HeartbeatB09RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int cycle = 0; cycle < 5; cycle++) {
        health_agent_config_t cfg_temp;
        nimcp_health_agent_default_config(&cfg_temp);
        nimcp_health_agent_t* temp = nimcp_health_agent_create(&cfg_temp);
        ASSERT_NE(temp, nullptr);
        for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
            B09_MODULES[i].set_fn(temp);
        }
        for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
            B09_MODULES[i].set_fn(nullptr);
        }
        nimcp_health_agent_destroy(temp);
    }
}

TEST_F(HeartbeatB09RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(agent_);
    }
    for (int cycle = 0; cycle < 5; cycle++) {
        nimcp_health_agent_start(agent_);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        nimcp_health_agent_stop(agent_);
    }
}

TEST_F(HeartbeatB09RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        SCOPED_TRACE(B09_MODULES[i].name);
        EXPECT_NE(B09_MODULES[i].set_fn, nullptr);
        B09_MODULES[i].set_fn(agent_);
        B09_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB09RegressionTest, StatsConsistentAfterModuleSetClear) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(agent_);
    }

    health_agent_stats_t stats1;
    nimcp_health_agent_get_stats(agent_, &stats1);
    nimcp_health_agent_heartbeat_ex(agent_, "B09_consistency", 0);

    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent_, &stats2);
    EXPECT_GE(stats2.heartbeats_received, stats1.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}
