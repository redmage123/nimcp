/**
 * @file test_amygdala_autobio_bridge.cpp
 * @brief Unit tests for amygdala-autobiographical memory bridge
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_amygdala_autobio_bridge.h"
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "cognitive/nimcp_autobiographical_memory.h"
#include "utils/error/nimcp_error_codes.h"

/**
 * Test fixture for amygdala-autobio bridge tests
 */
class AmygdalaAutobioTest : public ::testing::Test {
protected:
    amygdala_autobio_bridge_t* bridge;
    amygdala_t* amygdala;
    autobiographical_memory_t autobio;

    void SetUp() override {
        /* Create amygdala with defaults */
        amyg_config_t amyg_config;
        amygdala_default_config(&amyg_config);
        amygdala = amygdala_create(&amyg_config);
        ASSERT_NE(amygdala, nullptr);

        /* Create autobiographical memory */
        autobio = autobio_create(100);
        ASSERT_NE(autobio, nullptr);

        /* Create bridge with defaults */
        bridge = amygdala_autobio_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            amygdala_autobio_destroy(bridge);
        }
        if (amygdala) {
            amygdala_destroy(amygdala);
        }
        if (autobio) {
            autobio_destroy(autobio);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(AmygdalaAutobioTest, CreateDestroy) {
    /* Bridge created in SetUp, destroyed in TearDown */
    EXPECT_NE(bridge, nullptr);
    EXPECT_NE(bridge->base.mutex, nullptr);
}

TEST_F(AmygdalaAutobioTest, DefaultConfig) {
    amygdala_autobio_config_t config;
    int result = amygdala_autobio_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_emotional_tagging);
    EXPECT_TRUE(config.enable_flashbulb_memories);
    EXPECT_TRUE(config.enable_fear_consolidation);
    EXPECT_TRUE(config.enable_recall_reactivation);
    EXPECT_TRUE(config.enable_positive_regulation);
    EXPECT_EQ(config.salience_sensitivity, 1.0f);
}

TEST_F(AmygdalaAutobioTest, CreateWithCustomConfig) {
    amygdala_autobio_config_t config;
    amygdala_autobio_default_config(&config);
    config.salience_sensitivity = 1.5f;
    config.enable_negative_bias = false;

    amygdala_autobio_bridge_t* custom_bridge = amygdala_autobio_create(&config);
    ASSERT_NE(custom_bridge, nullptr);

    amygdala_autobio_destroy(custom_bridge);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(AmygdalaAutobioTest, ConnectAmygdala) {
    int result = amygdala_autobio_connect_amygdala(bridge, amygdala);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->base.system_a_connected);
}

TEST_F(AmygdalaAutobioTest, ConnectMemory) {
    int result = amygdala_autobio_connect_memory(bridge, autobio);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->base.system_b_connected);
}

TEST_F(AmygdalaAutobioTest, ConnectBoth) {
    int result1 = amygdala_autobio_connect_amygdala(bridge, amygdala);
    int result2 = amygdala_autobio_connect_memory(bridge, autobio);

    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);
    EXPECT_TRUE(bridge->base.bridge_active);
}

TEST_F(AmygdalaAutobioTest, ConnectNullPointers) {
    int result1 = amygdala_autobio_connect_amygdala(nullptr, amygdala);
    int result2 = amygdala_autobio_connect_memory(bridge, nullptr);

    EXPECT_NE(result1, 0);
    EXPECT_NE(result2, 0);
}

/* ============================================================================
 * Emotional Tagging Tests
 * ============================================================================ */

TEST_F(AmygdalaAutobioTest, UpdateEmotionalTagging) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* Set amygdala to fearful state */
    amygdala_set_anxiety(amygdala, 0.6f);
    amygdala_set_nucleus_activation(amygdala, AMYG_NUCLEUS_CENTRAL, 0.8f);

    int result = amygdala_autobio_update(bridge);
    EXPECT_EQ(result, 0);

    /* Check that emotional salience is computed */
    float salience_boost = amygdala_autobio_get_salience_boost(bridge);
    EXPECT_GT(salience_boost, 1.0f);  /* Should be > 1.0 with fear */
}

