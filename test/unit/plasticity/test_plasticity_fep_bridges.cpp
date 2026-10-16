/**
 * @file test_plasticity_fep_bridges.cpp
 * @brief Unit tests for all Plasticity-FEP Bridge modules
 *
 * WHAT: Comprehensive tests for plasticity-FEP bidirectional integrations
 * WHY:  Ensure synaptic plasticity integrates with FEP precision and predictions
 * HOW:  Test lifecycle, effects, and bio-async for each bridge type
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "plasticity/stp/nimcp_stp_fep_bridge.h"
#include "plasticity/predictive/nimcp_predictive_coding_fep_bridge.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators_fep_bridge.h"
#include "plasticity/noise/nimcp_pink_noise_fep_bridge.h"
#include "plasticity/nimcp_second_messengers_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"

class PlasticityFepBridgesTestBase : public ::testing::Test {
protected:
    fep_system_t* fep = nullptr;

    void SetUp() override {
        bio_router_init(NULL);
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, 8, 4);
        ASSERT_NE(fep, nullptr);
    }

    void TearDown() override {
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        bio_router_shutdown();
    }
};

/* ============================================================================
 * STP FEP Bridge Tests
 * ============================================================================ */

class StpFepBridgeTest : public PlasticityFepBridgesTestBase {
protected:
    stp_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        PlasticityFepBridgesTestBase::SetUp();
        stp_fep_config_t config;
        stp_fep_bridge_default_config(&config);
        bridge = stp_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            stp_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        PlasticityFepBridgesTestBase::TearDown();
    }
};

TEST_F(StpFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(StpFepBridgeTest, DefaultConfig) {
    stp_fep_config_t config;
    int ret = stp_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.precision_sensitivity, 0.0f);
    EXPECT_TRUE(config.enable_pe_modulation);
}

TEST_F(StpFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(stp_fep_bridge_default_config(nullptr), 0);
}

TEST_F(StpFepBridgeTest, CreateWithNullConfig) {
    stp_fep_bridge_t* br = stp_fep_bridge_create(nullptr);
    /* Should use defaults and succeed */
    if (br) stp_fep_bridge_destroy(br);
}

TEST_F(StpFepBridgeTest, DestroyNull) {
    stp_fep_bridge_destroy(nullptr);
}

