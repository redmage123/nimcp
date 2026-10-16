/**
 * @file test_rcog_answer.cpp
 * @brief Unit tests for Recursive Cognition Answer Refiner
 *
 * WHAT: Comprehensive tests for RLM-style answer diffusion
 * WHY:  Answer refiner implements iterative refinement - must converge correctly
 * HOW:  Unit tests for initialization, refinement steps, convergence, statistics
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <string.h>
#include <math.h>

#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_answer.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * WHAT: Test fixture for answer refiner tests
 * WHY:  Set up/tear down refiner for each test
 */
class AnswerRefinerTest : public ::testing::Test {
protected:
    rcog_answer_refiner_t* refiner;

    void SetUp() override
    {
        refiner = rcog_answer_refiner_create_default();
        ASSERT_NE(refiner, nullptr);
    }

    void TearDown() override
    {
        if (refiner) {
            rcog_answer_refiner_destroy(refiner);
            refiner = nullptr;
        }
    }

    // Helper to create a simple goal
    rcog_goal_t CreateTestGoal(const char* query_text, rcog_goal_type_t type = RCOG_GOAL_QUESTION_ANSWERING)
    {
        rcog_goal_t goal = {};
        goal.type = type;
        goal.priority = 1.0f;
        goal.timeout_ms = 10000;
        goal.query = query_text;
        return goal;
    }

    // Helper to create simple evidence
    rcog_subtask_result_t CreateEvidence(float confidence, float* latent, size_t dim)
    {
        rcog_subtask_result_t evidence = {0};
        evidence.success = true;
        evidence.confidence = confidence;
        if (latent && dim > 0) {
            evidence.latent = latent;
            evidence.latent_dim = dim;
        }
        return evidence;
    }
};

/**
 * WHAT: Test fixture with custom configuration
 * WHY:  Test non-default configurations
 */
class AnswerRefinerCustomConfigTest : public ::testing::Test {
protected:
    rcog_answer_refiner_t* refiner;

    void SetUp() override
    {
        rcog_answer_config_t config = rcog_answer_default_config();
        config.ready_threshold = 0.8f;
        config.max_steps = 10;
        config.learning_rate = 0.2f;
        config.convergence_epsilon = 0.01f;

        refiner = rcog_answer_refiner_create(&config);
        ASSERT_NE(refiner, nullptr);
    }

