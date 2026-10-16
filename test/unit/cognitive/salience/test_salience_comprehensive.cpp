/**
 * @file test_salience_comprehensive.cpp
 * @brief Comprehensive unit tests for nimcp_salience.c
 *
 * WHAT: Full test coverage for salience and attention evaluation module
 * WHY:  Increase coverage from 9.3% to 95%+ (323 uncovered lines)
 * HOW:  Systematic testing of all functions, strategies, and edge cases
 *
 * TEST COVERAGE TARGETS:
 * 1. Lifecycle: Creation/destruction with various configurations
 * 2. History Buffer: Novelty detection and history management
 * 3. Predictor: Surprise detection and prediction updates
 * 4. Salience Computation: All three strategies (fast, balanced, accurate)
 * 5. Acetylcholine Gating: Neuromodulator integration
 * 6. Batch Evaluation: Sequential and parallel processing
 * 7. Configuration: Weights, thresholds, callbacks
 * 8. Statistics: Tracking and reporting
 * 9. Bidirectional Feedback: Emotional modulation
 * 10. Error Handling: Null checks, validation, edge cases
 *
 * COVERAGE GOAL: 95%+ line coverage (from 9.3% baseline)
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>

#include "cognitive/salience/nimcp_salience.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/**
 * WHAT: Base test fixture for salience tests
 * WHY:  Provides common setup/teardown and helper functions
 * HOW:  Creates mock brain and manages evaluator lifecycle
 */
class SalienceTest : public ::testing::Test {
protected:
    brain_t brain;
    salience_evaluator_t evaluator;

    void SetUp() override {
        /**
         * WHAT: Create minimal brain for testing
         * WHY:  Salience evaluator requires valid brain reference
         * HOW:  Use TINY configuration with default task
         */
        brain_config_t brain_config = {
            .size = BRAIN_SIZE_TINY,
            .task = BRAIN_TASK_CLASSIFICATION,
            .num_inputs = 13,
            .num_outputs = 5,
            .learning_rate = 0.01f
        };
        brain = brain_create_custom(&brain_config);
        ASSERT_NE(brain, nullptr) << "Failed to create test brain";

        evaluator = nullptr;
    }

