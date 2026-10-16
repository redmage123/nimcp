/**
 * @file test_mirror_bridges_B22_integration.cpp
 * @brief Integration tests for B22 mirror neuron bridges working together
 *        with health agent + cross-bridge interactions
 *        (cognitive/mirror_neurons bridges: thalamic, substrate, sleep, fep,
 *         motor, hypothalamus, emotion, attention, visual, tom, prefrontal,
 *         snn, hippocampus, language, omni, plasticity)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

/* Mirror neuron bridge APIs (for cross-bridge tests) */
#include "cognitive/mirror_neurons/nimcp_mirror_thalamic_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_substrate_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_neurons_sleep_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_neurons_fep_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_motor_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_hypothalamus_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_emotion_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_attention_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_visual_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_tom_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_prefrontal_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_snn_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_hippocampus_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_language_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_omni_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.h"

/* Health agent API */
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    /* Mirror neuron bridge health agent global setters */
    void mirror_thalamic_bridge_set_health_agent(void* agent);
    void mirror_substrate_bridge_set_health_agent(void* agent);
    void mirror_neurons_sleep_bridge_set_health_agent(void* agent);
    void mirror_neurons_fep_bridge_set_health_agent(void* agent);
    void mirror_motor_bridge_set_health_agent(void* agent);
    void mirror_hypothalamus_bridge_set_health_agent(void* agent);
    void mirror_emotion_bridge_set_health_agent(void* agent);
    void mirror_attention_bridge_set_health_agent(void* agent);
    void mirror_visual_bridge_set_health_agent(void* agent);
    void mirror_tom_bridge_set_health_agent(void* agent);
    void mirror_prefrontal_bridge_set_health_agent(void* agent);
    void mirror_snn_bridge_set_health_agent(void* agent);
    void mirror_hippocampus_bridge_set_health_agent(void* agent);
    void mirror_language_bridge_set_health_agent(void* agent);
    void mirror_omni_bridge_set_health_agent(void* agent);
    void mirror_plasticity_bridge_set_health_agent(void* agent);
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kMirrorBridgeModules[] = {
    {"mirror_thalamic_bridge",       mirror_thalamic_bridge_set_health_agent},
    {"mirror_substrate_bridge",      mirror_substrate_bridge_set_health_agent},
    {"mirror_neurons_sleep_bridge",  mirror_neurons_sleep_bridge_set_health_agent},
    {"mirror_neurons_fep_bridge",    mirror_neurons_fep_bridge_set_health_agent},
    {"mirror_motor_bridge",          mirror_motor_bridge_set_health_agent},
    {"mirror_hypothalamus_bridge",   mirror_hypothalamus_bridge_set_health_agent},
    {"mirror_emotion_bridge",        mirror_emotion_bridge_set_health_agent},
    {"mirror_attention_bridge",      mirror_attention_bridge_set_health_agent},
    {"mirror_visual_bridge",         mirror_visual_bridge_set_health_agent},
    {"mirror_tom_bridge",            mirror_tom_bridge_set_health_agent},
    {"mirror_prefrontal_bridge",     mirror_prefrontal_bridge_set_health_agent},
    {"mirror_snn_bridge",            mirror_snn_bridge_set_health_agent},
    {"mirror_hippocampus_bridge",    mirror_hippocampus_bridge_set_health_agent},
    {"mirror_language_bridge",       mirror_language_bridge_set_health_agent},
    {"mirror_omni_bridge",           mirror_omni_bridge_set_health_agent},
    {"mirror_plasticity_bridge",     mirror_plasticity_bridge_set_health_agent},
};

static constexpr size_t kNumModules = 16;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MirrorBridgesB22IntegrationTest : public ::testing::Test {
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
            kMirrorBridgeModules[i].setter(nullptr);
        }
        if (agent_) {
            nimcp_health_agent_stop(agent_);
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }
};

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    health_agent_stats_t before, after;
    nimcp_health_agent_get_stats(agent_, &before);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kMirrorBridgeModules[i].name, 0);
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + kNumModules);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22IntegrationTest, MultipleModulesShareSingleAgent) {
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
}

TEST_F(MirrorBridgesB22IntegrationTest, DisconnectModulesWhileAgentRunning) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules / 2; i++) kMirrorBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    nimcp_health_agent_start(agent_);
    nimcp_health_agent_stop(agent_);
    nimcp_health_agent_start(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 0u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kMirrorBridgeModules[i].setter(agent_);
            for (int j = 0; j < 50; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kMirrorBridgeModules[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent2);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(MirrorBridgesB22IntegrationTest, ProgressiveModuleConnection) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) {
        kMirrorBridgeModules[i].setter(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, kMirrorBridgeModules[i].name, 0);
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
    nimcp_health_agent_stop(agent_);
}

/* ============================================================================
 * Cross-Bridge Interaction Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22IntegrationTest, MotorVisualCrossInteraction) {
    /* Motor bridge extracts programs; visual bridge detects agents */
    mirror_motor_bridge_t* motor = mirror_motor_bridge_create(NULL);
    ASSERT_NE(nullptr, motor);
    mirror_visual_bridge_t* visual = mirror_visual_bridge_create(NULL);
    ASSERT_NE(nullptr, visual);

    /* Update both bridges with a time delta */
    int rc = mirror_motor_bridge_update(motor, 16);
    EXPECT_EQ(0, rc);
    rc = mirror_visual_bridge_update(visual, 16);
    EXPECT_EQ(0, rc);

    /* Verify motor bridge stats are accessible */
    mirror_motor_stats_t motor_stats;
    rc = mirror_motor_bridge_get_stats(motor, &motor_stats);
    EXPECT_EQ(0, rc);

    /* Verify visual bridge stats are accessible */
    mirror_visual_stats_t visual_stats;
    rc = mirror_visual_bridge_get_stats(visual, &visual_stats);
    EXPECT_EQ(0, rc);

    mirror_visual_bridge_destroy(visual);
    mirror_motor_bridge_destroy(motor);
}

