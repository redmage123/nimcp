/**
 * @file test_emotion_cluster_bridges_B26_functions.cpp
 * @brief Unit tests for B26 emotion cluster bridge health agent integration
 *        (36 bridges + 8 non-bridge modules across 11 emotion directories)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cstring>

/* NO bridge headers — they use _Atomic (C11, not C++) via nimcp_pr_memory_node.h */
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    /* 44 health agent setters (36 bridges + 8 non-bridge modules) */

    /* emotion/ (6 bridges) */
    void emotion_fep_bridge_set_health_agent(void* agent);
    void emotion_plasticity_bridge_set_health_agent(void* agent);
    void emotion_snn_bridge_set_health_agent(void* agent);
    void emotion_substrate_bridge_set_health_agent(void* agent);
    void emotion_thalamic_bridge_set_health_agent(void* agent);
    void health_emotion_bridge_set_health_agent(void* agent);

    /* emotional_tagging/ (3 bridges + 1 module) */
    void emotional_tagging_fep_bridge_set_health_agent(void* agent);
    void emotional_tagging_substrate_bridge_set_health_agent(void* agent);
    void emotional_tagging_thalamic_bridge_set_health_agent(void* agent);
    void emotional_tagging_set_health_agent(void* agent);

    /* emotion_recognition/ (3 bridges + 1 module) */
    void emotion_recognition_fep_bridge_set_health_agent(void* agent);
    void emotion_recognition_substrate_bridge_set_health_agent(void* agent);
    void emotion_recognition_thalamic_bridge_set_health_agent(void* agent);
    void emotion_recognition_simple_set_health_agent(void* agent);

    /* emotions/ (1 bridge + 1 module) */
    void emotional_system_sleep_bridge_set_health_agent(void* agent);
    void emotional_system_set_health_agent(void* agent);

    /* emotion_tensor/ (3 bridges + 1 module) */
    void emotion_tensor_bridge_set_health_agent(void* agent);
    void emotion_tensor_substrate_bridge_set_health_agent(void* agent);
    void emotion_tensor_thalamic_bridge_set_health_agent(void* agent);
    void emotion_tensor_set_health_agent(void* agent);

    /* grief/ (3 bridges + 1 module) */
    void grief_fep_bridge_set_health_agent(void* agent);
    void grief_substrate_bridge_set_health_agent(void* agent);
    void grief_thalamic_bridge_set_health_agent(void* agent);
    void grief_and_loss_set_health_agent(void* agent);

    /* joy/ (3 bridges + 1 module) */
    void joy_fep_bridge_set_health_agent(void* agent);
    void joy_substrate_bridge_set_health_agent(void* agent);
    void joy_thalamic_bridge_set_health_agent(void* agent);
    void joy_euphoria_set_health_agent(void* agent);

    /* remorse/ (3 bridges + 1 module) */
    void remorse_fep_bridge_set_health_agent(void* agent);
    void remorse_substrate_bridge_set_health_agent(void* agent);
    void remorse_thalamic_bridge_set_health_agent(void* agent);
    void remorse_regret_set_health_agent(void* agent);

    /* love_loyalty_friendship/ (3 bridges) */
    void love_loyalty_friendship_fep_bridge_set_health_agent(void* agent);
    void llf_substrate_bridge_set_health_agent(void* agent);
    void llf_thalamic_bridge_set_health_agent(void* agent);

    /* shadow_emotions/ (3 bridges) */
    void shadow_emotions_fep_bridge_set_health_agent(void* agent);
    void shadow_emotions_substrate_bridge_set_health_agent(void* agent);
    void shadow_emotions_thalamic_bridge_set_health_agent(void* agent);

    /* empathetic_response/ (5 bridges + 1 module) */
    void empathetic_response_fep_bridge_set_health_agent(void* agent);
    void empathetic_response_substrate_bridge_set_health_agent(void* agent);
    void empathetic_response_thalamic_bridge_set_health_agent(void* agent);
    void empathy_plasticity_bridge_set_health_agent(void* agent);
    void empathy_snn_bridge_set_health_agent(void* agent);
    void empathetic_response_set_health_agent(void* agent);

    /* destroy(NULL) safety declarations */
    void emotion_fep_bridge_destroy(void* bridge);
    void emotion_plasticity_destroy(void* bridge);
    void emotion_snn_destroy(void* bridge);
    void emotion_substrate_bridge_destroy(void* bridge);
    void emotion_thalamic_bridge_destroy(void* bridge);
    void emotional_tagging_fep_destroy(void* bridge);
    void emotional_tagging_substrate_bridge_destroy(void* bridge);
    void emotional_tagging_thalamic_bridge_destroy(void* bridge);
    void emotion_recognition_fep_destroy(void* bridge);
    void emotion_recognition_substrate_bridge_destroy(void* bridge);
    void emotion_recognition_thalamic_bridge_destroy(void* bridge);
    void emotional_sleep_bridge_destroy(void* bridge);
    void emotion_tensor_bridge_destroy(void* bridge);
    void emotion_tensor_substrate_bridge_destroy(void* bridge);
    void emotion_tensor_thalamic_bridge_destroy(void* bridge);
    void grief_fep_destroy(void* bridge);
    void grief_substrate_bridge_destroy(void* bridge);
    void grief_thalamic_bridge_destroy(void* bridge);
    void joy_fep_destroy(void* bridge);
    void joy_substrate_bridge_destroy(void* bridge);
    void joy_thalamic_bridge_destroy(void* bridge);
    void remorse_fep_destroy(void* bridge);
    void remorse_substrate_bridge_destroy(void* bridge);
    void remorse_thalamic_bridge_destroy(void* bridge);
    void social_bond_fep_bridge_destroy(void* bridge);
    void llf_substrate_bridge_destroy(void* bridge);
    void llf_thalamic_bridge_destroy(void* bridge);
    void shadow_emotions_fep_bridge_destroy(void* bridge);
    void shadow_emotions_substrate_bridge_destroy(void* bridge);
    void shadow_emotions_thalamic_bridge_destroy(void* bridge);
    void empathetic_response_fep_destroy(void* bridge);
    void empathetic_response_substrate_bridge_destroy(void* bridge);
    void empathetic_response_thalamic_bridge_destroy(void* bridge);
    void empathy_plasticity_destroy(void* bridge);
    void empathy_snn_destroy(void* bridge);
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kEmotionClusterModules[] = {
    /* emotion/ */
    {"emotion_fep_bridge",                   emotion_fep_bridge_set_health_agent},
    {"emotion_plasticity_bridge",            emotion_plasticity_bridge_set_health_agent},
    {"emotion_snn_bridge",                   emotion_snn_bridge_set_health_agent},
    {"emotion_substrate_bridge",             emotion_substrate_bridge_set_health_agent},
    {"emotion_thalamic_bridge",              emotion_thalamic_bridge_set_health_agent},
    {"health_emotion_bridge",                health_emotion_bridge_set_health_agent},
    /* emotional_tagging/ */
    {"emotional_tagging_fep_bridge",         emotional_tagging_fep_bridge_set_health_agent},
    {"emotional_tagging_substrate_bridge",   emotional_tagging_substrate_bridge_set_health_agent},
    {"emotional_tagging_thalamic_bridge",    emotional_tagging_thalamic_bridge_set_health_agent},
    {"emotional_tagging",                    emotional_tagging_set_health_agent},
    /* emotion_recognition/ */
    {"emotion_recognition_fep_bridge",       emotion_recognition_fep_bridge_set_health_agent},
    {"emotion_recognition_substrate_bridge", emotion_recognition_substrate_bridge_set_health_agent},
    {"emotion_recognition_thalamic_bridge",  emotion_recognition_thalamic_bridge_set_health_agent},
    {"emotion_recognition_simple",           emotion_recognition_simple_set_health_agent},
    /* emotions/ */
    {"emotional_system_sleep_bridge",        emotional_system_sleep_bridge_set_health_agent},
    {"emotional_system",                     emotional_system_set_health_agent},
    /* emotion_tensor/ */
    {"emotion_tensor_bridge",                emotion_tensor_bridge_set_health_agent},
    {"emotion_tensor_substrate_bridge",      emotion_tensor_substrate_bridge_set_health_agent},
    {"emotion_tensor_thalamic_bridge",       emotion_tensor_thalamic_bridge_set_health_agent},
    {"emotion_tensor",                       emotion_tensor_set_health_agent},
    /* grief/ */
    {"grief_fep_bridge",                     grief_fep_bridge_set_health_agent},
    {"grief_substrate_bridge",               grief_substrate_bridge_set_health_agent},
    {"grief_thalamic_bridge",                grief_thalamic_bridge_set_health_agent},
    {"grief_and_loss",                       grief_and_loss_set_health_agent},
    /* joy/ */
    {"joy_fep_bridge",                       joy_fep_bridge_set_health_agent},
    {"joy_substrate_bridge",                 joy_substrate_bridge_set_health_agent},
    {"joy_thalamic_bridge",                  joy_thalamic_bridge_set_health_agent},
    {"joy_euphoria",                         joy_euphoria_set_health_agent},
    /* remorse/ */
    {"remorse_fep_bridge",                   remorse_fep_bridge_set_health_agent},
    {"remorse_substrate_bridge",             remorse_substrate_bridge_set_health_agent},
    {"remorse_thalamic_bridge",              remorse_thalamic_bridge_set_health_agent},
    {"remorse_regret",                       remorse_regret_set_health_agent},
    /* love_loyalty_friendship/ */
    {"love_loyalty_friendship_fep_bridge",   love_loyalty_friendship_fep_bridge_set_health_agent},
    {"llf_substrate_bridge",                 llf_substrate_bridge_set_health_agent},
    {"llf_thalamic_bridge",                  llf_thalamic_bridge_set_health_agent},
    /* shadow_emotions/ */
    {"shadow_emotions_fep_bridge",           shadow_emotions_fep_bridge_set_health_agent},
    {"shadow_emotions_substrate_bridge",     shadow_emotions_substrate_bridge_set_health_agent},
    {"shadow_emotions_thalamic_bridge",      shadow_emotions_thalamic_bridge_set_health_agent},
    /* empathetic_response/ */
    {"empathetic_response_fep_bridge",       empathetic_response_fep_bridge_set_health_agent},
    {"empathetic_response_substrate_bridge", empathetic_response_substrate_bridge_set_health_agent},
    {"empathetic_response_thalamic_bridge",  empathetic_response_thalamic_bridge_set_health_agent},
    {"empathy_plasticity_bridge",            empathy_plasticity_bridge_set_health_agent},
    {"empathy_snn_bridge",                   empathy_snn_bridge_set_health_agent},
    {"empathetic_response",                  empathetic_response_set_health_agent},
};