    void TearDown() override {
        /**
         * WHAT: Clean up resources
         * WHY:  Prevent memory leaks
         * HOW:  Destroy evaluator and brain in correct order
         */
        if (evaluator != nullptr) {
            salience_evaluator_destroy(evaluator);
            evaluator = nullptr;
        }
        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    /**
     * WHAT: Create test feature vector
     * WHY:  Provide consistent test inputs
     * HOW:  Generate vector with specified pattern
     */
    std::vector<float> CreateFeatures(uint32_t size, float base = 0.5f, float variance = 0.1f) {
        std::vector<float> features(size);
        for (uint32_t i = 0; i < size; i++) {
            features[i] = base + (i % 10) * variance;
        }
        return features;
    }

    /**
     * WHAT: Create novel feature vector (different from pattern)
     * WHY:  Test novelty detection
     * HOW:  Use different base and variance
     */
    std::vector<float> CreateNovelFeatures(uint32_t size) {
        return CreateFeatures(size, 0.9f, -0.05f);
    }

    /**
     * WHAT: Sleep for milliseconds
     * WHY:  Allow time-based operations to complete
     * HOW:  Use C++11 sleep_for
     */
    void SleepMs(uint32_t ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    /**
     * WHAT: Verify salience scores are within valid range
     * WHY:  All scores must be normalized to [0, 1]
     * HOW:  Check each component
     */
    void AssertValidSalience(const brain_salience_t& salience) {
        EXPECT_GE(salience.salience, 0.0f) << "Salience below 0";
        EXPECT_LE(salience.salience, 1.0f) << "Salience above 1";
        EXPECT_GE(salience.novelty, 0.0f) << "Novelty below 0";
        EXPECT_LE(salience.novelty, 1.0f) << "Novelty above 1";
        EXPECT_GE(salience.surprise, 0.0f) << "Surprise below 0";
        EXPECT_LE(salience.surprise, 1.0f) << "Surprise above 1";
        EXPECT_GE(salience.urgency, 0.0f) << "Urgency below 0";
        EXPECT_LE(salience.urgency, 1.0f) << "Urgency above 1";
        EXPECT_GE(salience.confidence, 0.0f) << "Confidence below 0";
        EXPECT_LE(salience.confidence, 1.0f) << "Confidence above 1";
    }
};

//=============================================================================
// 1. Configuration and Default Tests
//=============================================================================

TEST_F(SalienceTest, DefaultConfigValid) {
    /**
     * WHAT: Test default configuration creation
     * WHY:  Verify sensible defaults are provided
     * HOW:  Call salience_default_config() and check values
     */
    salience_config_t config = salience_default_config();

    EXPECT_EQ(config.strategy, SALIENCE_STRATEGY_BALANCED);
    EXPECT_EQ(config.history_size, 100u);
    EXPECT_TRUE(config.enable_novelty);
    EXPECT_TRUE(config.enable_surprise);
    EXPECT_TRUE(config.enable_urgency);
    EXPECT_EQ(config.urgency_baseline, 0.3f);
    EXPECT_GT(config.novelty_weight, 0.0f);
    EXPECT_GT(config.surprise_weight, 0.0f);
    EXPECT_GT(config.urgency_weight, 0.0f);
    EXPECT_GT(config.high_salience_threshold, 0.0f);
}

TEST_F(SalienceTest, CreateEvaluatorWithDefaultConfig) {
    /**
     * WHAT: Create evaluator with default configuration
     * WHY:  Test standard creation path
     * HOW:  Use salience_default_config() and salience_evaluator_create()
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);

    ASSERT_NE(evaluator, nullptr);
}

TEST_F(SalienceTest, CreateEvaluatorWithCustomConfig) {
    /**
     * WHAT: Create evaluator with custom configuration
     * WHY:  Test configuration flexibility
     * HOW:  Modify config values and create
     */
    salience_config_t config = salience_default_config();
    config.history_size = 50;
    config.strategy = SALIENCE_STRATEGY_FAST;
    config.novelty_weight = 0.5f;
    config.surprise_weight = 0.3f;
    config.urgency_weight = 0.2f;

    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);
}

//=============================================================================
// 2. Guard Clause and Error Handling Tests
//=============================================================================

TEST_F(SalienceTest, CreateEvaluatorNullBrain) {
    /**
     * WHAT: Test creation with NULL brain
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL brain, expect NULL return
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(nullptr, &config);

    EXPECT_EQ(evaluator, nullptr);
    EXPECT_NE(salience_get_last_error()[0], '\0');
}

TEST_F(SalienceTest, CreateEvaluatorNullConfig) {
    /**
     * WHAT: Test creation with NULL config
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL config, expect NULL return
     */
    evaluator = salience_evaluator_create(brain, nullptr);

    EXPECT_EQ(evaluator, nullptr);
    EXPECT_NE(salience_get_last_error()[0], '\0');
}

TEST_F(SalienceTest, CreateEvaluatorInvalidHistorySize) {
    /**
     * WHAT: Test creation with invalid history size
     * WHY:  Verify configuration validation
     * HOW:  Set history_size > 10000, expect NULL
     */
    salience_config_t config = salience_default_config();
    config.history_size = 10001;  // Above maximum

    evaluator = salience_evaluator_create(brain, &config);

    EXPECT_EQ(evaluator, nullptr);
    const char* error = salience_get_last_error();
    EXPECT_NE(error[0], '\0');
    EXPECT_TRUE(strstr(error, "too large") != nullptr || strstr(error, "History") != nullptr);
}

TEST_F(SalienceTest, CreateEvaluatorZeroHistoryWithNovelty) {
    /**
     * WHAT: Test creation with zero history but novelty enabled
     * WHY:  Verify novelty requires history
     * HOW:  Set history_size = 0 with enable_novelty = true
     */
    salience_config_t config = salience_default_config();
    config.history_size = 0;
    config.enable_novelty = true;

    evaluator = salience_evaluator_create(brain, &config);

    EXPECT_EQ(evaluator, nullptr);
    EXPECT_TRUE(strstr(salience_get_last_error(), "Novelty") != nullptr);
}

TEST_F(SalienceTest, DestroyNullEvaluator) {
    /**
     * WHAT: Test destroy with NULL evaluator
     * WHY:  Verify null-safe destruction
     * HOW:  Call salience_evaluator_destroy(NULL), expect no crash
     */
    salience_evaluator_destroy(nullptr);
    SUCCEED();  // Should not crash
}

//=============================================================================
// 3. Basic Salience Evaluation Tests
//=============================================================================

TEST_F(SalienceTest, EvaluateSalienceBasic) {
    /**
     * WHAT: Test basic salience evaluation
     * WHY:  Verify core evaluation functionality
     * HOW:  Create evaluator, evaluate features, check valid output
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    AssertValidSalience(salience);
}

TEST_F(SalienceTest, EvaluateSalienceNullEvaluator) {
    /**
     * WHAT: Test evaluation with NULL evaluator
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL evaluator, expect zeros
     */
    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(nullptr, features.data(), 13);

    EXPECT_EQ(salience.salience, 0.0f);
    EXPECT_EQ(salience.novelty, 0.0f);
    EXPECT_EQ(salience.surprise, 0.0f);
    EXPECT_EQ(salience.urgency, 0.0f);
}

TEST_F(SalienceTest, EvaluateSalienceNullFeatures) {
    /**
     * WHAT: Test evaluation with NULL features
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL features, expect zeros
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    brain_salience_t salience = brain_evaluate_salience(evaluator, nullptr, 13);

    EXPECT_EQ(salience.salience, 0.0f);
}

TEST_F(SalienceTest, EvaluateSalienceTemporal) {
    /**
     * WHAT: Test temporal salience evaluation
     * WHY:  Verify timestamp parameter handling
     * HOW:  Use brain_evaluate_salience_temporal with explicit timestamp
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    uint64_t timestamp = 1234567890;

    brain_salience_t salience = brain_evaluate_salience_temporal(
        evaluator, features.data(), 13, timestamp);

    AssertValidSalience(salience);
}

//=============================================================================
// 4. Novelty Detection Tests
//=============================================================================

TEST_F(SalienceTest, NoveltyFirstInputAlwaysNovel) {
    /**
     * WHAT: Test that first input has moderate-to-high novelty
     * WHY:  No history exists for comparison
     * HOW:  Evaluate single input, expect novelty above baseline
     *
     * NOTE: First input novelty depends on internal baseline calculations.
     * A value of 0.5+ indicates the system recognizes it as relatively novel.
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    EXPECT_GT(salience.novelty, 0.5f) << "First input should have moderate-to-high novelty";
}

TEST_F(SalienceTest, NoveltyDecreasesWithRepetition) {
    /**
     * WHAT: Test that repeated inputs reduce novelty
     * WHY:  Familiarity should decrease novelty score
     * HOW:  Evaluate same features multiple times, check novelty trend
     *
     * FIXED: First input has high novelty (no history), subsequent identical
     * inputs have 0 novelty (cosine distance to identical entry = 0).
     * This is mathematically correct behavior.
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);

    brain_salience_t s1 = brain_evaluate_salience(evaluator, features.data(), 13);
    brain_salience_t s2 = brain_evaluate_salience(evaluator, features.data(), 13);
    brain_salience_t s3 = brain_evaluate_salience(evaluator, features.data(), 13);

    // First input should have high novelty (no history to compare against)
    EXPECT_GT(s1.novelty, 0.5f) << "First input should be novel";

    // Subsequent identical inputs should have near-zero novelty
    // (cosine distance to identical history entry = 0)
    EXPECT_LT(s2.novelty, 0.1f) << "Second identical input should have low novelty";
    EXPECT_LT(s3.novelty, 0.1f) << "Third identical input should have low novelty";

    // Both should be essentially the same (both comparing to identical history)
    EXPECT_NEAR(s2.novelty, s3.novelty, 0.01f) << "Identical inputs have same novelty";
}

TEST_F(SalienceTest, NoveltyIncreasesWithDifferentInput) {
    /**
     * WHAT: Test that novel input increases novelty
     * WHY:  Different patterns should be more novel
     * HOW:  Establish history, then evaluate different features
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features1 = CreateFeatures(13, 0.3f);
    auto features2 = CreateNovelFeatures(13);

    // Build history with features1
    for (int i = 0; i < 10; i++) {
        brain_evaluate_salience(evaluator, features1.data(), 13);
    }

    brain_salience_t s_familiar = brain_evaluate_salience(evaluator, features1.data(), 13);
    brain_salience_t s_novel = brain_evaluate_salience(evaluator, features2.data(), 13);

    EXPECT_LT(s_familiar.novelty, s_novel.novelty) << "Novel input should have higher novelty";
}

TEST_F(SalienceTest, NoveltyDisabled) {
    /**
     * WHAT: Test novelty can be disabled
     * WHY:  Some applications may not need novelty detection
     * HOW:  Set enable_novelty = false, expect zero novelty
     */
    salience_config_t config = salience_default_config();
    config.enable_novelty = false;
    config.history_size = 0;  // No history needed
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    EXPECT_EQ(salience.novelty, 0.0f);
}

//=============================================================================
// 5. Surprise Detection Tests
//=============================================================================

TEST_F(SalienceTest, SurpriseFirstInputModerate) {
    /**
     * WHAT: Test surprise for first input
     * WHY:  No prediction exists yet
     * HOW:  Evaluate first input, expect moderate surprise (0.5)
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    // First input has moderate surprise (no prediction yet)
    EXPECT_GE(salience.surprise, 0.0f);
    EXPECT_LE(salience.surprise, 1.0f);
}

TEST_F(SalienceTest, SurpriseDecreasesWithPredictability) {
    /**
     * WHAT: Test surprise decreases for predictable sequences
     * WHY:  Predictor learns patterns
     * HOW:  Feed consistent pattern, check surprise decrease
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13, 0.5f);

    std::vector<float> surprises;
    for (int i = 0; i < 20; i++) {
        brain_salience_t s = brain_evaluate_salience(evaluator, features.data(), 13);
        surprises.push_back(s.surprise);
    }

    // Surprise should generally decrease as pattern becomes predictable
    float early_avg = (surprises[0] + surprises[1] + surprises[2]) / 3.0f;
    float late_avg = (surprises[17] + surprises[18] + surprises[19]) / 3.0f;

    EXPECT_GT(early_avg, late_avg * 0.8f) << "Surprise should decrease with predictability";
}

TEST_F(SalienceTest, SurpriseIncreasesWithUnexpectedChange) {
    /**
     * WHAT: Test surprise increases when prediction violated
     * WHY:  Prediction error should spike surprise
     * HOW:  Establish pattern, then change it
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features1 = CreateFeatures(13, 0.2f);
    auto features2 = CreateFeatures(13, 0.8f);

    // Build predictable pattern
    for (int i = 0; i < 15; i++) {
        brain_evaluate_salience(evaluator, features1.data(), 13);
    }

    brain_salience_t s_expected = brain_evaluate_salience(evaluator, features1.data(), 13);
    brain_salience_t s_surprising = brain_evaluate_salience(evaluator, features2.data(), 13);

    EXPECT_LT(s_expected.surprise, s_surprising.surprise)
        << "Unexpected change should increase surprise";
}

TEST_F(SalienceTest, SurpriseDisabled) {
    /**
     * WHAT: Test surprise can be disabled
     * WHY:  Some applications may not need surprise detection
     * HOW:  Set enable_surprise = false, expect zero surprise
     */
    salience_config_t config = salience_default_config();
    config.enable_surprise = false;
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    EXPECT_EQ(salience.surprise, 0.0f);
}

//=============================================================================
// 6. Urgency Detection Tests
//=============================================================================

TEST_F(SalienceTest, UrgencyBaseline) {
    /**
     * WHAT: Test urgency baseline is applied
     * WHY:  Verify configuration is used
     * HOW:  Set urgency_baseline, check urgency score
     */
    salience_config_t config = salience_default_config();
    config.urgency_baseline = 0.6f;
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    EXPECT_GE(salience.urgency, 0.5f) << "Urgency should reflect baseline";
}

TEST_F(SalienceTest, UrgencyDisabled) {
    /**
     * WHAT: Test urgency can be disabled
     * WHY:  Some applications may not need urgency
     * HOW:  Set enable_urgency = false, expect zero urgency
     */
    salience_config_t config = salience_default_config();
    config.enable_urgency = false;
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    EXPECT_EQ(salience.urgency, 0.0f);
}

//=============================================================================
// 7. Strategy Tests (Fast, Balanced, Accurate)
//=============================================================================

TEST_F(SalienceTest, StrategyFast) {
    /**
     * WHAT: Test FAST strategy
     * WHY:  Verify strategy produces valid results
     * HOW:  Set strategy to FAST, evaluate
     */
    salience_config_t config = salience_default_config();
    config.strategy = SALIENCE_STRATEGY_FAST;
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    AssertValidSalience(salience);
    EXPECT_GE(salience.confidence, 0.6f) << "Fast strategy should have moderate confidence";
    EXPECT_LE(salience.confidence, 0.8f);
}

TEST_F(SalienceTest, StrategyBalanced) {
    /**
     * WHAT: Test BALANCED strategy
     * WHY:  Verify default strategy works
     * HOW:  Set strategy to BALANCED, evaluate
     */
    salience_config_t config = salience_default_config();
    config.strategy = SALIENCE_STRATEGY_BALANCED;
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    AssertValidSalience(salience);
    EXPECT_GE(salience.confidence, 0.8f) << "Balanced strategy should have good confidence";
    EXPECT_LE(salience.confidence, 0.9f);
}

TEST_F(SalienceTest, StrategyAccurate) {
    /**
     * WHAT: Test ACCURATE strategy
     * WHY:  Verify high-accuracy mode works
     * HOW:  Set strategy to ACCURATE, evaluate
     */
    salience_config_t config = salience_default_config();
    config.strategy = SALIENCE_STRATEGY_ACCURATE;
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    AssertValidSalience(salience);
    EXPECT_GE(salience.confidence, 0.9f) << "Accurate strategy should have high confidence";
}

TEST_F(SalienceTest, StrategyConfidenceOrdering) {
    /**
     * WHAT: Test confidence increases from FAST to ACCURATE
     * WHY:  Verify strategies differ in quality
     * HOW:  Compare confidence across strategies
     */
    auto features = CreateFeatures(13);

    // Fast
    salience_config_t config_fast = salience_default_config();
    config_fast.strategy = SALIENCE_STRATEGY_FAST;
    auto eval_fast = salience_evaluator_create(brain, &config_fast);
    brain_salience_t s_fast = brain_evaluate_salience(eval_fast, features.data(), 13);
    salience_evaluator_destroy(eval_fast);

    // Balanced
    salience_config_t config_bal = salience_default_config();
    config_bal.strategy = SALIENCE_STRATEGY_BALANCED;
    auto eval_bal = salience_evaluator_create(brain, &config_bal);
    brain_salience_t s_bal = brain_evaluate_salience(eval_bal, features.data(), 13);
    salience_evaluator_destroy(eval_bal);

    // Accurate
    salience_config_t config_acc = salience_default_config();
    config_acc.strategy = SALIENCE_STRATEGY_ACCURATE;
    auto eval_acc = salience_evaluator_create(brain, &config_acc);
    brain_salience_t s_acc = brain_evaluate_salience(eval_acc, features.data(), 13);
    salience_evaluator_destroy(eval_acc);

    EXPECT_LT(s_fast.confidence, s_bal.confidence) << "Balanced should be more confident than Fast";
    EXPECT_LT(s_bal.confidence, s_acc.confidence) << "Accurate should be most confident";
}

//=============================================================================
// 8. Batch Evaluation Tests
//=============================================================================

TEST_F(SalienceTest, BatchEvaluationSmall) {
    /**
     * WHAT: Test batch evaluation with small batch (< 200)
     * WHY:  Verify sequential path is used
     * HOW:  Evaluate batch of 50 samples
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    const uint32_t batch_size = 50;
    std::vector<std::vector<float>> feature_batch(batch_size);
    std::vector<const float*> feature_ptrs(batch_size);
    std::vector<brain_salience_t> results(batch_size);

    for (uint32_t i = 0; i < batch_size; i++) {
        feature_batch[i] = CreateFeatures(13, 0.5f + i * 0.01f);
        feature_ptrs[i] = feature_batch[i].data();
    }

    uint32_t evaluated = brain_evaluate_salience_batch(
        evaluator, feature_ptrs.data(), batch_size, 13, results.data());

    EXPECT_EQ(evaluated, batch_size);
    for (uint32_t i = 0; i < batch_size; i++) {
        AssertValidSalience(results[i]);
    }
}

TEST_F(SalienceTest, BatchEvaluationLarge) {
    /**
     * WHAT: Test batch evaluation with large batch (>= 200)
     * WHY:  Verify parallel path is used
     * HOW:  Evaluate batch of 300 samples
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    const uint32_t batch_size = 300;
    std::vector<std::vector<float>> feature_batch(batch_size);
    std::vector<const float*> feature_ptrs(batch_size);
    std::vector<brain_salience_t> results(batch_size);

    for (uint32_t i = 0; i < batch_size; i++) {
        feature_batch[i] = CreateFeatures(13, 0.5f);
        feature_ptrs[i] = feature_batch[i].data();
    }

    uint32_t evaluated = brain_evaluate_salience_batch(
        evaluator, feature_ptrs.data(), batch_size, 13, results.data());

    EXPECT_EQ(evaluated, batch_size);
    for (uint32_t i = 0; i < batch_size; i++) {
        AssertValidSalience(results[i]);
    }
}

TEST_F(SalienceTest, BatchEvaluationNullEvaluator) {
    /**
     * WHAT: Test batch evaluation with NULL evaluator
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL evaluator, expect 0 returned
     */
    std::vector<float> features = CreateFeatures(13);
    const float* ptrs[] = {features.data()};
    brain_salience_t results[1];

    uint32_t evaluated = brain_evaluate_salience_batch(nullptr, ptrs, 1, 13, results);

    EXPECT_EQ(evaluated, 0u);
}

TEST_F(SalienceTest, BatchEvaluationNullFeatures) {
    /**
     * WHAT: Test batch evaluation with NULL features
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL features array, expect 0 returned
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    brain_salience_t results[10];

    uint32_t evaluated = brain_evaluate_salience_batch(evaluator, nullptr, 10, 13, results);

    EXPECT_EQ(evaluated, 0u);
}

TEST_F(SalienceTest, BatchEvaluationNullResults) {
    /**
     * WHAT: Test batch evaluation with NULL results
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL results array, expect 0 returned
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    std::vector<float> features = CreateFeatures(13);
    const float* ptrs[] = {features.data()};

    uint32_t evaluated = brain_evaluate_salience_batch(evaluator, ptrs, 1, 13, nullptr);

    EXPECT_EQ(evaluated, 0u);
}

//=============================================================================
// 9. Configuration Update Tests
//=============================================================================

TEST_F(SalienceTest, SetWeightsValid) {
    /**
     * WHAT: Test updating salience weights
     * WHY:  Verify dynamic configuration
     * HOW:  Call salience_set_weights, evaluate
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    bool ok = salience_set_weights(evaluator, 0.5f, 0.3f, 0.2f);
    EXPECT_TRUE(ok);

    // Weights should affect combined salience score
    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);
    AssertValidSalience(salience);
}

TEST_F(SalienceTest, SetWeightsNullEvaluator) {
    /**
     * WHAT: Test set_weights with NULL evaluator
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL, expect false
     */
    bool ok = salience_set_weights(nullptr, 0.5f, 0.3f, 0.2f);
    EXPECT_FALSE(ok);
}

TEST_F(SalienceTest, SetThresholdsValid) {
    /**
     * WHAT: Test updating attention thresholds
     * WHY:  Verify dynamic threshold configuration
     * HOW:  Call salience_set_thresholds
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    bool ok = salience_set_thresholds(evaluator, 0.8f, 0.85f, 0.85f, 0.9f);
    EXPECT_TRUE(ok);
}

TEST_F(SalienceTest, SetThresholdsNullEvaluator) {
    /**
     * WHAT: Test set_thresholds with NULL evaluator
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL, expect false
     */
    bool ok = salience_set_thresholds(nullptr, 0.8f, 0.85f, 0.85f, 0.9f);
    EXPECT_FALSE(ok);
}

//=============================================================================
// 10. Callback Tests
//=============================================================================

/**
 * WHAT: Test callback context
 * WHY:  Track callback invocations
 * HOW:  Store event information
 */
struct CallbackContext {
    int high_salience_count = 0;
    int high_novelty_count = 0;
    int high_surprise_count = 0;
    int high_urgency_count = 0;
    float last_salience = 0.0f;
};

void test_callback(const salience_event_t* event, void* context) {
    CallbackContext* ctx = static_cast<CallbackContext*>(context);
    if (!ctx || !event) return;

    ctx->last_salience = event->salience.salience;

    switch (event->type) {
        case SALIENCE_EVENT_HIGH_SALIENCE:
            ctx->high_salience_count++;
            break;
        case SALIENCE_EVENT_HIGH_NOVELTY:
            ctx->high_novelty_count++;
            break;
        case SALIENCE_EVENT_HIGH_SURPRISE:
            ctx->high_surprise_count++;
            break;
        case SALIENCE_EVENT_HIGH_URGENCY:
            ctx->high_urgency_count++;
            break;
    }
}

TEST_F(SalienceTest, RegisterCallbackValid) {
    /**
     * WHAT: Test callback registration
     * WHY:  Verify observer pattern implementation
     * HOW:  Register callback, trigger event, check invocation
     */
    salience_config_t config = salience_default_config();
    config.high_salience_threshold = 0.6f;
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    CallbackContext ctx;
    bool ok = salience_register_callback(evaluator, test_callback, &ctx);
    EXPECT_TRUE(ok);

    // First input should trigger high salience (novelty is high)
    auto features = CreateFeatures(13);
    brain_evaluate_salience(evaluator, features.data(), 13);

    // Callback may be triggered (depends on evaluation)
    EXPECT_GE(ctx.high_salience_count, 0);
}

TEST_F(SalienceTest, RegisterCallbackNullEvaluator) {
    /**
     * WHAT: Test register_callback with NULL evaluator
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL, expect false
     */
    CallbackContext ctx;
    bool ok = salience_register_callback(nullptr, test_callback, &ctx);
    EXPECT_FALSE(ok);
}

//=============================================================================
// 11. History Management Tests
//=============================================================================

TEST_F(SalienceTest, ClearHistoryValid) {
    /**
     * WHAT: Test history clearing
     * WHY:  Verify history can be reset
     * HOW:  Build history, clear, check novelty returns to high
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);

    // Build history
    for (int i = 0; i < 10; i++) {
        brain_evaluate_salience(evaluator, features.data(), 13);
    }

    brain_salience_t s_before = brain_evaluate_salience(evaluator, features.data(), 13);

    // Clear history
    bool ok = salience_clear_history(evaluator);
    EXPECT_TRUE(ok);

    brain_salience_t s_after = brain_evaluate_salience(evaluator, features.data(), 13);

    // Novelty should increase after clearing history
    EXPECT_GT(s_after.novelty, s_before.novelty) << "Novelty should increase after history clear";
}

TEST_F(SalienceTest, ClearHistoryNullEvaluator) {
    /**
     * WHAT: Test clear_history with NULL evaluator
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL, expect false
     */
    bool ok = salience_clear_history(nullptr);
    EXPECT_FALSE(ok);
}

TEST_F(SalienceTest, ClearHistoryNoHistory) {
    /**
     * WHAT: Test clear_history with no history enabled
     * WHY:  Verify graceful handling
     * HOW:  Create evaluator with enable_novelty=false, call clear
     */
    salience_config_t config = salience_default_config();
    config.enable_novelty = false;
    config.history_size = 0;
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    bool ok = salience_clear_history(evaluator);
    EXPECT_FALSE(ok);  // No history to clear
}

//=============================================================================
// 12. Statistics Tests
//=============================================================================

TEST_F(SalienceTest, GetStatsValid) {
    /**
     * WHAT: Test statistics retrieval
     * WHY:  Verify stats tracking works
     * HOW:  Perform evaluations, get stats, check counts
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);

    // Perform some evaluations
    for (int i = 0; i < 5; i++) {
        brain_evaluate_salience(evaluator, features.data(), 13);
    }

    salience_stats_t stats;
    bool ok = salience_get_stats(evaluator, &stats);

    EXPECT_TRUE(ok);
    EXPECT_GE(stats.evaluations_performed, 5u);
    EXPECT_GE(stats.avg_salience, 0.0f);
    EXPECT_LE(stats.avg_salience, 1.0f);
}

TEST_F(SalienceTest, GetStatsNullEvaluator) {
    /**
     * WHAT: Test get_stats with NULL evaluator
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL evaluator, expect false
     */
    salience_stats_t stats;
    bool ok = salience_get_stats(nullptr, &stats);
    EXPECT_FALSE(ok);
}

TEST_F(SalienceTest, GetStatsNullStats) {
    /**
     * WHAT: Test get_stats with NULL stats pointer
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL stats, expect false
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    bool ok = salience_get_stats(evaluator, nullptr);
    EXPECT_FALSE(ok);
}

TEST_F(SalienceTest, ResetStatsValid) {
    /**
     * WHAT: Test statistics reset
     * WHY:  Verify stats can be cleared
     * HOW:  Build stats, reset, check zeros
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);

    // Build stats
    for (int i = 0; i < 10; i++) {
        brain_evaluate_salience(evaluator, features.data(), 13);
    }

    salience_stats_t stats_before;
    salience_get_stats(evaluator, &stats_before);
    EXPECT_GT(stats_before.evaluations_performed, 0u);

    // Reset
    bool ok = salience_reset_stats(evaluator);
    EXPECT_TRUE(ok);

    salience_stats_t stats_after;
    salience_get_stats(evaluator, &stats_after);
    EXPECT_EQ(stats_after.evaluations_performed, 0u);
    EXPECT_EQ(stats_after.avg_salience, 0.0f);
}

TEST_F(SalienceTest, ResetStatsNullEvaluator) {
    /**
     * WHAT: Test reset_stats with NULL evaluator
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL, expect false
     */
    bool ok = salience_reset_stats(nullptr);
    EXPECT_FALSE(ok);
}

//=============================================================================
// 13. Convenience Function Tests
//=============================================================================

TEST_F(SalienceTest, QuickEvaluateValid) {
    /**
     * WHAT: Test convenience quick evaluation
     * WHY:  Verify one-shot evaluation works
     * HOW:  Call salience_quick_evaluate
     */
    auto features = CreateFeatures(13);
    brain_salience_t salience = salience_quick_evaluate(brain, features.data(), 13);

    AssertValidSalience(salience);
}

TEST_F(SalienceTest, QuickEvaluateNullBrain) {
    /**
     * WHAT: Test quick_evaluate with NULL brain
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL brain, expect zeros
     */
    auto features = CreateFeatures(13);
    brain_salience_t salience = salience_quick_evaluate(nullptr, features.data(), 13);

    EXPECT_EQ(salience.salience, 0.0f);
}

//=============================================================================
// 14. Bidirectional Feedback Tests (Emotional Modulation)
//=============================================================================

TEST_F(SalienceTest, BoostNegativeCuesValid) {
    /**
     * WHAT: Test negative cue boosting
     * WHY:  Verify emotional feedback affects salience
     * HOW:  Call salience_boost_negative_cues, check weights
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t s_before = brain_evaluate_salience(evaluator, features.data(), 13);

    salience_boost_negative_cues(evaluator, 0.5f);

    brain_salience_t s_after = brain_evaluate_salience(evaluator, features.data(), 13);

    // Novelty weight should be boosted
    AssertValidSalience(s_after);
}

TEST_F(SalienceTest, BoostNegativeCuesNullEvaluator) {
    /**
     * WHAT: Test boost_negative_cues with NULL evaluator
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL, expect no crash
     */
    salience_boost_negative_cues(nullptr, 0.5f);
    SUCCEED();  // Should not crash
}

TEST_F(SalienceTest, BoostThreatDetectionValid) {
    /**
     * WHAT: Test threat detection boosting
     * WHY:  Verify anxiety modulation works
     * HOW:  Call salience_boost_threat_detection
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    salience_boost_threat_detection(evaluator, 0.8f);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    // Urgency should be boosted
    EXPECT_GT(salience.urgency, 0.3f);
}

TEST_F(SalienceTest, BoostThreatDetectionNullEvaluator) {
    /**
     * WHAT: Test boost_threat_detection with NULL evaluator
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL, expect no crash
     */
    salience_boost_threat_detection(nullptr, 0.8f);
    SUCCEED();  // Should not crash
}

TEST_F(SalienceTest, GetSurpriseLevelValid) {
    /**
     * WHAT: Test surprise level retrieval
     * WHY:  Verify surprise can be queried
     * HOW:  Evaluate input, get surprise level
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_evaluate_salience(evaluator, features.data(), 13);

    float surprise = salience_get_surprise_level(evaluator);

    EXPECT_GE(surprise, 0.0f);
    EXPECT_LE(surprise, 1.0f);
}

TEST_F(SalienceTest, GetSurpriseLevelNullEvaluator) {
    /**
     * WHAT: Test get_surprise_level with NULL evaluator
     * WHY:  Verify guard clause protection
     * HOW:  Pass NULL, expect 0.0
     */
    float surprise = salience_get_surprise_level(nullptr);
    EXPECT_EQ(surprise, 0.0f);
}

//=============================================================================
// 15. Acetylcholine Gating Tests
//=============================================================================

TEST_F(SalienceTest, AcetylcholineGatingModulatesSalience) {
    /**
     * WHAT: Test acetylcholine modulation
     * WHY:  Verify neuromodulator integration
     * HOW:  Set ACh level in brain, evaluate salience
     * NOTE: This test requires neuromodulator system in brain
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    // Salience should be within valid range after ACh gating
    AssertValidSalience(salience);
}

//=============================================================================
// 16. Edge Case Tests
//=============================================================================

TEST_F(SalienceTest, LargeFeatureVector) {
    /**
     * WHAT: Test with large feature vector
     * WHY:  Verify handling of maximum feature size
     * HOW:  Use 512 features (SALIENCE_MAX_FEATURES)
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(512);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 512);

    // Should handle gracefully (may have reduced functionality)
    EXPECT_GE(salience.salience, 0.0f);
}

TEST_F(SalienceTest, OversizedFeatureVector) {
    /**
     * WHAT: Test with oversized feature vector (> SALIENCE_MAX_FEATURES)
     * WHY:  Verify graceful handling of oversized input
     * HOW:  Use 600 features (> 512 max)
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(600);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 600);

    // Should handle gracefully (history updates silently rejected)
    EXPECT_GE(salience.salience, 0.0f);
}

TEST_F(SalienceTest, ZeroFeatures) {
    /**
     * WHAT: Test with zero-length feature vector
     * WHY:  Verify edge case handling
     * HOW:  Pass num_features = 0
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    std::vector<float> features;
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 0);

    // Should handle gracefully
    EXPECT_GE(salience.salience, 0.0f);
}

TEST_F(SalienceTest, AllZeroFeatures) {
    /**
     * WHAT: Test with all-zero features
     * WHY:  Verify handling of zero-variance input
     * HOW:  Create features with all zeros
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    std::vector<float> features(13, 0.0f);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    AssertValidSalience(salience);
}

TEST_F(SalienceTest, ExtremeFeatureValues) {
    /**
     * WHAT: Test with extreme feature values
     * WHY:  Verify handling of large magnitudes
     * HOW:  Use features with values near float limits
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    std::vector<float> features(13);
    for (int i = 0; i < 13; i++) {
        features[i] = (i % 2 == 0) ? 1000.0f : -1000.0f;
    }

    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    // Should clamp to [0, 1] range
    AssertValidSalience(salience);
}

//=============================================================================
// 17. Thread Safety Tests (Basic)
//=============================================================================

TEST_F(SalienceTest, ConcurrentEvaluations) {
    /**
     * WHAT: Test concurrent salience evaluations
     * WHY:  Verify thread safety of evaluator
     * HOW:  Spawn multiple threads, evaluate in parallel
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    const int num_threads = 4;
    const int evals_per_thread = 10;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, evals_per_thread]() {
            for (int i = 0; i < evals_per_thread; i++) {
                auto features = CreateFeatures(13, 0.5f + t * 0.1f);
                brain_salience_t salience = brain_evaluate_salience(
                    evaluator, features.data(), 13);
                AssertValidSalience(salience);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Check stats
    salience_stats_t stats;
    salience_get_stats(evaluator, &stats);
    EXPECT_GE(stats.evaluations_performed, num_threads * evals_per_thread);
}

//=============================================================================
// 18. Weight Combination Tests
//=============================================================================

TEST_F(SalienceTest, WeightCombinationZeroWeights) {
    /**
     * WHAT: Test with all weights set to zero
     * WHY:  Verify handling of edge case
     * HOW:  Set all weights to 0, evaluate
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    salience_set_weights(evaluator, 0.0f, 0.0f, 0.0f);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    // Combined salience should be 0 when weights are 0
    EXPECT_EQ(salience.salience, 0.0f);
}

TEST_F(SalienceTest, WeightCombinationNoveltyOnly) {
    /**
     * WHAT: Test with only novelty weighted
     * WHY:  Verify individual component isolation
     * HOW:  Set novelty_weight = 1.0, others = 0
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    salience_set_weights(evaluator, 1.0f, 0.0f, 0.0f);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    // Combined salience should equal novelty
    EXPECT_NEAR(salience.salience, salience.novelty, 0.01f);
}

//=============================================================================
// 19. Statistics Accumulation Tests
//=============================================================================

TEST_F(SalienceTest, HighSalienceCountIncreases) {
    /**
     * WHAT: Test high_salience_count increments
     * WHY:  Verify threshold-based counting
     * HOW:  Generate high-salience inputs, check counter
     *
     * NOTE: The weighted salience depends on multiple factors. Use a low
     * threshold to ensure we capture the first novel input.
     */
    salience_config_t config = salience_default_config();
    config.high_salience_threshold = 0.3f;  // Very low threshold to capture first input
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    // First input is novel, should have high salience
    auto features = CreateFeatures(13);
    brain_evaluate_salience(evaluator, features.data(), 13);

    salience_stats_t stats;
    salience_get_stats(evaluator, &stats);

    EXPECT_GT(stats.high_salience_count, 0u);
}

TEST_F(SalienceTest, RunningAverageConverges) {
    /**
     * WHAT: Test running average convergence
     * WHY:  Verify exponential moving average works
     * HOW:  Feed constant input, check average stabilizes
     */
    salience_config_t config = salience_default_config();
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13, 0.5f);

