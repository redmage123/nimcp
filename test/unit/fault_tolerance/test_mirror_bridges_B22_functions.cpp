/**
 * @file test_mirror_bridges_B22_functions.cpp
 * @brief Unit tests for B22 mirror neuron bridge health agent integration
 *        and functional tests for each bridge's core API
 *        (cognitive/mirror_neurons bridges: thalamic, substrate, sleep, fep,
 *         motor, hypothalamus, emotion, attention, visual, tom, prefrontal,
 *         snn, hippocampus, language, omni, plasticity)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cstring>

/* Mirror bridge APIs - outside extern "C" since headers have their own guards */
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
    /* Mirror bridge health agent setters */
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

static constexpr size_t kNumModules = sizeof(kMirrorBridgeModules) / sizeof(kMirrorBridgeModules[0]);

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MirrorBridgesB22Test : public ::testing::Test {
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
 * Health Agent Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, AllModulesSetNull) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMirrorBridgeModules[i].name);
        kMirrorBridgeModules[i].setter(nullptr);
    }
}

TEST_F(MirrorBridgesB22Test, AllModulesSetValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMirrorBridgeModules[i].name);
        kMirrorBridgeModules[i].setter(agent_);
    }
}

TEST_F(MirrorBridgesB22Test, AllModulesReplaceAgent) {
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(nullptr, agent2);
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMirrorBridgeModules[i].name);
        kMirrorBridgeModules[i].setter(agent_);
        kMirrorBridgeModules[i].setter(agent2);
    }
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(MirrorBridgesB22Test, ModuleCount) {
    EXPECT_EQ(kNumModules, 16u);
}

TEST_F(MirrorBridgesB22Test, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMirrorBridgeModules[i].name);
        kMirrorBridgeModules[i].setter(agent_);
        kMirrorBridgeModules[i].setter(agent_);
    }
}

/* ============================================================================
 * Thalamic Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, ThalamicCreateDestroy) {
    mirror_thalamic_bridge_t* bridge = mirror_thalamic_bridge_create(NULL, NULL, NULL);
    ASSERT_NE(nullptr, bridge);
    mirror_thalamic_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, ThalamicReset) {
    mirror_thalamic_bridge_t* bridge = mirror_thalamic_bridge_create(NULL, NULL, NULL);
    ASSERT_NE(nullptr, bridge);
    int rc = mirror_thalamic_bridge_reset(bridge);
    EXPECT_EQ(0, rc);
    mirror_thalamic_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, ThalamicNullSafety) {
    mirror_thalamic_bridge_destroy(nullptr);
    int rc = mirror_thalamic_bridge_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Substrate Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, SubstrateNullSafety) {
    /* Substrate requires non-NULL substrate param, so just test destroy(NULL) */
    mirror_substrate_bridge_destroy(nullptr);
}

TEST_F(MirrorBridgesB22Test, SubstrateGetEffectsNull) {
    mirror_substrate_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    int rc = mirror_substrate_bridge_get_effects(nullptr, &effects);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Sleep Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, SleepCreateRequiresSleepSystem) {
    /* Sleep bridge requires a valid sleep_system_t; NULL returns NULL */
    mirror_neurons_sleep_bridge_t bridge = mirror_neurons_sleep_bridge_create(NULL, NULL);
    EXPECT_EQ(nullptr, bridge);
}

TEST_F(MirrorBridgesB22Test, SleepUpdateNullReturnsError) {
    int rc = mirror_neurons_sleep_update(NULL);
    EXPECT_NE(0, rc);
}

TEST_F(MirrorBridgesB22Test, SleepNullSafety) {
    mirror_neurons_sleep_bridge_destroy(nullptr);
}

/* ============================================================================
 * FEP Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, FepCreateDestroy) {
    mirror_neurons_fep_bridge_t* bridge = mirror_neurons_fep_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);
    mirror_neurons_fep_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, FepUpdate) {
    mirror_neurons_fep_bridge_t* bridge = mirror_neurons_fep_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);
    int rc = mirror_neurons_fep_bridge_update(bridge, 16.0f);
    EXPECT_EQ(0, rc);
    mirror_neurons_fep_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, FepNullSafety) {
    mirror_neurons_fep_bridge_destroy(nullptr);
    int rc = mirror_neurons_fep_bridge_update(nullptr, 16.0f);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Motor Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, MotorCreateDestroy) {
    mirror_motor_bridge_t* bridge = mirror_motor_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);
    mirror_motor_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, MotorUpdate) {
    mirror_motor_bridge_t* bridge = mirror_motor_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);
    int rc = mirror_motor_bridge_update(bridge, 16.0f);
    EXPECT_EQ(0, rc);
    mirror_motor_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, MotorNullSafety) {
    mirror_motor_bridge_destroy(nullptr);
    int rc = mirror_motor_bridge_update(nullptr, 16.0f);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Hypothalamus Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, HypothalamusNullSafety) {
    /* Hypothalamus requires non-NULL params, so just test destroy(NULL) */
    mirror_hypo_destroy(nullptr);
}

