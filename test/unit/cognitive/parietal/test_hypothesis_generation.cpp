/**
 * @file test_hypothesis_generation.cpp
 * @brief Unit tests for NIMCP Hypothesis Generation Engine (Phase 6.4)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_hypothesis_generation.h"

namespace {

constexpr float FLOAT_TOLERANCE = 1e-4f;

class HypothesisGenerationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        engine = hypothesis_engine_create();
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override
    {
        if (engine) {
            hypothesis_engine_destroy(engine);
            engine = nullptr;
        }
    }

    hypothesis_engine_t* engine = nullptr;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(HypothesisGenerationTest, CreateDefault)
{
    EXPECT_NE(engine, nullptr);
}

TEST_F(HypothesisGenerationTest, CreateCustom)
{
    hypogen_config_t config = hypothesis_engine_default_config();

    hypothesis_engine_t* custom = hypothesis_engine_create_custom(&config);
    ASSERT_NE(custom, nullptr);
    hypothesis_engine_destroy(custom);
}

TEST_F(HypothesisGenerationTest, CreateWithNullConfig)
{
    hypothesis_engine_t* created = hypothesis_engine_create_custom(nullptr);
    EXPECT_EQ(created, nullptr);
}

TEST_F(HypothesisGenerationTest, DestroyNullSafe)
{
    hypothesis_engine_destroy(nullptr);
}

TEST_F(HypothesisGenerationTest, DefaultConfig)
{
    hypogen_config_t config = hypothesis_engine_default_config();
    SUCCEED();
}

//=============================================================================
// Abductive Inference Tests
//=============================================================================

TEST_F(HypothesisGenerationTest, AbductiveInference)
{
    hypogen_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    strncpy(obs.description, "The grass is wet", 255);
    obs.confidence = 0.9f;

    hypogen_theory_t* theory = hypothesis_abductive_inference(engine, &obs);

    if (theory != nullptr) {
        EXPECT_GT(theory->explanatory_power, 0.0f);
        hypothesis_free_theory(theory);
    }
}

TEST_F(HypothesisGenerationTest, AbductiveInferenceNull)
{
    hypogen_theory_t* theory = hypothesis_abductive_inference(engine, nullptr);
    EXPECT_EQ(theory, nullptr);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(HypothesisGenerationTest, SetInflammation)
{
    int result = hypothesis_set_inflammation(engine, 0.3f);
    EXPECT_EQ(result, 0);
}

TEST_F(HypothesisGenerationTest, SetFatigue)
{
    int result = hypothesis_set_fatigue(engine, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(HypothesisGenerationTest, SetInflammationNull)
{
    int result = hypothesis_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(HypothesisGenerationTest, GetStatistics)
{
    hypogen_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = hypothesis_get_stats(engine, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(HypothesisGenerationTest, ResetStatistics)
{
    hypothesis_reset_stats(engine);
    SUCCEED();
}

} // namespace
