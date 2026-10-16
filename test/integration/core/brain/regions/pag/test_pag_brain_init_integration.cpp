/**
 * @file test_pag_brain_init_integration.cpp
 * @brief Integration tests for PAG brain initialization system
 *
 * WHAT: Tests PAG integration with brain factory initialization
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

#include "core/brain/regions/pag/nimcp_pag.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class PAGBrainInitTest : public ::testing::Test {
protected:
    nimcp_pag_t* pag;
    pag_config_t config;
    bool router_initialized;

    void SetUp() override {
        router_initialized = false;
        pag = NULL;

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
        pag_default_config(&config);
    }

    void TearDown() override {
        if (pag) {
            pag_destroy(pag);
            pag = NULL;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }

    /* Helper: create and initialize PAG in one step */
    nimcp_pag_t* createAndInitPAG(const pag_config_t* cfg) {
        nimcp_pag_t* instance = pag_create(cfg);
        if (instance) {
            pag_init(instance);
        }
        return instance;
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(PAGBrainInitTest, CreateWithDefaultConfig) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    /* PAG uses two-phase init: create allocates, init sets up state */
    int init_result = pag_init(pag);
    EXPECT_EQ(0, init_result);
    EXPECT_TRUE(pag->initialized);
}

TEST_F(PAGBrainInitTest, DestroyNull) {
    /* Should not crash */
    pag_destroy(NULL);
}

TEST_F(PAGBrainInitTest, ResetAfterCreate) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    int result = pag_reset(pag);
    EXPECT_EQ(0, result);
    EXPECT_TRUE(pag->initialized);
}

TEST_F(PAGBrainInitTest, MultipleCreateDestroyCycles) {
    for (int i = 0; i < 5; i++) {
        pag = pag_create(&config);
        ASSERT_NE(nullptr, pag) << "Cycle " << i << " create failed";

        /* Must call init after create */
        int init_result = pag_init(pag);
        EXPECT_EQ(0, init_result);
        EXPECT_TRUE(pag->initialized);

        pag_destroy(pag);
        pag = NULL;
    }
}

/*=============================================================================
 * COLUMN TESTS
 *===========================================================================*/

TEST_F(PAGBrainInitTest, AllColumnsAccessible) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    for (int i = 0; i < PAG_COLUMN_COUNT; i++) {
        float activity = pag_get_column_activity(pag, (pag_column_t)i);
        EXPECT_GE(activity, 0.0f);
        EXPECT_LE(activity, 1.0f);
    }
}

TEST_F(PAGBrainInitTest, GetDominantColumn) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    pag_column_t dominant = pag_get_dominant_column(pag);
    EXPECT_GE((int)dominant, 0);
    EXPECT_LT((int)dominant, PAG_COLUMN_COUNT);
}

TEST_F(PAGBrainInitTest, SetColumnModulation) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    int result = pag_set_column_modulation(pag, PAG_COLUMN_DORSOLATERAL, 0.8f);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * DEFENSE BEHAVIOR TESTS
 *===========================================================================*/

TEST_F(PAGBrainInitTest, ProcessThreat) {
    pag = createAndInitPAG(&config);
    ASSERT_NE(nullptr, pag);

    int result = pag_process_threat(pag, PAG_THREAT_PROXIMAL, 0.7f, 0.0f, 10.0f);
    EXPECT_EQ(0, result);
}

TEST_F(PAGBrainInitTest, GetDefenseState) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    pag_defense_state_t state;
    int result = pag_get_defense_state(pag, &state);
    EXPECT_EQ(0, result);
}

TEST_F(PAGBrainInitTest, ClearThreat) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    pag_process_threat(pag, PAG_THREAT_IMMINENT, 0.9f, 0.0f, 5.0f);
    int result = pag_clear_threat(pag);
    EXPECT_EQ(0, result);
}

TEST_F(PAGBrainInitTest, GetCopingStrategy) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    pag_coping_strategy_t coping = pag_get_coping_strategy(pag);
    EXPECT_TRUE(coping == PAG_COPING_ACTIVE ||
                coping == PAG_COPING_PASSIVE ||
                coping == PAG_COPING_MIXED);
}

/*=============================================================================
 * PAIN MODULATION TESTS
 *===========================================================================*/

TEST_F(PAGBrainInitTest, ProcessPain) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    pag_pain_input_t pain;
    memset(&pain, 0, sizeof(pain));
    pain.intensity = 0.6f;
    pain.unpleasantness = 0.5f;
    pain.nociceptive = true;

    int result = pag_process_pain(pag, &pain);
    EXPECT_EQ(0, result);
}

