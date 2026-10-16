/**
 * @file test_memory_core_nonbridge_B25nb_functions.cpp
 * @brief Unit tests for B25nb memory/core non-bridge module health agent integration
 *        (28 non-bridge modules in cognitive/memory/core/)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cstring>

/* NO bridge headers — they use _Atomic (C11, not C++) via nimcp_pr_memory_node.h */
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    /* 28 health agent setters (non-bridge modules in memory/core/) */
    void collective_memory_set_health_agent(void* agent);
    void counterfactual_set_health_agent(void* agent);
    void entanglement_set_health_agent(void* agent);
    void flashbulb_set_health_agent(void* agent);
    void fractal_set_health_agent(void* agent);
    void future_thinking_set_health_agent(void* agent);
    void gist_set_health_agent(void* agent);
    void kuramoto_set_health_agent(void* agent);
    void metamemory_set_health_agent(void* agent);
    void metamemory_monitor_set_health_agent(void* agent);
    void prime_signature_set_health_agent(void* agent);
    void procedural_set_health_agent(void* agent);
    void prospective_set_health_agent(void* agent);
    void prospective_scheduler_set_health_agent(void* agent);
    void pr_memory_node_set_health_agent(void* agent);
    void pr_pink_noise_set_health_agent(void* agent);
    void pr_training_plasticity_set_health_agent(void* agent);
    void quaternion_set_health_agent(void* agent);
    void reconsolidation_set_health_agent(void* agent);
    void resonance_set_health_agent(void* agent);
    void schemas_set_health_agent(void* agent);
    void skill_acquisition_set_health_agent(void* agent);
    void social_memory_set_health_agent(void* agent);
    void source_memory_set_health_agent(void* agent);
    void spaced_repetition_set_health_agent(void* agent);
    void theta_gamma_set_health_agent(void* agent);
    void transactive_set_health_agent(void* agent);
    void z_ladder_set_health_agent(void* agent);
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kMemoryCoreNonbridgeModules[] = {
    {"collective_memory",       collective_memory_set_health_agent},
    {"counterfactual",          counterfactual_set_health_agent},
    {"entanglement",            entanglement_set_health_agent},
    {"flashbulb",               flashbulb_set_health_agent},
    {"fractal",                 fractal_set_health_agent},
    {"future_thinking",         future_thinking_set_health_agent},
    {"gist",                    gist_set_health_agent},
    {"kuramoto",                kuramoto_set_health_agent},
    {"metamemory",              metamemory_set_health_agent},
    {"metamemory_monitor",      metamemory_monitor_set_health_agent},
    {"prime_signature",         prime_signature_set_health_agent},
    {"procedural",              procedural_set_health_agent},
    {"prospective",             prospective_set_health_agent},
    {"prospective_scheduler",   prospective_scheduler_set_health_agent},
    {"pr_memory_node",          pr_memory_node_set_health_agent},
    {"pr_pink_noise",           pr_pink_noise_set_health_agent},
    {"pr_training_plasticity",  pr_training_plasticity_set_health_agent},
    {"quaternion",              quaternion_set_health_agent},
    {"reconsolidation",         reconsolidation_set_health_agent},
    {"resonance",               resonance_set_health_agent},
    {"schemas",                 schemas_set_health_agent},
    {"skill_acquisition",       skill_acquisition_set_health_agent},
    {"social_memory",           social_memory_set_health_agent},
    {"source_memory",           source_memory_set_health_agent},
    {"spaced_repetition",       spaced_repetition_set_health_agent},
    {"theta_gamma",             theta_gamma_set_health_agent},
    {"transactive",             transactive_set_health_agent},
    {"z_ladder",                z_ladder_set_health_agent},
};

static constexpr size_t kNumModules = sizeof(kMemoryCoreNonbridgeModules) / sizeof(kMemoryCoreNonbridgeModules[0]);

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MemoryCoreNonbridgeB25nbTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent_ = nullptr;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = false;
        agent_ = nimcp_health_agent_create(&config);
        ASSERT_NE(nullptr, agent_);
    }

    void TearDown() override {
        for (size_t i = 0; i < kNumModules; i++) {
            kMemoryCoreNonbridgeModules[i].setter(nullptr);
        }
        if (agent_) {
            nimcp_health_agent_stop(agent_);
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }
};

/* ============================================================================
 * Health Agent Setter Tests
 * ============================================================================ */

TEST_F(MemoryCoreNonbridgeB25nbTest, ModuleCount) {
    EXPECT_EQ(kNumModules, 28u);
}

TEST_F(MemoryCoreNonbridgeB25nbTest, AllModulesSetNull) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreNonbridgeModules[i].name);
        kMemoryCoreNonbridgeModules[i].setter(nullptr);
    }
}

TEST_F(MemoryCoreNonbridgeB25nbTest, AllModulesSetValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreNonbridgeModules[i].name);
        kMemoryCoreNonbridgeModules[i].setter(agent_);
    }
}

TEST_F(MemoryCoreNonbridgeB25nbTest, AllModulesReplaceAgent) {
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(nullptr, agent2);
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreNonbridgeModules[i].name);
        kMemoryCoreNonbridgeModules[i].setter(agent_);
        kMemoryCoreNonbridgeModules[i].setter(agent2);
    }
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(MemoryCoreNonbridgeB25nbTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreNonbridgeModules[i].name);
        kMemoryCoreNonbridgeModules[i].setter(agent_);
        kMemoryCoreNonbridgeModules[i].setter(agent_);
    }
}

TEST_F(MemoryCoreNonbridgeB25nbTest, SetNullAfterValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreNonbridgeModules[i].name);
        kMemoryCoreNonbridgeModules[i].setter(agent_);
        kMemoryCoreNonbridgeModules[i].setter(nullptr);
    }
}
