/**
 * @file test_mirror_bridges_B22_regression.cpp
 * @brief Regression tests for B22 mirror neuron bridges: edge cases, stability,
 *        thread safety
 *        (cognitive/mirror_neurons bridges: thalamic, substrate, sleep, fep,
 *         motor, hypothalamus, emotion, attention, visual, tom, prefrontal,
 *         snn, hippocampus, language, omni, plasticity)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>
#include <climits>

/* Mirror bridge APIs - includes outside extern "C" (headers have their own guards) */
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
    /* Mirror bridge health agent setters (not declared in headers) */
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

class MirrorBridgesB22RegressionTest : public ::testing::Test {
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
 * Regression Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMirrorBridgeModules[i].name);
        kMirrorBridgeModules[i].setter(nullptr);
    }
}

TEST_F(MirrorBridgesB22RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMirrorBridgeModules[i].name);
        kMirrorBridgeModules[i].setter(agent_);
        kMirrorBridgeModules[i].setter(nullptr);
    }
}

TEST_F(MirrorBridgesB22RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMirrorBridgeModules[i].name);
        kMirrorBridgeModules[i].setter(agent_);
    }
}

TEST_F(MirrorBridgesB22RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kMirrorBridgeModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kMirrorBridgeModules[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    std::thread heartbeat_thread([this]() {
        for (int j = 0; j < 100; j++)
            nimcp_health_agent_heartbeat_ex(agent_, "B22_regression", 0);
    });
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(nullptr);
    heartbeat_thread.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22RegressionTest, RapidSetClearCycleAllModules) {
    for (int cycle = 0; cycle < 100; cycle++) {
        for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
        for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(nullptr);
    }
}

TEST_F(MirrorBridgesB22RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int cycle = 0; cycle < 50; cycle++) {
        health_agent_config_t cfg_temp;
        nimcp_health_agent_default_config(&cfg_temp);
        nimcp_health_agent_t* temp = nimcp_health_agent_create(&cfg_temp);
        ASSERT_NE(nullptr, temp);
        for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(temp);
        for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(nullptr);
        nimcp_health_agent_destroy(temp);
    }
}

TEST_F(MirrorBridgesB22RegressionTest, MotorBoundaryValues) {
    mirror_motor_bridge_t* bridge = mirror_motor_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);

    /* Update with 0 delta */
    int rc = mirror_motor_bridge_update(bridge, 0);
    EXPECT_EQ(0, rc);

    /* Update with large delta */
    rc = mirror_motor_bridge_update(bridge, 1000000);
    EXPECT_EQ(0, rc);

    mirror_motor_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22RegressionTest, FepUpdate) {
    mirror_neurons_fep_bridge_t* bridge = mirror_neurons_fep_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);

    /* Update with 0 delta */
    int rc = mirror_neurons_fep_bridge_update(bridge, 0);
    EXPECT_EQ(0, rc);

    mirror_neurons_fep_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22RegressionTest, SnnBoundaryValues) {
    mirror_snn_bridge_t* bridge = mirror_snn_create(NULL);
    ASSERT_NE(nullptr, bridge);

    /* Update with 0 delta */
    int rc = mirror_snn_update(bridge, 0.0f);
    EXPECT_EQ(0, rc);

    mirror_snn_destroy(bridge);
}

TEST_F(MirrorBridgesB22RegressionTest, ThalamicNullSignal) {
    /* Thalamic bridge requires mirror+router; create with NULLs */
    mirror_thalamic_bridge_t* bridge = mirror_thalamic_bridge_create(NULL, NULL, NULL);
    /* May return NULL since it requires non-NULL params */
    if (bridge) {
        /* route_action with NULL signal should return error */
        int rc = mirror_thalamic_route_action(bridge, NULL);
        EXPECT_NE(0, rc);
        mirror_thalamic_bridge_destroy(bridge);
    }
}

TEST_F(MirrorBridgesB22RegressionTest, VisualUpdate) {
    mirror_visual_bridge_t* bridge = mirror_visual_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);

    /* Update with 0 delta */
    int rc = mirror_visual_bridge_update(bridge, 0);
    EXPECT_EQ(0, rc);

    mirror_visual_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22RegressionTest, AllBridgesNullDestroyIdempotent) {
    /* destroy(NULL) should be safe for all 16 bridge types */
    mirror_thalamic_bridge_destroy(nullptr);
    mirror_substrate_bridge_destroy(nullptr);
    mirror_neurons_sleep_bridge_destroy(NULL);
    mirror_neurons_fep_bridge_destroy(nullptr);
    mirror_motor_bridge_destroy(nullptr);
    mirror_hypo_destroy(nullptr);
    mirror_emotion_destroy(nullptr);
    mirror_attention_destroy(nullptr);
    mirror_visual_bridge_destroy(nullptr);
    mirror_tom_destroy(NULL);
    mirror_prefrontal_bridge_destroy(NULL);
    mirror_snn_destroy(nullptr);
    mirror_hippocampus_bridge_destroy(nullptr);
    mirror_language_bridge_destroy(nullptr);
    mirror_omni_bridge_destroy(nullptr);
    mirror_plasticity_destroy(nullptr);
}

