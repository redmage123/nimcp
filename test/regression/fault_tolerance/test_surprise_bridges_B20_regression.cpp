/**
 * @file test_surprise_bridges_B20_regression.cpp
 * @brief Regression tests for B20 surprise bridges: edge cases, stability,
 *        thread safety
 *        (cognitive/salience surprise bridges: plasticity, SNN, substrate,
 *         thalamic, pink_noise, imagination, self_model)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>
#include <climits>

extern "C" {
    /* Surprise bridge health agent global setters */
    void surprise_plasticity_bridge_set_health_agent_global(void* agent);
    void surprise_snn_bridge_set_health_agent_global(void* agent);
    void surprise_substrate_bridge_set_health_agent_global(void* agent);
    void surprise_thalamic_bridge_set_health_agent_global(void* agent);
    void surprise_pink_noise_bridge_set_health_agent_global(void* agent);
    void surprise_imagination_bridge_set_health_agent_global(void* agent);
    void surprise_self_model_bridge_set_health_agent_global(void* agent);

    /* Surprise bridge APIs */
    #include "cognitive/salience/nimcp_surprise_plasticity_bridge.h"
    #include "cognitive/salience/nimcp_surprise_snn_bridge.h"
    #include "cognitive/salience/nimcp_surprise_substrate_bridge.h"
    #include "cognitive/salience/nimcp_surprise_thalamic_bridge.h"
    #include "cognitive/salience/nimcp_surprise_pink_noise_bridge.h"
    #include "cognitive/salience/nimcp_surprise_imagination_bridge.h"
    #include "cognitive/salience/nimcp_surprise_self_model_bridge.h"

    /* Health agent API */
    #include "utils/fault_tolerance/nimcp_health_agent.h"
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kSurpriseBridgeModules[] = {
    {"surprise_plasticity_bridge", surprise_plasticity_bridge_set_health_agent_global},
    {"surprise_snn_bridge",        surprise_snn_bridge_set_health_agent_global},
    {"surprise_substrate_bridge",  surprise_substrate_bridge_set_health_agent_global},
    {"surprise_thalamic_bridge",   surprise_thalamic_bridge_set_health_agent_global},
    {"surprise_pink_noise_bridge", surprise_pink_noise_bridge_set_health_agent_global},
    {"surprise_imagination_bridge", surprise_imagination_bridge_set_health_agent_global},
    {"surprise_self_model_bridge", surprise_self_model_bridge_set_health_agent_global},
};

static constexpr size_t kNumModules = sizeof(kSurpriseBridgeModules) / sizeof(kSurpriseBridgeModules[0]);

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SurpriseBridgesB20RegressionTest : public ::testing::Test {
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
            kSurpriseBridgeModules[i].setter(nullptr);
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

TEST_F(SurpriseBridgesB20RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kSurpriseBridgeModules[i].name);
        kSurpriseBridgeModules[i].setter(nullptr);
    }
}

TEST_F(SurpriseBridgesB20RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kSurpriseBridgeModules[i].name);
        kSurpriseBridgeModules[i].setter(agent_);
        kSurpriseBridgeModules[i].setter(nullptr);
    }
}

TEST_F(SurpriseBridgesB20RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kSurpriseBridgeModules[i].name);
        kSurpriseBridgeModules[i].setter(agent_);
    }
}

TEST_F(SurpriseBridgesB20RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kSurpriseBridgeModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kSurpriseBridgeModules[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    std::thread heartbeat_thread([this]() {
        for (int j = 0; j < 100; j++)
            nimcp_health_agent_heartbeat_ex(agent_, "B20_regression", 0);
    });
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(nullptr);
    heartbeat_thread.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20RegressionTest, RapidSetClearCycleAllModules) {
    for (int cycle = 0; cycle < 100; cycle++) {
        for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
        for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(nullptr);
    }
}

TEST_F(SurpriseBridgesB20RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int cycle = 0; cycle < 50; cycle++) {
        health_agent_config_t cfg_temp;
        nimcp_health_agent_default_config(&cfg_temp);
        nimcp_health_agent_t* temp = nimcp_health_agent_create(&cfg_temp);
        ASSERT_NE(nullptr, temp);
        for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(temp);
        for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(nullptr);
        nimcp_health_agent_destroy(temp);
    }
}

TEST_F(SurpriseBridgesB20RegressionTest, PlasticityBoundaryValues) {
    surprise_plasticity_bridge_t* bridge = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    /* surprise = 0 */
    int rc = surprise_plasticity_on_surprise_event(bridge, 0.0f, 0);
    EXPECT_EQ(0, rc);

    /* surprise = 1 */
    rc = surprise_plasticity_on_surprise_event(bridge, 1.0f, 1);
    EXPECT_EQ(0, rc);

    /* source_id = UINT32_MAX */
    rc = surprise_plasticity_on_surprise_event(bridge, 0.5f, UINT32_MAX);
    EXPECT_EQ(0, rc);

    surprise_plasticity_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20RegressionTest, SnnZeroNeuronsPerChannel) {
    surprise_snn_config_t cfg = surprise_snn_bridge_default_config();
    cfg.neurons_per_channel = 0;
    surprise_snn_bridge_t* bridge = surprise_snn_bridge_create(&cfg);
    /* May return NULL or handle gracefully */
    if (bridge) {
        surprise_snn_bridge_destroy(bridge);
    }
}

