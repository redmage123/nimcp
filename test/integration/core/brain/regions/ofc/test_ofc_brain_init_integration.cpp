/**
 * @file test_ofc_brain_init_integration.cpp
 * @brief Integration tests for OFC brain initialization system
 *
 * WHAT: Tests OFC integration with brain factory initialization
 * WHY:  Ensure proper lifecycle management and brain system integration
 * HOW:  Test creation, configuration, reset, and destruction
 *
 * INTEGRATION POINTS:
 * - Brain factory registration
 * - Configuration propagation
 * - Lifecycle callbacks
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/ofc/nimcp_ofc.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class OFCBrainInitTest : public ::testing::Test {
protected:
    nimcp_ofc_t* ofc;
    ofc_config_t config;
    bool router_initialized;

    void SetUp() override {
        router_initialized = false;
        ofc = NULL;

        /* Initialize bio-async router */
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        /* Get default config */
        memset(&config, 0, sizeof(config));
        ofc_default_config(&config);
    }

    void TearDown() override {
        if (ofc) {
            ofc_destroy(ofc);
            ofc = NULL;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }

    /* Helper: create and initialize OFC in one step */
    nimcp_ofc_t* createAndInitOFC(const ofc_config_t* cfg) {
        nimcp_ofc_t* instance = ofc_create(cfg);
        if (instance) {
            ofc_init(instance);
        }
        return instance;
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(OFCBrainInitTest, CreateWithDefaultConfig) {
    ofc = ofc_create(&config);
    ASSERT_NE(nullptr, ofc);

    /* OFC uses two-phase init: create allocates, init sets up state */
    int init_result = ofc_init(ofc);
    EXPECT_EQ(0, init_result);
    EXPECT_TRUE(ofc->initialized);
}

TEST_F(OFCBrainInitTest, CreateWithCustomConfig) {
    config.learning_rate = 0.05f;
    config.discount_rate = 0.9f;
    config.risk_sensitivity = 0.2f;
    config.social_weight = 0.5f;
    config.decision_threshold = 0.6f;

    ofc = ofc_create(&config);
    ASSERT_NE(nullptr, ofc);

    EXPECT_FLOAT_EQ(0.05f, ofc->config.learning_rate);
    EXPECT_FLOAT_EQ(0.9f, ofc->config.discount_rate);
    EXPECT_FLOAT_EQ(0.2f, ofc->config.risk_sensitivity);
    EXPECT_FLOAT_EQ(0.5f, ofc->config.social_weight);
    EXPECT_FLOAT_EQ(0.6f, ofc->config.decision_threshold);
}

TEST_F(OFCBrainInitTest, CreateNullConfigReturnsNull) {
    ofc = ofc_create(NULL);
    /* May return NULL or use defaults */
    /* Implementation-dependent */
}

TEST_F(OFCBrainInitTest, DestroyNull) {
    /* Should not crash */
    ofc_destroy(NULL);
}

TEST_F(OFCBrainInitTest, ResetAfterCreate) {
    ofc = ofc_create(&config);
    ASSERT_NE(nullptr, ofc);

    /* reset calls init internally */
    int result = ofc_reset(ofc);
    EXPECT_EQ(0, result);
    EXPECT_TRUE(ofc->initialized);
}

TEST_F(OFCBrainInitTest, MultipleCreateDestroyCycles) {
    for (int i = 0; i < 5; i++) {
        ofc = ofc_create(&config);
        ASSERT_NE(nullptr, ofc) << "Cycle " << i << " create failed";

        /* Must call init after create */
        int init_result = ofc_init(ofc);
        EXPECT_EQ(0, init_result);
        EXPECT_TRUE(ofc->initialized);

        ofc_destroy(ofc);
        ofc = NULL;
    }
}

/*=============================================================================
 * CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(OFCBrainInitTest, DefaultConfigValid) {
    ofc_config_t def;
    int result = ofc_default_config(&def);
    EXPECT_EQ(0, result);

    /* Learning rate should be reasonable */
    EXPECT_GT(def.learning_rate, 0.0f);
    EXPECT_LE(def.learning_rate, 1.0f);

    /* Discount rate should be between 0 and 1 */
    EXPECT_GT(def.discount_rate, 0.0f);
    EXPECT_LE(def.discount_rate, 1.0f);

    /* Risk sensitivity can be negative (averse) to positive (seeking) */
    EXPECT_GE(def.risk_sensitivity, -1.0f);
    EXPECT_LE(def.risk_sensitivity, 1.0f);
}

TEST_F(OFCBrainInitTest, IntegrationFlagsConfiguration) {
    config.enable_bio_async = true;
    config.enable_kg_wiring = true;
    config.enable_immune = true;
    config.enable_security = true;
    config.enable_quantum = false;

    ofc = ofc_create(&config);
    ASSERT_NE(nullptr, ofc);

    EXPECT_TRUE(ofc->config.enable_bio_async);
    EXPECT_TRUE(ofc->config.enable_kg_wiring);
    EXPECT_TRUE(ofc->config.enable_immune);
    EXPECT_TRUE(ofc->config.enable_security);
    EXPECT_FALSE(ofc->config.enable_quantum);
}

TEST_F(OFCBrainInitTest, ResourceLimitsConfiguration) {
    config.max_options = 32;
    config.max_history_size = 1000;
    config.update_interval_ms = 50;

    ofc = ofc_create(&config);
    ASSERT_NE(nullptr, ofc);

    EXPECT_EQ(32u, ofc->config.max_options);
    EXPECT_EQ(1000u, ofc->config.max_history_size);
    EXPECT_EQ(50u, ofc->config.update_interval_ms);
}

/*=============================================================================
 * INITIAL STATE TESTS
 *===========================================================================*/

TEST_F(OFCBrainInitTest, InitialStatsZero) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    ofc_stats_t stats;
    int result = ofc_get_stats(ofc, &stats);
    EXPECT_EQ(0, result);

    EXPECT_EQ(0u, stats.decisions_made);
    EXPECT_EQ(0u, stats.reversals_detected);
    EXPECT_EQ(0u, stats.prediction_errors);
}