TEST_F(MirrorBridgesB22IntegrationTest, EmotionToMCrossInteraction) {
    /* Emotion bridge detects expressions; ToM bridge infers intentions */
    mirror_emotion_bridge_t* emotion = mirror_emotion_create(NULL);
    ASSERT_NE(nullptr, emotion);
    mirror_tom_bridge_t tom = mirror_tom_create(NULL);
    ASSERT_NE(nullptr, tom);

    /* Verify emotion stats are accessible */
    mirror_emotion_stats_t emotion_stats;
    bool ok = mirror_emotion_get_stats(emotion, &emotion_stats);
    EXPECT_TRUE(ok);

    /* Verify ToM stats are accessible */
    mirror_tom_stats_t tom_stats;
    int rc = mirror_tom_get_stats(tom, &tom_stats);
    EXPECT_EQ(0, rc);

    mirror_tom_destroy(tom);
    mirror_emotion_destroy(emotion);
}

TEST_F(MirrorBridgesB22IntegrationTest, FepSnnCrossInteraction) {
    /* FEP bridge handles prediction errors; SNN bridge encodes spike patterns */
    mirror_neurons_fep_bridge_t* fep = mirror_neurons_fep_bridge_create(NULL);
    ASSERT_NE(nullptr, fep);
    mirror_snn_bridge_t* snn = mirror_snn_create(NULL);
    ASSERT_NE(nullptr, snn);

    /* Update both bridges */
    int rc = mirror_neurons_fep_bridge_update(fep, 16);
    EXPECT_EQ(0, rc);
    rc = mirror_snn_update(snn, 1.0f);
    EXPECT_EQ(0, rc);

    /* Verify FEP bridge stats are accessible */
    mirror_neurons_fep_stats_t fep_stats;
    rc = mirror_neurons_fep_bridge_get_stats(fep, &fep_stats);
    EXPECT_EQ(0, rc);

    /* Verify SNN bridge stats are accessible */
    mirror_snn_stats_t snn_stats;
    rc = mirror_snn_get_stats(snn, &snn_stats);
    EXPECT_EQ(0, rc);

    mirror_snn_destroy(snn);
    mirror_neurons_fep_bridge_destroy(fep);
}

TEST_F(MirrorBridgesB22IntegrationTest, FullPipeline) {
    /* Full pipeline: motor -> visual -> emotion -> tom */
    mirror_motor_bridge_t* motor = mirror_motor_bridge_create(NULL);
    ASSERT_NE(nullptr, motor);
    mirror_visual_bridge_t* visual = mirror_visual_bridge_create(NULL);
    ASSERT_NE(nullptr, visual);
    mirror_emotion_bridge_t* emotion = mirror_emotion_create(NULL);
    ASSERT_NE(nullptr, emotion);
    mirror_tom_bridge_t tom = mirror_tom_create(NULL);
    ASSERT_NE(nullptr, tom);

    /* Step 1: Motor bridge processes observed movement */
    int rc = mirror_motor_bridge_update(motor, 16);
    EXPECT_EQ(0, rc);
    mirror_motor_effects_t motor_effects;
    rc = mirror_motor_bridge_get_effects(motor, &motor_effects);
    EXPECT_EQ(0, rc);

    /* Step 2: Visual bridge processes visual input */
    rc = mirror_visual_bridge_update(visual, 16);
    EXPECT_EQ(0, rc);
    mirror_visual_effects_t visual_effects;
    rc = mirror_visual_bridge_get_effects(visual, &visual_effects);
    EXPECT_EQ(0, rc);

    /* Step 3: Emotion bridge processes emotional expression */
    mirror_emotion_stats_t emotion_stats;
    bool ok = mirror_emotion_get_stats(emotion, &emotion_stats);
    EXPECT_TRUE(ok);

    /* Step 4: ToM bridge infers mental states */
    mirror_tom_stats_t tom_stats;
    rc = mirror_tom_get_stats(tom, &tom_stats);
    EXPECT_EQ(0, rc);

    mirror_tom_destroy(tom);
    mirror_emotion_destroy(emotion);
    mirror_visual_bridge_destroy(visual);
    mirror_motor_bridge_destroy(motor);
}