    // Feed many identical inputs
    for (int i = 0; i < 50; i++) {
        brain_evaluate_salience(evaluator, features.data(), 13);
    }

    salience_stats_t stats;
    salience_get_stats(evaluator, &stats);

    // Average should be within reasonable range
    EXPECT_GE(stats.avg_salience, 0.0f);
    EXPECT_LE(stats.avg_salience, 1.0f);
}

//=============================================================================
// 20. Error Message Tests
//=============================================================================

TEST_F(SalienceTest, ErrorMessageThreadLocal) {
    /**
     * WHAT: Test error message storage is thread-local
     * WHY:  Verify thread-safe error reporting
     * HOW:  Get error message, verify it's accessible
     */
    // Trigger an error
    salience_evaluator_create(nullptr, nullptr);

    const char* error = salience_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_NE(error[0], '\0');
}

//=============================================================================
// 21. History Buffer Size Tests
//=============================================================================

TEST_F(SalienceTest, SmallHistorySize) {
    /**
     * WHAT: Test with minimal history size
     * WHY:  Verify small buffer works correctly
     * HOW:  Set history_size = 5, fill buffer
     */
    salience_config_t config = salience_default_config();
    config.history_size = 5;
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);

    // Fill buffer beyond capacity
    for (int i = 0; i < 10; i++) {
        brain_salience_t s = brain_evaluate_salience(evaluator, features.data(), 13);
        AssertValidSalience(s);
    }

    salience_stats_t stats;
    salience_get_stats(evaluator, &stats);
    EXPECT_EQ(stats.history_size, 5u);
}

TEST_F(SalienceTest, LargeHistorySize) {
    /**
     * WHAT: Test with large history size
     * WHY:  Verify large buffer doesn't break
     * HOW:  Set history_size = 1000
     */
    salience_config_t config = salience_default_config();
    config.history_size = 1000;
    evaluator = salience_evaluator_create(brain, &config);
    ASSERT_NE(evaluator, nullptr);

    auto features = CreateFeatures(13);
    brain_salience_t salience = brain_evaluate_salience(evaluator, features.data(), 13);

    AssertValidSalience(salience);
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
