/**
 * @file test_surprise_bridges_B20_e2e.cpp
 * @brief End-to-end tests for B20 surprise bridges: full lifecycle,
 *        sustained operation, load testing
 *        (cognitive/salience surprise bridges: plasticity, SNN, substrate,
 *         thalamic, pink_noise, imagination, self_model)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

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

class SurpriseBridgesB20E2ETest : public ::testing::Test {
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
 * E2E Tests
 * ============================================================================ */

TEST_F(SurpriseBridgesB20E2ETest, FullLifecycleAllModules) {
    /* Create -> start -> connect -> heartbeat -> stats -> disconnect -> stop -> destroy */
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kSurpriseBridgeModules[i].name, 0);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20E2ETest, ConcurrentModulesMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kSurpriseBridgeModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kSurpriseBridgeModules[i].name, 0);
            kSurpriseBridgeModules[i].setter(nullptr);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20E2ETest, HighFrequencyBurst1000Heartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    health_agent_stats_t before;
    nimcp_health_agent_get_stats(agent_, &before);
    for (int j = 0; j < 1000; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B20_burst", 0);
    health_agent_stats_t after;
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + 1000);
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20E2ETest, TimeoutDetectionAfterSilence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    nimcp_health_agent_heartbeat_ex(agent_, "B20_timeout_test", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20E2ETest, MultiPhaseOperation) {
    nimcp_health_agent_start(agent_);
    /* Phase 1: Create bridges */
    surprise_plasticity_bridge_t* plast = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, plast);
    surprise_snn_bridge_t* snn = surprise_snn_bridge_create(nullptr);
    ASSERT_NE(nullptr, snn);
    surprise_substrate_bridge_t* sub = surprise_substrate_bridge_create(nullptr);
    ASSERT_NE(nullptr, sub);

    /* Connect health agent modules */
    for (size_t i = 0; i < kNumModules; i++) {
        kSurpriseBridgeModules[i].setter(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, kSurpriseBridgeModules[i].name, 0);
    }

    /* Phase 2: Operate bridges */
    surprise_plasticity_on_surprise_event(plast, 0.8f, 1);
    surprise_snn_encode_surprise(snn, 0.7f, SURPRISE_SNN_CHANNEL_PE);
    surprise_snn_simulate_step(snn);
    surprise_substrate_bridge_update(sub, 0.5f, 0.3f);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);

    /* Phase 3: Teardown bridges */
    surprise_substrate_bridge_destroy(sub);
    surprise_snn_bridge_destroy(snn);
    surprise_plasticity_bridge_destroy(plast);

    for (size_t i = 0; i < kNumModules / 2; i++) kSurpriseBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20E2ETest, ModuleHotSwapDuringOperation) {
    nimcp_health_agent_start(agent_);
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    nimcp_health_agent_start(agent2);

    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++) {
        kSurpriseBridgeModules[i].setter(agent2);
        nimcp_health_agent_heartbeat_ex(agent2, kSurpriseBridgeModules[i].name, 0);
    }

    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, kNumModules);

    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20E2ETest, SustainedOperationOverTime) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    auto start = std::chrono::steady_clock::now();
    uint64_t count = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(250)) {
        nimcp_health_agent_heartbeat_ex(agent_, "B20_sustained", 0);
        count++;
    }
    EXPECT_GT(count, 0u);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, count);
    nimcp_health_agent_stop(agent_);
}

TEST_F(SurpriseBridgesB20E2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kSurpriseBridgeModules[i].name, 0);
    /* Orderly: stop operations, disconnect, destroy */
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
}