TEST_F(SurpriseBridgesB20RegressionTest, SubstrateZeroATP) {
    surprise_substrate_bridge_t* bridge = surprise_substrate_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_substrate_bridge_update(bridge, 0.0f, 0.0f);
    EXPECT_EQ(0, rc);
    surprise_substrate_effects_t effects;
    surprise_substrate_bridge_get_effects(bridge, &effects);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    surprise_substrate_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20RegressionTest, ThalamicInvalidSignalType) {
    surprise_thalamic_bridge_t* bridge = surprise_thalamic_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = 0xFF; /* Invalid bitmask */
    signal.surprise_magnitude = 0.5f;
    signal.source_module = 1;
    int rc = surprise_thalamic_route_surprise(bridge, &signal);
    /* Should handle gracefully - either route or reject */
    (void)rc;
    surprise_thalamic_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20RegressionTest, PinkNoiseZeroAmplitude) {
    surprise_pink_noise_config_t cfg = surprise_pink_noise_bridge_default_config();
    cfg.base_amplitude = 0.0f;
    surprise_pink_noise_bridge_t* bridge = surprise_pink_noise_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_pink_noise_inject(bridge);
    EXPECT_EQ(0, rc);
    /* Zero amplitude should produce zero or near-zero noise */
    for (uint32_t t = 0; t < SURPRISE_PINK_NOISE_NUM_TARGETS; t++) {
        float val = surprise_pink_noise_get_for_target(bridge, t);
        EXPECT_NEAR(val, 0.0f, 0.01f);
    }
    surprise_pink_noise_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20RegressionTest, ImaginationZeroMaxScenarios) {
    surprise_imagination_config_t cfg = surprise_imagination_bridge_default_config();
    cfg.max_scenarios = 0;
    surprise_imagination_bridge_t* bridge = surprise_imagination_bridge_create(&cfg);
    /* May return NULL or handle gracefully */
    if (bridge) {
        int rc = surprise_imagination_check_trigger(bridge, 0.9f, 1);
        (void)rc;
        surprise_imagination_bridge_destroy(bridge);
    }
}

TEST_F(SurpriseBridgesB20RegressionTest, SelfModelMaxCapabilities) {
    surprise_self_model_config_t cfg = surprise_self_model_bridge_default_config();
    /* Use small max to test eviction */
    cfg.max_tracked_capabilities = 2;
    surprise_self_model_bridge_t* bridge = surprise_self_model_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    /* Add more capabilities than max */
    for (uint32_t i = 0; i < 10; i++) {
        surprise_self_model_on_capability_surprise(
            bridge, i, 0.8f, SURPRISE_CAPABILITY_UPGRADE);
    }
    /* Should not crash, eviction should handle overflow */
    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.capability_surprises, 1u);
    surprise_self_model_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20RegressionTest, AllBridgesNullDestroyIdempotent) {
    /* destroy(NULL) should be safe for all 7 bridge types */
    surprise_plasticity_bridge_destroy(nullptr);
    surprise_snn_bridge_destroy(nullptr);
    surprise_substrate_bridge_destroy(nullptr);
    surprise_thalamic_bridge_destroy(nullptr);
    surprise_pink_noise_bridge_destroy(nullptr);
    surprise_imagination_bridge_destroy(nullptr);
    surprise_self_model_bridge_destroy(nullptr);
}

TEST_F(SurpriseBridgesB20RegressionTest, AllBridgesDoubleDestroy) {
    /* Create and destroy each bridge, then call destroy again on dangling pointer test.
     * We only test destroy(NULL) after destroy since the pointer becomes invalid. */
    surprise_plasticity_bridge_t* plast = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, plast);
    surprise_plasticity_bridge_destroy(plast);
    surprise_plasticity_bridge_destroy(nullptr); /* NULL after destroy is safe */

    surprise_snn_bridge_t* snn = surprise_snn_bridge_create(nullptr);
    ASSERT_NE(nullptr, snn);
    surprise_snn_bridge_destroy(snn);
    surprise_snn_bridge_destroy(nullptr);

    surprise_substrate_bridge_t* sub = surprise_substrate_bridge_create(nullptr);
    ASSERT_NE(nullptr, sub);
    surprise_substrate_bridge_destroy(sub);
    surprise_substrate_bridge_destroy(nullptr);

    surprise_thalamic_bridge_t* thal = surprise_thalamic_bridge_create(nullptr);
    ASSERT_NE(nullptr, thal);
    surprise_thalamic_bridge_destroy(thal);
    surprise_thalamic_bridge_destroy(nullptr);

    surprise_pink_noise_bridge_t* pn = surprise_pink_noise_bridge_create(nullptr);
    ASSERT_NE(nullptr, pn);
    surprise_pink_noise_bridge_destroy(pn);
    surprise_pink_noise_bridge_destroy(nullptr);

    surprise_imagination_bridge_t* imag = surprise_imagination_bridge_create(nullptr);
    ASSERT_NE(nullptr, imag);
    surprise_imagination_bridge_destroy(imag);
    surprise_imagination_bridge_destroy(nullptr);

    surprise_self_model_bridge_t* sm = surprise_self_model_bridge_create(nullptr);
    ASSERT_NE(nullptr, sm);
    surprise_self_model_bridge_destroy(sm);
    surprise_self_model_bridge_destroy(nullptr);
}

TEST_F(SurpriseBridgesB20RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kSurpriseBridgeModules[i].name);
        EXPECT_NE(kSurpriseBridgeModules[i].setter, nullptr);
        kSurpriseBridgeModules[i].setter(agent_);
        kSurpriseBridgeModules[i].setter(nullptr);
    }
}

TEST_F(SurpriseBridgesB20RegressionTest, StatsConsistentAfterOperations) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    health_agent_stats_t stats1;
    nimcp_health_agent_get_stats(agent_, &stats1);
    nimcp_health_agent_heartbeat_ex(agent_, "B20_consistency", 0);
    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent_, &stats2);
    EXPECT_GE(stats2.heartbeats_received, stats1.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}