static constexpr size_t kNumModules = sizeof(kEmotionClusterModules) / sizeof(kEmotionClusterModules[0]);

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class EmotionClusterB26Test : public ::testing::Test {
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
            kEmotionClusterModules[i].setter(nullptr);
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

TEST_F(EmotionClusterB26Test, ModuleCount) {
    EXPECT_EQ(kNumModules, 44u);
}

TEST_F(EmotionClusterB26Test, AllModulesSetNull) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kEmotionClusterModules[i].name);
        kEmotionClusterModules[i].setter(nullptr);
    }
}

TEST_F(EmotionClusterB26Test, AllModulesSetValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kEmotionClusterModules[i].name);
        kEmotionClusterModules[i].setter(agent_);
    }
}

TEST_F(EmotionClusterB26Test, AllModulesReplaceAgent) {
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(nullptr, agent2);
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kEmotionClusterModules[i].name);
        kEmotionClusterModules[i].setter(agent_);
        kEmotionClusterModules[i].setter(agent2);
    }
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(EmotionClusterB26Test, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kEmotionClusterModules[i].name);
        kEmotionClusterModules[i].setter(agent_);
        kEmotionClusterModules[i].setter(agent_);
    }
}

TEST_F(EmotionClusterB26Test, SetNullAfterValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kEmotionClusterModules[i].name);
        kEmotionClusterModules[i].setter(agent_);
        kEmotionClusterModules[i].setter(nullptr);
    }
}

