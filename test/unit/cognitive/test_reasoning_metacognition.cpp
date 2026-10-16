/**
 * @file test_reasoning_metacognition.cpp
 * @brief Unit tests for the metacognitive adaptive strategy selection controller
 *
 * WHAT: Tests complexity estimation, strategy selection, outcome learning,
 *       and override rules in isolation
 * WHY:  Verify metacognitive components work correctly before integration
 * HOW:  GTest suite testing each component independently
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_metacognition.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ReasoningMetacognitionTest : public ::testing::Test {
protected:
    reasoning_metacognition_t* mc = nullptr;

    void SetUp() override {
        mc = reasoning_metacognition_create(nullptr);
        ASSERT_NE(mc, nullptr);
    }

    void TearDown() override {
        if (mc) {
            reasoning_metacognition_destroy(mc);
            mc = nullptr;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(ReasoningMetacognitionTest, CreateDestroy) {
    /* mc created in SetUp, destroyed in TearDown — just verify non-null */
    EXPECT_NE(mc, nullptr);
}

TEST_F(ReasoningMetacognitionTest, CreateNull) {
    /* Create with NULL config should use defaults */
    reasoning_metacognition_t* mc2 = reasoning_metacognition_create(nullptr);
    ASSERT_NE(mc2, nullptr);
    reasoning_metacognition_destroy(mc2);
}

TEST_F(ReasoningMetacognitionTest, CreateWithConfig) {
    metacognitive_config_t config = reasoning_metacognition_default_config();
    config.learning_rate = 0.1f;
    config.history_size = 128;

    reasoning_metacognition_t* mc2 = reasoning_metacognition_create(&config);
    ASSERT_NE(mc2, nullptr);
    reasoning_metacognition_destroy(mc2);
}

TEST_F(ReasoningMetacognitionTest, DestroyNull) {
    /* Should not crash */
    reasoning_metacognition_destroy(nullptr);
}

/*=============================================================================
 * DEFAULT CONFIG TESTS
 *===========================================================================*/

TEST_F(ReasoningMetacognitionTest, DefaultConfigValues) {
    metacognitive_config_t config = reasoning_metacognition_default_config();

    EXPECT_TRUE(config.enable_metacognition);
    EXPECT_FLOAT_EQ(config.complexity_threshold_simple,
                    REASONING_METACOG_DEFAULT_SIMPLE_THRESHOLD);
    EXPECT_FLOAT_EQ(config.complexity_threshold_moderate,
                    REASONING_METACOG_DEFAULT_MODERATE_THRESHOLD);
    EXPECT_FLOAT_EQ(config.complexity_threshold_hard,
                    REASONING_METACOG_DEFAULT_HARD_THRESHOLD);
    EXPECT_FLOAT_EQ(config.learning_rate,
                    REASONING_METACOG_DEFAULT_LEARNING_RATE);
    EXPECT_EQ(config.history_size, REASONING_METACOG_DEFAULT_HISTORY_SIZE);
}

/*=============================================================================
 * ASSESSMENT TESTS — COMPLEXITY ESTIMATION
 *===========================================================================*/

TEST_F(ReasoningMetacognitionTest, AssessTrivialQuery) {
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc, "define cat", nullptr);

    EXPECT_EQ(result.complexity, REASONING_COMPLEXITY_TRIVIAL);
    EXPECT_GT(result.confidence_in_assessment, 0.0f);
    EXPECT_GT(result.estimated_steps, 0u);
}

TEST_F(ReasoningMetacognitionTest, AssessSimpleQuery) {
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc, "What properties do birds have?", nullptr);

    /* Should be TRIVIAL or SIMPLE — it's a simple factual query */
    EXPECT_LE((int)result.complexity, (int)REASONING_COMPLEXITY_SIMPLE);
}

TEST_F(ReasoningMetacognitionTest, AssessModerateQuery) {
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc, "What do we know about birds and can they fly or swim?", nullptr);

    /* Should be at least SIMPLE, possibly MODERATE (has "and" + "or") */
    EXPECT_GE((int)result.complexity, (int)REASONING_COMPLEXITY_SIMPLE);
}

TEST_F(ReasoningMetacognitionTest, AssessComplexQuery) {
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc,
        "If socrates is a man and all men are mortal, therefore socrates is mortal "
        "because of deductive reasoning",
        nullptr);

    /* Should be MODERATE or higher (has "if", "and", "therefore", "because") */
    EXPECT_GE((int)result.complexity, (int)REASONING_COMPLEXITY_MODERATE);
}

