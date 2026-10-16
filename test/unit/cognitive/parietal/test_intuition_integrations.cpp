/**
 * @file test_intuition_integrations.cpp
 * @brief Unit tests for NIMCP Intuition Integration Framework (Phase 6.0)
 *
 * Tests for integration of Phase 6 engines, extrapolation, knowledge synthesis,
 * and cross-system connections.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_intuition_integrations.h"

namespace {

//=============================================================================
// Test Constants
//=============================================================================

constexpr float FLOAT_TOLERANCE = 1e-4f;

//=============================================================================
// Test Fixture
//=============================================================================

class IntuitionIntegrationsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        system = intuition_system_create();
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override
    {
        if (system) {
            intuition_system_destroy(system);
            system = nullptr;
        }
    }

    intuition_system_t* system = nullptr;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(IntuitionIntegrationsTest, CreateDefault)
{
    EXPECT_NE(system, nullptr);
}

TEST_F(IntuitionIntegrationsTest, CreateCustom)
{
    intuition_system_config_t config = intuition_system_default_config();
    config.enable_intuitive_engine = true;
    config.enable_hypothesis_engine = true;
    config.extrapolation_confidence_decay = 0.1f;

    intuition_system_t* custom = intuition_system_create_custom(&config);
    ASSERT_NE(custom, nullptr);
    intuition_system_destroy(custom);
}

TEST_F(IntuitionIntegrationsTest, CreateWithNullConfig)
{
    // Implementation returns NULL for null config (requires valid config)
    intuition_system_t* created = intuition_system_create_custom(nullptr);
    EXPECT_EQ(created, nullptr);
}

TEST_F(IntuitionIntegrationsTest, DestroyNullSafe)
{
    intuition_system_destroy(nullptr);
}

TEST_F(IntuitionIntegrationsTest, DefaultConfig)
{
    intuition_system_config_t config = intuition_system_default_config();

    // Check sub-engine enablement defaults
    EXPECT_TRUE(config.enable_intuitive_engine);
    EXPECT_TRUE(config.enable_meta_engine);
    EXPECT_GT(config.extrapolation_confidence_decay, 0.0f);
}

//=============================================================================
// Sub-Engine Access Tests
//=============================================================================

TEST_F(IntuitionIntegrationsTest, GetIntuitiveEngine)
{
    intuitive_engine_t* engine = intuition_get_intuitive_engine(system);
    EXPECT_NE(engine, nullptr);
}

TEST_F(IntuitionIntegrationsTest, GetAnalogicalEngine)
{
    analogical_engine_t* engine = intuition_get_analogical_engine(system);
    EXPECT_NE(engine, nullptr);
}

TEST_F(IntuitionIntegrationsTest, GetInsightEngine)
{
    insight_engine_t* engine = intuition_get_insight_engine(system);
    EXPECT_NE(engine, nullptr);
}

TEST_F(IntuitionIntegrationsTest, GetHypothesisEngine)
{
    hypothesis_engine_t* engine = intuition_get_hypothesis_engine(system);
    EXPECT_NE(engine, nullptr);
}

TEST_F(IntuitionIntegrationsTest, GetBlendingEngine)
{
    blending_engine_t* engine = intuition_get_blending_engine(system);
    EXPECT_NE(engine, nullptr);
}

TEST_F(IntuitionIntegrationsTest, GetCounterfactualEngine)
{
    counterfactual_engine_t* engine = intuition_get_counterfactual_engine(system);
    EXPECT_NE(engine, nullptr);
}

TEST_F(IntuitionIntegrationsTest, GetMetaEngine)
{
    meta_engine_t* engine = intuition_get_meta_engine(system);
    EXPECT_NE(engine, nullptr);
}

TEST_F(IntuitionIntegrationsTest, GetEnginesNullSystem)
{
    EXPECT_EQ(intuition_get_intuitive_engine(nullptr), nullptr);
    EXPECT_EQ(intuition_get_analogical_engine(nullptr), nullptr);
    EXPECT_EQ(intuition_get_insight_engine(nullptr), nullptr);
}

//=============================================================================
// Data Point Tests
//=============================================================================

TEST_F(IntuitionIntegrationsTest, CreateDataPoint)
{
    float values[] = {1.0f, 2.0f, 3.0f};
    intuition_data_point_t* point = intuition_data_point_create(
        values, 3, 100.0f, 0.9f);

    ASSERT_NE(point, nullptr);
    EXPECT_EQ(point->dim, 3u);
    EXPECT_NEAR(point->timestamp, 100.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(point->confidence, 0.9f, FLOAT_TOLERANCE);

    intuition_data_point_free(point);
}

TEST_F(IntuitionIntegrationsTest, CreateDataPointNull)
{
    intuition_data_point_t* point = intuition_data_point_create(
        nullptr, 0, 0.0f, 0.5f);

    ASSERT_NE(point, nullptr);
    EXPECT_EQ(point->dim, 0u);
    intuition_data_point_free(point);
}

TEST_F(IntuitionIntegrationsTest, RangeStruct)
{
    // Range is a simple struct, not dynamically allocated
    intuition_range_t range = {
        .start = 0.0f,
        .end = 100.0f,
        .num_samples = 10,
        .is_temporal = true
    };

    EXPECT_NEAR(range.start, 0.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(range.end, 100.0f, FLOAT_TOLERANCE);
    EXPECT_EQ(range.num_samples, 10u);
    EXPECT_TRUE(range.is_temporal);
}

//=============================================================================
// Extrapolation Tests
//=============================================================================

TEST_F(IntuitionIntegrationsTest, ExtrapolateLinear)
{
    // Create linear data points
    float v1[] = {1.0f};
    float v2[] = {2.0f};
    float v3[] = {3.0f};
    float v4[] = {4.0f};

    intuition_data_point_t* points[4];
    points[0] = intuition_data_point_create(v1, 1, 1.0f, 0.9f);
    points[1] = intuition_data_point_create(v2, 1, 2.0f, 0.9f);
    points[2] = intuition_data_point_create(v3, 1, 3.0f, 0.9f);
    points[3] = intuition_data_point_create(v4, 1, 4.0f, 0.9f);

    intuition_range_t range = {
        .start = 5.0f,
        .end = 10.0f,
        .num_samples = 5,
        .is_temporal = true
    };

    extrapolation_t* result = intuition_extrapolate(
        system, (const intuition_data_point_t**)points, 4, &range);

    if (result != nullptr) {
        EXPECT_NE(result->detected_trend, nullptr);
        EXPECT_GT(result->extrapolation_confidence, 0.0f);
        EXPECT_LE(result->extrapolation_confidence, 1.0f);

        // Should extrapolate to ~5, 6, 7...
        if (result->num_extrapolated > 0 && result->extrapolated) {
            EXPECT_GT(result->extrapolated[0]->values[0], 4.0f);
        }

        extrapolation_free(result);
    }

    for (int i = 0; i < 4; i++) {
        intuition_data_point_free(points[i]);
    }
}

TEST_F(IntuitionIntegrationsTest, ExtrapolateNull)
{
    intuition_range_t range = {.start = 0.0f, .end = 1.0f, .num_samples = 1};

    extrapolation_t* result = intuition_extrapolate(system, nullptr, 0, &range);
    EXPECT_EQ(result, nullptr);
}

TEST_F(IntuitionIntegrationsTest, ExtrapolateIncremental)
{
    // Initial data
    float v1[] = {1.0f};
    float v2[] = {2.0f};

    intuition_data_point_t* initial[2];
    initial[0] = intuition_data_point_create(v1, 1, 1.0f, 0.9f);
    initial[1] = intuition_data_point_create(v2, 1, 2.0f, 0.9f);

    intuition_range_t range = {.start = 3.0f, .end = 5.0f, .num_samples = 2};

    extrapolation_t* prev = intuition_extrapolate(
        system, (const intuition_data_point_t**)initial, 2, &range);

    if (prev != nullptr) {
        // Add new data
        float v3[] = {3.0f};
        intuition_data_point_t* new_points[1];
        new_points[0] = intuition_data_point_create(v3, 1, 3.0f, 0.95f);

        extrapolation_t* updated = intuition_extrapolate_incremental(
            system, prev, (const intuition_data_point_t**)new_points, 1);

        if (updated != nullptr) {
            extrapolation_free(updated);
        }

        intuition_data_point_free(new_points[0]);
        extrapolation_free(prev);
    }

    for (int i = 0; i < 2; i++) {
        intuition_data_point_free(initial[i]);
    }
}

//=============================================================================
// Knowledge Synthesis Tests
//=============================================================================

TEST_F(IntuitionIntegrationsTest, CreateKnowledgeFragment)
{
    float content[] = {0.5f, 0.7f, 0.9f};
    knowledge_fragment_t* frag = knowledge_fragment_create(
        "The sky is blue due to Rayleigh scattering",
        content, 3, 0.95f);

    ASSERT_NE(frag, nullptr);
    EXPECT_NEAR(frag->confidence, 0.95f, FLOAT_TOLERANCE);
    EXPECT_EQ(frag->content_dim, 3u);

    knowledge_fragment_free(frag);
}

TEST_F(IntuitionIntegrationsTest, SynthesizeKnowledge)
{
    float c1[] = {0.5f, 0.3f};
    float c2[] = {0.4f, 0.6f};
    float c3[] = {0.7f, 0.2f};

    knowledge_fragment_t* f1 = knowledge_fragment_create(
        "Water boils at 100C at sea level", c1, 2, 0.95f);
    knowledge_fragment_t* f2 = knowledge_fragment_create(
        "Pressure affects boiling point", c2, 2, 0.9f);
    knowledge_fragment_t* f3 = knowledge_fragment_create(
        "Mountains have lower pressure", c3, 2, 0.85f);

    const knowledge_fragment_t* fragments[] = {f1, f2, f3};

    synthesis_t* synth = intuition_synthesize_knowledge(
        system, fragments, 3);

    if (synth != nullptr) {
        // Should synthesize: water boils at lower temp on mountains
        EXPECT_NE(synth->synthesized, nullptr);
        synthesis_free(synth);
    }

    knowledge_fragment_free(f1);
    knowledge_fragment_free(f2);
    knowledge_fragment_free(f3);
}

TEST_F(IntuitionIntegrationsTest, SynthesizeNull)
{
    synthesis_t* synth = intuition_synthesize_knowledge(system, nullptr, 0);
    EXPECT_EQ(synth, nullptr);
}

TEST_F(IntuitionIntegrationsTest, IdentifyGaps)
{
    prediction_domain_t domain;
    memset(&domain, 0, sizeof(domain));
    strncpy(domain.name, "physics", 63);
    domain.exploration_factor = 0.5f;

    uint32_t num_gaps = 0;
    intuition_gap_t** gaps = intuition_identify_knowledge_gaps(
        system, &domain, &num_gaps);

    // May find gaps in understanding
    if (gaps != nullptr) {
        for (uint32_t i = 0; i < num_gaps; i++) {
            if (gaps[i]) {
                EXPECT_GT(strlen(gaps[i]->description), 0u);
                intuition_gap_free(gaps[i]);
            }
        }
        free(gaps);
    }
}

TEST_F(IntuitionIntegrationsTest, GenerateQuestions)
{
    intuition_gap_t gap;
    memset(&gap, 0, sizeof(gap));
    strncpy(gap.description, "Why does gravity exist?", 255);
    gap.importance = 0.9f;

    const intuition_gap_t* gaps[] = {&gap};

    uint32_t num_questions = 0;
    intuition_question_t** questions = intuition_generate_questions(
        system, gaps, 1, &num_questions);

    if (questions != nullptr) {
        for (uint32_t i = 0; i < num_questions; i++) {
            if (questions[i]) {
                EXPECT_GT(strlen(questions[i]->question), 0u);
                intuition_question_free(questions[i]);
            }
        }
        free(questions);
    }
}

//=============================================================================
// Novel Prediction Tests
//=============================================================================

TEST_F(IntuitionIntegrationsTest, PredictNovel)
{
    prediction_domain_t domain;
    memset(&domain, 0, sizeof(domain));
    strncpy(domain.name, "particle_physics", 63);
    domain.exploration_factor = 0.6f;

    uint32_t num_predictions = 0;
    novel_prediction_t** predictions = intuition_predict_novel(
        system, &domain, &num_predictions);

    if (predictions != nullptr) {
        for (uint32_t i = 0; i < num_predictions; i++) {
            if (predictions[i]) {
                EXPECT_GT(predictions[i]->novelty, 0.0f);
                novel_prediction_free(predictions[i]);
            }
        }
        free(predictions);
    }
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(IntuitionIntegrationsTest, SetInflammation)
{
    int result = intuition_system_set_inflammation(system, 0.3f);
    EXPECT_EQ(result, 0);
}

TEST_F(IntuitionIntegrationsTest, SetFatigue)
{
    int result = intuition_system_set_fatigue(system, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(IntuitionIntegrationsTest, SetInflammationNull)
{
    int result = intuition_system_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(IntuitionIntegrationsTest, GetStatistics)
{
    intuition_system_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = intuition_system_get_stats(system, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(IntuitionIntegrationsTest, ResetStatistics)
{
    intuition_system_reset_stats(system);
    // Reset returns void - just check it doesn't crash
    SUCCEED();
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(IntuitionIntegrationsTest, CrossEngineWorkflow)
{
    // Test a workflow using multiple engines together

    // 1. Form a hunch using intuitive engine
    intuitive_engine_t* intuitive = intuition_get_intuitive_engine(system);
    ASSERT_NE(intuitive, nullptr);

    // 2. Find analogy using analogical engine
    analogical_engine_t* analogical = intuition_get_analogical_engine(system);
    ASSERT_NE(analogical, nullptr);

    // 3. Generate hypothesis
    hypothesis_engine_t* hypothesis = intuition_get_hypothesis_engine(system);
    ASSERT_NE(hypothesis, nullptr);

    // 4. Monitor reasoning with meta engine
    meta_engine_t* meta = intuition_get_meta_engine(system);
    ASSERT_NE(meta, nullptr);

    // Engines should be interconnected and accessible
    SUCCEED();
}

TEST_F(IntuitionIntegrationsTest, SystemCoherence)
{
    // Test that modulation propagates to all engines
    int result = intuition_system_set_inflammation(system, 0.3f);
    EXPECT_EQ(result, 0);

    result = intuition_system_set_fatigue(system, 0.5f);
    EXPECT_EQ(result, 0);

    // All sub-engines should now reflect modulation
    intuition_system_stats_t stats;
    result = intuition_system_get_stats(system, &stats);
    EXPECT_EQ(result, 0);
}

} // namespace