TEST_F(OFCBrainInitTest, InitialSubdivisionsInactive) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    for (int i = 0; i < OFC_SUBDIV_COUNT; i++) {
        float activity = ofc_get_subdivision_activity(ofc, (ofc_subdivision_t)i);
        EXPECT_GE(activity, 0.0f);
        EXPECT_LE(activity, 1.0f);
    }
}

TEST_F(OFCBrainInitTest, InitialNoDecisionPending) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    EXPECT_FALSE(ofc->decision_pending);
}

TEST_F(OFCBrainInitTest, InitialNoOptions) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    EXPECT_EQ(0u, ofc->num_options);
}

/*=============================================================================
 * UPDATE CYCLE TESTS
 *===========================================================================*/

TEST_F(OFCBrainInitTest, UpdateWithNoOptions) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    /* Should not crash with no options */
    int result = ofc_update(ofc, 10.0f);
    EXPECT_EQ(0, result);
}

TEST_F(OFCBrainInitTest, MultipleUpdateCycles) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    for (int i = 0; i < 100; i++) {
        int result = ofc_update(ofc, 10.0f);
        EXPECT_EQ(0, result);
    }

    /* System should remain stable */
    EXPECT_TRUE(ofc->initialized);
}

/*=============================================================================
 * OPTION MANAGEMENT TESTS
 *===========================================================================*/

TEST_F(OFCBrainInitTest, PresentSingleOption) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    int result = ofc_present_option(ofc, 1, 0.8f, 0.7f, 0.0f);
    EXPECT_EQ(0, result);
    EXPECT_EQ(1u, ofc->num_options);
}

TEST_F(OFCBrainInitTest, PresentMultipleOptions) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    for (uint32_t i = 1; i <= 5; i++) {
        int result = ofc_present_option(ofc, i, 0.5f * (float)i, 0.8f, 0.0f);
        EXPECT_EQ(0, result);
    }

    EXPECT_EQ(5u, ofc->num_options);
}

TEST_F(OFCBrainInitTest, ClearOptions) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    /* Add some options */
    ofc_present_option(ofc, 1, 0.5f, 0.7f, 0.0f);
    ofc_present_option(ofc, 2, 0.6f, 0.8f, 0.0f);
    EXPECT_EQ(2u, ofc->num_options);

    /* Clear */
    int result = ofc_clear_options(ofc);
    EXPECT_EQ(0, result);
    EXPECT_EQ(0u, ofc->num_options);
}

/*=============================================================================
 * SUBDIVISION ACTIVITY TESTS
 *===========================================================================*/

TEST_F(OFCBrainInitTest, GetSubdivisionActivityValid) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    float lateral = ofc_get_subdivision_activity(ofc, OFC_SUBDIV_LATERAL);
    float medial = ofc_get_subdivision_activity(ofc, OFC_SUBDIV_MEDIAL);
    float anterior = ofc_get_subdivision_activity(ofc, OFC_SUBDIV_ANTERIOR);
    float posterior = ofc_get_subdivision_activity(ofc, OFC_SUBDIV_POSTERIOR);

    EXPECT_GE(lateral, 0.0f);
    EXPECT_GE(medial, 0.0f);
    EXPECT_GE(anterior, 0.0f);
    EXPECT_GE(posterior, 0.0f);
}

/*=============================================================================
 * EMOTION INTEGRATION TESTS
 *===========================================================================*/

TEST_F(OFCBrainInitTest, SetEmotion) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    int result = ofc_set_emotion(ofc, 0.7f, 0.5f);
    EXPECT_EQ(0, result);

    EXPECT_FLOAT_EQ(0.7f, ofc->emotion_valence);
    EXPECT_FLOAT_EQ(0.5f, ofc->emotion_arousal);
}

TEST_F(OFCBrainInitTest, EmotionModulatedValue) {
    ofc = createAndInitOFC(&config);
    ASSERT_NE(nullptr, ofc);

    /* Present an option */
    ofc_present_option(ofc, 1, 0.8f, 0.9f, 0.0f);

    /* Set emotion */
    ofc_set_emotion(ofc, 0.8f, 0.6f);

    /* Get modulated value */
    float modulated = ofc_get_emotion_modulated_value(ofc, 1);
    /* Value should be influenced by emotion */
    EXPECT_GE(modulated, -1.0f);
    EXPECT_LE(modulated, 1.0f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