TEST_F(StpFepBridgeTest, ConnectFep) {
    int ret = stp_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(StpFepBridgeTest, ConnectFepNull) {
    EXPECT_NE(stp_fep_bridge_connect_fep(nullptr, fep), 0);
    EXPECT_NE(stp_fep_bridge_connect_fep(bridge, nullptr), 0);
}

TEST_F(StpFepBridgeTest, Disconnect) {
    stp_fep_bridge_connect_fep(bridge, fep);
    int ret = stp_fep_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(StpFepBridgeTest, Update) {
    stp_fep_bridge_connect_fep(bridge, fep);
    int ret = stp_fep_bridge_update(bridge, 10);
    EXPECT_EQ(ret, 0);
}

TEST_F(StpFepBridgeTest, GetStats) {
    stp_fep_bridge_connect_fep(bridge, fep);
    stp_fep_bridge_update(bridge, 10);

    stp_fep_stats_t stats;
    int ret = stp_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(StpFepBridgeTest, BioAsync) {
    EXPECT_FALSE(stp_fep_bridge_is_bio_async_connected(bridge));
    stp_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(stp_fep_bridge_is_bio_async_connected(bridge));
    stp_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(stp_fep_bridge_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Predictive Coding FEP Bridge Tests
 * ============================================================================ */

class PredictiveCodingFepBridgeTest : public PlasticityFepBridgesTestBase {
protected:
    predictive_coding_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        PlasticityFepBridgesTestBase::SetUp();
        predictive_coding_fep_config_t config;
        predictive_coding_fep_bridge_default_config(&config);
        bridge = predictive_coding_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            predictive_coding_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        PlasticityFepBridgesTestBase::TearDown();
    }
};

TEST_F(PredictiveCodingFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PredictiveCodingFepBridgeTest, DefaultConfig) {
    predictive_coding_fep_config_t config;
    int ret = predictive_coding_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.precision_sensitivity, 0.0f);
    EXPECT_TRUE(config.enable_precision_weighting);
}

TEST_F(PredictiveCodingFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(predictive_coding_fep_bridge_default_config(nullptr), 0);
}

TEST_F(PredictiveCodingFepBridgeTest, DestroyNull) {
    predictive_coding_fep_bridge_destroy(nullptr);
}

TEST_F(PredictiveCodingFepBridgeTest, ConnectFep) {
    int ret = predictive_coding_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveCodingFepBridgeTest, Update) {
    predictive_coding_fep_bridge_connect_fep(bridge, fep);
    int ret = predictive_coding_fep_bridge_update(bridge, 10);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveCodingFepBridgeTest, GetStats) {
    predictive_coding_fep_bridge_connect_fep(bridge, fep);
    predictive_coding_fep_bridge_update(bridge, 10);

    predictive_coding_fep_stats_t stats;
    int ret = predictive_coding_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveCodingFepBridgeTest, BioAsync) {
    EXPECT_FALSE(predictive_coding_fep_bridge_is_bio_async_connected(bridge));
    predictive_coding_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(predictive_coding_fep_bridge_is_bio_async_connected(bridge));
    predictive_coding_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(predictive_coding_fep_bridge_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Neuromodulators FEP Bridge Tests
 * ============================================================================ */

class NeuromodulatorsFepBridgeTest : public PlasticityFepBridgesTestBase {
protected:
    neuromod_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        PlasticityFepBridgesTestBase::SetUp();
        neuromod_fep_config_t config;
        neuromod_fep_bridge_default_config(&config);
        bridge = neuromod_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            neuromod_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        PlasticityFepBridgesTestBase::TearDown();
    }
};

TEST_F(NeuromodulatorsFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(NeuromodulatorsFepBridgeTest, DefaultConfig) {
    neuromod_fep_config_t config;
    int ret = neuromod_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.da_pe_sensitivity, 0.0f);
    EXPECT_TRUE(config.enable_da_pe_coupling);
}

TEST_F(NeuromodulatorsFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(neuromod_fep_bridge_default_config(nullptr), 0);
}

TEST_F(NeuromodulatorsFepBridgeTest, DestroyNull) {
    neuromod_fep_bridge_destroy(nullptr);
}

TEST_F(NeuromodulatorsFepBridgeTest, ConnectFep) {
    int ret = neuromod_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(NeuromodulatorsFepBridgeTest, Update) {
    neuromod_fep_bridge_connect_fep(bridge, fep);
    int ret = neuromod_fep_bridge_update(bridge, 10);
    EXPECT_EQ(ret, 0);
}

TEST_F(NeuromodulatorsFepBridgeTest, GetStats) {
    neuromod_fep_bridge_connect_fep(bridge, fep);
    neuromod_fep_bridge_update(bridge, 10);

    neuromod_fep_stats_t stats;
    int ret = neuromod_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(NeuromodulatorsFepBridgeTest, ComputeDaFromPe) {
    neuromod_fep_bridge_connect_fep(bridge, fep);
    float da = neuromod_fep_compute_da_from_pe(bridge, 0.5f);
    EXPECT_GT(da, 0.0f);
}

TEST_F(NeuromodulatorsFepBridgeTest, ComputeAchFromPrecision) {
    neuromod_fep_bridge_connect_fep(bridge, fep);
    float ach = neuromod_fep_compute_ach_from_precision(bridge, 0.8f);
    EXPECT_GT(ach, 0.0f);
}

TEST_F(NeuromodulatorsFepBridgeTest, BioAsync) {
    EXPECT_FALSE(neuromod_fep_bridge_is_bio_async_connected(bridge));
    neuromod_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(neuromod_fep_bridge_is_bio_async_connected(bridge));
    neuromod_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(neuromod_fep_bridge_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Pink Noise FEP Bridge Tests
 * ============================================================================ */

class PinkNoiseFepBridgeTest : public PlasticityFepBridgesTestBase {
protected:
    pink_noise_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        PlasticityFepBridgesTestBase::SetUp();
        pink_noise_fep_config_t config;
        pink_noise_fep_bridge_default_config(&config);
        bridge = pink_noise_fep_bridge_create(&config);
        if (bridge) {
            pink_noise_fep_bridge_connect_fep(bridge, fep);
        }
    }

    void TearDown() override {
        if (bridge) {
            pink_noise_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        PlasticityFepBridgesTestBase::TearDown();
    }
};

TEST_F(PinkNoiseFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PinkNoiseFepBridgeTest, DefaultConfig) {
    pink_noise_fep_config_t config;
    int ret = pink_noise_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
}

TEST_F(PinkNoiseFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(pink_noise_fep_bridge_default_config(nullptr), 0);
}

TEST_F(PinkNoiseFepBridgeTest, CreateWithNullConfig) {
    pink_noise_fep_bridge_t* br = pink_noise_fep_bridge_create(nullptr);
    /* Should use defaults and succeed or return NULL */
    if (br) pink_noise_fep_bridge_destroy(br);
}

TEST_F(PinkNoiseFepBridgeTest, DestroyNull) {
    pink_noise_fep_bridge_destroy(nullptr);
}

TEST_F(PinkNoiseFepBridgeTest, Update) {
    int ret = pink_noise_fep_bridge_update(bridge, 10);
    EXPECT_EQ(ret, 0);
}

TEST_F(PinkNoiseFepBridgeTest, GetStats) {
    pink_noise_fep_bridge_update(bridge, 10);
    pink_noise_fep_stats_t stats;
    int ret = pink_noise_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(PinkNoiseFepBridgeTest, BioAsync) {
    EXPECT_FALSE(pink_noise_fep_bridge_is_bio_async_connected(bridge));
    pink_noise_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(pink_noise_fep_bridge_is_bio_async_connected(bridge));
    pink_noise_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(pink_noise_fep_bridge_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Second Messengers FEP Bridge Tests
 * ============================================================================ */

class SecondMessengersFepBridgeTest : public PlasticityFepBridgesTestBase {
protected:
    sm_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        PlasticityFepBridgesTestBase::SetUp();
        sm_fep_config_t config;
        sm_fep_bridge_default_config(&config);
        bridge = sm_fep_bridge_create(&config, 0);  // neuron_id = 0
        if (bridge) {
            sm_fep_bridge_connect_fep(bridge, fep);
        }
    }

    void TearDown() override {
        if (bridge) {
            sm_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        PlasticityFepBridgesTestBase::TearDown();
    }
};

TEST_F(SecondMessengersFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SecondMessengersFepBridgeTest, DefaultConfig) {
    sm_fep_config_t config;
    int ret = sm_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecondMessengersFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(sm_fep_bridge_default_config(nullptr), 0);
}

TEST_F(SecondMessengersFepBridgeTest, CreateWithNullConfig) {
    sm_fep_bridge_t* br = sm_fep_bridge_create(nullptr, 0);
    /* Should use defaults and succeed */
    if (br) sm_fep_bridge_destroy(br);
}

TEST_F(SecondMessengersFepBridgeTest, DestroyNull) {
    sm_fep_bridge_destroy(nullptr);
}

TEST_F(SecondMessengersFepBridgeTest, Update) {
    int ret = sm_fep_bridge_update(bridge, 1);  // 1 ms
    EXPECT_EQ(ret, 0);
}

TEST_F(SecondMessengersFepBridgeTest, GetStats) {
    sm_fep_bridge_update(bridge, 1);
    sm_fep_stats_t stats;
    int ret = sm_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