TEST_F(MirrorBridgesB22Test, HypothalamusUpdateNull) {
    int rc = mirror_hypo_update(nullptr, 0);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Emotion Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, EmotionCreateDestroy) {
    mirror_emotion_bridge_t* bridge = mirror_emotion_create(NULL);
    ASSERT_NE(nullptr, bridge);
    mirror_emotion_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, EmotionNullSafety) {
    mirror_emotion_destroy(nullptr);
}

/* ============================================================================
 * Attention Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, AttentionCreateDestroy) {
    mirror_attention_bridge_t* bridge = mirror_attention_create(NULL);
    ASSERT_NE(nullptr, bridge);
    mirror_attention_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, AttentionNullSafety) {
    mirror_attention_destroy(nullptr);
}

/* ============================================================================
 * Visual Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, VisualCreateDestroy) {
    mirror_visual_bridge_t* bridge = mirror_visual_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);
    mirror_visual_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, VisualUpdate) {
    mirror_visual_bridge_t* bridge = mirror_visual_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);
    int rc = mirror_visual_bridge_update(bridge, 16.0f);
    EXPECT_EQ(0, rc);
    mirror_visual_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, VisualNullSafety) {
    mirror_visual_bridge_destroy(nullptr);
    int rc = mirror_visual_bridge_update(nullptr, 16.0f);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * ToM Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, TomCreateDestroy) {
    mirror_tom_bridge_t bridge = mirror_tom_create(NULL);
    ASSERT_NE(nullptr, bridge);
    mirror_tom_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, TomUpdate) {
    mirror_tom_bridge_t bridge = mirror_tom_create(NULL);
    ASSERT_NE(nullptr, bridge);
    int rc = mirror_tom_update(bridge, 16000);
    EXPECT_EQ(0, rc);
    mirror_tom_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, TomNullSafety) {
    mirror_tom_destroy(nullptr);
    int rc = mirror_tom_update(nullptr, 16000);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Prefrontal Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, PrefrontalNullSafety) {
    /* Prefrontal requires non-NULL params, so just test destroy(NULL) */
    mirror_prefrontal_bridge_destroy(nullptr);
}

TEST_F(MirrorBridgesB22Test, PrefrontalUpdateNull) {
    int rc = mirror_prefrontal_update(nullptr, 16.0f);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * SNN Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, SnnCreateDestroy) {
    mirror_snn_bridge_t* bridge = mirror_snn_create(NULL);
    ASSERT_NE(nullptr, bridge);
    mirror_snn_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, SnnUpdate) {
    mirror_snn_bridge_t* bridge = mirror_snn_create(NULL);
    ASSERT_NE(nullptr, bridge);
    int rc = mirror_snn_simulate(bridge, 16.0f);
    EXPECT_GE(rc, 0);  /* returns spike count >= 0, or -1 on error */
    mirror_snn_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, SnnNullSafety) {
    mirror_snn_destroy(nullptr);
    int rc = mirror_snn_simulate(nullptr, 16.0f);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Hippocampus Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, HippocampusCreateDestroy) {
    mirror_hippocampus_bridge_t* bridge = mirror_hippocampus_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);
    mirror_hippocampus_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, HippocampusUpdate) {
    mirror_hippocampus_bridge_t* bridge = mirror_hippocampus_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);
    int rc = mirror_hippocampus_bridge_update(bridge, 16.0f);
    EXPECT_EQ(0, rc);
    mirror_hippocampus_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, HippocampusNullSafety) {
    mirror_hippocampus_bridge_destroy(nullptr);
    int rc = mirror_hippocampus_bridge_update(nullptr, 16.0f);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Language Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, LanguageNullSafety) {
    /* Language requires non-NULL broca/wernicke params, so just test destroy(NULL) */
    mirror_language_bridge_destroy(nullptr);
}

TEST_F(MirrorBridgesB22Test, LanguageUpdateNull) {
    int rc = mirror_language_bridge_update(nullptr, 0);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Omni Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, OmniCreateDestroy) {
    mirror_omni_bridge_t* bridge = mirror_omni_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);
    mirror_omni_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, OmniUpdate) {
    mirror_omni_bridge_t* bridge = mirror_omni_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);
    int rc = mirror_omni_bridge_update(bridge, 16.0f);
    EXPECT_EQ(0, rc);
    mirror_omni_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, OmniNullSafety) {
    mirror_omni_bridge_destroy(nullptr);
    int rc = mirror_omni_bridge_update(nullptr, 16.0f);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Plasticity Bridge Functional Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22Test, PlasticityCreateDestroy) {
    mirror_plasticity_bridge_t* bridge = mirror_plasticity_create(NULL);
    ASSERT_NE(nullptr, bridge);
    mirror_plasticity_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, PlasticityUpdate) {
    mirror_plasticity_bridge_t* bridge = mirror_plasticity_create(NULL);
    ASSERT_NE(nullptr, bridge);
    int rc = mirror_plasticity_update(bridge, 16.0f);
    EXPECT_EQ(0, rc);
    mirror_plasticity_destroy(bridge);
}

TEST_F(MirrorBridgesB22Test, PlasticityNullSafety) {
    mirror_plasticity_destroy(nullptr);
    int rc = mirror_plasticity_update(nullptr, 16.0f);
    EXPECT_NE(0, rc);
}