TEST_F(PAGBrainInitTest, GetAnalgesiaState) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    pag_analgesia_state_t state;
    int result = pag_get_analgesia_state(pag, &state);
    EXPECT_EQ(0, result);
    EXPECT_GE(state.analgesia_level, 0.0f);
    EXPECT_LE(state.analgesia_level, 1.0f);
}

TEST_F(PAGBrainInitTest, TriggerStressAnalgesia) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    /* Process threat to create stress */
    pag_process_threat(pag, PAG_THREAT_IMMINENT, 0.9f, 0.0f, 2.0f);

    int result = pag_trigger_stress_analgesia(pag, 0.8f);
    EXPECT_EQ(0, result);
}

TEST_F(PAGBrainInitTest, GetDescendingInhibition) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    float inhibition = pag_get_descending_inhibition(pag);
    EXPECT_GE(inhibition, 0.0f);
    EXPECT_LE(inhibition, 1.0f);
}

/*=============================================================================
 * VOCALIZATION TESTS
 *===========================================================================*/

TEST_F(PAGBrainInitTest, TriggerVocalization) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    int result = pag_trigger_vocalization(pag, PAG_VOCAL_ALARM, 0.8f);
    EXPECT_EQ(0, result);
}

TEST_F(PAGBrainInitTest, StopVocalization) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    pag_trigger_vocalization(pag, PAG_VOCAL_DISTRESS, 0.6f);

    int result = pag_stop_vocalization(pag);
    EXPECT_EQ(0, result);
}

TEST_F(PAGBrainInitTest, GetVocalizationState) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    pag_vocal_state_t state;
    int result = pag_get_vocalization_state(pag, &state);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * AUTONOMIC OUTPUT TESTS
 *===========================================================================*/

TEST_F(PAGBrainInitTest, GetAutonomicState) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    pag_autonomic_state_t state;
    int result = pag_get_autonomic_state(pag, &state);
    EXPECT_EQ(0, result);
}

TEST_F(PAGBrainInitTest, TonicImmobility) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    bool immobility = pag_is_tonic_immobility(pag);
    /* Should be false initially */
    EXPECT_FALSE(immobility);
}

/*=============================================================================
 * EMOTIONAL STATE TESTS
 *===========================================================================*/

TEST_F(PAGBrainInitTest, GetEmotionalState) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    pag_emotional_state_t state;
    int result = pag_get_emotional_state(pag, &state);
    EXPECT_EQ(0, result);
}

TEST_F(PAGBrainInitTest, GetDominantEmotion) {
    pag = pag_create(&config);
    ASSERT_NE(nullptr, pag);

    pag_emotion_type_t emotion = pag_get_dominant_emotion(pag);
    EXPECT_GE((int)emotion, 0);
    EXPECT_LT((int)emotion, PAG_EMOTION_COUNT);
}

/*=============================================================================
 * UPDATE CYCLE TESTS
 *===========================================================================*/

TEST_F(PAGBrainInitTest, UpdateCycle) {
    pag = createAndInitPAG(&config);
    ASSERT_NE(nullptr, pag);

    for (int i = 0; i < 100; i++) {
        int result = pag_update(pag, 10.0f);
        EXPECT_EQ(0, result);
    }

    EXPECT_TRUE(pag->initialized);
}

/*=============================================================================
 * STRING FUNCTION TESTS
 *===========================================================================*/

TEST_F(PAGBrainInitTest, ColumnStrings) {
    for (int i = 0; i < PAG_COLUMN_COUNT; i++) {
        const char* str = pag_column_string((pag_column_t)i);
        EXPECT_NE(nullptr, str);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(PAGBrainInitTest, DefenseStrings) {
    for (int i = 0; i < PAG_DEFENSE_COUNT; i++) {
        const char* str = pag_defense_string((pag_defense_type_t)i);
        EXPECT_NE(nullptr, str);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(PAGBrainInitTest, ThreatStrings) {
    EXPECT_NE(nullptr, pag_threat_string(PAG_THREAT_DISTAL));
    EXPECT_NE(nullptr, pag_threat_string(PAG_THREAT_IMMINENT));
    EXPECT_NE(nullptr, pag_threat_string(PAG_THREAT_NONE));
}

TEST_F(PAGBrainInitTest, EmotionStrings) {
    for (int i = 0; i < PAG_EMOTION_COUNT; i++) {
        const char* str = pag_emotion_string((pag_emotion_type_t)i);
        EXPECT_NE(nullptr, str);
        EXPECT_GT(strlen(str), 0u);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