TEST_F(MirrorBridgesB22RegressionTest, AllBridgesDoubleDestroy) {
    /* Create and destroy each bridge that accepts all-NULL create params,
     * then call destroy(NULL) to verify safety. */

    /* motor */
    mirror_motor_bridge_t* motor = mirror_motor_bridge_create(NULL);
    ASSERT_NE(nullptr, motor);
    mirror_motor_bridge_destroy(motor);
    mirror_motor_bridge_destroy(nullptr);

    /* visual */
    mirror_visual_bridge_t* visual = mirror_visual_bridge_create(NULL);
    ASSERT_NE(nullptr, visual);
    mirror_visual_bridge_destroy(visual);
    mirror_visual_bridge_destroy(nullptr);

    /* fep */
    mirror_neurons_fep_bridge_t* fep = mirror_neurons_fep_bridge_create(NULL);
    ASSERT_NE(nullptr, fep);
    mirror_neurons_fep_bridge_destroy(fep);
    mirror_neurons_fep_bridge_destroy(nullptr);

    /* snn */
    mirror_snn_bridge_t* snn = mirror_snn_create(NULL);
    ASSERT_NE(nullptr, snn);
    mirror_snn_destroy(snn);
    mirror_snn_destroy(nullptr);

    /* emotion */
    mirror_emotion_bridge_t* emo = mirror_emotion_create(NULL);
    ASSERT_NE(nullptr, emo);
    mirror_emotion_destroy(emo);
    mirror_emotion_destroy(nullptr);

    /* attention */
    mirror_attention_bridge_t* attn = mirror_attention_create(NULL);
    ASSERT_NE(nullptr, attn);
    mirror_attention_destroy(attn);
    mirror_attention_destroy(nullptr);

    /* tom */
    mirror_tom_bridge_t tom = mirror_tom_create(NULL);
    ASSERT_NE(nullptr, tom);
    mirror_tom_destroy(tom);
    mirror_tom_destroy(NULL);

    /* omni */
    mirror_omni_bridge_t* omni = mirror_omni_bridge_create(NULL);
    ASSERT_NE(nullptr, omni);
    mirror_omni_bridge_destroy(omni);
    mirror_omni_bridge_destroy(nullptr);

    /* hippocampus */
    mirror_hippocampus_bridge_t* hippo = mirror_hippocampus_bridge_create(NULL);
    ASSERT_NE(nullptr, hippo);
    mirror_hippocampus_bridge_destroy(hippo);
    mirror_hippocampus_bridge_destroy(nullptr);

    /* plasticity */
    mirror_plasticity_bridge_t* plast = mirror_plasticity_create(NULL);
    ASSERT_NE(nullptr, plast);
    mirror_plasticity_destroy(plast);
    mirror_plasticity_destroy(nullptr);

    /* Bridges requiring non-NULL params: only test destroy(NULL) */
    mirror_thalamic_bridge_destroy(nullptr);
    mirror_substrate_bridge_destroy(nullptr);
    mirror_neurons_sleep_bridge_destroy(NULL);
    mirror_hypo_destroy(nullptr);
    mirror_prefrontal_bridge_destroy(NULL);
    mirror_language_bridge_destroy(nullptr);
}

TEST_F(MirrorBridgesB22RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMirrorBridgeModules[i].name);
        EXPECT_NE(kMirrorBridgeModules[i].setter, nullptr);
        kMirrorBridgeModules[i].setter(agent_);
        kMirrorBridgeModules[i].setter(nullptr);
    }
}

TEST_F(MirrorBridgesB22RegressionTest, StatsConsistentAfterOperations) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    health_agent_stats_t stats1;
    nimcp_health_agent_get_stats(agent_, &stats1);
    nimcp_health_agent_heartbeat_ex(agent_, "B22_consistency", 0);
    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent_, &stats2);
    EXPECT_GE(stats2.heartbeats_received, stats1.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22RegressionTest, OmniUpdate) {
    mirror_omni_bridge_t* bridge = mirror_omni_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);

    /* Update with small delta */
    int rc = mirror_omni_bridge_update(bridge, 16);
    EXPECT_EQ(0, rc);

    mirror_omni_bridge_destroy(bridge);
}

TEST_F(MirrorBridgesB22RegressionTest, HippocampusUpdate) {
    mirror_hippocampus_bridge_t* bridge = mirror_hippocampus_bridge_create(NULL);
    ASSERT_NE(nullptr, bridge);

    /* Update with small delta */
    int rc = mirror_hippocampus_bridge_update(bridge, 16);
    EXPECT_EQ(0, rc);

    mirror_hippocampus_bridge_destroy(bridge);
}