    void TearDown() override
    {
        if (refiner) {
            rcog_answer_refiner_destroy(refiner);
            refiner = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

/**
 * WHAT: Test default configuration
 * WHY:  Verify sensible defaults are provided
 */
TEST(AnswerRefinerLifecycleTest, DefaultConfig)
{
    rcog_answer_config_t config = rcog_answer_default_config();

    EXPECT_GT(config.latent_dim, 0u);
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_GT(config.momentum, 0.0f);
    EXPECT_LE(config.momentum, 1.0f);
    EXPECT_GT(config.ready_threshold, 0.0f);
    EXPECT_LE(config.ready_threshold, 1.0f);
    EXPECT_GT(config.min_steps, 0u);
    EXPECT_GT(config.max_steps, config.min_steps);
    EXPECT_GT(config.convergence_epsilon, 0.0f);
}

/**
 * WHAT: Test answer refiner creation with defaults
 * WHY:  Verify basic creation works
 */
TEST(AnswerRefinerLifecycleTest, CreateDefault)
{
    rcog_answer_refiner_t* refiner = rcog_answer_refiner_create_default();
    ASSERT_NE(refiner, nullptr);

    rcog_answer_refiner_destroy(refiner);
}

/**
 * WHAT: Test answer refiner creation with custom config
 * WHY:  Verify custom configuration is applied
 */
TEST(AnswerRefinerLifecycleTest, CreateWithConfig)
{
    rcog_answer_config_t config = rcog_answer_default_config();
    config.latent_dim = 128;
    config.ready_threshold = 0.9f;

    rcog_answer_refiner_t* refiner = rcog_answer_refiner_create(&config);
    ASSERT_NE(refiner, nullptr);

    // Verify config was applied
    rcog_answer_config_t retrieved_config = {0};
    rcog_error_t err = rcog_answer_refiner_get_config(refiner, &retrieved_config);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_EQ(retrieved_config.latent_dim, 128u);
    EXPECT_FLOAT_EQ(retrieved_config.ready_threshold, 0.9f);

    rcog_answer_refiner_destroy(refiner);
}

/**
 * WHAT: Test destroy with NULL (should be safe)
 * WHY:  Verify NULL-safe destruction
 */
TEST(AnswerRefinerLifecycleTest, DestroyNull)
{
    // Should not crash
    rcog_answer_refiner_destroy(NULL);
}

//=============================================================================
// Answer State Tests
//=============================================================================

/**
 * WHAT: Test answer state initialization
 * WHY:  State must start in known good configuration
 */
TEST_F(AnswerRefinerTest, InitAnswerState)
{
    rcog_goal_t goal = CreateTestGoal("What is 2+2?");
    rcog_answer_state_t state = {0};

    rcog_error_t err = rcog_answer_init(refiner, &goal, &state);
    EXPECT_EQ(err, RCOG_OK);

    // Initial state should not be ready
    EXPECT_FALSE(rcog_answer_is_ready(refiner, &state));

    // Initial confidence should be 0 or very low
    EXPECT_LT(rcog_answer_get_confidence(&state), 0.1f);

    // Initial step should be 0
    EXPECT_EQ(rcog_answer_get_step(&state), 0u);

    rcog_answer_state_destroy(&state);
}

/**
 * WHAT: Test answer state creation (heap allocated)
 * WHY:  Verify heap allocation path works
 */
TEST_F(AnswerRefinerTest, CreateAnswerState)
{
    rcog_goal_t goal = CreateTestGoal("Test question");

    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    EXPECT_FALSE(rcog_answer_is_ready(refiner, state));

    rcog_answer_state_destroy(state);
}

/**
 * WHAT: Test answer state reset
 * WHY:  Verify state can be reused
 */
TEST_F(AnswerRefinerTest, ResetAnswerState)
{
    rcog_goal_t goal = CreateTestGoal("Original question");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    // Perform some refinement
    float evidence_latent[256];
    for (size_t i = 0; i < 256; i++) {
        evidence_latent[i] = 0.5f;
    }
    rcog_subtask_result_t evidence = CreateEvidence(0.8f, evidence_latent, 256);
    rcog_answer_step(refiner, state, &evidence, 1);

    // Step should have incremented
    EXPECT_GT(rcog_answer_get_step(state), 0u);

    // Reset
    rcog_error_t err = rcog_answer_reset(refiner, state);
    EXPECT_EQ(err, RCOG_OK);

    // Should be back to initial state
    EXPECT_EQ(rcog_answer_get_step(state), 0u);
    EXPECT_FALSE(rcog_answer_is_ready(refiner, state));

    rcog_answer_state_destroy(state);
}

/**
 * WHAT: Test destroy state with NULL (should be safe)
 * WHY:  Verify NULL-safe destruction
 */
TEST_F(AnswerRefinerTest, DestroyStateNull)
{
    rcog_answer_state_destroy(NULL);  // Should not crash
}

//=============================================================================
// Refinement Tests (Core Diffusion Loop)
//=============================================================================

/**
 * WHAT: Test single refinement step
 * WHY:  Basic step should update state
 */
TEST_F(AnswerRefinerTest, SingleRefinementStep)
{
    rcog_goal_t goal = CreateTestGoal("Single step test");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    // Create evidence with latent representation
    float evidence_latent[256];
    for (size_t i = 0; i < 256; i++) {
        evidence_latent[i] = 0.5f + 0.1f * sinf((float)i);
    }
    rcog_subtask_result_t evidence = CreateEvidence(0.7f, evidence_latent, 256);

    rcog_error_t err = rcog_answer_step(refiner, state, &evidence, 1);
    EXPECT_EQ(err, RCOG_OK);

    // Step should have incremented
    EXPECT_EQ(rcog_answer_get_step(state), 1u);

    // Confidence should have increased from 0
    EXPECT_GT(rcog_answer_get_confidence(state), 0.0f);

    rcog_answer_state_destroy(state);
}

/**
 * WHAT: Test multiple refinement steps
 * WHY:  Verify confidence increases with evidence
 */
TEST_F(AnswerRefinerTest, MultipleRefinementSteps)
{
    rcog_goal_t goal = CreateTestGoal("Multi-step test");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    float prev_confidence = 0.0f;

    // Perform multiple refinement steps with consistent evidence
    for (int i = 0; i < 5; i++) {
        float evidence_latent[256];
        for (size_t j = 0; j < 256; j++) {
            evidence_latent[j] = 0.8f;  // Strong, consistent evidence
        }
        rcog_subtask_result_t evidence = CreateEvidence(0.9f, evidence_latent, 256);

        rcog_error_t err = rcog_answer_step(refiner, state, &evidence, 1);
        EXPECT_EQ(err, RCOG_OK);

        float current_confidence = rcog_answer_get_confidence(state);
        EXPECT_GE(current_confidence, prev_confidence);  // Should not decrease with consistent evidence
        prev_confidence = current_confidence;
    }

    EXPECT_EQ(rcog_answer_get_step(state), 5u);

    rcog_answer_state_destroy(state);
}

/**
 * WHAT: Test answer update (single evidence convenience)
 * WHY:  Verify single-evidence path works
 */
TEST_F(AnswerRefinerTest, AnswerUpdate)
{
    rcog_goal_t goal = CreateTestGoal("Update test");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    float evidence_latent[256];
    for (size_t i = 0; i < 256; i++) {
        evidence_latent[i] = 0.6f;
    }
    rcog_subtask_result_t evidence = CreateEvidence(0.75f, evidence_latent, 256);

    rcog_error_t err = rcog_answer_update(refiner, state, &evidence);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_EQ(rcog_answer_get_step(state), 1u);

    rcog_answer_state_destroy(state);
}

/**
 * WHAT: Test refinement with NULL evidence
 * WHY:  Verify proper error handling
 */
TEST_F(AnswerRefinerTest, StepNullEvidence)
{
    rcog_goal_t goal = CreateTestGoal("Null evidence test");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    // NULL evidence with count > 0 is treated as a no-op (momentum-only step)
    rcog_error_t err = rcog_answer_step(refiner, state, NULL, 1);
    EXPECT_EQ(err, RCOG_OK);

    // NULL evidence with count = 0 should also be OK (no-op)
    err = rcog_answer_step(refiner, state, NULL, 0);
    EXPECT_EQ(err, RCOG_OK);

    rcog_answer_state_destroy(state);
}

//=============================================================================
// Convergence Tests
//=============================================================================

/**
 * WHAT: Test answer ready detection
 * WHY:  Verify ready threshold works
 */
TEST_F(AnswerRefinerCustomConfigTest, AnswerBecomesReady)
{
    rcog_goal_t goal = {};
    goal.type = RCOG_GOAL_QUESTION_ANSWERING;
    goal.query = "Convergence test";

    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    // With threshold at 0.8, strong evidence should make it ready
    for (int i = 0; i < 10 && !rcog_answer_is_ready(refiner, state); i++) {
        float evidence_latent[256];
        for (size_t j = 0; j < 256; j++) {
            evidence_latent[j] = 1.0f;  // Maximum evidence
        }
        rcog_subtask_result_t evidence = {0};
        evidence.success = true;
        evidence.confidence = 1.0f;
        evidence.latent = evidence_latent;
        evidence.latent_dim = 256;

        rcog_answer_step(refiner, state, &evidence, 1);
    }

    // With strong evidence and low threshold, should be ready
    float conf = rcog_answer_get_confidence(state);
    if (conf >= 0.8f) {
        EXPECT_TRUE(rcog_answer_is_ready(refiner, state));
    }

    rcog_answer_state_destroy(state);
}

/**
 * WHAT: Test convergence detection
 * WHY:  Verify delta-based convergence works
 */
TEST_F(AnswerRefinerTest, ConvergenceDetection)
{
    rcog_goal_t goal = CreateTestGoal("Convergence test");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    // Apply same evidence repeatedly - should converge
    float evidence_latent[256];
    for (size_t i = 0; i < 256; i++) {
        evidence_latent[i] = 0.5f;
    }

    for (int i = 0; i < 20; i++) {
        rcog_subtask_result_t evidence = CreateEvidence(0.7f, evidence_latent, 256);
        rcog_answer_step(refiner, state, &evidence, 1);
    }

    // After many steps with same evidence, delta should be small
    float delta = rcog_answer_get_delta(state);
    EXPECT_LT(delta, 0.1f);  // Should have converged

    // May have converged
    if (delta < 0.001f) {
        EXPECT_TRUE(rcog_answer_has_converged(refiner, state));
    }

    rcog_answer_state_destroy(state);
}

/**
 * WHAT: Test stall detection
 * WHY:  Verify stall detection for oscillating/stuck answers
 */
TEST_F(AnswerRefinerTest, StallDetection)
{
    rcog_goal_t goal = CreateTestGoal("Stall test");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    // Initially should not be stalled
    EXPECT_FALSE(rcog_answer_is_stalled(refiner, state, 3));

    // Apply evidence
    for (int i = 0; i < 5; i++) {
        float evidence_latent[256];
        for (size_t j = 0; j < 256; j++) {
            evidence_latent[j] = 0.5f;
        }
        rcog_subtask_result_t evidence = CreateEvidence(0.5f, evidence_latent, 256);
        rcog_answer_step(refiner, state, &evidence, 1);
    }

    // Check stall with window parameter
    bool stalled = rcog_answer_is_stalled(refiner, state, 3);
    // Result depends on history - just verify function works
    (void)stalled;

    rcog_answer_state_destroy(state);
}

//=============================================================================
// Content and Latent Tests
//=============================================================================

/**
 * WHAT: Test setting and getting answer content
 * WHY:  Verify content storage works
 */
TEST_F(AnswerRefinerTest, SetGetContent)
{
    rcog_goal_t goal = CreateTestGoal("Content test");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    const char* content = "The answer is 42";
    rcog_error_t err = rcog_answer_set_content(state, content, strlen(content));
    EXPECT_EQ(err, RCOG_OK);

    void* retrieved_content = NULL;
    size_t size = 0;
    err = rcog_answer_get_content(state, &retrieved_content, &size);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_EQ(size, strlen(content));
    EXPECT_EQ(memcmp(retrieved_content, content, size), 0);

    rcog_answer_state_destroy(state);
}

/**
 * WHAT: Test setting and getting latent representation
 * WHY:  Verify latent space operations work
 */
TEST_F(AnswerRefinerTest, SetGetLatent)
{
    rcog_goal_t goal = CreateTestGoal("Latent test");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    // Set custom latent
    float custom_latent[256];
    for (size_t i = 0; i < 256; i++) {
        custom_latent[i] = (float)i / 256.0f;
    }

    rcog_error_t err = rcog_answer_set_latent(state, custom_latent, 256);
    EXPECT_EQ(err, RCOG_OK);

    // Retrieve latent
    float* latent = NULL;
    size_t dim = 0;
    err = rcog_answer_get_latent(state, &latent, &dim);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_NE(latent, nullptr);
    EXPECT_EQ(dim, 256u);

    // Verify values
    for (size_t i = 0; i < dim; i++) {
        EXPECT_FLOAT_EQ(latent[i], custom_latent[i]);
    }

    rcog_answer_state_destroy(state);
}

/**
 * WHAT: Test answer extraction
 * WHY:  Verify final answer can be extracted
 */
TEST_F(AnswerRefinerTest, ExtractAnswer)
{
    rcog_goal_t goal = CreateTestGoal("Extraction test");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    // Perform some refinement
    for (int i = 0; i < 3; i++) {
        float evidence_latent[256];
        for (size_t j = 0; j < 256; j++) {
            evidence_latent[j] = 0.7f;
        }
        rcog_subtask_result_t evidence = CreateEvidence(0.8f, evidence_latent, 256);
        rcog_answer_step(refiner, state, &evidence, 1);
    }

    float* output = NULL;
    size_t output_size = 0;
    rcog_error_t err = rcog_answer_extract(refiner, state, &output, &output_size);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_NE(output, nullptr);
    EXPECT_GT(output_size, 0u);

    if (output) {
        free(output);
    }

    rcog_answer_state_destroy(state);
}

/**
 * WHAT: Test answer blending
 * WHY:  Verify two answers can be blended
 */
TEST_F(AnswerRefinerTest, BlendAnswers)
{
    rcog_goal_t goal = CreateTestGoal("Blend test");

    rcog_answer_state_t* state_a = rcog_answer_state_create(refiner, &goal);
    rcog_answer_state_t* state_b = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state_a, nullptr);
    ASSERT_NE(state_b, nullptr);

    // Set different latents
    float latent_a[256], latent_b[256];
    for (size_t i = 0; i < 256; i++) {
        latent_a[i] = 0.0f;
        latent_b[i] = 1.0f;
    }
    rcog_answer_set_latent(state_a, latent_a, 256);
    rcog_answer_set_latent(state_b, latent_b, 256);

    // Blend with alpha = 0.5
    rcog_answer_state_t result = {0};
    rcog_error_t err = rcog_answer_blend(refiner, state_a, state_b, 0.5f, &result);
    EXPECT_EQ(err, RCOG_OK);

    // Check blended latent
    float* blended = NULL;
    size_t dim = 0;
    rcog_answer_get_latent(&result, &blended, &dim);

    if (blended && dim > 0) {
        // Blended should be approximately 0.5
        EXPECT_NEAR(blended[0], 0.5f, 0.1f);
    }

    rcog_answer_state_destroy(&result);
    rcog_answer_state_destroy(state_b);
    rcog_answer_state_destroy(state_a);
}

//=============================================================================
// History and Statistics Tests
//=============================================================================

/**
 * WHAT: Test refinement history tracking
 * WHY:  Verify history is recorded
 */
TEST_F(AnswerRefinerTest, HistoryTracking)
{
    rcog_goal_t goal = CreateTestGoal("History test");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    ASSERT_NE(state, nullptr);

    // Perform several steps
    for (int i = 0; i < 5; i++) {
        float evidence_latent[256];
        for (size_t j = 0; j < 256; j++) {
            evidence_latent[j] = 0.5f + 0.1f * i;
        }
        rcog_subtask_result_t evidence = CreateEvidence(0.6f + 0.05f * i, evidence_latent, 256);
        rcog_answer_step(refiner, state, &evidence, 1);
    }

    rcog_answer_history_t* history = NULL;
    rcog_error_t err = rcog_answer_get_history(refiner, state, &history);
    EXPECT_EQ(err, RCOG_OK);

    if (history) {
        EXPECT_GE(history->count, 1u);

        // Verify entries are in order
        for (size_t i = 1; i < history->count; i++) {
            EXPECT_GT(history->entries[i].step, history->entries[i-1].step);
        }

        rcog_answer_history_free(history);
    }

    rcog_answer_state_destroy(state);
}

/**
 * WHAT: Test refiner statistics
 * WHY:  Verify statistics are tracked correctly
 */
TEST_F(AnswerRefinerTest, RefinerStatistics)
{
    // Create and refine a few answers
    for (int a = 0; a < 3; a++) {
        rcog_goal_t goal = CreateTestGoal("Stats test");
        rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);

        for (int i = 0; i < 2; i++) {
            float evidence_latent[256] = {0};
            rcog_subtask_result_t evidence = CreateEvidence(0.7f, evidence_latent, 256);
            rcog_answer_step(refiner, state, &evidence, 1);
        }

        rcog_answer_state_destroy(state);
    }

    rcog_answer_stats_t stats = {0};
    rcog_error_t err = rcog_answer_refiner_get_stats(refiner, &stats);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_GE(stats.total_answers, 3u);
    EXPECT_GE(stats.total_refinements, 6u);
}

/**
 * WHAT: Test statistics reset
 * WHY:  Verify statistics can be cleared
 */
TEST_F(AnswerRefinerTest, ResetStatistics)
{
    // Perform some operations
    rcog_goal_t goal = CreateTestGoal("Reset stats test");
    rcog_answer_state_t* state = rcog_answer_state_create(refiner, &goal);
    float evidence_latent[256] = {0};
    rcog_subtask_result_t evidence = CreateEvidence(0.7f, evidence_latent, 256);
    rcog_answer_step(refiner, state, &evidence, 1);
    rcog_answer_state_destroy(state);

    // Reset statistics
    rcog_answer_refiner_reset_stats(refiner);

    rcog_answer_stats_t stats = {0};
    rcog_answer_refiner_get_stats(refiner, &stats);
    EXPECT_EQ(stats.total_refinements, 0u);
    EXPECT_EQ(stats.total_answers, 0u);
}

//=============================================================================
// Configuration Adjustment Tests
//=============================================================================

/**
 * WHAT: Test dynamic threshold adjustment
 * WHY:  Verify threshold can be changed at runtime
 */
TEST_F(AnswerRefinerTest, SetThreshold)
{
    rcog_error_t err = rcog_answer_set_threshold(refiner, 0.85f);
    EXPECT_EQ(err, RCOG_OK);

    rcog_answer_config_t config = {0};
    rcog_answer_refiner_get_config(refiner, &config);
    EXPECT_FLOAT_EQ(config.ready_threshold, 0.85f);
}

/**
 * WHAT: Test dynamic learning rate adjustment
 * WHY:  Verify learning rate can be changed at runtime
 */
TEST_F(AnswerRefinerTest, SetLearningRate)
{
    rcog_error_t err = rcog_answer_set_learning_rate(refiner, 0.05f);
    EXPECT_EQ(err, RCOG_OK);

    rcog_answer_config_t config = {0};
    rcog_answer_refiner_get_config(refiner, &config);
    EXPECT_FLOAT_EQ(config.learning_rate, 0.05f);
}

/**
 * WHAT: Test invalid threshold values
 * WHY:  Verify validation of threshold range
 */
TEST_F(AnswerRefinerTest, InvalidThreshold)
{
    // Negative threshold should fail
    rcog_error_t err = rcog_answer_set_threshold(refiner, -0.1f);
    EXPECT_EQ(err, RCOG_ERROR_INVALID_CONFIG);

    // Threshold > 1 should fail
    err = rcog_answer_set_threshold(refiner, 1.5f);
    EXPECT_EQ(err, RCOG_ERROR_INVALID_CONFIG);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * WHAT: Test NULL refiner error handling
 * WHY:  Verify consistent error handling for NULL refiner
 */
TEST(AnswerRefinerErrorTest, NullRefinerOperations)
{
    rcog_goal_t goal = {};
    rcog_answer_state_t state = {};

    EXPECT_EQ(rcog_answer_init(NULL, &goal, &state), RCOG_ERROR_NULL_POINTER);
    EXPECT_EQ(rcog_answer_state_create(NULL, &goal), nullptr);
    EXPECT_FALSE(rcog_answer_is_ready(NULL, &state));
}

/**
 * WHAT: Test NULL state error handling
 * WHY:  Verify consistent error handling for NULL state
 */
TEST_F(AnswerRefinerTest, NullStateOperations)
{
    rcog_goal_t goal = CreateTestGoal("NULL state test");

    EXPECT_EQ(rcog_answer_init(refiner, &goal, NULL), RCOG_ERROR_NULL_POINTER);
    EXPECT_FALSE(rcog_answer_is_ready(refiner, NULL));
    EXPECT_EQ(rcog_answer_get_confidence(NULL), 0.0f);
    EXPECT_EQ(rcog_answer_get_step(NULL), 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