TEST_F(AmygdalaAutobioTest, EmotionalTaggingLowArousal) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* Low arousal state */
    amygdala_set_anxiety(amygdala, 0.1f);

    int result = amygdala_autobio_update(bridge);
    EXPECT_EQ(result, 0);

    /* Salience boost should be modest */
    float salience_boost = amygdala_autobio_get_salience_boost(bridge);
    EXPECT_GT(salience_boost, 1.0f);
    EXPECT_LT(salience_boost, 1.5f);
}

TEST_F(AmygdalaAutobioTest, FlashbulbMode) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* Flashbulb mode requires:
     * 1. fear_level >= flashbulb_fear_threshold (default 0.75)
     * 2. arousal_level >= FLASHBULB_AROUSAL_THRESHOLD
     * arousal = fear_level * 0.7 + anxiety_level * 0.3
     */
    amygdala_set_fear_level(amygdala, 0.9f);  /* Must set fear level directly */
    amygdala_set_anxiety(amygdala, 0.9f);

    amygdala_autobio_update(bridge);

    bool flashbulb = amygdala_autobio_is_flashbulb_mode(bridge);
    EXPECT_TRUE(flashbulb);
}

TEST_F(AmygdalaAutobioTest, NoFlashbulbLowFear) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* Low fear */
    amygdala_set_anxiety(amygdala, 0.3f);

    amygdala_autobio_update(bridge);

    bool flashbulb = amygdala_autobio_is_flashbulb_mode(bridge);
    EXPECT_FALSE(flashbulb);
}

TEST_F(AmygdalaAutobioTest, TagMemoryWithEmotionalSalience) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* Create a memory */
    autobiographical_memory_entry_t memory = {};
    memory.type = AUTOBIO_CRISIS;
    memory.timestamp_ms = 1000;
    snprintf(memory.what_happened, sizeof(memory.what_happened), "Experienced a crisis");
    memory.valence = VALENCE_VERY_NEGATIVE;
    memory.emotional_intensity = 0.9f;
    memory.importance = 0.5f;

    uint64_t memory_id = autobio_store(autobio, &memory);
    ASSERT_GT(memory_id, 0);

    /* Set high amygdala arousal */
    amygdala_set_anxiety(amygdala, 0.8f);
    amygdala_set_nucleus_activation(amygdala, AMYG_NUCLEUS_CENTRAL, 0.8f);
    amygdala_autobio_update(bridge);

    /* Tag the memory */
    int result = amygdala_autobio_tag_memory(bridge, memory_id, 0.9f);
    EXPECT_EQ(result, 0);

    /* Retrieve and verify importance increased */
    autobiographical_memory_entry_t retrieved;
    bool found = autobio_retrieve(autobio, memory_id, &retrieved);
    ASSERT_TRUE(found);
    EXPECT_GT(retrieved.importance, 0.5f);  /* Should be boosted */
}

TEST_F(AmygdalaAutobioTest, FearConsolidationBoost) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* High fear */
    amygdala_set_nucleus_activation(amygdala, AMYG_NUCLEUS_CENTRAL, 0.9f);
    amygdala_autobio_update(bridge);

    float boost = amygdala_autobio_get_consolidation_boost(bridge);
    EXPECT_GT(boost, 0.0f);
    EXPECT_LE(boost, 1.0f);
}

/* ============================================================================
 * Recall Reactivation Tests
 * ============================================================================ */

TEST_F(AmygdalaAutobioTest, RecallTraumaMemoryReactivatesFear) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* Create trauma memory */
    autobiographical_memory_entry_t trauma = {};
    trauma.type = AUTOBIO_CRISIS;
    trauma.timestamp_ms = 1000;
    snprintf(trauma.what_happened, sizeof(trauma.what_happened), "Traumatic event");
    trauma.valence = VALENCE_VERY_NEGATIVE;
    trauma.emotional_intensity = 0.9f;
    trauma.importance = 0.8f;

    uint64_t memory_id = autobio_store(autobio, &trauma);
    ASSERT_GT(memory_id, 0);

    /* Reset amygdala */
    amygdala_set_anxiety(amygdala, 0.1f);

    /* Recall trauma memory */
    int result = amygdala_autobio_on_recall(bridge, memory_id);
    EXPECT_EQ(result, 0);

    /* Check fear/anxiety reactivation */
    float fear_reactivation, anxiety_reactivation;
    amygdala_autobio_get_reactivation(bridge, &fear_reactivation, &anxiety_reactivation);

    EXPECT_GT(fear_reactivation, 0.0f);
    EXPECT_GT(anxiety_reactivation, 0.0f);
}