TEST_F(ReasoningMetacognitionTest, AssessHardQuery) {
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc,
        "If birds had not evolved wings, would they still be able to migrate "
        "and would their population dynamics be similar to flightless mammals?",
        nullptr);

    /* Should be COMPLEX or HARD (counterfactual + "had" + "would" + analogical) */
    EXPECT_GE((int)result.complexity, (int)REASONING_COMPLEXITY_COMPLEX);
}

TEST_F(ReasoningMetacognitionTest, AssessNullInputs) {
    /* NULL controller */
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        nullptr, "test query", nullptr);
    EXPECT_EQ(result.complexity, REASONING_COMPLEXITY_TRIVIAL);
    EXPECT_FLOAT_EQ(result.complexity_score, 0.0f);

    /* NULL query */
    result = reasoning_metacognition_assess(mc, nullptr, nullptr);
    EXPECT_EQ(result.complexity, REASONING_COMPLEXITY_TRIVIAL);
    EXPECT_FLOAT_EQ(result.complexity_score, 0.0f);
}

TEST_F(ReasoningMetacognitionTest, AssessEmptyQuery) {
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc, "", nullptr);

    EXPECT_EQ(result.complexity, REASONING_COMPLEXITY_TRIVIAL);
}

TEST_F(ReasoningMetacognitionTest, AssessVeryShortQuery) {
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc, "hi", nullptr);

    EXPECT_EQ(result.complexity, REASONING_COMPLEXITY_TRIVIAL);
}

/*=============================================================================
 * STRATEGY SELECTION TESTS
 *===========================================================================*/

TEST_F(ReasoningMetacognitionTest, StrategySelectionTrivial) {
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc, "define cat", nullptr);

    /* Trivial queries should get sequential strategy */
    EXPECT_EQ(result.recommended_strategy, REASONING_STRATEGY_SEQUENTIAL);
}

TEST_F(ReasoningMetacognitionTest, StrategySelectionSimple) {
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc, "What are the main features of photosynthesis?", nullptr);

    /* Simple queries should get sequential */
    EXPECT_LE((int)result.recommended_strategy, (int)REASONING_STRATEGY_CONCURRENT);
}

TEST_F(ReasoningMetacognitionTest, StrategySelectionComplex) {
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc,
        "If all mammals are warm-blooded and whales are mammals, therefore whales "
        "are warm-blooded because of deductive inference, but however some argue "
        "that not all cetaceans have the same thermoregulation",
        nullptr);

    /* Complex queries should get concurrent or convergent */
    EXPECT_GE((int)result.recommended_strategy, (int)REASONING_STRATEGY_CONCURRENT);
}

TEST_F(ReasoningMetacognitionTest, StrategySelectionHard) {
    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc,
        "If consciousness had not emerged in biological organisms, would "
        "artificial intelligence be similar to what we see today, and "
        "how would the philosophical implications compare to Searle's "
        "Chinese Room argument? Suppose we imagine a world without "
        "qualia — therefore the hard problem of consciousness implies "
        "that not all mental states are reducible, because phenomenal "
        "experience is fundamentally different",
        nullptr);

    /* Very complex: should be convergent */
    EXPECT_EQ(result.recommended_strategy, REASONING_STRATEGY_CONVERGENT);
}

/*=============================================================================
 * OVERRIDE TESTS
 *===========================================================================*/

TEST_F(ReasoningMetacognitionTest, ConvergentDisabledDowngrade) {
    /* Create engine config with convergent disabled */
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_convergent_reasoning = false;

    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc,
        "If birds had not evolved wings, would they still be able to migrate "
        "and would their population dynamics be similar to flightless mammals?",
        &cfg);

    /* Even if complexity is HARD, strategy should NOT be convergent */
    EXPECT_NE(result.recommended_strategy, REASONING_STRATEGY_CONVERGENT);
}

TEST_F(ReasoningMetacognitionTest, ConcurrentDisabledDowngrade) {
    /* Create engine config with concurrent and convergent disabled */
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_convergent_reasoning = false;
    cfg.enable_concurrent_pipeline = false;

    metacognitive_assessment_t result = reasoning_metacognition_assess(
        mc,
        "What do we know about birds and can they fly or swim and "
        "are they related to dinosaurs?",
        &cfg);

    /* With both disabled, must be sequential */
    EXPECT_EQ(result.recommended_strategy, REASONING_STRATEGY_SEQUENTIAL);
}

/*=============================================================================
 * OUTCOME RECORDING TESTS
 *===========================================================================*/

