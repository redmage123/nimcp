/**
 * @file test_fep_consciousness.cpp
 * @brief Unit tests for FEP Consciousness module
 */

#include <gtest/gtest.h>
#include "cognitive/free_energy/nimcp_fep_consciousness.h"

class FEPConsciousnessTest : public ::testing::Test {
protected:
    fep_consciousness_bridge_t* bridge = nullptr;

    void SetUp() override {
        fep_consciousness_config_t config;
        fep_consciousness_default_config(&config);
        bridge = fep_consciousness_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            fep_consciousness_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(FEPConsciousnessTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(FEPConsciousnessTest, CreateWithNullConfig) {
    fep_consciousness_bridge_t* b = fep_consciousness_create(nullptr);
    ASSERT_NE(b, nullptr);
    fep_consciousness_destroy(b);
}

TEST_F(FEPConsciousnessTest, DefaultConfig) {
    fep_consciousness_config_t config;
    fep_consciousness_default_config(&config);
    EXPECT_GT(config.phi_threshold, 0.0f);
    EXPECT_GT(config.attention_gain, 0.0f);
}

TEST_F(FEPConsciousnessTest, GetState) {
    fep_consciousness_state_t state;
    int ret = fep_consciousness_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPConsciousnessTest, GateAction) {
    uint32_t proposed = 5;
    uint32_t gated;
    int ret = fep_consciousness_gate_action(bridge, proposed, &gated);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPConsciousnessTest, ModulatePrecision) {
    float precision = 1.0f;
    int ret = fep_consciousness_modulate_precision(bridge, &precision);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(precision, 0.0f);
}

TEST_F(FEPConsciousnessTest, Update) {
    int ret = fep_consciousness_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPConsciousnessTest, Disconnect) {
    int ret = fep_consciousness_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPConsciousnessTest, BioAsyncConnectDisconnect) {
    EXPECT_FALSE(fep_consciousness_is_bio_async_connected(bridge));
    fep_consciousness_connect_bio_async(bridge);
    fep_consciousness_disconnect_bio_async(bridge);
    EXPECT_FALSE(fep_consciousness_is_bio_async_connected(bridge));
}