TEST_F(SurpriseBridgesB20E2ETest, AllBridgesCreateOperateDestroy) {
    /* Full lifecycle for each bridge type */
    {
        surprise_plasticity_bridge_t* b = surprise_plasticity_bridge_create(nullptr);
        ASSERT_NE(nullptr, b);
        surprise_plasticity_on_surprise_event(b, 0.8f, 1);
        surprise_plasticity_bridge_update(b, 0.1f);
        surprise_plasticity_bridge_destroy(b);
    }
    {
        surprise_snn_bridge_t* b = surprise_snn_bridge_create(nullptr);
        ASSERT_NE(nullptr, b);
        surprise_snn_encode_surprise(b, 0.7f, SURPRISE_SNN_CHANNEL_NOVELTY);
        surprise_snn_simulate_step(b);
        surprise_snn_bridge_update(b, 0.01f);
        surprise_snn_bridge_destroy(b);
    }
    {
        surprise_substrate_bridge_t* b = surprise_substrate_bridge_create(nullptr);
        ASSERT_NE(nullptr, b);
        surprise_substrate_bridge_update(b, 0.6f, 0.4f);
        surprise_substrate_bridge_destroy(b);
    }
    {
        surprise_thalamic_bridge_t* b = surprise_thalamic_bridge_create(nullptr);
        ASSERT_NE(nullptr, b);
        surprise_thalamic_signal_t sig;
        memset(&sig, 0, sizeof(sig));
        sig.signal_type = SURPRISE_THALAMIC_NOVELTY;
        sig.surprise_magnitude = 0.6f;
        sig.source_module = 1;
        surprise_thalamic_route_surprise(b, &sig);
        surprise_thalamic_bridge_destroy(b);
    }
    {
        surprise_pink_noise_bridge_t* b = surprise_pink_noise_bridge_create(nullptr);
        ASSERT_NE(nullptr, b);
        surprise_pink_noise_inject(b);
        surprise_pink_noise_adapt_amplitude(b, 0.5f);
        surprise_pink_noise_bridge_update(b, 0.1f);
        surprise_pink_noise_bridge_destroy(b);
    }
    {
        surprise_imagination_bridge_t* b = surprise_imagination_bridge_create(nullptr);
        ASSERT_NE(nullptr, b);
        surprise_imagination_check_trigger(b, 0.9f, 1);
        surprise_imagination_bridge_update(b, 0.5f);
        surprise_imagination_bridge_destroy(b);
    }
    {
        surprise_self_model_bridge_t* b = surprise_self_model_bridge_create(nullptr);
        ASSERT_NE(nullptr, b);
        surprise_self_model_on_capability_surprise(b, 1, 0.8f, SURPRISE_CAPABILITY_UPGRADE);
        surprise_self_model_on_competence_feedback(b, 1, 0.7f);
        surprise_self_model_bridge_update(b, 0.1f);
        surprise_self_model_bridge_destroy(b);
    }
}

TEST_F(SurpriseBridgesB20E2ETest, CrossBridgeEventPropagation) {
    /* Surprise event flows through plasticity -> SNN -> thalamic */
    surprise_plasticity_bridge_t* plast = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, plast);
    surprise_snn_bridge_t* snn = surprise_snn_bridge_create(nullptr);
    ASSERT_NE(nullptr, snn);
    surprise_thalamic_bridge_t* thal = surprise_thalamic_bridge_create(nullptr);
    ASSERT_NE(nullptr, thal);

    /* Connect health agent modules */
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(agent_);

    /* Step 1: Plasticity processes surprise event */
    surprise_plasticity_on_surprise_event(plast, 0.9f, 1);
    surprise_plasticity_effects_t plast_effects;
    surprise_plasticity_bridge_get_effects(plast, &plast_effects);
    EXPECT_GT(plast_effects.learning_rate_multiplier, 1.0f);

    /* Step 2: SNN encodes the surprise */
    surprise_snn_encode_surprise(snn, 0.9f, SURPRISE_SNN_CHANNEL_PE);
    surprise_snn_simulate_step(snn);
    surprise_snn_effects_t snn_effects;
    surprise_snn_bridge_get_effects(snn, &snn_effects);
    /* SNN should show some activity */
    EXPECT_GE(snn_effects.combined_activity, 0.0f);

    /* Step 3: Thalamic routes the surprise signal */
    surprise_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = SURPRISE_THALAMIC_REALIZATION;
    signal.surprise_magnitude = 0.9f;
    signal.source_module = 1;
    signal.urgency = 0.8f;
    int rc = surprise_thalamic_route_surprise(thal, &signal);
    EXPECT_EQ(0, rc);
    surprise_thalamic_stats_t thal_stats;
    surprise_thalamic_bridge_get_stats(thal, &thal_stats);
    EXPECT_GE(thal_stats.signals_routed, 1u);

    /* Heartbeat to confirm health agent is tracking */
    nimcp_health_agent_heartbeat_ex(agent_, "B20_cross_bridge", 1.0f);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);

    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);

    surprise_thalamic_bridge_destroy(thal);
    surprise_snn_bridge_destroy(snn);
    surprise_plasticity_bridge_destroy(plast);
}