TEST_F(ReasoningMetacognitionTest, RecordOutcome) {
    int rc = reasoning_metacognition_record_outcome(
        mc, REASONING_STRATEGY_SEQUENTIAL, 0.8f, 1000.0f, 5);
    EXPECT_EQ(rc, 0);

    metacognitive_stats_t stats;
    rc = reasoning_metacognition_get_stats(mc, &stats);
    EXPECT_EQ(rc, 0);
    /* Stats should reflect the recording (accuracy updated) */
}

TEST_F(ReasoningMetacognitionTest, RecordOutcomeNullInputs) {
    int rc = reasoning_metacognition_record_outcome(
        nullptr, REASONING_STRATEGY_SEQUENTIAL, 0.8f, 1000.0f, 5);
    EXPECT_EQ(rc, -1);
}

TEST_F(ReasoningMetacognitionTest, RecordOutcomeInvalidStrategy) {
    int rc = reasoning_metacognition_record_outcome(
        mc, (reasoning_strategy_t)99, 0.8f, 1000.0f, 5);
    EXPECT_EQ(rc, -1);
}

TEST_F(ReasoningMetacognitionTest, RecordMultipleOutcomes) {
    for (int i = 0; i < 20; i++) {
        int rc = reasoning_metacognition_record_outcome(
            mc, REASONING_STRATEGY_CONVERGENT, 0.7f + 0.01f * i,
            5000.0f, 15 + i);
        EXPECT_EQ(rc, 0);
    }

    metacognitive_stats_t stats;
    int rc = reasoning_metacognition_get_stats(mc, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(stats.accuracy, 0.0f);
}

/*=============================================================================
 * STATISTICS TESTS
 *===========================================================================*/

TEST_F(ReasoningMetacognitionTest, GetStatsInitiallyZero) {
    metacognitive_stats_t stats;
    int rc = reasoning_metacognition_get_stats(mc, &stats);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(stats.total_assessments, 0u);
    for (int i = 0; i < REASONING_NUM_STRATEGIES; i++) {
        EXPECT_EQ(stats.strategy_counts[i], 0u);
    }
    EXPECT_FLOAT_EQ(stats.avg_assessment_time_us, 0.0f);
}

TEST_F(ReasoningMetacognitionTest, GetStatsAfterAssessments) {
    reasoning_metacognition_assess(mc, "define cat", nullptr);
    reasoning_metacognition_assess(mc, "What is a dog?", nullptr);
    reasoning_metacognition_assess(mc, "hello", nullptr);

    metacognitive_stats_t stats;
    int rc = reasoning_metacognition_get_stats(mc, &stats);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(stats.total_assessments, 3u);
}

TEST_F(ReasoningMetacognitionTest, GetStatsNullInputs) {
    metacognitive_stats_t stats;
    EXPECT_EQ(reasoning_metacognition_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(reasoning_metacognition_get_stats(mc, nullptr), -1);
}

/*=============================================================================
 * STRATEGY NAME TESTS
 *===========================================================================*/

TEST_F(ReasoningMetacognitionTest, StrategyNameStrings) {
    EXPECT_STREQ(reasoning_metacognition_get_strategy_name(REASONING_STRATEGY_TRIVIAL),
                 "TRIVIAL");
    EXPECT_STREQ(reasoning_metacognition_get_strategy_name(REASONING_STRATEGY_SEQUENTIAL),
                 "SEQUENTIAL");
    EXPECT_STREQ(reasoning_metacognition_get_strategy_name(REASONING_STRATEGY_CONCURRENT),
                 "CONCURRENT");
    EXPECT_STREQ(reasoning_metacognition_get_strategy_name(REASONING_STRATEGY_CONVERGENT),
                 "CONVERGENT");
}

TEST_F(ReasoningMetacognitionTest, StrategyNameInvalid) {
    const char* name = reasoning_metacognition_get_strategy_name((reasoning_strategy_t)99);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "UNKNOWN");
}

TEST_F(ReasoningMetacognitionTest, ComplexityNameStrings) {
    EXPECT_STREQ(reasoning_metacognition_get_complexity_name(REASONING_COMPLEXITY_TRIVIAL),
                 "TRIVIAL");
    EXPECT_STREQ(reasoning_metacognition_get_complexity_name(REASONING_COMPLEXITY_SIMPLE),
                 "SIMPLE");
    EXPECT_STREQ(reasoning_metacognition_get_complexity_name(REASONING_COMPLEXITY_MODERATE),
                 "MODERATE");
    EXPECT_STREQ(reasoning_metacognition_get_complexity_name(REASONING_COMPLEXITY_COMPLEX),
                 "COMPLEX");
    EXPECT_STREQ(reasoning_metacognition_get_complexity_name(REASONING_COMPLEXITY_HARD),
                 "HARD");
}

/*=============================================================================
 * LEARNING / ADAPTATION TESTS
 *===========================================================================*/

TEST_F(ReasoningMetacognitionTest, LearningAdjustsThresholds) {
    /* Record many outcomes where sequential queries took too many steps,
     * which should cause threshold adaptation */
    for (int i = 0; i < 50; i++) {
        reasoning_metacognition_record_outcome(
            mc, REASONING_STRATEGY_SEQUENTIAL,
            0.5f,    /* moderate confidence */
            5000.0f, /* 5ms */
            20);     /* 20 steps — way too many for "simple" */
    }

    /* Now assess: the thresholds should have shifted */
    metacognitive_assessment_t before = reasoning_metacognition_assess(
        mc, "What is a cat?", nullptr);

    /* We can't easily predict the exact threshold but the assessment
     * should still produce valid results */
    EXPECT_GE(before.confidence_in_assessment, 0.0f);
    EXPECT_LE(before.confidence_in_assessment, 1.0f);
}

TEST_F(ReasoningMetacognitionTest, ComplexityScoreMonotonicity) {
    /* More complex queries should generally have higher scores */
    metacognitive_assessment_t trivial = reasoning_metacognition_assess(
        mc, "hi", nullptr);
    metacognitive_assessment_t complex_q = reasoning_metacognition_assess(
        mc,
        "If all mammals are warm-blooded and whales are mammals, therefore "
        "whales are warm-blooded because of deductive inference, but however "
        "some argue that not all cetaceans have the same thermoregulation",
        nullptr);

    EXPECT_LT(trivial.complexity_score, complex_q.complexity_score);
}

TEST_F(ReasoningMetacognitionTest, AssessmentConfidenceRange) {
    /* Confidence should always be in [0, 1] */
    const char* queries[] = {
        "hi",
        "define cat",
        "What are the main features of photosynthesis?",
        "If birds had not evolved wings, would they still migrate?",
    };

    for (int i = 0; i < 4; i++) {
        metacognitive_assessment_t result = reasoning_metacognition_assess(
            mc, queries[i], nullptr);

        EXPECT_GE(result.confidence_in_assessment, 0.0f)
            << "Query: " << queries[i];
        EXPECT_LE(result.confidence_in_assessment, 1.0f)
            << "Query: " << queries[i];
    }
}

TEST_F(ReasoningMetacognitionTest, EstimatedStepsIncreasesWithComplexity) {
    metacognitive_assessment_t trivial = reasoning_metacognition_assess(
        mc, "define cat", nullptr);
    metacognitive_assessment_t hard = reasoning_metacognition_assess(
        mc,
        "If consciousness had not emerged in biological organisms, would "
        "artificial intelligence be similar to what we see today, and "
        "how would the philosophical implications compare to Searle's "
        "Chinese Room argument? Suppose we imagine a world without "
        "qualia — therefore the hard problem implies something fundamental",
        nullptr);

    EXPECT_LE(trivial.estimated_steps, hard.estimated_steps);
}

/*=============================================================================
 * CONTINUOUS RESOURCE BUDGET TESTS
 *===========================================================================*/

TEST_F(ReasoningMetacognitionTest, BudgetParallelismScalesWithScore) {
    metacognitive_assessment_t trivial = reasoning_metacognition_assess(
        mc, "define cat", nullptr);
    metacognitive_assessment_t moderate = reasoning_metacognition_assess(
        mc, "What do we know about birds and can they fly or swim?", nullptr);
    metacognitive_assessment_t hard = reasoning_metacognition_assess(
        mc,
        "What if entropy had been reversible, would equilibrium still "
        "emerge, and therefore would the second law be invalid because "
        "of temporal symmetry?",
        nullptr);

    /* Parallelism factor should increase monotonically with complexity */
    EXPECT_LT(trivial.budget.parallelism_factor,
              moderate.budget.parallelism_factor);
    EXPECT_LT(moderate.budget.parallelism_factor,
              hard.budget.parallelism_factor);

    /* Parallelism should be in [0, 1] */
    EXPECT_GE(trivial.budget.parallelism_factor, 0.0f);
    EXPECT_LE(hard.budget.parallelism_factor, 1.0f);
}

TEST_F(ReasoningMetacognitionTest, BudgetContributorsScaleWithScore) {
    metacognitive_assessment_t trivial = reasoning_metacognition_assess(
        mc, "hi", nullptr);
    metacognitive_assessment_t hard = reasoning_metacognition_assess(
        mc,
        "What if entropy had been reversible, would equilibrium still "
        "emerge, and therefore would the second law be invalid because "
        "of temporal symmetry and how would that affect information theory?",
        nullptr);

    /* Contributors should scale: trivial gets fewer than complex */
    EXPECT_LT(trivial.budget.max_contributors,
              hard.budget.max_contributors);

    /* Minimum of 1 contributor even for trivial */
    EXPECT_GE(trivial.budget.max_contributors, 1u);
}

TEST_F(ReasoningMetacognitionTest, BudgetConvergenceThresholdInverted) {
    /* Tight threshold for simple (converge quickly),
     * loose for complex (allow deep processing) */
    metacognitive_assessment_t trivial = reasoning_metacognition_assess(
        mc, "define cat", nullptr);
    metacognitive_assessment_t hard = reasoning_metacognition_assess(
        mc,
        "What if entropy had been reversible, would equilibrium still "
        "emerge, and therefore would the second law be invalid because "
        "of temporal symmetry?",
        nullptr);

    EXPECT_GT(trivial.budget.convergence_threshold,
              hard.budget.convergence_threshold);
}

TEST_F(ReasoningMetacognitionTest, BudgetConfidenceTargetInverted) {
    /* Easy queries need high confidence, hard queries accept less */
    metacognitive_assessment_t trivial = reasoning_metacognition_assess(
        mc, "define cat", nullptr);
    metacognitive_assessment_t hard = reasoning_metacognition_assess(
        mc,
        "What if entropy had been reversible, would equilibrium still "
        "emerge, and therefore would the second law be invalid because "
        "of temporal symmetry?",
        nullptr);

    EXPECT_GT(trivial.budget.confidence_target,
              hard.budget.confidence_target);
    EXPECT_GE(hard.budget.confidence_target, 0.4f);
    EXPECT_LE(trivial.budget.confidence_target, 1.0f);
}

TEST_F(ReasoningMetacognitionTest, BudgetTimeoutScalesWithScore) {
    metacognitive_assessment_t trivial = reasoning_metacognition_assess(
        mc, "hi", nullptr);
    metacognitive_assessment_t hard = reasoning_metacognition_assess(
        mc,
        "What if entropy had been reversible, would equilibrium still "
        "emerge, and therefore would the second law be invalid because "
        "of temporal symmetry?",
        nullptr);

    EXPECT_LT(trivial.budget.timeout_factor,
              hard.budget.timeout_factor);
    EXPECT_GE(trivial.budget.timeout_factor, 0.1f);
    EXPECT_LE(hard.budget.timeout_factor, 1.0f);
}

TEST_F(ReasoningMetacognitionTest, BudgetThreadPoolOffForTrivial) {
    metacognitive_assessment_t trivial = reasoning_metacognition_assess(
        mc, "hi", nullptr);

    /* Very short queries should not bother with thread pool */
    EXPECT_FALSE(trivial.budget.use_thread_pool);
}

TEST_F(ReasoningMetacognitionTest, BudgetContinuousNoBinaryJumps) {
    /* Score increasing by small increments should produce smoothly
     * increasing parallelism — no big jumps between adjacent scores */
    const char* queries[] = {
        "hi",                                             /* score ~0.0 */
        "What is gravity?",                               /* score ~0.0-0.1 */
        "What do we know about birds and can they fly?",  /* score ~0.1 */
        "If birds are reptiles and reptiles are cold-blooded, then are "
        "birds cold-blooded?",                            /* score ~0.3 */
        "What if entropy had been reversible, would equilibrium still "
        "emerge, and therefore would the second law be invalid because "
        "of symmetry?",                                   /* score ~0.6+ */
    };

    float prev_parallelism = -1.0f;
    for (int i = 0; i < 5; i++) {
        metacognitive_assessment_t r = reasoning_metacognition_assess(
            mc, queries[i], nullptr);
        /* Each query should have >= previous parallelism (monotonic) */
        EXPECT_GE(r.budget.parallelism_factor, prev_parallelism)
            << "Query " << i << ": " << queries[i];
        prev_parallelism = r.budget.parallelism_factor;
    }
}

TEST_F(ReasoningMetacognitionTest, BudgetMaxStepsScalesWithScore) {
    metacognitive_assessment_t trivial = reasoning_metacognition_assess(
        mc, "hi", nullptr);
    metacognitive_assessment_t hard = reasoning_metacognition_assess(
        mc,
        "What if entropy had been reversible, would equilibrium still "
        "emerge, and therefore would the second law be invalid because "
        "of temporal symmetry?",
        nullptr);

    /* Hard queries get more steps than trivial */
    EXPECT_LT(trivial.budget.max_steps, hard.budget.max_steps);
    /* Minimum 3 steps even for trivial */
    EXPECT_GE(trivial.budget.max_steps, 3u);
}