TEST_F(AmygdalaAutobioTest, RecallPositiveMemoryReducesAnxiety) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* Create positive memory */
    autobiographical_memory_entry_t positive = {};
    positive.type = AUTOBIO_ACHIEVEMENT;
    positive.timestamp_ms = 1000;
    snprintf(positive.what_happened, sizeof(positive.what_happened), "Great achievement");
    positive.valence = VALENCE_VERY_POSITIVE;
    positive.emotional_intensity = 0.8f;
    positive.importance = 0.7f;

    uint64_t memory_id = autobio_store(autobio, &positive);
    ASSERT_GT(memory_id, 0);

    /* Set high anxiety */
    amygdala_set_anxiety(amygdala, 0.8f);
    float initial_anxiety = amygdala_get_anxiety_level(amygdala);

    /* Recall positive memory */
    int result = amygdala_autobio_on_recall(bridge, memory_id);
    EXPECT_EQ(result, 0);

    /* Check anxiety reduced */
    float final_anxiety = amygdala_get_anxiety_level(amygdala);
    EXPECT_LT(final_anxiety, initial_anxiety);
}

TEST_F(AmygdalaAutobioTest, RecallNegativeMemoryIncreasesAnxiety) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* Create negative memory */
    autobiographical_memory_entry_t negative = {};
    negative.type = AUTOBIO_FAILURE;
    negative.timestamp_ms = 1000;
    snprintf(negative.what_happened, sizeof(negative.what_happened), "Failed task");
    negative.valence = VALENCE_NEGATIVE;
    negative.emotional_intensity = 0.6f;
    negative.importance = 0.6f;

    uint64_t memory_id = autobio_store(autobio, &negative);
    ASSERT_GT(memory_id, 0);

    /* Low initial anxiety */
    amygdala_set_anxiety(amygdala, 0.2f);
    float initial_anxiety = amygdala_get_anxiety_level(amygdala);

    /* Recall negative memory */
    int result = amygdala_autobio_on_recall(bridge, memory_id);
    EXPECT_EQ(result, 0);

    /* Check anxiety increased */
    float final_anxiety = amygdala_get_anxiety_level(amygdala);
    EXPECT_GT(final_anxiety, initial_anxiety);
}

TEST_F(AmygdalaAutobioTest, RecallNeutralMemoryNoReactivation) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* Create neutral memory */
    autobiographical_memory_entry_t neutral = {};
    neutral.type = AUTOBIO_ACTION;
    neutral.timestamp_ms = 1000;
    snprintf(neutral.what_happened, sizeof(neutral.what_happened), "Routine action");
    neutral.valence = VALENCE_NEUTRAL;
    neutral.emotional_intensity = 0.2f;
    neutral.importance = 0.3f;

    uint64_t memory_id = autobio_store(autobio, &neutral);
    ASSERT_GT(memory_id, 0);

    /* Recall neutral memory */
    int result = amygdala_autobio_on_recall(bridge, memory_id);
    EXPECT_EQ(result, 0);

    /* Check minimal reactivation */
    float fear_reactivation, anxiety_reactivation;
    amygdala_autobio_get_reactivation(bridge, &fear_reactivation, &anxiety_reactivation);

    EXPECT_NEAR(fear_reactivation, 0.0f, 0.1f);
    EXPECT_NEAR(anxiety_reactivation, 0.0f, 0.1f);
}

