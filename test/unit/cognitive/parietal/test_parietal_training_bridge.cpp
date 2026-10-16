/**
 * @file test_parietal_training_bridge.cpp
 * @brief Unit tests for Parietal-Training Bridge
 * @date 2026-01-20
 */

#include <gtest/gtest.h>

extern "C" {
#include "cognitive/parietal/nimcp_parietal_training_bridge.h"
}

class ParietalTrainingBridgeTest : public ::testing::Test {
protected:
    parietal_training_config_t config;
    parietal_training_bridge_t* bridge = nullptr;

    void SetUp() override {
        parietal_training_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            parietal_training_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(ParietalTrainingBridgeTest, DefaultConfigSetsReasonableDefaults) {
    parietal_training_config_t cfg;
    int result = parietal_training_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.base_learning_rate, 0.0f);
    EXPECT_LT(cfg.base_learning_rate, 1.0f);
    EXPECT_TRUE(cfg.register_with_training);
    EXPECT_TRUE(cfg.connect_to_plasticity);
    EXPECT_TRUE(cfg.enable_stdp_learning);
}

TEST_F(ParietalTrainingBridgeTest, DefaultConfigNullReturnsError) {
    int result = parietal_training_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ParietalTrainingBridgeTest, AllDomainsEnabledByDefault) {
    parietal_training_config_t cfg;
    parietal_training_default_config(&cfg);

    for (int i = 0; i < PARIETAL_DOMAIN_COUNT; i++) {
        EXPECT_TRUE(cfg.domains[i].enabled) << "Domain " << i << " not enabled";
        EXPECT_GT(cfg.domains[i].learning_rate, 0.0f);
    }
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ParietalTrainingBridgeTest, CreateWithNullParietalFails) {
    bridge = parietal_training_create(&config, nullptr, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(ParietalTrainingBridgeTest, DestroyNullIsSafe) {
    parietal_training_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * State Tests
 * ============================================================================ */

TEST_F(ParietalTrainingBridgeTest, GetStateNullReturnsError) {
    parietal_train_state_t state = parietal_training_get_state(nullptr);
    EXPECT_EQ(state, PARIETAL_TRAIN_STATE_ERROR);
}

TEST_F(ParietalTrainingBridgeTest, IsConnectedNullReturnsFalse) {
    bool connected = parietal_training_is_connected(nullptr);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Domain Configuration Tests
 * ============================================================================ */

TEST_F(ParietalTrainingBridgeTest, SetDomainLRNullBridgeFails) {
    int result = parietal_training_set_domain_lr(nullptr,
        PARIETAL_DOMAIN_COORDINATE_TRANSFORM, 0.01f);
    EXPECT_EQ(result, -1);
}

TEST_F(ParietalTrainingBridgeTest, SetDomainEnabledNullBridgeFails) {
    int result = parietal_training_set_domain_enabled(nullptr,
        PARIETAL_DOMAIN_COORDINATE_TRANSFORM, false);
    EXPECT_EQ(result, -1);
}

TEST_F(ParietalTrainingBridgeTest, SetDomainInvalidDomainFails) {
    int result = parietal_training_set_domain_lr(nullptr,
        (parietal_learning_domain_t)(PARIETAL_DOMAIN_COUNT + 1), 0.01f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

TEST_F(ParietalTrainingBridgeTest, SetCallbackNullBridgeFails) {
    int result = parietal_training_set_learning_callback(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ParietalTrainingBridgeTest, SetUpdateCallbackNullBridgeFails) {
    int result = parietal_training_set_update_callback(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(ParietalTrainingBridgeTest, GetStatsNullBridgeFails) {
    parietal_training_stats_t stats;
    int result = parietal_training_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(ParietalTrainingBridgeTest, GetStatsNullStatsFails) {
    int result = parietal_training_get_stats(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ParietalTrainingBridgeTest, ResetStatsNullIsSafe) {
    parietal_training_reset_stats(nullptr);
    // Should not crash
}

/* ============================================================================
 * Signal Processing Tests
 * ============================================================================ */

TEST_F(ParietalTrainingBridgeTest, ProcessSignalNullBridgeReturnsNone) {
    parietal_learning_signal_t signal = {};
    parietal_train_response_t response = parietal_training_process_signal(nullptr, &signal);
    EXPECT_EQ(response, PARIETAL_TRAIN_RESPONSE_NONE);
}

TEST_F(ParietalTrainingBridgeTest, ProcessSignalNullSignalReturnsNone) {
    parietal_train_response_t response = parietal_training_process_signal(nullptr, nullptr);
    EXPECT_EQ(response, PARIETAL_TRAIN_RESPONSE_NONE);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(ParietalTrainingBridgeTest, ConnectNullBridgeFails) {
    int result = parietal_training_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ParietalTrainingBridgeTest, DisconnectNullBridgeFails) {
    int result = parietal_training_disconnect(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ParietalTrainingBridgeTest, ConnectPlasticityNullBridgeFails) {
    int result = parietal_training_connect_plasticity(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ParietalTrainingBridgeTest, ConnectBioAsyncNullBridgeFails) {
    int result = parietal_training_connect_bio_async(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Batch Update Tests
 * ============================================================================ */

TEST_F(ParietalTrainingBridgeTest, FlushBatchNullBridgeFails) {
    int result = parietal_training_flush_batch(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ParietalTrainingBridgeTest, UpdateWeightsNullBridgeFails) {
    int result = parietal_training_update_weights(nullptr,
        PARIETAL_DOMAIN_COORDINATE_TRANSFORM, 0.01f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(ParietalTrainingBridgeTest, DomainNameReturnsValidStrings) {
    for (int i = 0; i < PARIETAL_DOMAIN_COUNT; i++) {
        const char* name = parietal_training_domain_name((parietal_learning_domain_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "unknown");
    }
}

TEST_F(ParietalTrainingBridgeTest, DomainNameInvalidReturnsUnknown) {
    const char* name = parietal_training_domain_name((parietal_learning_domain_t)100);
    EXPECT_STREQ(name, "unknown");
}

TEST_F(ParietalTrainingBridgeTest, ResponseNameReturnsValidStrings) {
    const char* names[] = {"none", "update_weights", "adjust_threshold",
                           "modulate_gain", "consolidate"};

    for (int i = 0; i <= PARIETAL_TRAIN_RESPONSE_CONSOLIDATE; i++) {
        const char* name = parietal_training_response_name((parietal_train_response_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STREQ(name, names[i]);
    }
}

TEST_F(ParietalTrainingBridgeTest, StateNameReturnsValidStrings) {
    for (int i = 0; i <= PARIETAL_TRAIN_STATE_ERROR; i++) {
        const char* name = parietal_training_state_name((parietal_train_state_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "unknown");
    }
}

TEST_F(ParietalTrainingBridgeTest, VersionReturnsValidString) {
    const char* version = parietal_training_bridge_version();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);
}
