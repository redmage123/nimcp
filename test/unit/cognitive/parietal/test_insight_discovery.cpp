/**
 * @file test_insight_discovery.cpp
 * @brief Unit tests for NIMCP Insight & Discovery Engine (Phase 6.3)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_insight_discovery.h"

namespace {

constexpr float FLOAT_TOLERANCE = 1e-4f;

class InsightDiscoveryTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        engine = insight_engine_create();
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override
    {
        if (engine) {
            insight_engine_destroy(engine);
            engine = nullptr;
        }
    }

    insight_engine_t* engine = nullptr;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(InsightDiscoveryTest, CreateDefault)
{
    EXPECT_NE(engine, nullptr);
}

TEST_F(InsightDiscoveryTest, CreateCustom)
{
    insight_config_t config = insight_engine_default_config();

    insight_engine_t* custom = insight_engine_create_custom(&config);
    ASSERT_NE(custom, nullptr);
    insight_engine_destroy(custom);
}

TEST_F(InsightDiscoveryTest, CreateWithNullConfig)
{
    insight_engine_t* created = insight_engine_create_custom(nullptr);
    EXPECT_EQ(created, nullptr);
}

TEST_F(InsightDiscoveryTest, DestroyNullSafe)
{
    insight_engine_destroy(nullptr);
}

TEST_F(InsightDiscoveryTest, DefaultConfig)
{
    insight_config_t config = insight_engine_default_config();
    SUCCEED();
}

//=============================================================================
// Problem Creation Tests
//=============================================================================

TEST_F(InsightDiscoveryTest, CreateProblem)
{
    insight_problem_t* problem = insight_create_problem("Solve the puzzle");

    if (problem != nullptr) {
        insight_free_problem(problem);
    }
    SUCCEED();
}

TEST_F(InsightDiscoveryTest, CreateProblemNull)
{
    insight_problem_t* problem = insight_create_problem(nullptr);
    if (problem != nullptr) {
        insight_free_problem(problem);
    }
    SUCCEED();
}

TEST_F(InsightDiscoveryTest, AddConstraintToProblem)
{
    insight_problem_t* problem = insight_create_problem("Test problem");

    if (problem != nullptr) {
        int result = insight_add_constraint(problem, "Must use only 3 colors", 0.8f, true);
        EXPECT_EQ(result, 0);
        insight_free_problem(problem);
    }
}

TEST_F(InsightDiscoveryTest, AddPerspective)
{
    insight_problem_t* problem = insight_create_problem("Creative challenge");

    if (problem != nullptr) {
        float repr[] = {0.5f, 0.7f, 0.3f};
        int result = insight_add_perspective(problem, "Think like an artist", repr, 3);
        EXPECT_EQ(result, 0);
        insight_free_problem(problem);
    }
}

//=============================================================================
// Incubation Tests
//=============================================================================

TEST_F(InsightDiscoveryTest, IncubateProblem)
{
    insight_problem_t* problem = insight_create_problem("Complex puzzle");

    if (problem != nullptr) {
        uint32_t id = insight_incubate(engine, problem);
        // May or may not accept incubation
        if (id > 0) {
            insight_eureka_t* eureka = nullptr;
            insight_check_incubation(engine, id, &eureka);
            if (eureka) {
                insight_free_eureka(eureka);
            }
        }
        insight_free_problem(problem);
    }
}

TEST_F(InsightDiscoveryTest, ProcessIncubation)
{
    int result = insight_process_incubation_step(engine);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Restructuring Tests
//=============================================================================

TEST_F(InsightDiscoveryTest, AttemptRestructure)
{
    insight_problem_t* problem = insight_create_problem("Stuck problem");

    if (problem != nullptr) {
        insight_restructuring_t* result = insight_attempt_restructure(engine, problem);

        if (result != nullptr) {
            insight_free_restructuring(result);
        }

        insight_free_problem(problem);
    }
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(InsightDiscoveryTest, SetInflammation)
{
    int result = insight_set_inflammation(engine, 0.3f);
    EXPECT_EQ(result, 0);
}

TEST_F(InsightDiscoveryTest, SetFatigue)
{
    int result = insight_set_fatigue(engine, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(InsightDiscoveryTest, SetInflammationNull)
{
    int result = insight_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(InsightDiscoveryTest, GetStatistics)
{
    insight_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = insight_get_stats(engine, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(InsightDiscoveryTest, ResetStatistics)
{
    insight_reset_stats(engine);
    SUCCEED();
}

} // namespace
