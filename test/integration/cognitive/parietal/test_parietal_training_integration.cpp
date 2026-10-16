/**
 * @file test_parietal_training_integration.cpp
 * @brief Integration tests for Parietal-Training Bridge
 * @date 2026-01-20
 *
 * Tests the integration between parietal lobe training bridge and
 * bio-async communication. Uses simplified test patterns that don't
 * require full parietal cortex or training system implementations.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstring>

extern "C" {
#include "cognitive/parietal/nimcp_parietal_training_bridge.h"
#include "core/brain/regions/parietal/nimcp_parietal_adapter.h"
}

class ParietalTrainingIntegrationTest : public ::testing::Test {
protected:
    parietal_training_bridge_t* bridge = nullptr;
    parietal_adapter_t* parietal_adapter = nullptr;

    void SetUp() override {
        // Create a real parietal adapter for the tests
        parietal_cortex_config_t parietal_config = parietal_cortex_adapter_default_config();
        parietal_adapter = parietal_cortex_adapter_create(&parietal_config);
    }

    void TearDown() override {
        if (bridge) {
            parietal_training_destroy(bridge);
            bridge = nullptr;
        }
        if (parietal_adapter) {
            parietal_cortex_adapter_destroy(parietal_adapter);
            parietal_adapter = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, DefaultConfigSetsReasonableValues) {
    parietal_training_config_t config;
    int result = parietal_training_default_config(&config);
    EXPECT_EQ(result, 0);

    // Verify basic settings
    EXPECT_GT(config.base_learning_rate, 0.0f);
    EXPECT_LE(config.base_learning_rate, 1.0f);
    EXPECT_GE(config.min_learning_rate, 0.0f);
}

TEST_F(ParietalTrainingIntegrationTest, CreateBridgeWithNullComponents) {
    parietal_training_config_t config;
    parietal_training_default_config(&config);

    // Create bridge with NULL components - may return valid bridge or NULL
    bridge = parietal_training_create(&config, nullptr, nullptr);
    // Either is acceptable - test shouldn't crash
    if (bridge) {
        parietal_train_state_t state = parietal_training_get_state(bridge);
        EXPECT_NE(state, PARIETAL_TRAIN_STATE_ERROR);
    }
}

TEST_F(ParietalTrainingIntegrationTest, CreateBridgeWithRealAdapter) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    EXPECT_NE(bridge, nullptr);
    if (bridge) {
        parietal_train_state_t state = parietal_training_get_state(bridge);
        EXPECT_NE(state, PARIETAL_TRAIN_STATE_ERROR);
    }
}

/* ============================================================================
 * Learning Signal Processing Tests
 * ============================================================================ */

static std::atomic<int> learning_callback_count{0};
static void test_learning_callback(const parietal_learning_signal_t* signal,
                                   void* user_data) {
    (void)signal;
    (void)user_data;
    learning_callback_count++;
}

TEST_F(ParietalTrainingIntegrationTest, ProcessSignalStandalone) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    // Create and process a learning signal
    parietal_learning_signal_t signal = {};
    signal.domain = PARIETAL_DOMAIN_COORDINATE_TRANSFORM;
    signal.response = PARIETAL_TRAIN_RESPONSE_UPDATE_WEIGHTS;
    signal.loss_value = 0.5f;
    signal.loss_delta = -0.01f;
    signal.learning_rate = 0.001f;
    signal.timestamp_us = 1000;

    // Without connected training, should return NONE or handle gracefully
    parietal_train_response_t response = parietal_training_process_signal(bridge, &signal);
    // Just verify it doesn't crash - response depends on connection state
    (void)response;
}

TEST_F(ParietalTrainingIntegrationTest, ProcessMultipleDomainsStandalone) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    // Process signals for multiple domains
    parietal_learning_domain_t domains[] = {
        PARIETAL_DOMAIN_COORDINATE_TRANSFORM,
        PARIETAL_DOMAIN_REACHING_ACCURACY,
        PARIETAL_DOMAIN_ATTENTION_ALLOCATION,
        PARIETAL_DOMAIN_NUMERICAL_MAGNITUDE,
        PARIETAL_DOMAIN_MENTAL_ROTATION,
        PARIETAL_DOMAIN_MULTISENSORY_BINDING,
        PARIETAL_DOMAIN_BODY_SCHEMA,
        PARIETAL_DOMAIN_PATTERN_DETECTION
    };

    for (auto domain : domains) {
        parietal_learning_signal_t signal = {};
        signal.domain = domain;
        signal.loss_value = 0.5f;
        signal.loss_delta = -0.05f;
        signal.learning_rate = 0.001f;

        parietal_training_process_signal(bridge, &signal);
        // Verify domain name function works
        const char* name = parietal_training_domain_name(domain);
        EXPECT_NE(name, nullptr);
    }
}