/* ============================================================================
 * Per-Directory Setter Validation
 * ============================================================================ */

TEST_F(EmotionClusterB26Test, EmotionDirectorySetters) {
    /* 6 bridges in emotion/ */
    emotion_fep_bridge_set_health_agent(agent_);
    emotion_plasticity_bridge_set_health_agent(agent_);
    emotion_snn_bridge_set_health_agent(agent_);
    emotion_substrate_bridge_set_health_agent(agent_);
    emotion_thalamic_bridge_set_health_agent(agent_);
    health_emotion_bridge_set_health_agent(agent_);
}

TEST_F(EmotionClusterB26Test, EmotionalTaggingDirectorySetters) {
    emotional_tagging_fep_bridge_set_health_agent(agent_);
    emotional_tagging_substrate_bridge_set_health_agent(agent_);
    emotional_tagging_thalamic_bridge_set_health_agent(agent_);
    emotional_tagging_set_health_agent(agent_);
}

TEST_F(EmotionClusterB26Test, EmotionRecognitionDirectorySetters) {
    emotion_recognition_fep_bridge_set_health_agent(agent_);
    emotion_recognition_substrate_bridge_set_health_agent(agent_);
    emotion_recognition_thalamic_bridge_set_health_agent(agent_);
    emotion_recognition_simple_set_health_agent(agent_);
}

