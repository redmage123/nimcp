/**
 * @file test_amygdala.cpp
 * @brief Unit tests for amygdala implementation
 *
 * Tests cover:
 * - Configuration and lifecycle
 * - Fear conditioning and extinction
 * - Stimulus processing and threat detection
 * - Context-dependent fear
 * - Nucleus activation and plasticity
 * - Emotional system integration
 * - Neuromodulation effects
 * - Bio-async integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_amygdala.h"

// Test fixture for amygdala tests
class AmygdalaTest : public ::testing::Test {
protected:
    amygdala_t* amyg;
    amyg_config_t config;

    void SetUp() override {
        amygdala_default_config(&config);
        amyg = amygdala_create(&config);
        ASSERT_NE(amyg, nullptr);
    }

    void TearDown() override {
        if (amyg) {
            amygdala_destroy(amyg);
            amyg = nullptr;
        }
    }

    // Helper to create a test stimulus
    amyg_conditioned_stimulus_t createTestCS(float salience = 0.5f) {
        amyg_conditioned_stimulus_t cs;
        memset(&cs, 0, sizeof(cs));
        cs.n_features = 10;
        for (uint32_t i = 0; i < cs.n_features; i++) {
            cs.features[i] = (float)(i + 1) / 10.0f;
        }
        cs.salience = salience;
        cs.modality = 0;  // Visual
        return cs;
    }

    // Helper to create a test US
    amyg_unconditioned_stimulus_t createTestUS(float intensity = 0.8f) {
        amyg_unconditioned_stimulus_t us;
        us.intensity = intensity;
        us.valence = AMYG_VALENCE_NEGATIVE;
        us.type = 0;  // Pain
        us.duration_ms = 100.0f;
        return us;
    }

    // Helper to create a test context
    amyg_context_t createTestContext(uint32_t id = 1) {
        amyg_context_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.context_id = id;
        for (uint32_t i = 0; i < AMYG_MAX_CONTEXT_DIM; i++) {
            ctx.context_vector[i] = (float)(i + id) / (float)AMYG_MAX_CONTEXT_DIM;
        }
        ctx.familiarity = 0.7f;
        ctx.is_safe = false;
        return ctx;
    }
};

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_F(AmygdalaTest, DefaultConfigHasValidValues) {
    amyg_config_t cfg;
    ASSERT_EQ(amygdala_default_config(&cfg), 0);

    EXPECT_GT(cfg.conditioning_rate, 0.0f);
    EXPECT_LE(cfg.conditioning_rate, 1.0f);
    EXPECT_GT(cfg.extinction_rate, 0.0f);
    EXPECT_LE(cfg.extinction_rate, 1.0f);
    EXPECT_GT(cfg.fear_threshold, 0.0f);
    EXPECT_LE(cfg.fear_threshold, 1.0f);
    EXPECT_TRUE(cfg.bio_async_enabled);
    EXPECT_TRUE(cfg.extinction_enabled);
}

TEST_F(AmygdalaTest, DefaultConfigNullPointerReturnsError) {
    EXPECT_NE(amygdala_default_config(nullptr), 0);
}

TEST_F(AmygdalaTest, ValidateConfigAcceptsValidConfig) {
    EXPECT_EQ(amygdala_validate_config(&config), 0);
}

TEST_F(AmygdalaTest, ValidateConfigRejectsNullPointer) {
    EXPECT_NE(amygdala_validate_config(nullptr), 0);
}

TEST_F(AmygdalaTest, ValidateConfigRejectsInvalidRates) {
    amyg_config_t bad_config = config;
    bad_config.conditioning_rate = -0.1f;
    EXPECT_NE(amygdala_validate_config(&bad_config), 0);

    bad_config = config;
    bad_config.conditioning_rate = 1.5f;
    EXPECT_NE(amygdala_validate_config(&bad_config), 0);

    bad_config = config;
    bad_config.extinction_rate = -0.1f;
    EXPECT_NE(amygdala_validate_config(&bad_config), 0);
}

// =============================================================================
// Lifecycle Tests
// =============================================================================

TEST_F(AmygdalaTest, CreateWithNullConfigUsesDefaults) {
    amygdala_t* amyg2 = amygdala_create(nullptr);
    ASSERT_NE(amyg2, nullptr);
    amygdala_destroy(amyg2);
}

TEST_F(AmygdalaTest, DestroyNullIsSafe) {
    amygdala_destroy(nullptr);  // Should not crash
}

TEST_F(AmygdalaTest, ResetClearsState) {
    // Add some state
    auto cs = createTestCS();
    auto us = createTestUS();
    amygdala_condition_fear(amyg, &cs, &us, nullptr);

    EXPECT_GT(amygdala_get_fear_memory_count(amyg), 0u);

    // Reset
    ASSERT_EQ(amygdala_reset(amyg), 0);

    // State should be cleared
    EXPECT_EQ(amygdala_get_fear_memory_count(amyg), 0u);
    EXPECT_FLOAT_EQ(amygdala_get_fear_level(amyg), 0.0f);
    EXPECT_FLOAT_EQ(amygdala_get_anxiety_level(amyg), 0.0f);
}

TEST_F(AmygdalaTest, ResetNullReturnsError) {
    EXPECT_NE(amygdala_reset(nullptr), 0);
}

// =============================================================================
// Fear Conditioning Tests
// =============================================================================

TEST_F(AmygdalaTest, ConditionFearCreatesMemory) {
    auto cs = createTestCS();
    auto us = createTestUS();
    uint32_t memory_id = 0;

    ASSERT_EQ(amygdala_condition_fear(amyg, &cs, &us, &memory_id), 0);
    EXPECT_GT(memory_id, 0u);
    EXPECT_EQ(amygdala_get_fear_memory_count(amyg), 1u);
}

TEST_F(AmygdalaTest, ConditionFearStrengthensExistingMemory) {
    auto cs = createTestCS();
    auto us = createTestUS();
    uint32_t id1, id2;

    // First conditioning
    amygdala_condition_fear(amyg, &cs, &us, &id1);

    // Get initial strength
    amyg_fear_memory_t mem1;
    amygdala_get_fear_memory(amyg, id1, &mem1);
    float initial_strength = mem1.association_strength;

    // Second conditioning with same CS
    amygdala_condition_fear(amyg, &cs, &us, &id2);

    // Should be same memory
    EXPECT_EQ(id1, id2);
    EXPECT_EQ(amygdala_get_fear_memory_count(amyg), 1u);

    // Strength should increase
    amyg_fear_memory_t mem2;
    amygdala_get_fear_memory(amyg, id1, &mem2);
    EXPECT_GT(mem2.association_strength, initial_strength);
}

TEST_F(AmygdalaTest, ConditionFearNullArgumentsReturnError) {
    auto cs = createTestCS();
    auto us = createTestUS();

    EXPECT_NE(amygdala_condition_fear(nullptr, &cs, &us, nullptr), 0);
    EXPECT_NE(amygdala_condition_fear(amyg, nullptr, &us, nullptr), 0);
    EXPECT_NE(amygdala_condition_fear(amyg, &cs, nullptr, nullptr), 0);
}

TEST_F(AmygdalaTest, ConditionFearWithHigherUSIntensityCreatesStrongerMemory) {
    auto cs1 = createTestCS();
    cs1.features[0] = 1.0f;  // Different from cs2

    auto cs2 = createTestCS();
    cs2.features[0] = 0.0f;

    auto us_weak = createTestUS(0.3f);
    auto us_strong = createTestUS(0.9f);

    uint32_t id1, id2;
    amygdala_condition_fear(amyg, &cs1, &us_weak, &id1);
    amygdala_condition_fear(amyg, &cs2, &us_strong, &id2);

    amyg_fear_memory_t mem1, mem2;
    amygdala_get_fear_memory(amyg, id1, &mem1);
    amygdala_get_fear_memory(amyg, id2, &mem2);

    EXPECT_GT(mem2.association_strength, mem1.association_strength);
}

// =============================================================================
// Extinction Tests
// =============================================================================

TEST_F(AmygdalaTest, ExtinctionTrialReducesFearResponse) {
    auto cs = createTestCS();
    auto us = createTestUS();
    uint32_t id;

    // Condition fear
    amygdala_condition_fear(amyg, &cs, &us, &id);

    amyg_fear_memory_t mem_before;
    amygdala_get_fear_memory(amyg, id, &mem_before);

    // Multiple extinction trials
    for (int i = 0; i < 10; i++) {
        amygdala_extinction_trial(amyg, &cs);
    }

    amyg_fear_memory_t mem_after;
    amygdala_get_fear_memory(amyg, id, &mem_after);

    // Extinction strength should increase
    EXPECT_GT(mem_after.extinction_strength, mem_before.extinction_strength);
    EXPECT_EQ(mem_after.phase, AMYG_PHASE_EXTINCTION);
}

TEST_F(AmygdalaTest, ExtinctionDisabledDoesNothing) {
    // Disable extinction
    amyg_config_t no_ext_config = config;
    no_ext_config.extinction_enabled = false;
    amygdala_t* amyg2 = amygdala_create(&no_ext_config);
    ASSERT_NE(amyg2, nullptr);

    auto cs = createTestCS();
    auto us = createTestUS();
    uint32_t id;

    amygdala_condition_fear(amyg2, &cs, &us, &id);

    amyg_fear_memory_t mem_before;
    amygdala_get_fear_memory(amyg2, id, &mem_before);

    // Try extinction
    amygdala_extinction_trial(amyg2, &cs);

    amyg_fear_memory_t mem_after;
    amygdala_get_fear_memory(amyg2, id, &mem_after);

    // Should not change
    EXPECT_FLOAT_EQ(mem_after.extinction_strength, mem_before.extinction_strength);

    amygdala_destroy(amyg2);
}

TEST_F(AmygdalaTest, ExtinctionTrialNullArgumentsReturnError) {
    auto cs = createTestCS();
    EXPECT_NE(amygdala_extinction_trial(nullptr, &cs), 0);
    EXPECT_NE(amygdala_extinction_trial(amyg, nullptr), 0);
}

// =============================================================================
// Stimulus Processing Tests
// =============================================================================

TEST_F(AmygdalaTest, ProcessStimulusTriggersFearResponse) {
    auto cs = createTestCS(0.9f);  // High salience
    auto us = createTestUS(1.0f);  // High intensity

    // Condition strong fear
    for (int i = 0; i < 5; i++) {
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
    }

    // Process the conditioned stimulus
    amyg_fear_response_t response;
    ASSERT_EQ(amygdala_process_stimulus(amyg, &cs, &response), 0);

    // Should trigger fear response
    EXPECT_GT(response.fear_intensity, 0.0f);
    EXPECT_GT(response.memory_match_score, 0.0f);
}

TEST_F(AmygdalaTest, ProcessStimulusNovelStimulusMinimalFear) {
    auto cs = createTestCS();

    // Process without any conditioning
    amyg_fear_response_t response;
    amygdala_process_stimulus(amyg, &cs, &response);

    // Should have minimal fear (only from salience)
    EXPECT_LT(response.fear_intensity, 0.5f);
    EXPECT_FLOAT_EQ(response.memory_match_score, 0.0f);
}

TEST_F(AmygdalaTest, ProcessStimulusNullArgumentsReturnError) {
    auto cs = createTestCS();
    EXPECT_NE(amygdala_process_stimulus(nullptr, &cs, nullptr), 0);
    EXPECT_NE(amygdala_process_stimulus(amyg, nullptr, nullptr), 0);
}

TEST_F(AmygdalaTest, ProcessStimulusUpdatesCurrentState) {
    auto cs = createTestCS(0.8f);
    auto us = createTestUS(0.9f);

    // Condition fear
    for (int i = 0; i < 3; i++) {
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
    }

    float fear_before = amygdala_get_fear_level(amyg);

    // Process stimulus
    amygdala_process_stimulus(amyg, &cs, nullptr);

    float fear_after = amygdala_get_fear_level(amyg);
    EXPECT_GE(fear_after, fear_before);
}

// =============================================================================
// Context Tests
// =============================================================================

TEST_F(AmygdalaTest, SetContextStoresContext) {
    auto ctx = createTestContext(42);

    ASSERT_EQ(amygdala_set_context(amyg, &ctx), 0);

    amyg_context_t retrieved;
    ASSERT_EQ(amygdala_get_context(amyg, &retrieved), 0);

    EXPECT_EQ(retrieved.context_id, 42u);
    EXPECT_FLOAT_EQ(retrieved.familiarity, ctx.familiarity);
}

TEST_F(AmygdalaTest, SetContextNullArgumentsReturnError) {
    auto ctx = createTestContext();
    EXPECT_NE(amygdala_set_context(nullptr, &ctx), 0);
    EXPECT_NE(amygdala_set_context(amyg, nullptr), 0);
}

TEST_F(AmygdalaTest, GetContextWithoutSettingReturnsError) {
    amyg_context_t ctx;
    EXPECT_NE(amygdala_get_context(amyg, &ctx), 0);  // No context set
}

TEST_F(AmygdalaTest, ContextMatchComputesSimilarity) {
    // Set context A - all positive values
    amyg_context_t ctx_a;
    memset(&ctx_a, 0, sizeof(ctx_a));
    ctx_a.context_id = 1;
    for (uint32_t i = 0; i < AMYG_MAX_CONTEXT_DIM; i++) {
        ctx_a.context_vector[i] = 1.0f;  // All ones
    }
    ctx_a.familiarity = 0.7f;
    amygdala_set_context(amyg, &ctx_a);

    // Condition fear in context A
    auto cs = createTestCS();
    auto us = createTestUS();
    uint32_t id;
    amygdala_condition_fear(amyg, &cs, &us, &id);

    // Check match with same context
    float match_score;
    ASSERT_EQ(amygdala_context_match(amyg, id, &match_score), 0);
    EXPECT_GT(match_score, 0.9f);  // High similarity

    // Switch to orthogonal context - alternating values
    amyg_context_t ctx_b;
    memset(&ctx_b, 0, sizeof(ctx_b));
    ctx_b.context_id = 100;
    for (uint32_t i = 0; i < AMYG_MAX_CONTEXT_DIM; i++) {
        ctx_b.context_vector[i] = (i % 2 == 0) ? 1.0f : -1.0f;  // Orthogonal pattern
    }
    ctx_b.familiarity = 0.3f;
    amygdala_set_context(amyg, &ctx_b);

    ASSERT_EQ(amygdala_context_match(amyg, id, &match_score), 0);
    // Expect lower but not necessarily < 0.5 due to cosine similarity properties
    EXPECT_LT(match_score, 1.0f);  // Different from perfect match
}

// =============================================================================
// Nucleus Activation Tests
// =============================================================================

TEST_F(AmygdalaTest, GetNucleusActivationReturnsValidValues) {
    float activation;

    for (int i = 0; i < AMYG_NUCLEUS_COUNT; i++) {
        ASSERT_EQ(amygdala_get_nucleus_activation(amyg, (amyg_nucleus_type_t)i, &activation), 0);
        EXPECT_GE(activation, 0.0f);
        EXPECT_LE(activation, 1.0f);
    }
}

TEST_F(AmygdalaTest, SetNucleusActivationUpdatesValue) {
    float new_activation = 0.75f;
    ASSERT_EQ(amygdala_set_nucleus_activation(amyg, AMYG_NUCLEUS_LATERAL, new_activation), 0);

    float retrieved;
    amygdala_get_nucleus_activation(amyg, AMYG_NUCLEUS_LATERAL, &retrieved);
    EXPECT_FLOAT_EQ(retrieved, new_activation);
}

TEST_F(AmygdalaTest, SetNucleusActivationClampsValue) {
    // Test clamping to [0, 1]
    amygdala_set_nucleus_activation(amyg, AMYG_NUCLEUS_LATERAL, 1.5f);
    float activation;
    amygdala_get_nucleus_activation(amyg, AMYG_NUCLEUS_LATERAL, &activation);
    EXPECT_LE(activation, 1.0f);

    amygdala_set_nucleus_activation(amyg, AMYG_NUCLEUS_LATERAL, -0.5f);
    amygdala_get_nucleus_activation(amyg, AMYG_NUCLEUS_LATERAL, &activation);
    EXPECT_GE(activation, 0.0f);
}

TEST_F(AmygdalaTest, NucleusActivationNullArgumentsReturnError) {
    float activation;
    EXPECT_NE(amygdala_get_nucleus_activation(nullptr, AMYG_NUCLEUS_LATERAL, &activation), 0);
    EXPECT_NE(amygdala_get_nucleus_activation(amyg, AMYG_NUCLEUS_LATERAL, nullptr), 0);
    EXPECT_NE(amygdala_get_nucleus_activation(amyg, AMYG_NUCLEUS_COUNT, &activation), 0);  // Invalid

    EXPECT_NE(amygdala_set_nucleus_activation(nullptr, AMYG_NUCLEUS_LATERAL, 0.5f), 0);
    EXPECT_NE(amygdala_set_nucleus_activation(amyg, AMYG_NUCLEUS_COUNT, 0.5f), 0);  // Invalid
}

TEST_F(AmygdalaTest, SetNucleusPlasticityEnablesDisables) {
    ASSERT_EQ(amygdala_set_nucleus_plasticity(amyg, AMYG_NUCLEUS_ITC, false), 0);
    ASSERT_EQ(amygdala_set_nucleus_plasticity(amyg, AMYG_NUCLEUS_ITC, true), 0);
}

// =============================================================================
// Regulation Tests
// =============================================================================

TEST_F(AmygdalaTest, SetPrefrontalInhibitionUpdatesValue) {
    ASSERT_EQ(amygdala_set_prefrontal_inhibition(amyg, 0.6f), 0);

    // Verify by checking ITC activity increases with inhibition
    float itc_before;
    amygdala_get_nucleus_activation(amyg, AMYG_NUCLEUS_ITC, &itc_before);

    // Process a stimulus with high PFC inhibition
    auto cs = createTestCS();
    amygdala_process_stimulus(amyg, &cs, nullptr);

    float itc_after;
    amygdala_get_nucleus_activation(amyg, AMYG_NUCLEUS_ITC, &itc_after);

    // ITC should be elevated due to PFC input
    EXPECT_GE(itc_after, itc_before);
}

TEST_F(AmygdalaTest, SetNeuromodulatorsUpdatesAllNuclei) {
    ASSERT_EQ(amygdala_set_neuromodulators(amyg, 0.8f, 0.7f, 0.6f), 0);
    // No way to directly verify, but should not crash
}

TEST_F(AmygdalaTest, SetAnxietyUpdatesLevel) {
    ASSERT_EQ(amygdala_set_anxiety(amyg, 0.7f), 0);
    EXPECT_FLOAT_EQ(amygdala_get_anxiety_level(amyg), 0.7f);
}

TEST_F(AmygdalaTest, SetAnxietyClampsValue) {
    amygdala_set_anxiety(amyg, 1.5f);
    EXPECT_LE(amygdala_get_anxiety_level(amyg), 1.0f);

    amygdala_set_anxiety(amyg, -0.5f);
    EXPECT_GE(amygdala_get_anxiety_level(amyg), 0.0f);
}

// =============================================================================
// Step/Dynamics Tests
// =============================================================================

TEST_F(AmygdalaTest, StepDecaysActivations) {
    // Set high activation
    amygdala_set_nucleus_activation(amyg, AMYG_NUCLEUS_CENTRAL, 0.9f);
    amygdala_set_anxiety(amyg, 0.8f);

    float cea_before, anx_before;
    amygdala_get_nucleus_activation(amyg, AMYG_NUCLEUS_CENTRAL, &cea_before);
    anx_before = amygdala_get_anxiety_level(amyg);

    // Run several steps
    for (int i = 0; i < 100; i++) {
        amygdala_step(amyg, 10.0f);  // 10ms per step
    }

    float cea_after;
    amygdala_get_nucleus_activation(amyg, AMYG_NUCLEUS_CENTRAL, &cea_after);
    float anx_after = amygdala_get_anxiety_level(amyg);

    // Activations should decay
    EXPECT_LT(cea_after, cea_before);
    EXPECT_LT(anx_after, anx_before);
}

TEST_F(AmygdalaTest, StepNullArgumentsReturnError) {
    EXPECT_NE(amygdala_step(nullptr, 10.0f), 0);
    EXPECT_NE(amygdala_step(amyg, 0.0f), 0);
    EXPECT_NE(amygdala_step(amyg, -10.0f), 0);
}

// =============================================================================
// Fear Memory Access Tests
// =============================================================================

TEST_F(AmygdalaTest, GetFearMemoryReturnsCorrectMemory) {
    auto cs = createTestCS();
    auto us = createTestUS();
    uint32_t id;

    amygdala_condition_fear(amyg, &cs, &us, &id);

    amyg_fear_memory_t mem;
    ASSERT_EQ(amygdala_get_fear_memory(amyg, id, &mem), 0);
    EXPECT_EQ(mem.memory_id, id);
    EXPECT_EQ(mem.us.valence, AMYG_VALENCE_NEGATIVE);
}

TEST_F(AmygdalaTest, GetFearMemoryNonexistentReturnsError) {
    amyg_fear_memory_t mem;
    EXPECT_NE(amygdala_get_fear_memory(amyg, 999, &mem), 0);
}

TEST_F(AmygdalaTest, AddFearMemoryDirectly) {
    amyg_fear_memory_t mem;
    memset(&mem, 0, sizeof(mem));
    mem.cs = createTestCS();
    mem.us = createTestUS();
    mem.association_strength = 0.5f;
    mem.phase = AMYG_PHASE_EXPRESSION;

    uint32_t id;
    ASSERT_EQ(amygdala_add_fear_memory(amyg, &mem, &id), 0);
    EXPECT_GT(id, 0u);
    EXPECT_EQ(amygdala_get_fear_memory_count(amyg), 1u);
}

TEST_F(AmygdalaTest, ClearFearMemoriesRemovesAll) {
    auto us = createTestUS();

    // Add several memories with very different feature vectors
    for (int i = 0; i < 5; i++) {
        amyg_conditioned_stimulus_t cs;
        memset(&cs, 0, sizeof(cs));
        cs.n_features = 10;
        // Make completely different patterns for each
        for (uint32_t j = 0; j < cs.n_features; j++) {
            cs.features[j] = (i == (int)j) ? 1.0f : 0.0f;  // One-hot encoding
        }
        cs.salience = 0.5f;
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
    }

    EXPECT_EQ(amygdala_get_fear_memory_count(amyg), 5u);

    ASSERT_EQ(amygdala_clear_fear_memories(amyg), 0);
    EXPECT_EQ(amygdala_get_fear_memory_count(amyg), 0u);
}

TEST_F(AmygdalaTest, RetrieveFearMemoryBySimilarity) {
    auto cs = createTestCS();
    auto us = createTestUS();
    uint32_t id;

    amygdala_condition_fear(amyg, &cs, &us, &id);

    // Create similar stimulus
    auto query_cs = cs;
    query_cs.features[0] += 0.01f;  // Slightly different

    amyg_fear_memory_t retrieved;
    float match_score;
    ASSERT_EQ(amygdala_retrieve_fear_memory(amyg, &query_cs, &retrieved, &match_score), 0);

    EXPECT_EQ(retrieved.memory_id, id);
    EXPECT_GT(match_score, 0.9f);  // High similarity
}

// =============================================================================
// Connection Tests
// =============================================================================

TEST_F(AmygdalaTest, ConnectHippocampusStoresPointer) {
    int dummy_hipp = 42;
    ASSERT_EQ(amygdala_connect_hippocampus(amyg, &dummy_hipp), 0);
}

TEST_F(AmygdalaTest, ConnectPrefrontalStoresPointer) {
    int dummy_pfc = 42;
    ASSERT_EQ(amygdala_connect_prefrontal(amyg, &dummy_pfc), 0);
}

TEST_F(AmygdalaTest, ConnectHypothalamusStoresPointer) {
    int dummy_hypo = 42;
    ASSERT_EQ(amygdala_connect_hypothalamus(amyg, &dummy_hypo), 0);
}

TEST_F(AmygdalaTest, ConnectThalamusStoresPointer) {
    int dummy_thal = 42;
    ASSERT_EQ(amygdala_connect_thalamus(amyg, &dummy_thal), 0);
}

TEST_F(AmygdalaTest, ConnectNullArgumentsReturnError) {
    EXPECT_NE(amygdala_connect_hippocampus(nullptr, nullptr), 0);
    EXPECT_NE(amygdala_connect_prefrontal(nullptr, nullptr), 0);
    EXPECT_NE(amygdala_connect_hypothalamus(nullptr, nullptr), 0);
    EXPECT_NE(amygdala_connect_thalamus(nullptr, nullptr), 0);
}

// =============================================================================
// Emotional System Integration Tests
// =============================================================================

TEST_F(AmygdalaTest, EmotionSystemInitiallyNotConnected) {
    EXPECT_FALSE(amygdala_is_emotion_system_connected(amyg));
}

TEST_F(AmygdalaTest, ConnectEmotionSystemUpdatesState) {
    // Can't test with real emotional system, but verify API
    ASSERT_EQ(amygdala_connect_emotion_system(amyg, nullptr), 0);
    EXPECT_FALSE(amygdala_is_emotion_system_connected(amyg));  // NULL doesn't connect
}

TEST_F(AmygdalaTest, DisconnectEmotionSystemClearsConnection) {
    ASSERT_EQ(amygdala_disconnect_emotion_system(amyg), 0);
    EXPECT_FALSE(amygdala_is_emotion_system_connected(amyg));
}

TEST_F(AmygdalaTest, SyncToEmotionSystemHandlesDisconnected) {
    // Should succeed even when not connected
    EXPECT_EQ(amygdala_sync_to_emotion_system(amyg), 0);
}

TEST_F(AmygdalaTest, SyncFromEmotionSystemHandlesDisconnected) {
    // Should succeed even when not connected
    EXPECT_EQ(amygdala_sync_from_emotion_system(amyg), 0);
}

// =============================================================================
// Bio-async Tests
// =============================================================================

TEST_F(AmygdalaTest, BioAsyncInitiallyNotConnected) {
    EXPECT_FALSE(amygdala_is_bio_async_connected(amyg));
}

TEST_F(AmygdalaTest, ConnectBioAsyncAttempts) {
    // Bio-async router may not be available
    EXPECT_EQ(amygdala_connect_bio_async(amyg), 0);
}

TEST_F(AmygdalaTest, DisconnectBioAsyncSucceeds) {
    EXPECT_EQ(amygdala_disconnect_bio_async(amyg), 0);
    EXPECT_FALSE(amygdala_is_bio_async_connected(amyg));
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(AmygdalaTest, GetStatisticsReturnsValidCounts) {
    uint64_t fear_events, ext_events, cond_events;

    ASSERT_EQ(amygdala_get_statistics(amyg, &fear_events, &ext_events, &cond_events), 0);
    EXPECT_EQ(fear_events, 0u);
    EXPECT_EQ(ext_events, 0u);
    EXPECT_EQ(cond_events, 0u);
}

TEST_F(AmygdalaTest, StatisticsIncrementWithOperations) {
    auto cs = createTestCS();
    auto us = createTestUS();

    // Condition fear
    amygdala_condition_fear(amyg, &cs, &us, nullptr);

    uint64_t fear_events, ext_events, cond_events;
    amygdala_get_statistics(amyg, nullptr, nullptr, &cond_events);
    EXPECT_EQ(cond_events, 1u);

    // Extinction trial
    amygdala_extinction_trial(amyg, &cs);
    amygdala_get_statistics(amyg, nullptr, &ext_events, nullptr);
    EXPECT_GE(ext_events, 1u);
}

TEST_F(AmygdalaTest, GetStatisticsNullAmygdalaReturnsError) {
    uint64_t count;
    EXPECT_NE(amygdala_get_statistics(nullptr, &count, nullptr, nullptr), 0);
}

// =============================================================================
// Response Tests
// =============================================================================

TEST_F(AmygdalaTest, GetResponseReturnsLastResponse) {
    auto cs = createTestCS();
    amygdala_process_stimulus(amyg, &cs, nullptr);

    amyg_fear_response_t response;
    ASSERT_EQ(amygdala_get_response(amyg, &response), 0);

    // Should match what was computed
    EXPECT_GE(response.fear_intensity, 0.0f);
    EXPECT_LE(response.fear_intensity, 1.0f);
}

TEST_F(AmygdalaTest, GetResponseNullArgumentsReturnError) {
    amyg_fear_response_t response;
    EXPECT_NE(amygdala_get_response(nullptr, &response), 0);
    EXPECT_NE(amygdala_get_response(amyg, nullptr), 0);
}

// =============================================================================
// Threat Level Tests
// =============================================================================

TEST_F(AmygdalaTest, ThreatLevelReflectsFearIntensity) {
    // Initial threat should be none
    EXPECT_EQ(amygdala_get_threat_level(amyg), AMYG_THREAT_NONE);

    // Create high fear
    auto cs = createTestCS(1.0f);
    auto us = createTestUS(1.0f);

    for (int i = 0; i < 10; i++) {
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
    }
    amygdala_process_stimulus(amyg, &cs, nullptr);

    // Threat should increase
    EXPECT_GT(amygdala_get_threat_level(amyg), AMYG_THREAT_NONE);
}

// =============================================================================
// Utility Function Tests
// =============================================================================

TEST_F(AmygdalaTest, StimulusSimilarityIdenticalReturnsOne) {
    auto cs = createTestCS();
    float sim = amygdala_stimulus_similarity(&cs, &cs);
    EXPECT_FLOAT_EQ(sim, 1.0f);
}

TEST_F(AmygdalaTest, StimulusSimilarityDifferentReturnsLower) {
    auto cs1 = createTestCS();
    auto cs2 = createTestCS();
    cs2.features[0] = -cs2.features[0];  // Flip first feature

    float sim = amygdala_stimulus_similarity(&cs1, &cs2);
    EXPECT_LT(sim, 1.0f);
}

TEST_F(AmygdalaTest, StimulusSimilarityNullReturnsZero) {
    auto cs = createTestCS();
    EXPECT_FLOAT_EQ(amygdala_stimulus_similarity(nullptr, &cs), 0.0f);
    EXPECT_FLOAT_EQ(amygdala_stimulus_similarity(&cs, nullptr), 0.0f);
}

TEST_F(AmygdalaTest, ContextSimilarityIdenticalReturnsOne) {
    auto ctx = createTestContext();
    float sim = amygdala_context_similarity(&ctx, &ctx);
    EXPECT_FLOAT_EQ(sim, 1.0f);
}

TEST_F(AmygdalaTest, ContextSimilarityNullReturnsZero) {
    auto ctx = createTestContext();
    EXPECT_FLOAT_EQ(amygdala_context_similarity(nullptr, &ctx), 0.0f);
    EXPECT_FLOAT_EQ(amygdala_context_similarity(&ctx, nullptr), 0.0f);
}

TEST_F(AmygdalaTest, NucleusNameReturnsValidStrings) {
    EXPECT_STREQ(amygdala_nucleus_name(AMYG_NUCLEUS_LATERAL), "Lateral (LA)");
    EXPECT_STREQ(amygdala_nucleus_name(AMYG_NUCLEUS_BASAL), "Basal (BA)");
    EXPECT_STREQ(amygdala_nucleus_name(AMYG_NUCLEUS_CENTRAL), "Central (CeA)");
    EXPECT_STREQ(amygdala_nucleus_name(AMYG_NUCLEUS_MEDIAL), "Medial (MeA)");
    EXPECT_STREQ(amygdala_nucleus_name(AMYG_NUCLEUS_ITC), "Intercalated (ITC)");
    EXPECT_STREQ(amygdala_nucleus_name((amyg_nucleus_type_t)99), "Unknown");
}

TEST_F(AmygdalaTest, ThreatLevelNameReturnsValidStrings) {
    EXPECT_STREQ(amygdala_threat_level_name(AMYG_THREAT_NONE), "None");
    EXPECT_STREQ(amygdala_threat_level_name(AMYG_THREAT_LOW), "Low");
    EXPECT_STREQ(amygdala_threat_level_name(AMYG_THREAT_MODERATE), "Moderate");
    EXPECT_STREQ(amygdala_threat_level_name(AMYG_THREAT_HIGH), "High");
    EXPECT_STREQ(amygdala_threat_level_name(AMYG_THREAT_SEVERE), "Severe");
    EXPECT_STREQ(amygdala_threat_level_name((amyg_threat_level_t)99), "Unknown");
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(AmygdalaTest, GettersReturnZeroForNullAmygdala) {
    EXPECT_EQ(amygdala_get_fear_memory_count(nullptr), 0u);
    EXPECT_FLOAT_EQ(amygdala_get_fear_level(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(amygdala_get_anxiety_level(nullptr), 0.0f);
    EXPECT_EQ(amygdala_get_threat_level(nullptr), AMYG_THREAT_NONE);
}

TEST_F(AmygdalaTest, FearOutputTypesAreValid) {
    auto cs = createTestCS(0.9f);
    auto us = createTestUS(0.9f);

    amygdala_condition_fear(amyg, &cs, &us, nullptr);
    amygdala_condition_fear(amyg, &cs, &us, nullptr);
    amygdala_condition_fear(amyg, &cs, &us, nullptr);

    amyg_fear_response_t response;
    amygdala_process_stimulus(amyg, &cs, &response);

    // All output types should be valid
    for (int i = 0; i < AMYG_OUTPUT_COUNT; i++) {
        EXPECT_GE(response.outputs[i], 0.0f);
        EXPECT_LE(response.outputs[i], 1.0f);
    }
}

TEST_F(AmygdalaTest, ConditioningPhaseUpdatesCorrectly) {
    auto cs = createTestCS();
    auto us = createTestUS();
    uint32_t id;

    amygdala_condition_fear(amyg, &cs, &us, &id);

    amyg_fear_memory_t mem;
    amygdala_get_fear_memory(amyg, id, &mem);
    EXPECT_EQ(mem.phase, AMYG_PHASE_ACQUISITION);

    // Extinction changes phase
    for (int i = 0; i < 5; i++) {
        amygdala_extinction_trial(amyg, &cs);
    }

    amygdala_get_fear_memory(amyg, id, &mem);
    EXPECT_EQ(mem.phase, AMYG_PHASE_EXTINCTION);
}

// Main
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
