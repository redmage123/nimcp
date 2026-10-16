//=============================================================================
// test_amygdala_regression.cpp - Amygdala Regression Tests
//=============================================================================
/**
 * @file test_amygdala_regression.cpp
 * @brief Regression tests for amygdala system
 *
 * WHAT: Tests for determinism, memory safety, numerical stability
 * WHY:  Ensure amygdala behavior is stable across versions
 * HOW:  GTest framework with determinism, bounds, and consistency checks
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class AmygdalaRegressionTest : public ::testing::Test {
protected:
    amygdala_t* amyg = nullptr;

    void SetUp() override {
        amyg_config_t config;
        amygdala_default_config(&config);
        amyg = amygdala_create(&config);
    }

    void TearDown() override {
        if (amyg) {
            amygdala_destroy(amyg);
            amyg = nullptr;
        }
    }

    // Helper: Create deterministic CS pattern
    amyg_conditioned_stimulus_t createCS(uint32_t seed, float salience = 0.5f) {
        amyg_conditioned_stimulus_t cs;
        memset(&cs, 0, sizeof(cs));
        cs.n_features = 10;
        for (uint32_t i = 0; i < cs.n_features; i++) {
            cs.features[i] = sinf((float)(seed + i) * 0.7f);
        }
        cs.salience = salience;
        cs.modality = 0;
        return cs;
    }

    // Helper: Create US
    amyg_unconditioned_stimulus_t createUS(float intensity = 0.8f) {
        amyg_unconditioned_stimulus_t us;
        us.intensity = intensity;
        us.valence = AMYG_VALENCE_NEGATIVE;
        us.type = 0;
        us.duration_ms = 100.0f;
        return us;
    }

    // Helper: Create context
    amyg_context_t createContext(uint32_t id) {
        amyg_context_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.context_id = id;
        for (uint32_t i = 0; i < AMYG_MAX_CONTEXT_DIM; i++) {
            ctx.context_vector[i] = cosf((float)(id + i) * 0.3f);
        }
        ctx.familiarity = 0.5f;
        ctx.is_safe = false;
        return ctx;
    }
};

//=============================================================================
// Determinism Tests
//=============================================================================

TEST_F(AmygdalaRegressionTest, StimulusProcessingConsistent) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(42, 0.7f);
    auto us = createUS(0.8f);

    // Condition fear
    amygdala_condition_fear(amyg, &cs, &us, nullptr);

    // First processing
    amyg_fear_response_t resp1;
    amygdala_process_stimulus(amyg, &cs, &resp1);

    // Reset and repeat
    amygdala_reset(amyg);
    amygdala_condition_fear(amyg, &cs, &us, nullptr);

    amyg_fear_response_t resp2;
    amygdala_process_stimulus(amyg, &cs, &resp2);

    // Should be close (within 10% tolerance for floating point accumulation)
    EXPECT_NEAR(resp1.fear_intensity, resp2.fear_intensity, 0.1f);
    for (int i = 0; i < AMYG_OUTPUT_COUNT; i++) {
        EXPECT_NEAR(resp1.outputs[i], resp2.outputs[i], 0.1f);
    }
}

TEST_F(AmygdalaRegressionTest, ContextAffectsFearResponse) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(123, 0.6f);
    auto us = createUS(0.7f);
    auto ctx = createContext(1);

    amygdala_set_context(amyg, &ctx);
    amygdala_condition_fear(amyg, &cs, &us, nullptr);

    amyg_fear_response_t resp1;
    amygdala_process_stimulus(amyg, &cs, &resp1);

    // Reset and repeat with same context
    amygdala_reset(amyg);
    amygdala_set_context(amyg, &ctx);
    amygdala_condition_fear(amyg, &cs, &us, nullptr);

    amyg_fear_response_t resp2;
    amygdala_process_stimulus(amyg, &cs, &resp2);

    // Should be close (within tolerance)
    EXPECT_NEAR(resp1.fear_intensity, resp2.fear_intensity, 0.1f);
}

TEST_F(AmygdalaRegressionTest, StepDecaysBehavior) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(42, 0.8f);
    auto us = createUS(0.9f);
    amygdala_condition_fear(amyg, &cs, &us, nullptr);
    amygdala_process_stimulus(amyg, &cs, nullptr);

    float initial_fear = amygdala_get_fear_level(amyg);

    // Step forward
    for (int i = 0; i < 100; i++) {
        amygdala_step(amyg, 10.0f);
    }
    float after_step = amygdala_get_fear_level(amyg);

    // Fear should decay over time
    EXPECT_LE(after_step, initial_fear);
}

TEST_F(AmygdalaRegressionTest, ExtinctionIncreases) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(42, 0.8f);
    auto us = createUS(0.9f);
    uint32_t id;

    // Condition
    amygdala_condition_fear(amyg, &cs, &us, &id);

    amyg_fear_memory_t mem_before;
    amygdala_get_fear_memory(amyg, id, &mem_before);
    float extinction_before = mem_before.extinction_strength;

    // Extinction trials
    for (int i = 0; i < 10; i++) {
        amygdala_extinction_trial(amyg, &cs);
    }

    amyg_fear_memory_t mem_after;
    amygdala_get_fear_memory(amyg, id, &mem_after);

    // Extinction should have increased
    EXPECT_GT(mem_after.extinction_strength, extinction_before);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(AmygdalaRegressionTest, FearLevelBounded) {
    ASSERT_NE(amyg, nullptr);

    // Test with extreme stimuli
    auto cs = createCS(1, 1.0f);
    auto us = createUS(1.0f);

    // Many conditioning trials
    for (int i = 0; i < 100; i++) {
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
    }

    amyg_fear_response_t resp;
    amygdala_process_stimulus(amyg, &cs, &resp);

    // Fear should be bounded [0, 1]
    EXPECT_GE(resp.fear_intensity, 0.0f);
    EXPECT_LE(resp.fear_intensity, 1.0f);
}

TEST_F(AmygdalaRegressionTest, AnxietyBounded) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(1, 1.0f);
    auto us = createUS(1.0f);

    // Many fear events
    for (int i = 0; i < 100; i++) {
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
        amygdala_process_stimulus(amyg, &cs, nullptr);
    }

    float anxiety = amygdala_get_anxiety_level(amyg);
    EXPECT_GE(anxiety, 0.0f);
    EXPECT_LE(anxiety, 1.0f);
}

TEST_F(AmygdalaRegressionTest, NucleusActivationBounded) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(1, 1.0f);
    auto us = createUS(1.0f);

    for (int i = 0; i < 50; i++) {
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
        amygdala_process_stimulus(amyg, &cs, nullptr);
    }

    for (int n = 0; n < AMYG_NUCLEUS_COUNT; n++) {
        float activation;
        amygdala_get_nucleus_activation(amyg, (amyg_nucleus_type_t)n, &activation);
        EXPECT_GE(activation, 0.0f) << "Nucleus " << n;
        EXPECT_LE(activation, 1.0f) << "Nucleus " << n;
    }
}

TEST_F(AmygdalaRegressionTest, OutputsBounded) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(1, 1.0f);
    auto us = createUS(1.0f);

    for (int i = 0; i < 10; i++) {
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
    }

    amyg_fear_response_t resp;
    amygdala_process_stimulus(amyg, &cs, &resp);

    for (int i = 0; i < AMYG_OUTPUT_COUNT; i++) {
        EXPECT_GE(resp.outputs[i], 0.0f) << "Output " << i;
        EXPECT_LE(resp.outputs[i], 1.0f) << "Output " << i;
    }
}

TEST_F(AmygdalaRegressionTest, AssociationStrengthBounded) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(1, 1.0f);
    auto us = createUS(1.0f);
    uint32_t id;

    // Many conditioning trials
    for (int i = 0; i < 100; i++) {
        amygdala_condition_fear(amyg, &cs, &us, &id);
    }

    amyg_fear_memory_t mem;
    amygdala_get_fear_memory(amyg, id, &mem);

    EXPECT_GE(mem.association_strength, 0.0f);
    EXPECT_LE(mem.association_strength, 1.0f);
    EXPECT_GE(mem.extinction_strength, 0.0f);
    EXPECT_LE(mem.extinction_strength, 1.0f);
}

TEST_F(AmygdalaRegressionTest, NoNaNValues) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(1, 0.5f);
    auto us = createUS(0.5f);

    amygdala_condition_fear(amyg, &cs, &us, nullptr);

    for (int i = 0; i < 1000; i++) {
        amygdala_process_stimulus(amyg, &cs, nullptr);
        amygdala_step(amyg, 10.0f);
    }

    float fear = amygdala_get_fear_level(amyg);
    float anxiety = amygdala_get_anxiety_level(amyg);

    EXPECT_FALSE(std::isnan(fear));
    EXPECT_FALSE(std::isnan(anxiety));
    EXPECT_FALSE(std::isinf(fear));
    EXPECT_FALSE(std::isinf(anxiety));

    for (int n = 0; n < AMYG_NUCLEUS_COUNT; n++) {
        float activation;
        amygdala_get_nucleus_activation(amyg, (amyg_nucleus_type_t)n, &activation);
        EXPECT_FALSE(std::isnan(activation)) << "Nucleus " << n;
        EXPECT_FALSE(std::isinf(activation)) << "Nucleus " << n;
    }
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(AmygdalaRegressionTest, MaxFearMemories) {
    ASSERT_NE(amyg, nullptr);

    auto us = createUS(0.5f);

    // Try to add max + 1 memories
    for (int i = 0; i < AMYG_MAX_FEAR_MEMORIES + 10; i++) {
        auto cs = createCS(i, 0.5f);
        int result = amygdala_condition_fear(amyg, &cs, &us, nullptr);

        if (i < AMYG_MAX_FEAR_MEMORIES) {
            // Should succeed up to limit
            EXPECT_EQ(result, 0) << "Failed at index " << i;
        }
    }

    // Should be capped at max
    EXPECT_LE(amygdala_get_fear_memory_count(amyg), AMYG_MAX_FEAR_MEMORIES);
}

TEST_F(AmygdalaRegressionTest, NullPointerHandling) {
    EXPECT_NE(amygdala_create(nullptr), nullptr);  // Should use defaults

    EXPECT_EQ(amygdala_default_config(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(amygdala_validate_config(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(amygdala_reset(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(amygdala_step(nullptr, 10.0f), NIMCP_ERROR_NULL_POINTER);

    amyg_fear_response_t resp;
    EXPECT_EQ(amygdala_process_stimulus(nullptr, nullptr, &resp), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(amygdala_get_response(nullptr, &resp), NIMCP_ERROR_NULL_POINTER);

    amyg_conditioned_stimulus_t cs = createCS(1);
    EXPECT_EQ(amygdala_process_stimulus(nullptr, &cs, &resp), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(amygdala_process_stimulus(amyg, nullptr, &resp), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AmygdalaRegressionTest, InvalidNucleus) {
    ASSERT_NE(amyg, nullptr);

    float activation;
    EXPECT_EQ(amygdala_get_nucleus_activation(amyg, (amyg_nucleus_type_t)99, &activation),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(amygdala_set_nucleus_activation(amyg, (amyg_nucleus_type_t)99, 0.5f),
              NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(AmygdalaRegressionTest, InvalidParameters) {
    ASSERT_NE(amyg, nullptr);

    // Invalid dt
    EXPECT_EQ(amygdala_step(amyg, 0.0f), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(amygdala_step(amyg, -10.0f), NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(AmygdalaRegressionTest, CreateDestroyStability) {
    // Create and destroy many times
    for (int i = 0; i < 100; i++) {
        amyg_config_t config;
        amygdala_default_config(&config);
        amygdala_t* temp = amygdala_create(&config);
        ASSERT_NE(temp, nullptr) << "Failed at iteration " << i;

        // Do some work
        auto cs = createCS(i);
        auto us = createUS(0.5f);
        amygdala_condition_fear(temp, &cs, &us, nullptr);
        amygdala_process_stimulus(temp, &cs, nullptr);

        amygdala_destroy(temp);
    }
}

//=============================================================================
// Consistency Tests
//=============================================================================

TEST_F(AmygdalaRegressionTest, FearDecaysCorrectly) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(1, 0.9f);
    auto us = createUS(0.9f);

    for (int i = 0; i < 5; i++) {
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
    }
    amygdala_process_stimulus(amyg, &cs, nullptr);

    float initial_fear = amygdala_get_fear_level(amyg);

    // Step forward without stimulation
    for (int i = 0; i < 100; i++) {
        amygdala_step(amyg, 100.0f);
    }

    float final_fear = amygdala_get_fear_level(amyg);

    // Fear should decay
    EXPECT_LT(final_fear, initial_fear);
}

TEST_F(AmygdalaRegressionTest, ExtinctionReducesFear) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(1, 0.8f);
    auto us = createUS(0.8f);
    uint32_t id;

    // Condition
    for (int i = 0; i < 5; i++) {
        amygdala_condition_fear(amyg, &cs, &us, &id);
    }

    amyg_fear_memory_t before;
    amygdala_get_fear_memory(amyg, id, &before);
    float effective_before = before.association_strength * (1.0f - before.extinction_strength);

    // Extinction
    for (int i = 0; i < 20; i++) {
        amygdala_extinction_trial(amyg, &cs);
    }

    amyg_fear_memory_t after;
    amygdala_get_fear_memory(amyg, id, &after);
    float effective_after = after.association_strength * (1.0f - after.extinction_strength);

    // Effective fear should be reduced
    EXPECT_LT(effective_after, effective_before);
}

TEST_F(AmygdalaRegressionTest, StrongerUSStrongerMemory) {
    ASSERT_NE(amyg, nullptr);

    auto cs1 = createCS(1);
    auto cs2 = createCS(2);
    auto us_weak = createUS(0.2f);
    auto us_strong = createUS(0.9f);

    uint32_t id_weak, id_strong;
    amygdala_condition_fear(amyg, &cs1, &us_weak, &id_weak);
    amygdala_condition_fear(amyg, &cs2, &us_strong, &id_strong);

    amyg_fear_memory_t weak, strong;
    amygdala_get_fear_memory(amyg, id_weak, &weak);
    amygdala_get_fear_memory(amyg, id_strong, &strong);

    EXPECT_GT(strong.association_strength, weak.association_strength);
}

TEST_F(AmygdalaRegressionTest, ContextSimilarityConsistent) {
    ASSERT_NE(amyg, nullptr);

    auto ctx1 = createContext(1);
    auto ctx2 = createContext(1);  // Same
    auto ctx3 = createContext(100);  // Different

    // Same context should be perfectly similar
    float same = amygdala_context_similarity(&ctx1, &ctx2);
    EXPECT_NEAR(same, 1.0f, 0.001f);

    // Different contexts should be less similar
    float diff = amygdala_context_similarity(&ctx1, &ctx3);
    EXPECT_LT(diff, same);
}

TEST_F(AmygdalaRegressionTest, StimulusSimilarityConsistent) {
    ASSERT_NE(amyg, nullptr);

    auto cs1 = createCS(1);
    auto cs2 = createCS(1);  // Same
    auto cs3 = createCS(100);  // Different

    // Same stimulus should be perfectly similar
    float same = amygdala_stimulus_similarity(&cs1, &cs2);
    EXPECT_NEAR(same, 1.0f, 0.001f);

    // Different stimuli should be less similar
    float diff = amygdala_stimulus_similarity(&cs1, &cs3);
    EXPECT_LT(diff, same);
}

TEST_F(AmygdalaRegressionTest, PrefrontalInhibitionReducesFear) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(1, 0.8f);
    auto us = createUS(0.8f);

    for (int i = 0; i < 5; i++) {
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
    }

    // No inhibition
    amygdala_set_prefrontal_inhibition(amyg, 0.0f);
    amyg_fear_response_t no_inhib;
    amygdala_process_stimulus(amyg, &cs, &no_inhib);

    // Reset and apply strong inhibition
    amygdala_reset(amyg);
    for (int i = 0; i < 5; i++) {
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
    }
    amygdala_set_prefrontal_inhibition(amyg, 0.9f);
    amyg_fear_response_t with_inhib;
    amygdala_process_stimulus(amyg, &cs, &with_inhib);

    // ITC should be more activated with PFC inhibition
    float itc_no_inhib, itc_with_inhib;
    amygdala_get_nucleus_activation(amyg, AMYG_NUCLEUS_ITC, &itc_with_inhib);

    // Reset and check without
    amygdala_reset(amyg);
    for (int i = 0; i < 5; i++) {
        amygdala_condition_fear(amyg, &cs, &us, nullptr);
    }
    amygdala_set_prefrontal_inhibition(amyg, 0.0f);
    amygdala_process_stimulus(amyg, &cs, nullptr);
    amygdala_get_nucleus_activation(amyg, AMYG_NUCLEUS_ITC, &itc_no_inhib);

    EXPECT_GT(itc_with_inhib, itc_no_inhib);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(AmygdalaRegressionTest, ManySteps) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(1, 0.5f);
    auto us = createUS(0.5f);
    amygdala_condition_fear(amyg, &cs, &us, nullptr);

    // Many time steps
    for (int i = 0; i < 10000; i++) {
        if (i % 100 == 0) {
            amygdala_process_stimulus(amyg, &cs, nullptr);
        }
        amygdala_step(amyg, 1.0f);
    }

    // Should still be stable
    float fear = amygdala_get_fear_level(amyg);
    EXPECT_GE(fear, 0.0f);
    EXPECT_LE(fear, 1.0f);
    EXPECT_FALSE(std::isnan(fear));
}

TEST_F(AmygdalaRegressionTest, ManyMemories) {
    ASSERT_NE(amyg, nullptr);

    auto us = createUS(0.5f);

    // Add many distinct memories
    for (int i = 0; i < 100; i++) {
        // Use one-hot patterns for distinctness
        amyg_conditioned_stimulus_t cs;
        memset(&cs, 0, sizeof(cs));
        cs.n_features = AMYG_MAX_CS_FEATURES;
        cs.features[i % AMYG_MAX_CS_FEATURES] = 1.0f;
        cs.salience = 0.5f;

        amygdala_condition_fear(amyg, &cs, &us, nullptr);
    }

    // Should handle gracefully
    uint32_t count = amygdala_get_fear_memory_count(amyg);
    EXPECT_LE(count, AMYG_MAX_FEAR_MEMORIES);
}

TEST_F(AmygdalaRegressionTest, RapidProcessing) {
    ASSERT_NE(amyg, nullptr);

    auto cs = createCS(1, 0.7f);
    auto us = createUS(0.7f);
    amygdala_condition_fear(amyg, &cs, &us, nullptr);

    // Many rapid stimulus presentations
    for (int i = 0; i < 1000; i++) {
        amyg_fear_response_t resp;
        amygdala_process_stimulus(amyg, &cs, &resp);

        EXPECT_GE(resp.fear_intensity, 0.0f);
        EXPECT_LE(resp.fear_intensity, 1.0f);
    }
}

// Main
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