TEST_F(EmotionClusterB26Test, EmotionsDirectorySetters) {
    emotional_system_sleep_bridge_set_health_agent(agent_);
    emotional_system_set_health_agent(agent_);
}

TEST_F(EmotionClusterB26Test, EmotionTensorDirectorySetters) {
    emotion_tensor_bridge_set_health_agent(agent_);
    emotion_tensor_substrate_bridge_set_health_agent(agent_);
    emotion_tensor_thalamic_bridge_set_health_agent(agent_);
    emotion_tensor_set_health_agent(agent_);
}

TEST_F(EmotionClusterB26Test, GriefDirectorySetters) {
    grief_fep_bridge_set_health_agent(agent_);
    grief_substrate_bridge_set_health_agent(agent_);
    grief_thalamic_bridge_set_health_agent(agent_);
    grief_and_loss_set_health_agent(agent_);
}

TEST_F(EmotionClusterB26Test, JoyDirectorySetters) {
    joy_fep_bridge_set_health_agent(agent_);
    joy_substrate_bridge_set_health_agent(agent_);
    joy_thalamic_bridge_set_health_agent(agent_);
    joy_euphoria_set_health_agent(agent_);
}

TEST_F(EmotionClusterB26Test, RemorseDirectorySetters) {
    remorse_fep_bridge_set_health_agent(agent_);
    remorse_substrate_bridge_set_health_agent(agent_);
    remorse_thalamic_bridge_set_health_agent(agent_);
    remorse_regret_set_health_agent(agent_);
}

TEST_F(EmotionClusterB26Test, LoveLoyaltyFriendshipDirectorySetters) {
    love_loyalty_friendship_fep_bridge_set_health_agent(agent_);
    llf_substrate_bridge_set_health_agent(agent_);
    llf_thalamic_bridge_set_health_agent(agent_);
}

