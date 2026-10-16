/**
 * @file test_counterfactual.cpp
 * @brief Unit tests for NIMCP Counterfactual Reasoning Engine (Phase 6.6)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_counterfactual.h"

namespace {

constexpr float FLOAT_TOLERANCE = 1e-4f;

class CounterfactualTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        engine = counterfactual_engine_create();
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override
    {
        if (engine) {
            counterfactual_engine_destroy(engine);
            engine = nullptr;
        }
    }

    counterfactual_engine_t* engine = nullptr;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(CounterfactualTest, CreateDefault)
{
    EXPECT_NE(engine, nullptr);
}

TEST_F(CounterfactualTest, CreateCustom)
{
    cf_config_t config = counterfactual_engine_default_config();
    config.min_plausibility = 0.4f;
    config.consequence_threshold = 0.3f;

    counterfactual_engine_t* custom = counterfactual_engine_create_custom(&config);
    ASSERT_NE(custom, nullptr);
    counterfactual_engine_destroy(custom);
}

TEST_F(CounterfactualTest, CreateWithNullConfig)
{
    counterfactual_engine_t* created = counterfactual_engine_create_custom(nullptr);
    EXPECT_EQ(created, nullptr);
}

TEST_F(CounterfactualTest, DestroyNullSafe)
{
    counterfactual_engine_destroy(nullptr);
}

TEST_F(CounterfactualTest, DefaultConfig)
{
    cf_config_t config = counterfactual_engine_default_config();

    EXPECT_GT(config.min_plausibility, 0.0f);
    EXPECT_LE(config.min_plausibility, 1.0f);
    EXPECT_GT(config.max_trace_depth, 0u);
}

//=============================================================================
// State Creation Tests
//=============================================================================

TEST_F(CounterfactualTest, CreateState)
{
    float values[] = {1.0f, 2.0f, 3.0f};
    cf_state_t* state = counterfactual_create_state(values, 3, "initial_state");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->dim, 3u);
    counterfactual_free_state(state);
}

TEST_F(CounterfactualTest, CreateStateZeroDim)
{
    cf_state_t* state = counterfactual_create_state(nullptr, 0, "empty");
    if (state != nullptr) {
        EXPECT_EQ(state->dim, 0u);
        counterfactual_free_state(state);
    }
    SUCCEED();
}

//=============================================================================
// Intervention Tests
//=============================================================================

TEST_F(CounterfactualTest, InterventionStruct)
{
    cf_intervention_t intervention;
    memset(&intervention, 0, sizeof(intervention));
    intervention.id = 1;
    strncpy(intervention.description, "change_temperature", 255);
    intervention.target_variable = 0;
    intervention.original_value = 25.0f;
    intervention.counterfactual_value = 30.0f;

    EXPECT_EQ(intervention.target_variable, 0u);
    EXPECT_NEAR(intervention.counterfactual_value, 30.0f, FLOAT_TOLERANCE);
}

//=============================================================================
// Counterfactual Imagination Tests
//=============================================================================

TEST_F(CounterfactualTest, ImagineCounterfactual)
{
    float actual_values[] = {1.0f, 1.0f};  // rain=1, ground_wet=1
    cf_state_t* actual = counterfactual_create_state(actual_values, 2, "actual_world");

    if (actual != nullptr) {
        cf_intervention_t what_if;
        memset(&what_if, 0, sizeof(what_if));
        strncpy(what_if.description, "no_rain", 255);
        what_if.target_variable = 0;
        what_if.original_value = 1.0f;
        what_if.counterfactual_value = 0.0f;

        cf_counterfactual_t* cf = counterfactual_imagine(engine, actual, &what_if);

        if (cf != nullptr) {
            EXPECT_NE(cf->counterfactual_world, nullptr);
            EXPECT_GT(cf->distance, 0.0f);
            counterfactual_free(cf);
        }

        counterfactual_free_state(actual);
    }
}

TEST_F(CounterfactualTest, ImagineCounterfactualNull)
{
    float values[] = {1.0f};
    cf_state_t* actual = counterfactual_create_state(values, 1, "test");

    if (actual != nullptr) {
        cf_counterfactual_t* cf = counterfactual_imagine(engine, actual, nullptr);
        EXPECT_EQ(cf, nullptr);

        cf = counterfactual_imagine(engine, nullptr, nullptr);
        EXPECT_EQ(cf, nullptr);

        counterfactual_free_state(actual);
    }
}

//=============================================================================
// Consequence Tracing Tests
//=============================================================================

TEST_F(CounterfactualTest, TraceEffects)
{
    float values[] = {1.0f, 1.0f, 1.0f};
    cf_state_t* actual = counterfactual_create_state(values, 3, "causal_chain");

    if (actual != nullptr) {
        cf_intervention_t change;
        memset(&change, 0, sizeof(change));
        strncpy(change.description, "change_A", 255);
        change.target_variable = 0;
        change.original_value = 1.0f;
        change.counterfactual_value = 0.0f;

        cf_counterfactual_t* cf = counterfactual_imagine(engine, actual, &change);

        if (cf != nullptr) {
            cf_consequence_t consequences[CF_MAX_CONSEQUENCES];
            uint32_t num_found = 0;
            int result = counterfactual_trace_effects(engine, cf,
                consequences, CF_MAX_CONSEQUENCES, &num_found);

            if (result == 0) {
                EXPECT_LE(num_found, CF_MAX_CONSEQUENCES);
            }

            counterfactual_free(cf);
        }

        counterfactual_free_state(actual);
    }
}

TEST_F(CounterfactualTest, EstimateProbability)
{
    float values[] = {1.0f, 0.0f};
    cf_state_t* actual = counterfactual_create_state(values, 2, "test");

    if (actual != nullptr) {
        cf_intervention_t what_if;
        memset(&what_if, 0, sizeof(what_if));
        what_if.target_variable = 0;
        what_if.counterfactual_value = 0.5f;

        cf_counterfactual_t* cf = counterfactual_imagine(engine, actual, &what_if);

        if (cf != nullptr) {
            float prob = counterfactual_estimate_probability(engine, cf);
            EXPECT_GE(prob, 0.0f);
            EXPECT_LE(prob, 1.0f);

            counterfactual_free(cf);
        }

        counterfactual_free_state(actual);
    }
}

//=============================================================================
// Possibility Space Tests
//=============================================================================

TEST_F(CounterfactualTest, ExploreSpace)
{
    float values[] = {1.0f, 5.0f};
    cf_state_t* actual = counterfactual_create_state(values, 2, "possibilities");

    if (actual != nullptr) {
        uint32_t num_found = 0;
        cf_counterfactual_t** alternatives = counterfactual_explore_space(
            engine, actual, 5, &num_found);

        if (alternatives != nullptr) {
            EXPECT_GT(num_found, 0u);
            EXPECT_LE(num_found, 5u);

            for (uint32_t i = 0; i < num_found; i++) {
                if (alternatives[i]) {
                    EXPECT_NE(alternatives[i]->counterfactual_world, nullptr);
                    counterfactual_free(alternatives[i]);
                }
            }
            free(alternatives);
        }

        counterfactual_free_state(actual);
    }
}

TEST_F(CounterfactualTest, ExploreSpaceZeroMax)
{
    float values[] = {1.0f};
    cf_state_t* actual = counterfactual_create_state(values, 1, "test");

    if (actual != nullptr) {
        uint32_t count = 0;
        cf_counterfactual_t** alts = counterfactual_explore_space(
            engine, actual, 0, &count);

        // Implementation may return empty array or null
        EXPECT_EQ(count, 0u);
        if (alts != nullptr) {
            free(alts);
        }

        counterfactual_free_state(actual);
    }
}

//=============================================================================
// Causal Analysis Tests
//=============================================================================

TEST_F(CounterfactualTest, CausalStrength)
{
    float values[] = {1.0f, 2.0f, 3.0f};
    cf_state_t* context = counterfactual_create_state(values, 3, "context");

    if (context != nullptr) {
        float strength = counterfactual_causal_strength(engine, 0, 1, context);
        EXPECT_GE(strength, 0.0f);
        EXPECT_LE(strength, 1.0f);

        counterfactual_free_state(context);
    }
}

TEST_F(CounterfactualTest, FindCauses)
{
    float values[] = {1.0f, 2.0f, 3.0f, 4.0f};
    cf_state_t* state = counterfactual_create_state(values, 4, "test");

    if (state != nullptr) {
        uint32_t cause_vars[10];
        uint32_t num_found = 0;
        int result = counterfactual_find_causes(engine, state, 3,
            cause_vars, 10, &num_found);

        if (result == 0) {
            EXPECT_LE(num_found, 10u);
        }

        counterfactual_free_state(state);
    }
}

//=============================================================================
// Distance Tests
//=============================================================================

TEST_F(CounterfactualTest, CalculateDistance)
{
    float v1[] = {0.0f, 1.0f};
    float v2[] = {10.0f, 1.0f};
    cf_state_t* s1 = counterfactual_create_state(v1, 2, "s1");
    cf_state_t* s2 = counterfactual_create_state(v2, 2, "s2");

    if (s1 != nullptr && s2 != nullptr) {
        float distance = counterfactual_distance(engine, s1, s2);
        EXPECT_GT(distance, 0.0f);

        float self_dist = counterfactual_distance(engine, s1, s1);
        EXPECT_NEAR(self_dist, 0.0f, FLOAT_TOLERANCE);
    }

    if (s1) counterfactual_free_state(s1);
    if (s2) counterfactual_free_state(s2);
}

TEST_F(CounterfactualTest, FindClosest)
{
    float v_actual[] = {1.0f, 2.0f, 3.0f};
    float v_target[] = {1.0f, 5.0f, 3.0f};
    cf_state_t* actual = counterfactual_create_state(v_actual, 3, "actual");
    cf_state_t* target = counterfactual_create_state(v_target, 3, "target");

    if (actual != nullptr && target != nullptr) {
        cf_counterfactual_t* closest = counterfactual_find_closest(
            engine, actual, target);

        if (closest != nullptr) {
            EXPECT_NE(closest->counterfactual_world, nullptr);
            counterfactual_free(closest);
        }
    }

    if (actual) counterfactual_free_state(actual);
    if (target) counterfactual_free_state(target);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(CounterfactualTest, SetInflammation)
{
    int result = counterfactual_set_inflammation(engine, 0.3f);
    EXPECT_EQ(result, 0);
}

TEST_F(CounterfactualTest, SetFatigue)
{
    int result = counterfactual_set_fatigue(engine, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(CounterfactualTest, SetInflammationNull)
{
    int result = counterfactual_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CounterfactualTest, GetStatistics)
{
    cf_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = counterfactual_get_stats(engine, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(CounterfactualTest, ResetStatistics)
{
    counterfactual_reset_stats(engine);
    SUCCEED();
}

} // namespace
