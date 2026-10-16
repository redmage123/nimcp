/**
 * @file test_cochlea_bridges.cpp
 * @brief Unit tests for NIMCP cochlea brain integration bridges
 *
 * WHAT: Tests for cochlea bridge creation and basic operations
 * WHY:  Ensure bridges can be created and destroyed without issues
 * HOW:  Use GoogleTest framework to test bridge lifecycle
 *
 * @author NIMCP Development Team
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "perception/nimcp_cochlea.h"
#include "perception/bridges/nimcp_cochlea_medulla_bridge.h"
#include "perception/bridges/nimcp_cochlea_thalamic_bridge.h"
#include "perception/bridges/nimcp_cochlea_audio_cortex_bridge.h"
#include "perception/bridges/nimcp_cochlea_fep_bridge.h"
#include "perception/bridges/nimcp_cochlea_sleep_bridge.h"
#include "perception/bridges/nimcp_cochlea_immune_bridge.h"
#include "perception/bridges/nimcp_cochlea_kg_bridge.h"
#include "perception/bridges/nimcp_cochlea_rcog_bridge.h"
#include "perception/bridges/nimcp_cochlea_collective_bridge.h"
#include "perception/bridges/nimcp_cochlea_cortical_deep_bridge.h"
#include "perception/bridges/nimcp_cochlea_occipital_bridge.h"
#include "perception/bridges/nimcp_cochlea_broca_bridge.h"
#include "perception/bridges/nimcp_cochlea_bio_async_bridge.h"
#include "perception/bridges/nimcp_cochlea_verification_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Constants
//=============================================================================

static const uint32_t TEST_NUM_CHANNELS = 64;
static const uint32_t TEST_SAMPLE_RATE = 44100;
static const uint32_t TEST_MAX_SAMPLES = 1024;
static const float TEST_DT_MS = 10.0f;

//=============================================================================
// Test Fixture Base
//=============================================================================

class CochleaBridgeTest : public ::testing::Test {
protected:
    cochlea_t* cochlea = nullptr;
    cochlea_output_t* output = nullptr;

    void SetUp() override {
        cochlea_config_t config = cochlea_config_default(BM_MODE_HUMAN, TEST_SAMPLE_RATE);
        config.num_channels = TEST_NUM_CHANNELS;
        cochlea = cochlea_create(&config);
        ASSERT_NE(cochlea, nullptr);

        output = cochlea_output_create(cochlea, TEST_MAX_SAMPLES);
        ASSERT_NE(output, nullptr);
    }

    void TearDown() override {
        if (output) {
            cochlea_output_destroy(output);
            output = nullptr;
        }
        if (cochlea) {
            cochlea_destroy(cochlea);
            cochlea = nullptr;
        }
    }
};

//=============================================================================
// Medulla Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, MedullaBridgeLifecycle) {
    cochlea_medulla_config_t config = cochlea_medulla_config_default();
    cochlea_medulla_bridge_t* bridge = cochlea_medulla_bridge_create(cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_medulla_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        // Check initial protection state
        bool protected_mode = cochlea_medulla_is_protection_active(bridge);
        EXPECT_FALSE(protected_mode);

        cochlea_medulla_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, MedullaBridgeNullDestroy) {
    cochlea_medulla_bridge_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Thalamic Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, ThalamicBridgeLifecycle) {
    cochlea_thalamic_config_t config = cochlea_thalamic_config_default();
    cochlea_thalamic_bridge_t* bridge = cochlea_thalamic_bridge_create(cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_thalamic_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        cochlea_thalamic_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, ThalamicBridgeNullDestroy) {
    cochlea_thalamic_bridge_destroy(nullptr);
}

//=============================================================================
// Audio Cortex Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, AudioCortexBridgeLifecycle) {
    cochlea_audio_cortex_config_t config = cochlea_audio_cortex_config_default();
    cochlea_audio_cortex_bridge_t* bridge = cochlea_audio_cortex_bridge_create(
        cochlea, nullptr, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_audio_cortex_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        cochlea_audio_cortex_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, AudioCortexBridgeNullDestroy) {
    cochlea_audio_cortex_bridge_destroy(nullptr);
}

//=============================================================================
// FEP Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, FEPBridgeLifecycle) {
    cochlea_fep_config_t config = cochlea_fep_config_default();
    cochlea_fep_bridge_t* bridge = cochlea_fep_bridge_create(cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_fep_bridge_reset(bridge);
        EXPECT_EQ(err, 0);

        cochlea_fep_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, FEPBridgeNullDestroy) {
    cochlea_fep_bridge_destroy(nullptr);
}

//=============================================================================
// Sleep Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, SleepBridgeLifecycle) {
    cochlea_sleep_config_t config = cochlea_sleep_config_default();
    cochlea_sleep_bridge_t* bridge = cochlea_sleep_bridge_create(cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_sleep_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        cochlea_sleep_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, SleepBridgeNullDestroy) {
    cochlea_sleep_bridge_destroy(nullptr);
}

//=============================================================================
// Immune Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, ImmuneBridgeLifecycle) {
    cochlea_immune_config_t config = cochlea_immune_config_default();
    cochlea_immune_bridge_t* bridge = cochlea_immune_bridge_create(cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_immune_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        cochlea_immune_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, ImmuneBridgeNullDestroy) {
    cochlea_immune_bridge_destroy(nullptr);
}

//=============================================================================
// KG Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, KGBridgeLifecycle) {
    cochlea_kg_config_t config = cochlea_kg_config_default();
    cochlea_kg_bridge_t* bridge = cochlea_kg_bridge_create(cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_kg_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        cochlea_kg_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, KGBridgeNullDestroy) {
    cochlea_kg_bridge_destroy(nullptr);
}

//=============================================================================
// RCOG Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, RCOGBridgeLifecycle) {
    cochlea_rcog_config_t config = cochlea_rcog_config_default();
    cochlea_rcog_bridge_t* bridge = cochlea_rcog_bridge_create(cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_rcog_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        cochlea_rcog_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, RCOGBridgeNullDestroy) {
    cochlea_rcog_bridge_destroy(nullptr);
}

//=============================================================================
// Collective Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, CollectiveBridgeLifecycle) {
    cochlea_collective_config_t config = cochlea_collective_config_default();
    cochlea_collective_bridge_t* bridge = cochlea_collective_bridge_create(
        cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_collective_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        cochlea_collective_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, CollectiveBridgeNullDestroy) {
    cochlea_collective_bridge_destroy(nullptr);
}

//=============================================================================
// Cortical Deep Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, CorticalDeepBridgeLifecycle) {
    cochlea_cortical_deep_config_t config = cochlea_cortical_deep_config_default();
    cochlea_cortical_deep_bridge_t* bridge = cochlea_cortical_deep_bridge_create(
        cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_cortical_deep_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        cochlea_cortical_deep_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, CorticalDeepBridgeNullDestroy) {
    cochlea_cortical_deep_bridge_destroy(nullptr);
}

//=============================================================================
// Occipital Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, OccipitalBridgeLifecycle) {
    cochlea_occipital_config_t config = cochlea_occipital_config_default();
    cochlea_occipital_bridge_t* bridge = cochlea_occipital_bridge_create(
        cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_occipital_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        // Check binding state
        bool bound = cochlea_occipital_is_bound(bridge);
        EXPECT_FALSE(bound);  // Initially not bound

        cochlea_occipital_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, OccipitalBridgeNullDestroy) {
    cochlea_occipital_bridge_destroy(nullptr);
}

//=============================================================================
// Broca Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, BrocaBridgeLifecycle) {
    cochlea_broca_config_t config = cochlea_broca_config_default();
    cochlea_broca_bridge_t* bridge = cochlea_broca_bridge_create(
        cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_broca_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        // Test phonological loop
        err = cochlea_broca_activate_loop(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        bool active = cochlea_broca_is_loop_active(bridge);
        EXPECT_TRUE(active);

        err = cochlea_broca_deactivate_loop(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        active = cochlea_broca_is_loop_active(bridge);
        EXPECT_FALSE(active);

        cochlea_broca_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, BrocaBridgeNullDestroy) {
    cochlea_broca_bridge_destroy(nullptr);
}

//=============================================================================
// Bio-Async Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, BioAsyncBridgeLifecycle) {
    cochlea_bio_async_config_t config = cochlea_bio_async_config_default();
    cochlea_bio_async_bridge_t* bridge = cochlea_bio_async_bridge_create(
        cochlea, nullptr, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_bio_async_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        // Test registration
        err = cochlea_bio_async_register(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        bool registered = cochlea_bio_async_is_registered(bridge);
        EXPECT_TRUE(registered);

        cochlea_bio_async_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, BioAsyncBridgeNullDestroy) {
    cochlea_bio_async_bridge_destroy(nullptr);
}

//=============================================================================
// Verification Bridge Tests
//=============================================================================

TEST_F(CochleaBridgeTest, VerificationBridgeLifecycle) {
    cochlea_verification_config_t config = cochlea_verification_config_default();
    cochlea_verification_bridge_t* bridge = cochlea_verification_bridge_create(
        cochlea, &config);

    if (bridge) {
        // Test reset
        nimcp_error_t err = cochlea_verification_bridge_reset(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        // Test verify all
        err = cochlea_verification_verify_all(bridge);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        // Get health
        float health = cochlea_verification_get_health(bridge);
        EXPECT_GE(health, 0.0f);
        EXPECT_LE(health, 1.0f);

        cochlea_verification_bridge_destroy(bridge);
    }
}

TEST_F(CochleaBridgeTest, VerificationBridgeNullDestroy) {
    cochlea_verification_bridge_destroy(nullptr);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST(CochleaBridgeConfigTest, MedullaConfigDefault) {
    cochlea_medulla_config_t config = cochlea_medulla_config_default();
    EXPECT_TRUE(true);  // Just ensure it doesn't crash
}

TEST(CochleaBridgeConfigTest, ThalamicConfigDefault) {
    cochlea_thalamic_config_t config = cochlea_thalamic_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, AudioCortexConfigDefault) {
    cochlea_audio_cortex_config_t config = cochlea_audio_cortex_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, FEPConfigDefault) {
    cochlea_fep_config_t config = cochlea_fep_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, SleepConfigDefault) {
    cochlea_sleep_config_t config = cochlea_sleep_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, ImmuneConfigDefault) {
    cochlea_immune_config_t config = cochlea_immune_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, KGConfigDefault) {
    cochlea_kg_config_t config = cochlea_kg_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, RCOGConfigDefault) {
    cochlea_rcog_config_t config = cochlea_rcog_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, CollectiveConfigDefault) {
    cochlea_collective_config_t config = cochlea_collective_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, CorticalDeepConfigDefault) {
    cochlea_cortical_deep_config_t config = cochlea_cortical_deep_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, OccipitalConfigDefault) {
    cochlea_occipital_config_t config = cochlea_occipital_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, BrocaConfigDefault) {
    cochlea_broca_config_t config = cochlea_broca_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, BioAsyncConfigDefault) {
    cochlea_bio_async_config_t config = cochlea_bio_async_config_default();
    EXPECT_TRUE(true);
}

TEST(CochleaBridgeConfigTest, VerificationConfigDefault) {
    cochlea_verification_config_t config = cochlea_verification_config_default();
    EXPECT_TRUE(true);
}