TEST_F(EmotionClusterB26Test, ShadowEmotionsDirectorySetters) {
    shadow_emotions_fep_bridge_set_health_agent(agent_);
    shadow_emotions_substrate_bridge_set_health_agent(agent_);
    shadow_emotions_thalamic_bridge_set_health_agent(agent_);
}

TEST_F(EmotionClusterB26Test, EmpatheticResponseDirectorySetters) {
    empathetic_response_fep_bridge_set_health_agent(agent_);
    empathetic_response_substrate_bridge_set_health_agent(agent_);
    empathetic_response_thalamic_bridge_set_health_agent(agent_);
    empathy_plasticity_bridge_set_health_agent(agent_);
    empathy_snn_bridge_set_health_agent(agent_);
    empathetic_response_set_health_agent(agent_);
}

/* ============================================================================
 * Null Safety - destroy(nullptr)
 * ============================================================================ */

TEST_F(EmotionClusterB26Test, NullDestroyEmotionBridges) {
    emotion_fep_bridge_destroy(nullptr);
    emotion_plasticity_destroy(nullptr);
    emotion_snn_destroy(nullptr);
    emotion_substrate_bridge_destroy(nullptr);
    emotion_thalamic_bridge_destroy(nullptr);
}

TEST_F(EmotionClusterB26Test, NullDestroyEmotionalTaggingBridges) {
    emotional_tagging_fep_destroy(nullptr);
    emotional_tagging_substrate_bridge_destroy(nullptr);
    emotional_tagging_thalamic_bridge_destroy(nullptr);
}

TEST_F(EmotionClusterB26Test, NullDestroyEmotionRecognitionBridges) {
    emotion_recognition_fep_destroy(nullptr);
    emotion_recognition_substrate_bridge_destroy(nullptr);
    emotion_recognition_thalamic_bridge_destroy(nullptr);
}

TEST_F(EmotionClusterB26Test, NullDestroyEmotionsBridges) {
    emotional_sleep_bridge_destroy(nullptr);
}

TEST_F(EmotionClusterB26Test, NullDestroyEmotionTensorBridges) {
    emotion_tensor_bridge_destroy(nullptr);
    emotion_tensor_substrate_bridge_destroy(nullptr);
    emotion_tensor_thalamic_bridge_destroy(nullptr);
}

TEST_F(EmotionClusterB26Test, NullDestroyGriefBridges) {
    grief_fep_destroy(nullptr);
    grief_substrate_bridge_destroy(nullptr);
    grief_thalamic_bridge_destroy(nullptr);
}

TEST_F(EmotionClusterB26Test, NullDestroyJoyBridges) {
    joy_fep_destroy(nullptr);
    joy_substrate_bridge_destroy(nullptr);
    joy_thalamic_bridge_destroy(nullptr);
}

TEST_F(EmotionClusterB26Test, NullDestroyRemorseBridges) {
    remorse_fep_destroy(nullptr);
    remorse_substrate_bridge_destroy(nullptr);
    remorse_thalamic_bridge_destroy(nullptr);
}

TEST_F(EmotionClusterB26Test, NullDestroyLLFBridges) {
    social_bond_fep_bridge_destroy(nullptr);
    llf_substrate_bridge_destroy(nullptr);
    llf_thalamic_bridge_destroy(nullptr);
}

TEST_F(EmotionClusterB26Test, NullDestroyShadowEmotionsBridges) {
    shadow_emotions_fep_bridge_destroy(nullptr);
    shadow_emotions_substrate_bridge_destroy(nullptr);
    shadow_emotions_thalamic_bridge_destroy(nullptr);
}

TEST_F(EmotionClusterB26Test, NullDestroyEmpatheticResponseBridges) {
    empathetic_response_fep_destroy(nullptr);
    empathetic_response_substrate_bridge_destroy(nullptr);
    empathetic_response_thalamic_bridge_destroy(nullptr);
    empathy_plasticity_destroy(nullptr);
    empathy_snn_destroy(nullptr);
}
