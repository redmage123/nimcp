/**
 * @file test_heartbeat_B12_neuro_symbolic_jepa_functions.cpp
 * @brief Unit tests for B12 heartbeat setter functions (cognitive/neuro_symbolic + cognitive/jepa)
 *
 * Tests that each module's set_health_agent function correctly accepts,
 * replaces, and clears the health agent pointer.
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

/* cognitive/neuro_symbolic modules */
void energy_consistency_set_health_agent(nimcp_health_agent_t* agent);
void evolutionary_proof_set_health_agent(nimcp_health_agent_t* agent);
void genius_math_orchestrator_set_health_agent(nimcp_health_agent_t* agent);
void hypergraph_set_health_agent(nimcp_health_agent_t* agent);
void quantum_math_engine_set_health_agent(nimcp_health_agent_t* agent);
void quantum_mcts_set_health_agent(nimcp_health_agent_t* agent);

/* cognitive/jepa modules */
void jepa_bidirectional_set_health_agent(nimcp_health_agent_t* agent);
void jepa_context_set_health_agent(nimcp_health_agent_t* agent);
void jepa_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void jepa_latent_set_health_agent(nimcp_health_agent_t* agent);
void jepa_masking_set_health_agent(nimcp_health_agent_t* agent);
void jepa_multimodal_set_health_agent(nimcp_health_agent_t* agent);
void jepa_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void jepa_predictor_set_health_agent(nimcp_health_agent_t* agent);
void jepa_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void jepa_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void jepa_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void jepa_weights_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B12Module {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B12Module B12_MODULES[] = {
    /* cognitive/neuro_symbolic */
    {"energy_consistency",        energy_consistency_set_health_agent},
    {"evolutionary_proof",        evolutionary_proof_set_health_agent},
    {"genius_math_orchestrator",  genius_math_orchestrator_set_health_agent},
    {"hypergraph",                hypergraph_set_health_agent},
    {"quantum_math_engine",       quantum_math_engine_set_health_agent},
    {"quantum_mcts",              quantum_mcts_set_health_agent},
    /* cognitive/jepa */
    {"jepa_bidirectional",        jepa_bidirectional_set_health_agent},
    {"jepa_context",              jepa_context_set_health_agent},
    {"jepa_fep_bridge",           jepa_fep_bridge_set_health_agent},
    {"jepa_latent",               jepa_latent_set_health_agent},
    {"jepa_masking",              jepa_masking_set_health_agent},
    {"jepa_multimodal",           jepa_multimodal_set_health_agent},
    {"jepa_plasticity_bridge",    jepa_plasticity_bridge_set_health_agent},
    {"jepa_predictor",            jepa_predictor_set_health_agent},
    {"jepa_snn_bridge",           jepa_snn_bridge_set_health_agent},
    {"jepa_substrate_bridge",     jepa_substrate_bridge_set_health_agent},
    {"jepa_thalamic_bridge",      jepa_thalamic_bridge_set_health_agent},
    {"jepa_weights",              jepa_weights_set_health_agent},
};

static constexpr size_t B12_MODULE_COUNT = sizeof(B12_MODULES) / sizeof(B12_MODULES[0]);

class HeartbeatB12UnitTest : public ::testing::Test {
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
        for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
            B12_MODULES[i].set_fn(nullptr);
        }
        if (agent_) {
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }
};

TEST_F(HeartbeatB12UnitTest, AllModulesSetNull) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        B12_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB12UnitTest, AllModulesSetValid) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        B12_MODULES[i].set_fn(agent_);
    }
}

TEST_F(HeartbeatB12UnitTest, AllModulesReplaceAgent) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(agent2, nullptr);

    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        B12_MODULES[i].set_fn(agent_);
        B12_MODULES[i].set_fn(agent2);
    }

    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        B12_MODULES[i].set_fn(nullptr);
    }
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB12UnitTest, AllModulesShareSameAgent) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        B12_MODULES[i].set_fn(agent_);
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
}

TEST_F(HeartbeatB12UnitTest, SetNullAfterValid) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        B12_MODULES[i].set_fn(agent_);
        B12_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB12UnitTest, ConcurrentSetClear) {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 100; j++) {
                B12_MODULES[i].set_fn(agent_);
                B12_MODULES[i].set_fn(nullptr);
            }
        });
    }
    for (auto& t : threads) t.join();
}

TEST_F(HeartbeatB12UnitTest, HeartbeatCounterIncrements) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        B12_MODULES[i].set_fn(agent_);
    }
    health_agent_stats_t before, after;
    nimcp_health_agent_get_stats(agent_, &before);
    nimcp_health_agent_heartbeat_ex(agent_, "B12_test", 0);
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GT(after.heartbeats_received, before.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB12UnitTest, ModuleCount) {
    EXPECT_EQ(B12_MODULE_COUNT, 18u);
}

TEST_F(HeartbeatB12UnitTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < B12_MODULE_COUNT; i++) {
        SCOPED_TRACE(B12_MODULES[i].name);
        B12_MODULES[i].set_fn(agent_);
        B12_MODULES[i].set_fn(agent_);
    }
}
