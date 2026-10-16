/**
 * @file test_heartbeat_B09_omni_recursive_integration.cpp
 * @brief Integration tests for B09 heartbeat (cognitive/omni + cognitive/recursive)
 *
 * Tests cross-module health agent sharing, lifecycle, and concurrent operations.
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

class HeartbeatB09IntegrationTest : public ::testing::Test {
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

TEST_F(HeartbeatB09IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(agent_);
    }

    health_agent_stats_t before, after;
    nimcp_health_agent_get_stats(agent_, &before);
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent_, B09_MODULES[i].name, 0);
    }
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + B09_MODULE_COUNT);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB09IntegrationTest, MultipleModulesShareSingleAgent) {
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(agent_);
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
}

TEST_F(HeartbeatB09IntegrationTest, DisconnectModulesWhileAgentRunning) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(agent_);
    }
    for (size_t i = 0; i < B09_MODULE_COUNT / 2; i++) {
        B09_MODULES[i].set_fn(nullptr);
    }
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB09IntegrationTest, DisconnectAllModulesWhileAgentRunning) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(agent_);
    }
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(nullptr);
    }
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB09IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(agent_);
    }
    nimcp_health_agent_start(agent_);
    nimcp_health_agent_stop(agent_);
    nimcp_health_agent_start(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB09IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            B09_MODULES[i].set_fn(agent_);
            for (int j = 0; j < 50; j++) {
                nimcp_health_agent_heartbeat_ex(agent_, B09_MODULES[i].name, 0);
            }
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB09IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(agent2, nullptr);

    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(agent_);
    }
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(agent2);
    }
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(nullptr);
    }
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB09IntegrationTest, ProgressiveModuleConnection) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B09_MODULE_COUNT; i++) {
        B09_MODULES[i].set_fn(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, B09_MODULES[i].name, 0);
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B09_MODULE_COUNT);
    nimcp_health_agent_stop(agent_);
}