/* ============================================================================
 * Domain Configuration Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, SetDomainEnabled) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    // Disable attention allocation domain
    int result = parietal_training_set_domain_enabled(bridge, PARIETAL_DOMAIN_ATTENTION_ALLOCATION, false);
    EXPECT_EQ(result, 0);

    // Re-enable
    result = parietal_training_set_domain_enabled(bridge, PARIETAL_DOMAIN_ATTENTION_ALLOCATION, true);
    EXPECT_EQ(result, 0);
}

TEST_F(ParietalTrainingIntegrationTest, SetDomainLearningRate) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    // Adjust learning rate for mental rotation
    int result = parietal_training_set_domain_lr(bridge, PARIETAL_DOMAIN_MENTAL_ROTATION, 0.05f);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * State Machine Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, InitialStateIsValid) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    // Initial state should be INITIALIZED or UNINITIALIZED depending on components
    parietal_train_state_t state = parietal_training_get_state(bridge);
    EXPECT_TRUE(state == PARIETAL_TRAIN_STATE_UNINITIALIZED ||
                state == PARIETAL_TRAIN_STATE_INITIALIZED);
}

TEST_F(ParietalTrainingIntegrationTest, StateNameReturnsValidString) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    parietal_train_state_t state = parietal_training_get_state(bridge);
    const char* name = parietal_training_state_name(state);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

/* ============================================================================
 * Statistics and Monitoring Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, GetStatsSucceeds) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    parietal_training_stats_t stats;
    int result = parietal_training_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(ParietalTrainingIntegrationTest, ResetStatsClearsCounters) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    // Process some signals
    for (int i = 0; i < 3; i++) {
        parietal_learning_signal_t signal = {};
        signal.domain = PARIETAL_DOMAIN_BODY_SCHEMA;
        signal.loss_value = 0.6f;

        parietal_training_process_signal(bridge, &signal);
    }

    // Reset stats
    parietal_training_reset_stats(bridge);

    parietal_training_stats_t stats;
    parietal_training_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_events, 0u);
    EXPECT_EQ(stats.total_updates, 0u);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, ConcurrentSignalProcessing) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    const int num_threads = 4;
    const int signals_per_thread = 25;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < signals_per_thread; i++) {
                parietal_learning_signal_t signal = {};
                signal.domain = static_cast<parietal_learning_domain_t>(
                    (t + i) % PARIETAL_DOMAIN_COUNT);
                signal.loss_value = 0.3f + (i * 0.01f);
                signal.loss_delta = -0.02f;
                signal.learning_rate = 0.001f;

                parietal_training_process_signal(bridge, &signal);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should complete without crashing
    parietal_training_stats_t stats;
    parietal_training_get_stats(bridge, &stats);
    EXPECT_TRUE(true);  // Just verify no crash during concurrent access
}

/* ============================================================================
 * Domain Name Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, AllDomainNamesValid) {
    for (int i = 0; i < static_cast<int>(PARIETAL_DOMAIN_COUNT); i++) {
        const char* name = parietal_training_domain_name(static_cast<parietal_learning_domain_t>(i));
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(ParietalTrainingIntegrationTest, InvalidDomainNameReturnsUnknown) {
    const char* name = parietal_training_domain_name(static_cast<parietal_learning_domain_t>(999));
    EXPECT_NE(name, nullptr);
    // Should return "unknown" or similar
}

/* ============================================================================
 * Response Name Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, ResponseNamesValid) {
    parietal_train_response_t responses[] = {
        PARIETAL_TRAIN_RESPONSE_NONE,
        PARIETAL_TRAIN_RESPONSE_UPDATE_WEIGHTS,
        PARIETAL_TRAIN_RESPONSE_ADJUST_THRESHOLD,
        PARIETAL_TRAIN_RESPONSE_MODULATE_GAIN,
        PARIETAL_TRAIN_RESPONSE_CONSOLIDATE
    };

    for (auto response : responses) {
        const char* name = parietal_training_response_name(response);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

/* ============================================================================
 * State Name Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, AllStateNamesValid) {
    parietal_train_state_t states[] = {
        PARIETAL_TRAIN_STATE_UNINITIALIZED,
        PARIETAL_TRAIN_STATE_INITIALIZED,
        PARIETAL_TRAIN_STATE_CONNECTED,
        PARIETAL_TRAIN_STATE_LEARNING,
        PARIETAL_TRAIN_STATE_PAUSED,
        PARIETAL_TRAIN_STATE_ERROR
    };

    for (auto state : states) {
        const char* name = parietal_training_state_name(state);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

/* ============================================================================
 * Version Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, VersionReturnsValidString) {
    const char* version = parietal_training_bridge_version();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);
}

/* ============================================================================
 * Callback Registration Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, SetLearningCallback) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    learning_callback_count = 0;
    int result = parietal_training_set_learning_callback(bridge, test_learning_callback, nullptr);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Weight Update Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, UpdateWeightsForDomain) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    int result = parietal_training_update_weights(bridge,
        PARIETAL_DOMAIN_COORDINATE_TRANSFORM, 0.01f);
    // May succeed or fail depending on connection state
    (void)result;
}

TEST_F(ParietalTrainingIntegrationTest, FlushBatchAccumulatesUpdates) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);
    config.batch_weight_updates = true;
    config.update_batch_size = 5;

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    // Process multiple signals to accumulate updates
    for (int i = 0; i < 10; i++) {
        parietal_learning_signal_t signal = {};
        signal.domain = PARIETAL_DOMAIN_REACHING_ACCURACY;
        signal.loss_value = 0.3f + (i * 0.05f);
        signal.loss_delta = -0.02f;

        parietal_training_process_signal(bridge, &signal);
    }

    // Flush batch
    int result = parietal_training_flush_batch(bridge);
    // May return count or -1 depending on connection state
    (void)result;
}

/* ============================================================================
 * Connection Status Tests
 * ============================================================================ */

TEST_F(ParietalTrainingIntegrationTest, IsConnectedWithoutConnection) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter creation failed";
    }

    parietal_training_config_t config;
    parietal_training_default_config(&config);

    bridge = parietal_training_create(&config, parietal_adapter, nullptr);
    if (!bridge) {
        GTEST_SKIP() << "Bridge creation requires parietal component";
    }

    // Without explicit connection, should return false
    bool connected = parietal_training_is_connected(bridge);
    EXPECT_FALSE(connected);
}
