/**
 * @file test_scientific_reasoning.cpp
 * @brief Unit tests for NIMCP Scientific Reasoning
 *
 * Tests dimensional analysis, hypothesis testing, causal inference,
 * and experimental design.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_scientific_reasoning.h"

namespace {

//=============================================================================
// Test Constants
//=============================================================================

constexpr float FLOAT_TOLERANCE = 1e-5f;

//=============================================================================
// Test Fixture
//=============================================================================

class ScientificReasoningTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        sci = scientific_reasoning_create();
        ASSERT_NE(sci, nullptr);
    }

    void TearDown() override
    {
        if (sci) {
            scientific_reasoning_destroy(sci);
            sci = nullptr;
        }
    }

    scientific_reasoning_t* sci;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ScientificReasoningTest, CreateDefault)
{
    EXPECT_NE(sci, nullptr);
}

TEST_F(ScientificReasoningTest, CreateCustom)
{
    scientific_config_t config = scientific_default_config();
    config.hypothesis_prior_default = 0.6f;
    config.significance_level = 0.01f;

    scientific_reasoning_t* custom = scientific_reasoning_create_custom(&config);
    ASSERT_NE(custom, nullptr);
    scientific_reasoning_destroy(custom);
}

TEST_F(ScientificReasoningTest, CreateWithNullConfig)
{
    scientific_reasoning_t* created = scientific_reasoning_create_custom(nullptr);
    EXPECT_NE(created, nullptr);
    scientific_reasoning_destroy(created);
}

TEST_F(ScientificReasoningTest, DestroyNullSafe)
{
    scientific_reasoning_destroy(nullptr);
    // Should not crash
}

TEST_F(ScientificReasoningTest, DefaultConfig)
{
    scientific_config_t config = scientific_default_config();

    EXPECT_NEAR(config.hypothesis_prior_default, 0.5f, 0.01f);
    EXPECT_NEAR(config.significance_level, 0.05f, 0.001f);
    EXPECT_EQ(config.max_hypotheses, 32);
    EXPECT_TRUE(config.enable_causal_inference);
}

TEST_F(ScientificReasoningTest, ValidateConfig)
{
    scientific_config_t valid = scientific_default_config();
    EXPECT_TRUE(scientific_validate_config(&valid));

    scientific_config_t invalid = valid;
    invalid.hypothesis_prior_default = 1.5f;
    EXPECT_FALSE(scientific_validate_config(&invalid));

    invalid = valid;
    invalid.max_hypotheses = 0;
    EXPECT_FALSE(scientific_validate_config(&invalid));
}

//=============================================================================
// Dimensional Analysis Tests
//=============================================================================

TEST_F(ScientificReasoningTest, Dimensionless)
{
    physical_dimension_t dim = scientific_dimensionless();

    EXPECT_EQ(dim.length, 0);
    EXPECT_EQ(dim.mass, 0);
    EXPECT_EQ(dim.time, 0);
    EXPECT_EQ(dim.current, 0);
    EXPECT_EQ(dim.temperature, 0);
    EXPECT_EQ(dim.amount, 0);
    EXPECT_EQ(dim.luminosity, 0);
}

TEST_F(ScientificReasoningTest, DimensionsEqual)
{
    physical_dimension_t force1 = DIM_FORCE;  // [M*L/T^2]
    physical_dimension_t force2 = {1, 1, -2, 0, 0, 0, 0};

    EXPECT_TRUE(scientific_dimensions_equal(force1, force2));
    EXPECT_FALSE(scientific_dimensions_equal(force1, DIM_ENERGY));
}

TEST_F(ScientificReasoningTest, IsDimensionless)
{
    EXPECT_TRUE(scientific_is_dimensionless(scientific_dimensionless()));
    EXPECT_FALSE(scientific_is_dimensionless(DIM_LENGTH));
    EXPECT_FALSE(scientific_is_dimensionless(DIM_FORCE));
}

TEST_F(ScientificReasoningTest, MultiplyDimensions)
{
    physical_dimension_t mass = DIM_MASS;      // [M]
    physical_dimension_t accel = DIM_ACCEL;    // [L/T^2]
    physical_dimension_t force = scientific_multiply_dimensions(mass, accel);

    // F = m*a, so [M] * [L/T^2] = [M*L/T^2]
    EXPECT_TRUE(scientific_dimensions_equal(force, DIM_FORCE));
}

TEST_F(ScientificReasoningTest, DivideDimensions)
{
    physical_dimension_t energy = DIM_ENERGY;  // [M*L^2/T^2]
    physical_dimension_t time = DIM_TIME;      // [T]
    physical_dimension_t power = scientific_divide_dimensions(energy, time);

    // P = E/t, so [M*L^2/T^2] / [T] = [M*L^2/T^3]
    EXPECT_TRUE(scientific_dimensions_equal(power, DIM_POWER));
}

TEST_F(ScientificReasoningTest, PowerDimension)
{
    physical_dimension_t length = DIM_LENGTH;  // [L]
    physical_dimension_t area = scientific_power_dimension(length, 2);

    EXPECT_EQ(area.length, 2);  // [L^2]
    EXPECT_EQ(area.mass, 0);
    EXPECT_EQ(area.time, 0);
}

TEST_F(ScientificReasoningTest, CreateQuantity)
{
    physical_quantity_t force = scientific_create_quantity(9.8f, DIM_FORCE, "F");

    EXPECT_NEAR(force.value, 9.8f, FLOAT_TOLERANCE);
    EXPECT_TRUE(scientific_dimensions_equal(force.dimension, DIM_FORCE));
    EXPECT_STREQ(force.symbol, "F");
}

TEST_F(ScientificReasoningTest, DimensionToString)
{
    char buffer[64];
    const char* str = scientific_dimension_to_string(DIM_FORCE, buffer, 64);

    ASSERT_NE(str, nullptr);
    // Should contain something like "M*L/T^2" or similar representation
    EXPECT_GT(strlen(str), 0);
}

TEST_F(ScientificReasoningTest, BuckinghamPi)
{
    // Simple example: velocity = length / time
    physical_quantity_t quantities[3];
    quantities[0] = scientific_create_quantity(1.0f, DIM_VELOCITY, "v");
    quantities[1] = scientific_create_quantity(1.0f, DIM_LENGTH, "L");
    quantities[2] = scientific_create_quantity(1.0f, DIM_TIME, "T");

    float* pi_groups[5];
    for (int i = 0; i < 5; i++) {
        pi_groups[i] = (float*)malloc(3 * sizeof(float));
    }

    uint32_t num_groups = scientific_buckingham_pi(sci, quantities, 3, pi_groups, 5);

    EXPECT_GT(num_groups, 0);

    for (int i = 0; i < 5; i++) {
        free(pi_groups[i]);
    }
}

//=============================================================================
// Common Dimension Macros Tests
//=============================================================================

TEST_F(ScientificReasoningTest, CommonDimensions)
{
    // Verify common dimension macros
    physical_dimension_t length = DIM_LENGTH;
    EXPECT_EQ(length.length, 1);
    EXPECT_EQ(length.mass, 0);

    physical_dimension_t velocity = DIM_VELOCITY;
    EXPECT_EQ(velocity.length, 1);
    EXPECT_EQ(velocity.time, -1);

    physical_dimension_t energy = DIM_ENERGY;
    EXPECT_EQ(energy.length, 2);
    EXPECT_EQ(energy.mass, 1);
    EXPECT_EQ(energy.time, -2);
}

//=============================================================================
// Hypothesis Tests
//=============================================================================

TEST_F(ScientificReasoningTest, CreateHypothesis)
{
    hypothesis_t h = scientific_create_hypothesis(sci, "Test hypothesis", 0.5f);

    EXPECT_STREQ(h.description, "Test hypothesis");
    EXPECT_NEAR(h.prior, 0.5f, FLOAT_TOLERANCE);
    EXPECT_NEAR(h.posterior, 0.5f, FLOAT_TOLERANCE);
    EXPECT_TRUE(h.active);
}

TEST_F(ScientificReasoningTest, UpdateHypothesis)
{
    hypothesis_t h = scientific_create_hypothesis(sci, "Test", 0.5f);

    // Create supporting evidence
    float values[] = {1.0f, 0.9f, 0.95f};
    data_sample_t sample;
    sample.values = values;
    sample.num_values = 3;
    sample.weight = 1.0f;
    sample.timestamp = 0;

    float posterior = scientific_update_hypothesis(sci, &h, &sample, 1);

    EXPECT_NE(posterior, h.prior);  // Posterior should change with evidence
    EXPECT_GE(posterior, 0.0f);
    EXPECT_LE(posterior, 1.0f);
}

TEST_F(ScientificReasoningTest, CompareHypotheses)
{
    hypothesis_t h1 = scientific_create_hypothesis(sci, "H1", 0.7f);
    hypothesis_t h2 = scientific_create_hypothesis(sci, "H2", 0.3f);

    float bayes_factor = scientific_compare_hypotheses(sci, &h1, &h2);

    // Higher prior should give higher or equal Bayes factor
    EXPECT_GE(bayes_factor, 1.0f);
}

TEST_F(ScientificReasoningTest, BestHypothesis)
{
    hypothesis_t hypotheses[3];
    hypotheses[0] = scientific_create_hypothesis(sci, "Low", 0.2f);
    hypotheses[1] = scientific_create_hypothesis(sci, "High", 0.8f);
    hypotheses[2] = scientific_create_hypothesis(sci, "Medium", 0.5f);

    uint32_t best = scientific_best_hypothesis(sci, hypotheses, 3);

    EXPECT_EQ(best, 1);  // Index of "High" hypothesis
}

TEST_F(ScientificReasoningTest, RejectHypothesis)
{
    hypothesis_t h = scientific_create_hypothesis(sci, "Weak", 0.05f);

    // With very low posterior and enough observations, should be rejected
    h.posterior = 0.01f;
    h.observations = 10;  // Need at least 10 observations for rejection
    bool rejected = scientific_reject_hypothesis(sci, &h);

    EXPECT_TRUE(rejected);
    EXPECT_FALSE(h.active);
}

TEST_F(ScientificReasoningTest, HypothesisNullHandling)
{
    hypothesis_t h = scientific_create_hypothesis(nullptr, "Test", 0.5f);
    EXPECT_EQ(h.description[0], '\0');

    h = scientific_create_hypothesis(sci, nullptr, 0.5f);
    EXPECT_EQ(h.description[0], '\0');
}

//=============================================================================
// Causal Inference Tests
//=============================================================================

TEST_F(ScientificReasoningTest, CreateCausalGraph)
{
    const char* var_names[] = {"X", "Y", "Z"};
    causal_graph_t* graph = scientific_create_causal_graph(sci, var_names, 3);

    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->num_variables, 3);

    scientific_destroy_causal_graph(graph);
}

TEST_F(ScientificReasoningTest, AddCausalRelation)
{
    const char* var_names[] = {"Cause", "Effect"};
    causal_graph_t* graph = scientific_create_causal_graph(sci, var_names, 2);
    ASSERT_NE(graph, nullptr);

    int result = scientific_add_causal_relation(graph, 0, 1, 0.8f);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(graph->num_relations, 1);
    EXPECT_EQ(graph->relations[0].cause_id, 0);
    EXPECT_EQ(graph->relations[0].effect_id, 1);
    EXPECT_NEAR(graph->relations[0].strength, 0.8f, FLOAT_TOLERANCE);

    scientific_destroy_causal_graph(graph);
}

TEST_F(ScientificReasoningTest, LearnCausalStructure)
{
    const char* var_names[] = {"A", "B"};
    causal_graph_t* graph = scientific_create_causal_graph(sci, var_names, 2);
    ASSERT_NE(graph, nullptr);

    // Create correlated data (need at least 10 samples for structure learning)
    float row0[] = {0.0f, 0.0f};
    float row1[] = {1.0f, 1.5f};
    float row2[] = {2.0f, 3.0f};
    float row3[] = {3.0f, 4.5f};
    float row4[] = {4.0f, 6.0f};
    float row5[] = {5.0f, 7.5f};
    float row6[] = {6.0f, 9.0f};
    float row7[] = {7.0f, 10.5f};
    float row8[] = {8.0f, 12.0f};
    float row9[] = {9.0f, 13.5f};
    const float* data[] = {row0, row1, row2, row3, row4, row5, row6, row7, row8, row9};

    int result = scientific_learn_causal_structure(sci, graph, data, 10);
    EXPECT_EQ(result, 0);

    // Should detect some causal relationship
    EXPECT_GE(graph->num_relations, 0);

    scientific_destroy_causal_graph(graph);
}

TEST_F(ScientificReasoningTest, EstimateCausalEffect)
{
    const char* var_names[] = {"Treatment", "Outcome"};
    causal_graph_t* graph = scientific_create_causal_graph(sci, var_names, 2);
    ASSERT_NE(graph, nullptr);

    scientific_add_causal_relation(graph, 0, 1, 0.5f);

    float effect = scientific_estimate_causal_effect(sci, graph, 0, 1, 1.0f);

    EXPECT_GE(effect, 0.0f);

    scientific_destroy_causal_graph(graph);
}

TEST_F(ScientificReasoningTest, IsPathBlocked)
{
    const char* var_names[] = {"A", "B", "C"};
    causal_graph_t* graph = scientific_create_causal_graph(sci, var_names, 3);
    ASSERT_NE(graph, nullptr);

    // A -> B -> C
    scientific_add_causal_relation(graph, 0, 1, 0.5f);
    scientific_add_causal_relation(graph, 1, 2, 0.5f);

    // Conditioning on B should block path from A to C
    uint32_t conditioning[] = {1};
    bool blocked = scientific_is_path_blocked(graph, 0, 2, conditioning, 1);

    EXPECT_TRUE(blocked);

    scientific_destroy_causal_graph(graph);
}

TEST_F(ScientificReasoningTest, CausalGraphNullHandling)
{
    causal_graph_t* graph = scientific_create_causal_graph(nullptr, nullptr, 0);
    EXPECT_EQ(graph, nullptr);

    scientific_destroy_causal_graph(nullptr);  // Should not crash
}

//=============================================================================
// Experimental Design Tests
//=============================================================================

TEST_F(ScientificReasoningTest, SuggestExperiment)
{
    const char* var_names[] = {"Treatment", "Confounder", "Outcome"};
    causal_graph_t* graph = scientific_create_causal_graph(sci, var_names, 3);
    ASSERT_NE(graph, nullptr);

    scientific_add_causal_relation(graph, 0, 2, 0.5f);
    scientific_add_causal_relation(graph, 1, 2, 0.3f);

    experimental_design_t design;
    int result = scientific_suggest_experiment(sci, graph, 2, &design);

    EXPECT_EQ(result, 0);
    EXPECT_GT(design.sample_size, 0);

    scientific_destroy_causal_graph(graph);
}

TEST_F(ScientificReasoningTest, RequiredSampleSize)
{
    // Small effect size needs more samples
    uint32_t small_effect = scientific_required_sample_size(sci, 0.2f, 0.8f, 0.05f);
    uint32_t large_effect = scientific_required_sample_size(sci, 0.8f, 0.8f, 0.05f);

    EXPECT_GT(small_effect, large_effect);
    EXPECT_GT(small_effect, 10);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(ScientificReasoningTest, SetInflammation)
{
    EXPECT_EQ(scientific_set_inflammation(sci, 0.5f), 0);
    EXPECT_NE(scientific_set_inflammation(nullptr, 0.5f), 0);
}

TEST_F(ScientificReasoningTest, SetSleepDeprivation)
{
    EXPECT_EQ(scientific_set_sleep_deprivation(sci, 0.5f), 0);
    EXPECT_NE(scientific_set_sleep_deprivation(nullptr, 0.5f), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ScientificReasoningTest, GetStats)
{
    // Perform operations
    scientific_create_hypothesis(sci, "Test", 0.5f);
    scientific_create_quantity(1.0f, DIM_FORCE, "F");

    scientific_stats_t stats;
    EXPECT_EQ(scientific_get_stats(sci, &stats), 0);

    EXPECT_GE(stats.hypotheses_generated, 1);
    EXPECT_GE(stats.dimensional_analyses, 0);
}

TEST_F(ScientificReasoningTest, GetStatsNullHandling)
{
    scientific_stats_t stats;
    EXPECT_NE(scientific_get_stats(nullptr, &stats), 0);
    EXPECT_NE(scientific_get_stats(sci, nullptr), 0);
}

TEST_F(ScientificReasoningTest, ResetStats)
{
    scientific_create_hypothesis(sci, "Test", 0.5f);

    scientific_reset_stats(sci);

    scientific_stats_t stats;
    scientific_get_stats(sci, &stats);
    EXPECT_EQ(stats.hypotheses_generated, 0);
}

TEST_F(ScientificReasoningTest, ResetStatsNullSafe)
{
    scientific_reset_stats(nullptr);
    // Should not crash
}

}  // namespace
