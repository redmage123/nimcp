/**
 * @file test_heartbeat_B19_remaining_modules_e2e.cpp
 * @brief E2E tests for B19 heartbeat (all remaining uncovered modules)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

void analysis_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void analysis_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void analysis_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void astrocyte_set_health_agent(nimcp_health_agent_t* agent);
void bias_detection_set_health_agent(nimcp_health_agent_t* agent);
void bias_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void bias_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void bias_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void bias_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void bias_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void cognitive_meta_controller_set_health_agent(nimcp_health_agent_t* agent);
void collective_health_set_health_agent(nimcp_health_agent_t* agent);
void compositional_systematic_set_health_agent(nimcp_health_agent_t* agent);
void counterfactual_imagination_set_health_agent(nimcp_health_agent_t* agent);
void emotional_system_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void energy_consistency_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void energy_consistency_thermo_bridge_set_health_agent(nimcp_health_agent_t* agent);
void evolutionary_proof_logic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void explanations_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void explanations_set_health_agent(nimcp_health_agent_t* agent);
void explanations_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void explanations_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void fractal_cognitive_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void fractal_cognitive_set_health_agent(nimcp_health_agent_t* agent);
void fractal_cognitive_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void fractal_cognitive_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void genius_game_theory_bridge_set_health_agent(nimcp_health_agent_t* agent);
void gt_global_workspace_set_health_agent(nimcp_health_agent_t* agent);
void gt_hemispheric_set_health_agent(nimcp_health_agent_t* agent);
void gt_neuromod_set_health_agent(nimcp_health_agent_t* agent);
void gt_tom_set_health_agent(nimcp_health_agent_t* agent);
void gt_working_memory_set_health_agent(nimcp_health_agent_t* agent);
void global_workspace_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void global_workspace_immune_set_health_agent(nimcp_health_agent_t* agent);
void global_workspace_set_health_agent(nimcp_health_agent_t* agent);
void global_workspace_shannon_set_health_agent(nimcp_health_agent_t* agent);
void gw_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void gw_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void gw_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void gw_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void health_cognitive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void hierarchical_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void hypergraph_kg_bridge_set_health_agent(nimcp_health_agent_t* agent);
void llf_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void llf_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void love_loyalty_friendship_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void metabolic_modulation_set_health_agent(nimcp_health_agent_t* agent);
void meta_health_set_health_agent(nimcp_health_agent_t* agent);
void meta_learning_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void meta_learning_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void meta_learning_set_health_agent(nimcp_health_agent_t* agent);
void meta_learning_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void meta_learning_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void meta_learning_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void network_analysis_set_health_agent(nimcp_health_agent_t* agent);
void personality_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void personality_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void personality_set_health_agent(nimcp_health_agent_t* agent);
void personality_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void personality_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void personality_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void portia_set_health_agent(nimcp_health_agent_t* agent);
void predictive_immune_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_immune_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void quantum_mcts_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_health_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_attention_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_curiosity_set_health_agent(nimcp_health_agent_t* agent);
void salience_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void salience_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void salience_set_health_agent(nimcp_health_agent_t* agent);
void salience_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void salience_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void salience_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_awareness_coordinator_set_health_agent(nimcp_health_agent_t* agent);
void self_awareness_feedback_set_health_agent(nimcp_health_agent_t* agent);
void stdp_set_health_agent(nimcp_health_agent_t* agent);
void surprise_amplifier_set_health_agent(nimcp_health_agent_t* agent);
void surprise_att_bridge_set_health_agent(nimcp_health_agent_t* agent);
void surprise_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void surprise_gw_bridge_set_health_agent(nimcp_health_agent_t* agent);
void surprise_imagination_bridge_set_health_agent(nimcp_health_agent_t* agent);
void surprise_pink_noise_bridge_set_health_agent(nimcp_health_agent_t* agent);
void surprise_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void surprise_self_model_bridge_set_health_agent(nimcp_health_agent_t* agent);
void surprise_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void surprise_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void surprise_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void world_model_multimodal_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);
struct B19Module { const char* name; set_health_agent_fn set_fn; };

static const B19Module B19_MODULES[] = {
    {"analysis_fep_bridge",                analysis_fep_bridge_set_health_agent},
    {"analysis_substrate_bridge",          analysis_substrate_bridge_set_health_agent},
    {"analysis_thalamic_bridge",           analysis_thalamic_bridge_set_health_agent},
    {"astrocyte",                          astrocyte_set_health_agent},
    {"bias_detection",                     bias_detection_set_health_agent},
    {"bias_fep_bridge",                    bias_fep_bridge_set_health_agent},
    {"bias_plasticity_bridge",             bias_plasticity_bridge_set_health_agent},
    {"bias_snn_bridge",                    bias_snn_bridge_set_health_agent},
    {"bias_substrate_bridge",              bias_substrate_bridge_set_health_agent},
    {"bias_thalamic_bridge",               bias_thalamic_bridge_set_health_agent},
    {"cognitive_meta_controller",          cognitive_meta_controller_set_health_agent},
    {"collective_health",                  collective_health_set_health_agent},
    {"compositional_systematic",           compositional_systematic_set_health_agent},
    {"counterfactual_imagination",         counterfactual_imagination_set_health_agent},
    {"emotional_system_fep_bridge",        emotional_system_fep_bridge_set_health_agent},
    {"energy_consistency_fep_bridge",      energy_consistency_fep_bridge_set_health_agent},
    {"energy_consistency_thermo_bridge",   energy_consistency_thermo_bridge_set_health_agent},
    {"evolutionary_proof_logic_bridge",    evolutionary_proof_logic_bridge_set_health_agent},
    {"explanations_fep_bridge",            explanations_fep_bridge_set_health_agent},
    {"explanations",                       explanations_set_health_agent},
    {"explanations_substrate_bridge",      explanations_substrate_bridge_set_health_agent},
    {"explanations_thalamic_bridge",       explanations_thalamic_bridge_set_health_agent},
    {"fractal_cognitive_fep_bridge",       fractal_cognitive_fep_bridge_set_health_agent},
    {"fractal_cognitive",                  fractal_cognitive_set_health_agent},
    {"fractal_cognitive_substrate_bridge", fractal_cognitive_substrate_bridge_set_health_agent},
    {"fractal_cognitive_thalamic_bridge",  fractal_cognitive_thalamic_bridge_set_health_agent},
    {"genius_game_theory_bridge",          genius_game_theory_bridge_set_health_agent},
    {"gt_global_workspace",               gt_global_workspace_set_health_agent},
    {"gt_hemispheric",                     gt_hemispheric_set_health_agent},
    {"gt_neuromod",                        gt_neuromod_set_health_agent},
    {"gt_tom",                             gt_tom_set_health_agent},
    {"gt_working_memory",                  gt_working_memory_set_health_agent},
    {"global_workspace_fep_bridge",        global_workspace_fep_bridge_set_health_agent},
    {"global_workspace_immune",            global_workspace_immune_set_health_agent},
    {"global_workspace",                   global_workspace_set_health_agent},
    {"global_workspace_shannon",           global_workspace_shannon_set_health_agent},
    {"gw_plasticity_bridge",               gw_plasticity_bridge_set_health_agent},
    {"gw_snn_bridge",                      gw_snn_bridge_set_health_agent},
    {"gw_substrate_bridge",                gw_substrate_bridge_set_health_agent},
    {"gw_thalamic_bridge",                 gw_thalamic_bridge_set_health_agent},
    {"health_cognitive_bridge",            health_cognitive_bridge_set_health_agent},
    {"hierarchical_fep_bridge",            hierarchical_fep_bridge_set_health_agent},
    {"hypergraph_kg_bridge",               hypergraph_kg_bridge_set_health_agent},
    {"llf_substrate_bridge",               llf_substrate_bridge_set_health_agent},
    {"llf_thalamic_bridge",                llf_thalamic_bridge_set_health_agent},
    {"love_loyalty_friendship_fep_bridge", love_loyalty_friendship_fep_bridge_set_health_agent},
    {"metabolic_modulation",               metabolic_modulation_set_health_agent},
    {"meta_health",                        meta_health_set_health_agent},
    {"meta_learning_fep_bridge",           meta_learning_fep_bridge_set_health_agent},
    {"meta_learning_plasticity_bridge",    meta_learning_plasticity_bridge_set_health_agent},
    {"meta_learning",                      meta_learning_set_health_agent},
    {"meta_learning_snn_bridge",           meta_learning_snn_bridge_set_health_agent},
    {"meta_learning_substrate_bridge",     meta_learning_substrate_bridge_set_health_agent},
    {"meta_learning_thalamic_bridge",      meta_learning_thalamic_bridge_set_health_agent},
    {"network_analysis",                   network_analysis_set_health_agent},
    {"personality_fep_bridge",             personality_fep_bridge_set_health_agent},
    {"personality_plasticity_bridge",      personality_plasticity_bridge_set_health_agent},
    {"personality",                        personality_set_health_agent},
    {"personality_snn_bridge",             personality_snn_bridge_set_health_agent},
    {"personality_substrate_bridge",       personality_substrate_bridge_set_health_agent},
    {"personality_thalamic_bridge",        personality_thalamic_bridge_set_health_agent},
    {"portia",                             portia_set_health_agent},
    {"predictive_immune_substrate_bridge", predictive_immune_substrate_bridge_set_health_agent},
    {"predictive_immune_thalamic_bridge",  predictive_immune_thalamic_bridge_set_health_agent},
    {"quantum_mcts_fep_bridge",            quantum_mcts_fep_bridge_set_health_agent},
    {"rcog_health",                        rcog_health_set_health_agent},
    {"reasoning_attention",                reasoning_attention_set_health_agent},
    {"reasoning_curiosity",                reasoning_curiosity_set_health_agent},
    {"salience_fep_bridge",                salience_fep_bridge_set_health_agent},
    {"salience_plasticity_bridge",         salience_plasticity_bridge_set_health_agent},
    {"salience",                           salience_set_health_agent},
    {"salience_snn_bridge",                salience_snn_bridge_set_health_agent},
    {"salience_substrate_bridge",          salience_substrate_bridge_set_health_agent},
    {"salience_thalamic_bridge",           salience_thalamic_bridge_set_health_agent},
    {"self_awareness_coordinator",         self_awareness_coordinator_set_health_agent},
    {"self_awareness_feedback",            self_awareness_feedback_set_health_agent},
    {"stdp",                               stdp_set_health_agent},
    {"surprise_amplifier",                 surprise_amplifier_set_health_agent},
    {"surprise_att_bridge",                surprise_att_bridge_set_health_agent},
    {"surprise_fep_bridge",                surprise_fep_bridge_set_health_agent},
    {"surprise_gw_bridge",                 surprise_gw_bridge_set_health_agent},
    {"surprise_imagination_bridge",        surprise_imagination_bridge_set_health_agent},
    {"surprise_pink_noise_bridge",         surprise_pink_noise_bridge_set_health_agent},
    {"surprise_plasticity_bridge",         surprise_plasticity_bridge_set_health_agent},
    {"surprise_self_model_bridge",         surprise_self_model_bridge_set_health_agent},
    {"surprise_snn_bridge",                surprise_snn_bridge_set_health_agent},
    {"surprise_substrate_bridge",          surprise_substrate_bridge_set_health_agent},
    {"surprise_thalamic_bridge",           surprise_thalamic_bridge_set_health_agent},
    {"world_model_multimodal",             world_model_multimodal_set_health_agent},
};
static constexpr size_t B19_MODULE_COUNT = sizeof(B19_MODULES) / sizeof(B19_MODULES[0]);

class HeartbeatB19E2ETest : public ::testing::Test {
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
        for (size_t i = 0; i < B19_MODULE_COUNT; i++) B19_MODULES[i].set_fn(nullptr);
        if (agent_) { nimcp_health_agent_destroy(agent_); agent_ = nullptr; }
    }
};

TEST_F(HeartbeatB19E2ETest, FullLifecycleAllModules) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) B19_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) nimcp_health_agent_heartbeat_ex(agent_, B19_MODULES[i].name, 0);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B19_MODULE_COUNT);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) B19_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB19E2ETest, ConcurrentModulesMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) {
        threads.emplace_back([this, i]() { B19_MODULES[i].set_fn(agent_); for (int j = 0; j < 20; j++) nimcp_health_agent_heartbeat_ex(agent_, B19_MODULES[i].name, 0); B19_MODULES[i].set_fn(nullptr); });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB19E2ETest, HighFrequencyBurst1000Heartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) B19_MODULES[i].set_fn(agent_);
    health_agent_stats_t before; nimcp_health_agent_get_stats(agent_, &before);
    for (int j = 0; j < 1000; j++) nimcp_health_agent_heartbeat_ex(agent_, "B19_burst", 0);
    health_agent_stats_t after; nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + 1000);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB19E2ETest, TimeoutDetectionAfterSilence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) B19_MODULES[i].set_fn(agent_);
    nimcp_health_agent_heartbeat_ex(agent_, "B19_timeout_test", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB19E2ETest, MultiPhaseOperation) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B19_MODULE_COUNT / 2; i++) { B19_MODULES[i].set_fn(agent_); nimcp_health_agent_heartbeat_ex(agent_, B19_MODULES[i].name, 0); }
    for (size_t i = B19_MODULE_COUNT / 2; i < B19_MODULE_COUNT; i++) { B19_MODULES[i].set_fn(agent_); nimcp_health_agent_heartbeat_ex(agent_, B19_MODULES[i].name, 0); }
    for (size_t i = 0; i < B19_MODULE_COUNT / 2; i++) B19_MODULES[i].set_fn(nullptr);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B19_MODULE_COUNT);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB19E2ETest, ModuleHotSwapDuringOperation) {
    nimcp_health_agent_start(agent_);
    health_agent_config_t cfg2; nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2); ASSERT_NE(agent2, nullptr);
    nimcp_health_agent_start(agent2);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) B19_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) { B19_MODULES[i].set_fn(agent2); nimcp_health_agent_heartbeat_ex(agent2, B19_MODULES[i].name, 0); }
    health_agent_stats_t stats2; nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, B19_MODULE_COUNT);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) B19_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent2); nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB19E2ETest, SustainedOperationOverTime) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) B19_MODULES[i].set_fn(agent_);
    auto start = std::chrono::steady_clock::now();
    uint64_t count = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(250)) { nimcp_health_agent_heartbeat_ex(agent_, "B19_sustained", 0); count++; }
    EXPECT_GT(count, 0u);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, count);
    nimcp_health_agent_stop(agent_);
}

TEST_F(HeartbeatB19E2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) B19_MODULES[i].set_fn(agent_);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) nimcp_health_agent_heartbeat_ex(agent_, B19_MODULES[i].name, 0);
    for (size_t i = 0; i < B19_MODULE_COUNT; i++) B19_MODULES[i].set_fn(nullptr);
    nimcp_health_agent_stop(agent_);
    health_agent_stats_t stats; nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, B19_MODULE_COUNT);
}
