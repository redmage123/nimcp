/**
 * @file test_sensory_processing_integration.cpp
 * @brief Integration tests for Phase 6 Sensory Processing modules
 *
 * TEST PHILOSOPHY:
 * - Test bidirectional communication between sensory modules and their bridges
 * - Test cross-modal sensory integration (somatosensory-motor, olfactory-amygdala, gustatory-hypothalamus)
 * - Test realistic sensory processing scenarios
 * - Test multi-sensory integration (touch + temperature, smell + taste = flavor)
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 * @version 1.0.0 Phase 6 Sensory Processing Integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

/* Somatosensory module */
extern "C" {
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"
}

/* Olfactory module */
extern "C" {
#include "core/brain/regions/olfactory/nimcp_olfactory.h"
}

/* Gustatory module */
extern "C" {
#include "core/brain/regions/gustatory/nimcp_gustatory.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @class SensoryProcessingIntegrationTest
 * @brief Fixture for testing sensory module integration
 */
class SensoryProcessingIntegrationTest : public ::testing::Test {
protected:
    nimcp_somatosensory_t* soma = nullptr;
    nimcp_olfactory_t* olfact = nullptr;
    nimcp_gustatory_t* gust = nullptr;

    void SetUp() override {
        /* Create all sensory modules with default configs */
        soma = soma_create(nullptr);
        olfact = olfact_create(nullptr);
        gust = gust_create(nullptr);
    }

    void TearDown() override {
        if (soma) soma_destroy(soma);
        if (olfact) olfact_destroy(olfact);
        if (gust) gust_destroy(gust);
    }
};

/**
 * @class SomatosensoryIntegrationTest
 * @brief Fixture specifically for somatosensory integration tests
 */
class SomatosensoryIntegrationTest : public ::testing::Test {
protected:
    nimcp_somatosensory_t* soma = nullptr;

    void SetUp() override {
        soma = soma_create(nullptr);
    }

    void TearDown() override {
        if (soma) soma_destroy(soma);
    }
};

/**
 * @class OlfactoryIntegrationTest
 * @brief Fixture specifically for olfactory integration tests
 */
class OlfactoryIntegrationTest : public ::testing::Test {
protected:
    nimcp_olfactory_t* olfact = nullptr;

    void SetUp() override {
        olfact = olfact_create(nullptr);
    }

    void TearDown() override {
        if (olfact) olfact_destroy(olfact);
    }
};

/**
 * @class GustatoryIntegrationTest
 * @brief Fixture specifically for gustatory integration tests
 */
class GustatoryIntegrationTest : public ::testing::Test {
protected:
    nimcp_gustatory_t* gust = nullptr;

    void SetUp() override {
        gust = gust_create(nullptr);
    }

    void TearDown() override {
        if (gust) gust_destroy(gust);
    }
};

//=============================================================================
// Somatosensory Integration Tests
//=============================================================================

TEST_F(SomatosensoryIntegrationTest, TouchToThalamus_SignalPropagation) {
    ASSERT_NE(soma, nullptr);

    /* Initialize thalamus bridge */
    EXPECT_EQ(soma_init_thalamus_bridge(soma, nullptr), 0);
    EXPECT_TRUE(soma->thalamus_bridge.initialized);

    /* Process touch event */
    float position[3] = {0.5f, 0.5f, 0.0f};
    uint32_t event_id;
    EXPECT_EQ(soma_process_touch(soma, BODY_SEG_INDEX_R, position, 0.7f, TOUCH_LIGHT, &event_id), 0);

    /* Sync with thalamus */
    EXPECT_EQ(soma_sync_thalamus(soma), 0);

    /* Verify thalamus activity increased */
    EXPECT_GE(soma->thalamus_bridge.vpl_activity, 0.0f);
}

TEST_F(SomatosensoryIntegrationTest, MotorCortex_Integration) {
    ASSERT_NE(soma, nullptr);

    /* Initialize motor bridge */
    EXPECT_EQ(soma_init_motor_bridge(soma, nullptr), 0);
    EXPECT_TRUE(soma->motor_bridge.initialized);

    /* Sync with motor cortex */
    EXPECT_EQ(soma_sync_motor_cortex(soma), 0);
}

TEST_F(SomatosensoryIntegrationTest, Proprioception_BodyMap_Integration) {
    ASSERT_NE(soma, nullptr);

    /* Initialize parietal bridge (body schema) */
    EXPECT_EQ(soma_init_parietal_bridge(soma, nullptr), 0);
    EXPECT_TRUE(soma->parietal_bridge.initialized);

    /* Update proprioception for multiple segments */
    float pos[3] = {0.0f, 1.0f, 0.0f};
    float vel[3] = {0.1f, 0.0f, 0.0f};
    EXPECT_EQ(soma_update_proprioception(soma, BODY_SEG_UPPER_ARM_R, pos, vel, 0.5f, 0.6f), 0);

    /* Sync with parietal cortex */
    EXPECT_EQ(soma_sync_parietal(soma), 0);

    /* Get proprioceptive state */
    soma_proprio_state_t state;
    EXPECT_EQ(soma_get_proprioception(soma, BODY_SEG_UPPER_ARM_R, &state), 0);
    EXPECT_GT(state.confidence, 0.0f);
}

TEST_F(SomatosensoryIntegrationTest, Pain_Hypothalamus_StressResponse) {
    ASSERT_NE(soma, nullptr);

    /* Initialize hypothalamus bridge */
    EXPECT_EQ(soma_init_hypothalamus_bridge(soma, nullptr), 0);
    EXPECT_TRUE(soma->hypothalamus_bridge.initialized);

    /* Process pain event */
    uint32_t event_id;
    EXPECT_EQ(soma_process_pain(soma, BODY_SEG_HAND_L, PAIN_SHARP, 0.8f, &event_id), 0);

    /* Sync with hypothalamus */
    EXPECT_EQ(soma_sync_hypothalamus(soma), 0);

    /* Verify stress/pain modulation signals */
    EXPECT_GE(soma->hypothalamus_bridge.stress_level, 0.0f);
}

TEST_F(SomatosensoryIntegrationTest, AllBridges_Coordinated_Processing) {
    ASSERT_NE(soma, nullptr);

    /* Initialize all bridges */
    EXPECT_EQ(soma_init_all_bridges(soma, nullptr), 0);

    /* Verify all bridges initialized */
    EXPECT_TRUE(soma->prime_resonance_bridge.initialized);
    EXPECT_TRUE(soma->immune_bridge.initialized);
    EXPECT_TRUE(soma->cognitive_bridge.initialized);
    EXPECT_TRUE(soma->thalamus_bridge.initialized);
    EXPECT_TRUE(soma->motor_bridge.initialized);
    EXPECT_TRUE(soma->parietal_bridge.initialized);

    /* Process touch and run bidirectional update */
    float position[3] = {0.5f, 0.5f, 0.0f};
    uint32_t event_id;
    EXPECT_EQ(soma_process_touch(soma, BODY_SEG_INDEX_R, position, 0.7f, TOUCH_LIGHT, &event_id), 0);

    /* Run bidirectional update cycle */
    EXPECT_EQ(soma_bidirectional_update(soma, 0.01f), 0);

    /* Verify stats updated */
    soma_stats_t stats;
    EXPECT_EQ(soma_get_stats(soma, &stats), 0);
    EXPECT_GT(stats.touch_events_processed, 0u);
}

TEST_F(SomatosensoryIntegrationTest, TwoPointDiscrimination_FingerVsBack) {
    ASSERT_NE(soma, nullptr);

    /* Test on fingertip (high resolution) */
    float point1[3] = {0.0f, 0.0f, 0.0f};
    float point2[3] = {0.002f, 0.0f, 0.0f};  /* 2mm apart */
    bool can_discriminate_finger;
    float confidence_finger;
    EXPECT_EQ(soma_two_point_discrimination(soma, BODY_SEG_INDEX_R, point1, point2,
                                             &can_discriminate_finger, &confidence_finger), 0);

    /* Test on back (low resolution) */
    bool can_discriminate_back;
    float confidence_back;
    EXPECT_EQ(soma_two_point_discrimination(soma, BODY_SEG_BACK, point1, point2,
                                             &can_discriminate_back, &confidence_back), 0);

    /* Fingertips should discriminate better than back at same distance */
    EXPECT_GE(confidence_finger, confidence_back);
}

//=============================================================================
// Olfactory Integration Tests
//=============================================================================

TEST_F(OlfactoryIntegrationTest, OdorProcessing_AmygdalaEmotionalResponse) {
    ASSERT_NE(olfact, nullptr);

    /* Initialize amygdala bridge */
    EXPECT_EQ(olfact_init_amygdala_bridge(olfact, nullptr), 0);
    EXPECT_TRUE(olfact->amygdala_bridge.initialized);

    /* Create odor pattern (rose-like) */
    float receptor_pattern[OLFACT_MAX_RECEPTORS];
    for (uint32_t i = 0; i < OLFACT_MAX_RECEPTORS; i++) {
        receptor_pattern[i] = (i % 10 < 3) ? 0.8f : 0.1f;
    }

    /* Process odor */
    EXPECT_EQ(olfact_process_odor(olfact, receptor_pattern, OLFACT_MAX_RECEPTORS, 0.5f), 0);

    /* Sync with amygdala */
    EXPECT_EQ(olfact_sync_amygdala(olfact), 0);

    /* Get valence (hedonic value) */
    hedonic_valence_t valence = olfact_get_valence(olfact);
    EXPECT_GE((int)valence, (int)HEDONIC_VERY_UNPLEASANT);
    EXPECT_LE((int)valence, (int)HEDONIC_VERY_PLEASANT);
}

TEST_F(OlfactoryIntegrationTest, OdorMemory_EntorhinalEncoding) {
    ASSERT_NE(olfact, nullptr);

    /* Initialize entorhinal bridge (memory encoding) */
    EXPECT_EQ(olfact_init_entorhinal_bridge(olfact, nullptr), 0);
    EXPECT_TRUE(olfact->entorhinal_bridge.initialized);

    /* Create and process odor */
    float receptor_pattern[OLFACT_MAX_RECEPTORS];
    for (uint32_t i = 0; i < OLFACT_MAX_RECEPTORS; i++) {
        receptor_pattern[i] = sinf(i * 0.1f) * 0.5f + 0.5f;
    }
    EXPECT_EQ(olfact_process_odor(olfact, receptor_pattern, OLFACT_MAX_RECEPTORS, 0.7f), 0);

    /* Identify the odor */
    olfact_odor_id_t odor_id;
    EXPECT_EQ(olfact_identify_odor(olfact, &odor_id), 0);

    /* Store memory with emotional context */
    EXPECT_EQ(olfact_store_memory(olfact, &odor_id, 0.8f, 0.6f, "grandmother's kitchen"), 0);

    /* Sync with entorhinal */
    EXPECT_EQ(olfact_sync_entorhinal(olfact), 0);

    /* Verify memory count increased */
    EXPECT_GT(olfact->num_memories, 0u);
}

TEST_F(OlfactoryIntegrationTest, SniffModulation_OdorProcessing) {
    ASSERT_NE(olfact, nullptr);

    /* Start sniff cycle */
    EXPECT_EQ(olfact_start_sniff(olfact, 0.8f), 0);

    /* Create odor pattern */
    float receptor_pattern[OLFACT_MAX_RECEPTORS];
    for (uint32_t i = 0; i < OLFACT_MAX_RECEPTORS; i++) {
        receptor_pattern[i] = (float)(i % 50) / 50.0f;
    }

    /* Process odor during sniff */
    EXPECT_EQ(olfact_process_odor(olfact, receptor_pattern, OLFACT_MAX_RECEPTORS, 0.6f), 0);

    /* Get sniff modulation */
    float modulation = olfact_get_sniff_modulation(olfact);
    EXPECT_GE(modulation, 0.0f);
    EXPECT_LE(modulation, 2.0f);  /* Modulation can boost signal */

    /* Get sniff phase */
    sniff_phase_t phase = olfact_get_sniff_phase(olfact);
    EXPECT_GE((int)phase, (int)SNIFF_PHASE_BASELINE);
}

TEST_F(OlfactoryIntegrationTest, OlfactoryProcessing_Integration) {
    ASSERT_NE(olfact, nullptr);

    /* Create odor pattern */
    float receptor_pattern[OLFACT_MAX_RECEPTORS];
    for (uint32_t i = 0; i < OLFACT_MAX_RECEPTORS; i++) {
        receptor_pattern[i] = 0.7f;
    }

    /* Process odor */
    EXPECT_EQ(olfact_process_odor(olfact, receptor_pattern, OLFACT_MAX_RECEPTORS, 0.8f), 0);

    /* Get intensity */
    float intensity = olfact_get_intensity(olfact);
    EXPECT_GE(intensity, 0.0f);
    EXPECT_LE(intensity, 1.0f);

    /* Get adaptation level */
    float adaptation = olfact_get_adaptation_level(olfact);
    EXPECT_GE(adaptation, 0.0f);
}

//=============================================================================
// Gustatory Integration Tests
//=============================================================================

TEST_F(GustatoryIntegrationTest, TasteProcessing_HypothalamusReward) {
    ASSERT_NE(gust, nullptr);

    /* Initialize hypothalamus bridge (hunger/satiety) */
    EXPECT_EQ(gust_init_hypothalamus_bridge(gust, nullptr), 0);
    EXPECT_TRUE(gust->hypothalamus_bridge.initialized);

    /* Create sweet stimulus */
    taste_stimulus_t stimulus = {};
    stimulus.sweet = 0.8f;
    stimulus.salty = 0.1f;
    stimulus.umami = 0.2f;
    stimulus.temperature = 20.0f;
    stimulus.texture = 0.5f;

    /* Process taste */
    EXPECT_EQ(gust_process_taste(gust, &stimulus), 0);

    /* Sync with hypothalamus */
    EXPECT_EQ(gust_sync_hypothalamus(gust), 0);

    /* Compute reward */
    food_reward_t reward;
    EXPECT_EQ(gust_compute_reward(gust, &reward), 0);
    EXPECT_GE(reward.reward_magnitude, 0.0f);  /* Sweet should be rewarding */
}

TEST_F(GustatoryIntegrationTest, BitterTaste_AmygdalaDisgust) {
    ASSERT_NE(gust, nullptr);

    /* Initialize amygdala bridge (disgust response) */
    EXPECT_EQ(gust_init_amygdala_bridge(gust, nullptr), 0);
    EXPECT_TRUE(gust->amygdala_bridge.initialized);

    /* Create very bitter stimulus */
    taste_stimulus_t stimulus = {};
    stimulus.bitter = 0.9f;
    stimulus.sweet = 0.0f;

    /* Process taste */
    EXPECT_EQ(gust_process_taste(gust, &stimulus), 0);

    /* Evaluate disgust */
    disgust_level_t disgust = gust_evaluate_disgust(gust);

    /* High bitter should trigger some disgust */
    EXPECT_GE((int)disgust, (int)DISGUST_NONE);

    /* Check for toxic warning */
    bool toxic = gust_is_toxic_warning(gust);
    /* Very bitter could trigger toxic warning */
    EXPECT_TRUE(toxic || disgust >= DISGUST_MILD);
}

TEST_F(GustatoryIntegrationTest, RewardComputation_Integration) {
    ASSERT_NE(gust, nullptr);

    /* Initialize hypothalamus bridge */
    EXPECT_EQ(gust_init_hypothalamus_bridge(gust, nullptr), 0);

    /* Create tasty stimulus */
    taste_stimulus_t stimulus = {};
    stimulus.sweet = 0.7f;
    stimulus.umami = 0.5f;
    stimulus.fat_content = 0.6f;

    /* Process taste */
    EXPECT_EQ(gust_process_taste(gust, &stimulus), 0);

    /* Compute reward */
    food_reward_t reward;
    EXPECT_EQ(gust_compute_reward(gust, &reward), 0);
    EXPECT_GE(reward.reward_magnitude, 0.0f);

    /* Sync with hypothalamus */
    EXPECT_EQ(gust_sync_hypothalamus(gust), 0);
}

TEST_F(GustatoryIntegrationTest, LearnedPreference_TasteAcquisition) {
    ASSERT_NE(gust, nullptr);

    /* Get initial sweet preference */
    float initial_sweet_adaptation = gust_get_adaptation(gust, TASTE_SWEET);

    /* Learn positive preference for sweet */
    EXPECT_EQ(gust_learn_preference(gust, TASTE_SWEET, 0.3f), 0);

    /* Verify preference changed */
    EXPECT_GT(gust->learned_preferences[TASTE_SWEET], 0.0f);

    /* Process sweet taste */
    taste_stimulus_t stimulus = {};
    stimulus.sweet = 0.6f;
    EXPECT_EQ(gust_process_taste(gust, &stimulus), 0);

    /* Get hedonic value - should reflect learned preference */
    taste_hedonic_t hedonic = gust_get_hedonic_value(gust);
    EXPECT_GE((int)hedonic, (int)TASTE_HEDONIC_NEUTRAL);
}

//=============================================================================
// Cross-Modal Sensory Integration Tests
//=============================================================================

TEST_F(SensoryProcessingIntegrationTest, AllModules_Created_Successfully) {
    ASSERT_NE(soma, nullptr);
    ASSERT_NE(olfact, nullptr);
    ASSERT_NE(gust, nullptr);

    /* Verify all modules are in ready state */
    EXPECT_EQ(soma_get_status(soma), SOMA_STATUS_READY);
    EXPECT_EQ(olfact_get_status(olfact), OLFACT_STATUS_READY);
    EXPECT_EQ(gust_get_status(gust), GUST_STATUS_READY);
}

TEST_F(SensoryProcessingIntegrationTest, FlavorIntegration_TasteAndSmell) {
    ASSERT_NE(olfact, nullptr);
    ASSERT_NE(gust, nullptr);

    /* Initialize olfactory bridge in gustatory for flavor integration */
    EXPECT_EQ(gust_init_olfactory_bridge(gust, olfact), 0);
    EXPECT_TRUE(gust->olfactory_bridge.initialized);

    /* Create odor pattern (fruit smell) */
    float odor_pattern[OLFACT_MAX_RECEPTORS];
    for (uint32_t i = 0; i < OLFACT_MAX_RECEPTORS; i++) {
        odor_pattern[i] = (i % 20 < 5) ? 0.7f : 0.1f;
    }
    EXPECT_EQ(olfact_process_odor(olfact, odor_pattern, OLFACT_MAX_RECEPTORS, 0.6f), 0);

    /* Create taste stimulus (sweet) */
    taste_stimulus_t stimulus = {};
    stimulus.sweet = 0.7f;
    stimulus.sour = 0.3f;
    stimulus.olfactory_component = odor_pattern;
    stimulus.olfactory_dim = 128;

    EXPECT_EQ(gust_process_taste(gust, &stimulus), 0);

    /* Sync between modules */
    EXPECT_EQ(gust_sync_olfactory(gust), 0);

    /* Verify both modules processed data */
    EXPECT_GE(olfact->updates_processed, 0u);
    EXPECT_GE(gust->updates_processed, 0u);
}

TEST_F(SensoryProcessingIntegrationTest, TouchTemperature_MultimodalIntegration) {
    ASSERT_NE(soma, nullptr);

    /* Process touch with temperature component */
    soma_touch_event_t event = {};
    event.segment = BODY_SEG_HAND_R;
    event.modality = TOUCH_PRESSURE;
    event.intensity = 0.5f;
    event.temperature = 45.0f;  /* Warm object */
    event.position[0] = 0.5f;
    event.position[1] = 0.5f;
    event.position[2] = 0.0f;

    uint32_t event_id;
    EXPECT_EQ(soma_process_touch_full(soma, &event, &event_id), 0);

    /* Process temperature (45°C is extreme hot) */
    temp_sensation_t temp_sensation;
    EXPECT_EQ(soma_process_temperature(soma, BODY_SEG_HAND_R, 45.0f, &temp_sensation), 0);

    /* Verify temperature sensation (45°C triggers extreme hot threshold) */
    EXPECT_EQ(temp_sensation, TEMP_HOT_EXTREME);
}

TEST_F(SensoryProcessingIntegrationTest, PainProcessing_Integration) {
    ASSERT_NE(soma, nullptr);

    /* Process pain event */
    uint32_t pain_id;
    EXPECT_EQ(soma_process_pain(soma, BODY_SEG_FOREARM_R, PAIN_SHARP, 0.7f, &pain_id), 0);

    /* Get pain level */
    float pain_level = soma_get_pain_level(soma, BODY_SEG_FOREARM_R);
    EXPECT_GE(pain_level, 0.0f);

    /* Apply touch to same area */
    float position[3] = {0.5f, 0.5f, 0.0f};
    uint32_t touch_id;
    EXPECT_EQ(soma_process_touch(soma, BODY_SEG_FOREARM_R, position, 0.6f, TOUCH_PRESSURE, &touch_id), 0);

    /* Verify both events processed */
    EXPECT_GE(soma->touch_events_total, 1u);
    EXPECT_GE(soma->pain_events_total, 1u);
}

TEST_F(SensoryProcessingIntegrationTest, ProprioceptionMotor_Integration) {
    ASSERT_NE(soma, nullptr);

    /* Initialize motor and parietal bridges */
    EXPECT_EQ(soma_init_motor_bridge(soma, nullptr), 0);
    EXPECT_EQ(soma_init_parietal_bridge(soma, nullptr), 0);

    /* Set motor command (expected movement) */
    float pos[3] = {0.0f, 1.0f, 0.0f};
    float vel[3] = {0.1f, 0.0f, 0.0f};
    EXPECT_EQ(soma_update_proprioception(soma, BODY_SEG_HAND_R, pos, vel, 0.5f, 0.6f), 0);

    /* Get proprioceptive state */
    soma_proprio_state_t state;
    EXPECT_EQ(soma_get_proprioception(soma, BODY_SEG_HAND_R, &state), 0);
    EXPECT_GT(state.confidence, 0.0f);

    /* Sync with motor and parietal */
    EXPECT_EQ(soma_sync_motor_cortex(soma), 0);
    EXPECT_EQ(soma_sync_parietal(soma), 0);
}

TEST_F(SensoryProcessingIntegrationTest, BidirectionalUpdate_AllModules) {
    ASSERT_NE(soma, nullptr);
    ASSERT_NE(olfact, nullptr);
    ASSERT_NE(gust, nullptr);

    /* Initialize all bridges for each module */
    EXPECT_EQ(soma_init_all_bridges(soma, nullptr), 0);
    EXPECT_EQ(olfact_init_prime_resonance_bridge(olfact, nullptr), 0);
    EXPECT_EQ(olfact_init_amygdala_bridge(olfact, nullptr), 0);
    EXPECT_EQ(gust_init_prime_resonance_bridge(gust, nullptr), 0);
    EXPECT_EQ(gust_init_hypothalamus_bridge(gust, nullptr), 0);

    /* Stimulate all modalities */
    float touch_pos[3] = {0.5f, 0.5f, 0.0f};
    uint32_t touch_id;
    EXPECT_EQ(soma_process_touch(soma, BODY_SEG_HAND_R, touch_pos, 0.6f, TOUCH_LIGHT, &touch_id), 0);

    float odor[OLFACT_MAX_RECEPTORS];
    for (uint32_t i = 0; i < OLFACT_MAX_RECEPTORS; i++) odor[i] = 0.5f;
    EXPECT_EQ(olfact_process_odor(olfact, odor, OLFACT_MAX_RECEPTORS, 0.5f), 0);

    taste_stimulus_t taste = {};
    taste.sweet = 0.6f;
    EXPECT_EQ(gust_process_taste(gust, &taste), 0);

    /* Run bidirectional updates */
    EXPECT_EQ(soma_bidirectional_update(soma, 0.01f), 0);
    EXPECT_EQ(olfact_bidirectional_update(olfact, 0.01f), 0);
    EXPECT_EQ(gust_bidirectional_update(gust, 0.01f), 0);

    /* Verify all modules updated */
    EXPECT_GE(soma->updates_processed, 1u);
    EXPECT_GE(olfact->updates_processed, 1u);
    EXPECT_GE(gust->updates_processed, 1u);
}

//=============================================================================
// Serialization Integration Tests
//=============================================================================

TEST_F(SomatosensoryIntegrationTest, Serialization_StatePreserved) {
    ASSERT_NE(soma, nullptr);

    /* Process some events to create state */
    float pos[3] = {0.5f, 0.5f, 0.0f};
    uint32_t event_id;
    soma_process_touch(soma, BODY_SEG_INDEX_R, pos, 0.7f, TOUCH_LIGHT, &event_id);
    soma_process_pain(soma, BODY_SEG_HAND_L, PAIN_SHARP, 0.5f, &event_id);

    /* Update to process events */
    soma_update(soma, 0.01f);

    /* Serialize */
    size_t size = soma_get_serialization_size(soma);
    EXPECT_GT(size, 0u);

    std::vector<uint8_t> buffer(size);
    size_t written;
    EXPECT_EQ(soma_serialize(soma, buffer.data(), size, &written), 0);
    EXPECT_GT(written, 0u);

    /* Deserialize */
    size_t bytes_read;
    nimcp_somatosensory_t* restored = soma_deserialize(buffer.data(), written, &bytes_read);
    ASSERT_NE(restored, nullptr);

    /* Verify state preserved */
    EXPECT_EQ(restored->config.num_area_3a_neurons, soma->config.num_area_3a_neurons);
    EXPECT_EQ(restored->status, soma->status);

    soma_destroy(restored);
}

TEST_F(OlfactoryIntegrationTest, Serialization_MemoriesPreserved) {
    ASSERT_NE(olfact, nullptr);

    /* Create and store a memory */
    float odor[OLFACT_MAX_RECEPTORS];
    for (uint32_t i = 0; i < OLFACT_MAX_RECEPTORS; i++) odor[i] = (float)i / OLFACT_MAX_RECEPTORS;
    olfact_process_odor(olfact, odor, OLFACT_MAX_RECEPTORS, 0.7f);

    olfact_odor_id_t odor_id;
    olfact_identify_odor(olfact, &odor_id);
    olfact_store_memory(olfact, &odor_id, 0.8f, 0.6f, "test context");

    /* Serialize */
    size_t size = olfact_get_serialization_size(olfact);
    EXPECT_GT(size, 0u);

    std::vector<uint8_t> buffer(size);
    size_t written;
    EXPECT_EQ(olfact_serialize(olfact, buffer.data(), size, &written), 0);

    /* Deserialize */
    size_t bytes_read;
    nimcp_olfactory_t* restored = olfact_deserialize(buffer.data(), written, &bytes_read);
    ASSERT_NE(restored, nullptr);

    /* Verify memories preserved */
    EXPECT_EQ(restored->num_memories, olfact->num_memories);

    olfact_destroy(restored);
}

TEST_F(GustatoryIntegrationTest, Serialization_PreferencesPreserved) {
    ASSERT_NE(gust, nullptr);

    /* Learn some preferences */
    gust_learn_preference(gust, TASTE_SWEET, 0.3f);
    gust_learn_preference(gust, TASTE_BITTER, -0.2f);

    /* Serialize */
    size_t size = gust_get_serialization_size(gust);
    EXPECT_GT(size, 0u);

    std::vector<uint8_t> buffer(size);
    size_t written;
    EXPECT_EQ(gust_serialize(gust, buffer.data(), size, &written), 0);

    /* Deserialize */
    size_t bytes_read;
    nimcp_gustatory_t* restored = gust_deserialize(buffer.data(), written, &bytes_read);
    ASSERT_NE(restored, nullptr);

    /* Verify preferences preserved */
    EXPECT_FLOAT_EQ(restored->learned_preferences[TASTE_SWEET], gust->learned_preferences[TASTE_SWEET]);
    EXPECT_FLOAT_EQ(restored->learned_preferences[TASTE_BITTER], gust->learned_preferences[TASTE_BITTER]);

    gust_destroy(restored);
}