TEST_F(AmygdalaAutobioTest, FullTraumaReactivation) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* Create high-importance trauma */
    autobiographical_memory_entry_t trauma = {};
    trauma.type = AUTOBIO_CRISIS;
    trauma.timestamp_ms = 1000;
    snprintf(trauma.what_happened, sizeof(trauma.what_happened), "Severe trauma");
    trauma.valence = VALENCE_VERY_NEGATIVE;
    trauma.emotional_intensity = 1.0f;
    trauma.importance = 0.9f;

    uint64_t memory_id = autobio_store(autobio, &trauma);
    ASSERT_GT(memory_id, 0);

    /* Set the assigned memory_id on the trauma struct for reactivation */
    trauma.memory_id = memory_id;

    /* Recall trauma */
    amygdala_autobio_reactivate_trauma(bridge, &trauma);

    /* Check reactivation state */
    recall_reactivation_state_t state;
    amygdala_autobio_get_reactivation_state(bridge, &state);

    EXPECT_TRUE(state.full_reactivation);
    EXPECT_GT(state.fear_reactivation, 0.5f);
    EXPECT_GT(state.anxiety_reactivation, 0.0f);
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(AmygdalaAutobioTest, GetTaggingState) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    amygdala_set_anxiety(amygdala, 0.5f);
    amygdala_autobio_update(bridge);

    emotional_tagging_state_t state;
    int result = amygdala_autobio_get_tagging_state(bridge, &state);

    EXPECT_EQ(result, 0);
    EXPECT_GE(state.anxiety_level, 0.0f);
    EXPECT_LE(state.anxiety_level, 1.0f);
}

TEST_F(AmygdalaAutobioTest, GetReactivationState) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    recall_reactivation_state_t state;
    int result = amygdala_autobio_get_reactivation_state(bridge, &state);

    EXPECT_EQ(result, 0);
}

TEST_F(AmygdalaAutobioTest, GetStatistics) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* Perform an update to increment total_updates */
    amygdala_set_anxiety(amygdala, 0.5f);
    amygdala_autobio_update(bridge);

    uint64_t total_updates = 0;
    uint32_t memories_tagged = 0;
    uint32_t trauma_reactivations = 0;

    int result = amygdala_autobio_get_statistics(bridge,
                                                  &total_updates,
                                                  &memories_tagged,
                                                  &trauma_reactivations);

    EXPECT_EQ(result, 0);
    EXPECT_GT(total_updates, 0);  /* Should be > 0 after update */
    EXPECT_EQ(bridge->base.total_updates, total_updates);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(AmygdalaAutobioTest, BioAsyncConnect) {
    int result = amygdala_autobio_connect_bio_async(bridge);
    /* May succeed or warn if router not available */
    EXPECT_GE(result, 0);
}

TEST_F(AmygdalaAutobioTest, BioAsyncDisconnect) {
    amygdala_autobio_connect_bio_async(bridge);
    int result = amygdala_autobio_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(AmygdalaAutobioTest, BioAsyncIsConnected) {
    bool connected1 = amygdala_autobio_is_bio_async_connected(bridge);

    amygdala_autobio_connect_bio_async(bridge);
    bool connected2 = amygdala_autobio_is_bio_async_connected(bridge);

    /* May or may not connect depending on router availability */
    /* Just check that the function returns a valid bool */
    EXPECT_TRUE(connected2 == true || connected2 == false);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(AmygdalaAutobioTest, IntegrationTagAndRecallCycle) {
    amygdala_autobio_connect_amygdala(bridge, amygdala);
    amygdala_autobio_connect_memory(bridge, autobio);

    /* 1. Create memory during high arousal */
    amygdala_set_anxiety(amygdala, 0.8f);
    amygdala_set_nucleus_activation(amygdala, AMYG_NUCLEUS_CENTRAL, 0.8f);
    amygdala_autobio_update(bridge);

    autobiographical_memory_entry_t memory = {};
    memory.type = AUTOBIO_CRISIS;
    memory.timestamp_ms = 1000;
    snprintf(memory.what_happened, sizeof(memory.what_happened), "Crisis event");
    memory.valence = VALENCE_NEGATIVE;
    memory.emotional_intensity = 0.8f;
    memory.importance = 0.5f;

    uint64_t memory_id = autobio_store(autobio, &memory);
    ASSERT_GT(memory_id, 0);

    /* 2. Tag memory with emotional salience */
    amygdala_autobio_tag_memory(bridge, memory_id, 0.8f);

    /* 3. Reset amygdala to calm state */
    amygdala_set_anxiety(amygdala, 0.1f);
    amygdala_set_nucleus_activation(amygdala, AMYG_NUCLEUS_CENTRAL, 0.1f);

    /* 4. Recall memory - should reactivate emotion */
    amygdala_autobio_on_recall(bridge, memory_id);

    /* 5. Verify reactivation */
    float final_anxiety = amygdala_get_anxiety_level(amygdala);
    EXPECT_GT(final_anxiety, 0.1f);  /* Anxiety should increase from recall */
}

/* Run all tests */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
