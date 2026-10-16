/**
 * @file test_reticular_brain_init_integration.cpp
 * @brief Integration tests for Reticular Formation brain initialization
 *
 * WHAT: Tests Reticular Formation integration with brain factory
 * WHY:  Ensure proper lifecycle and arousal/autonomic system integration
 * HOW:  Test creation, configuration, arousal, neuromodulators, and autonomic
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/reticular/nimcp_reticular.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ReticularBrainInitTest : public ::testing::Test {
protected:
    nimcp_reticular_t* reticular;
    reticular_config_t config;
    bool router_initialized;

    void SetUp() override {
        router_initialized = false;
        reticular = NULL;

        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        memset(&config, 0, sizeof(config));
        reticular_default_config(&config);
    }

    void TearDown() override {
        if (reticular) {
            reticular_destroy(reticular);
            reticular = NULL;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }

    /* Helper: create and initialize Reticular Formation in one step */
    nimcp_reticular_t* createAndInitReticular(const reticular_config_t* cfg) {
        nimcp_reticular_t* instance = reticular_create(cfg);
        if (instance) {
            reticular_init(instance);
        }
        return instance;
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(ReticularBrainInitTest, CreateWithDefaultConfig) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    /* Reticular Formation uses two-phase init: create allocates, init sets up state */
    int init_result = reticular_init(reticular);
    EXPECT_EQ(0, init_result);
    EXPECT_TRUE(reticular->initialized);
}

TEST_F(ReticularBrainInitTest, DestroyNull) {
    reticular_destroy(NULL);
}

TEST_F(ReticularBrainInitTest, ResetAfterCreate) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    int result = reticular_reset(reticular);
    EXPECT_EQ(0, result);
    EXPECT_TRUE(reticular->initialized);
}

TEST_F(ReticularBrainInitTest, MultipleCreateDestroyCycles) {
    for (int i = 0; i < 5; i++) {
        reticular = reticular_create(&config);
        ASSERT_NE(nullptr, reticular) << "Cycle " << i << " failed";
        reticular_destroy(reticular);
        reticular = NULL;
    }
}

/*=============================================================================
 * AROUSAL TESTS
 *===========================================================================*/

TEST_F(ReticularBrainInitTest, GetArousal) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    float arousal = reticular_get_arousal(reticular);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);
}

TEST_F(ReticularBrainInitTest, GetArousalState) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    reticular_arousal_state_t state = reticular_get_arousal_state(reticular);
    EXPECT_GE((int)state, 0);
    EXPECT_LT((int)state, RETICULAR_AROUSAL_COUNT);
}

TEST_F(ReticularBrainInitTest, ApplyArousalStimulus) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    int result = reticular_apply_arousal_stimulus(reticular, 0.5f, 0);
    EXPECT_EQ(0, result);
}

TEST_F(ReticularBrainInitTest, WakeUp) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    int result = reticular_wake(reticular, 0.8f);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * NEUROMODULATOR TESTS
 *===========================================================================*/

TEST_F(ReticularBrainInitTest, GetModulator) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    float serotonin = reticular_get_modulator(reticular, RETICULAR_MODULATOR_SEROTONIN);
    EXPECT_GE(serotonin, 0.0f);
    EXPECT_LE(serotonin, 1.0f);

    float norepinephrine = reticular_get_modulator(reticular, RETICULAR_MODULATOR_NOREPINEPHRINE);
    EXPECT_GE(norepinephrine, 0.0f);
    EXPECT_LE(norepinephrine, 1.0f);
}

TEST_F(ReticularBrainInitTest, SetModulatorRelease) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    int result = reticular_set_modulator_release(reticular, RETICULAR_MODULATOR_DOPAMINE, 0.6f);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * NUCLEUS TESTS
 *===========================================================================*/

TEST_F(ReticularBrainInitTest, GetNucleusActivity) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    float activity = reticular_get_nucleus_activity(reticular, RETICULAR_NUCLEUS_LOCUS_COERULEUS);
    EXPECT_GE(activity, 0.0f);
    EXPECT_LE(activity, 1.0f);
}

TEST_F(ReticularBrainInitTest, StimulateNucleus) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    int result = reticular_stimulate_nucleus(reticular, RETICULAR_NUCLEUS_RAPHE_DORSAL, 0.5f, 100.0f);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * AUTONOMIC TESTS
 *===========================================================================*/

TEST_F(ReticularBrainInitTest, GetAutonomicBalance) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    float balance = reticular_get_autonomic_balance(reticular, RETICULAR_AUTONOMIC_CARDIOVASCULAR);
    EXPECT_GE(balance, -1.0f);
    EXPECT_LE(balance, 1.0f);
}

TEST_F(ReticularBrainInitTest, ApplySympathetic) {
    reticular = reticular_create(&config);
    ASSERT_NE(nullptr, reticular);

    int result = reticular_apply_sympathetic_drive(reticular, 0.7f);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * UPDATE CYCLE TESTS
 *===========================================================================*/

TEST_F(ReticularBrainInitTest, UpdateArousalCycle) {
    reticular = createAndInitReticular(&config);
    ASSERT_NE(nullptr, reticular);

    for (int i = 0; i < 100; i++) {
        int result = reticular_update_arousal(reticular, 10.0f);
        EXPECT_EQ(0, result);
    }

    EXPECT_TRUE(reticular->initialized);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
