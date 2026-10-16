/**
 * @file test_triplet_stdp_integration.cpp
 * @brief Integration tests for Triplet STDP with sleep and immune systems
 * @version 1.0.0
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include "plasticity/stdp/nimcp_triplet_stdp.h"
#include "plasticity/stdp/nimcp_triplet_stdp_sleep_bridge.h"
#include "plasticity/stdp/nimcp_triplet_stdp_immune_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"

class TripletSTDPIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        synapse = nullptr;
        sleep_system = nullptr;
        sleep_bridge = nullptr;
    }

    void TearDown() override {
        if (sleep_bridge) {
            triplet_stdp_sleep_bridge_destroy(sleep_bridge);
        }
        if (sleep_system) {
            sleep_system_destroy(sleep_system);
        }
        if (synapse) {
            triplet_stdp_synapse_destroy(synapse);
        }
    }

    triplet_stdp_synapse_t* synapse;
    sleep_system_t sleep_system;
    triplet_stdp_sleep_bridge_t sleep_bridge;
};

TEST_F(TripletSTDPIntegrationTest, SleepBridgeCreation) {
    sleep_system = sleep_system_create(NULL);
    ASSERT_NE(sleep_system, nullptr);

    sleep_bridge = triplet_stdp_sleep_bridge_create(nullptr, sleep_system);
    ASSERT_NE(sleep_bridge, nullptr);
}

TEST_F(TripletSTDPIntegrationTest, SleepModulationEffects) {
    sleep_system = sleep_system_create(NULL);
    ASSERT_NE(sleep_system, nullptr);

    sleep_bridge = triplet_stdp_sleep_bridge_create(nullptr, sleep_system);
    ASSERT_NE(sleep_bridge, nullptr);

    triplet_stdp_sleep_effects_t effects;

    /* Awake state */
    sleep_enter_state(sleep_system, SLEEP_STATE_AWAKE);
    triplet_stdp_sleep_update(sleep_bridge);
    ASSERT_EQ(triplet_stdp_sleep_get_effects(sleep_bridge, &effects), 0);

    EXPECT_FLOAT_EQ(effects.tau_fast_factor, 1.0f);
    EXPECT_FLOAT_EQ(effects.tau_slow_factor, 1.0f);

    /* REM state should enhance triplet terms */
    sleep_enter_state(sleep_system, SLEEP_STATE_REM);
    triplet_stdp_sleep_update(sleep_bridge);
    ASSERT_EQ(triplet_stdp_sleep_get_effects(sleep_bridge, &effects), 0);

    EXPECT_GT(effects.a3_factor, 1.0f);  /* Triplet enhanced */
    EXPECT_GT(effects.tau_slow_factor, 1.0f);  /* Slow traces persist */
}

TEST_F(TripletSTDPIntegrationTest, SleepStateTransitions) {
    sleep_system = sleep_system_create(NULL);
    sleep_bridge = triplet_stdp_sleep_bridge_create(nullptr, sleep_system);
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);

    ASSERT_NE(sleep_system, nullptr);
    ASSERT_NE(sleep_bridge, nullptr);
    ASSERT_NE(synapse, nullptr);

    /* Test all sleep states */
    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM
    };

    for (sleep_state_t state : states) {
        sleep_enter_state(sleep_system, state);
        triplet_stdp_sleep_update(sleep_bridge);
        triplet_stdp_set_sleep_state(synapse, state);

        triplet_stdp_sleep_effects_t effects;
        ASSERT_EQ(triplet_stdp_sleep_get_effects(sleep_bridge, &effects), 0);

        /* All factors should be positive */
        EXPECT_GT(effects.tau_fast_factor, 0.0f);
        EXPECT_GT(effects.tau_slow_factor, 0.0f);
        EXPECT_GT(effects.a2_factor, 0.0f);
        EXPECT_GT(effects.a3_factor, 0.0f);
    }
}

TEST_F(TripletSTDPIntegrationTest, SynapseWithSleepBridge) {
    sleep_system = sleep_system_create(NULL);
    sleep_bridge = triplet_stdp_sleep_bridge_create(nullptr, sleep_system);
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);

    ASSERT_NE(sleep_system, nullptr);
    ASSERT_NE(sleep_bridge, nullptr);
    ASSERT_NE(synapse, nullptr);

    /* Connect synapse to sleep bridge */
    ASSERT_EQ(triplet_stdp_connect_sleep_bridge(synapse, sleep_bridge), 0);

    /* Synapse should function normally */
    float dw = triplet_stdp_pre_spike(synapse, 0.0f);
    EXPECT_FLOAT_EQ(dw, 0.0f);  /* No post trace yet */
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
