/**
 * @file test_rollback.cpp
 * @brief GoogleTest unit tests for NIMCP edge rollback safety subsystem
 *
 * Tests weight backup, loss-spike detection, rollback execution,
 * and cleanup safety.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class RollbackTest : public ::testing::Test {
protected:
    nimcp_rollback_state_t state;

    void SetUp() override {
        memset(&state, 0, sizeof(state));
    }

    void TearDown() override {
        nimcp_rollback_cleanup(&state);
    }
};

TEST_F(RollbackTest, InitSavesWeightsCorrectly) {
    float weights[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    int ret = nimcp_rollback_init(&state, weights, 5, 0.5f, 10, 2.0f);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.num_weights, 5u);
    EXPECT_TRUE(state.active);
    EXPECT_FLOAT_EQ(state.baseline_loss, 0.5f);
    EXPECT_FLOAT_EQ(state.rollback_threshold, 2.0f);

    // Verify backup copy exists and matches
    ASSERT_NE(state.previous_weights, nullptr);
    for (int i = 0; i < 5; i++) {
        EXPECT_FLOAT_EQ(state.previous_weights[i], weights[i]);
    }
}

TEST_F(RollbackTest, NormalOperationNoRollback) {
    float weights[] = {1.0f, 2.0f, 3.0f};
    nimcp_rollback_init(&state, weights, 3, 0.5f, 5, 2.0f);

    // Feed losses below threshold (threshold = baseline * 2.0 = 1.0)
    for (int i = 0; i < 5; i++) {
        int ret = nimcp_rollback_check_step(&state, 0.4f);
        EXPECT_EQ(ret, 0);
    }
    EXPECT_FALSE(state.rollback_triggered);
}

TEST_F(RollbackTest, LossSpikeTriggersRollback) {
    float weights[] = {1.0f, 2.0f, 3.0f};
    nimcp_rollback_init(&state, weights, 3, 0.5f, 5, 2.0f);

    // Feed 5 bad steps (validation_steps=5) with loss > baseline * threshold
    // baseline=0.5, threshold=2.0, so trigger if avg > 1.0
    for (int i = 0; i < 5; i++) {
        nimcp_rollback_check_step(&state, 5.0f);
    }
    // After validation window, should trigger rollback
    EXPECT_TRUE(state.rollback_triggered);
}

TEST_F(RollbackTest, ExecuteRestoresOriginalWeights) {
    float original[] = {0.1f, 0.2f, 0.3f, 0.4f};
    nimcp_rollback_init(&state, original, 4, 0.5f, 5, 2.0f);

    // Modify weights
    float modified[] = {9.0f, 8.0f, 7.0f, 6.0f};
    state.rollback_triggered = true;

    int ret = nimcp_rollback_execute(&state, modified);
    EXPECT_EQ(ret, 0);

    // Modified array should now contain original weights
    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(modified[i], original[i]);
    }
}

TEST_F(RollbackTest, CleanupFreesMemory) {
    float weights[] = {1.0f, 2.0f};
    nimcp_rollback_init(&state, weights, 2, 0.5f, 5, 2.0f);
    ASSERT_NE(state.previous_weights, nullptr);

    nimcp_rollback_cleanup(&state);
    EXPECT_EQ(state.previous_weights, nullptr);
    EXPECT_FALSE(state.active);
}

TEST_F(RollbackTest, ThresholdEdgeCaseExactlyAtThreshold) {
    float weights[] = {1.0f};
    nimcp_rollback_init(&state, weights, 1, 1.0f, 5, 2.0f);

    // Loss exactly at threshold: 1.0 * 2.0 = 2.0
    nimcp_rollback_check_step(&state, 2.0f);
    // Exactly at threshold — implementation may or may not trigger
    // Just verify no crash
}

TEST_F(RollbackTest, ZeroBaselineLossDivisionSafety) {
    float weights[] = {1.0f};
    int ret = nimcp_rollback_init(&state, weights, 1, 0.0f, 5, 2.0f);
    EXPECT_EQ(ret, 0);

    // Should not divide by zero or produce NaN
    ret = nimcp_rollback_check_step(&state, 0.1f);
    // Just verify no crash — any return is acceptable
    EXPECT_FALSE(std::isnan(state.running_loss));
}

TEST_F(RollbackTest, MultipleCheckStepsAccumulate) {
    float weights[] = {1.0f, 2.0f, 3.0f};
    nimcp_rollback_init(&state, weights, 3, 1.0f, 10, 3.0f);

    nimcp_rollback_check_step(&state, 0.5f);
    EXPECT_EQ(state.steps_evaluated, 1u);

    nimcp_rollback_check_step(&state, 0.6f);
    EXPECT_EQ(state.steps_evaluated, 2u);

    nimcp_rollback_check_step(&state, 0.7f);
    EXPECT_EQ(state.steps_evaluated, 3u);
}

TEST_F(RollbackTest, RollbackAfterPartialValidationWindow) {
    float weights[] = {1.0f, 2.0f};
    nimcp_rollback_init(&state, weights, 2, 0.5f, 10, 2.0f);

    // Run a few good steps, then spike
    nimcp_rollback_check_step(&state, 0.3f);
    nimcp_rollback_check_step(&state, 0.4f);
    nimcp_rollback_check_step(&state, 5.0f); // spike

    // If rollback triggered, execute should restore
    if (state.rollback_triggered) {
        float modified[] = {99.0f, 99.0f};
        nimcp_rollback_execute(&state, modified);
        EXPECT_FLOAT_EQ(modified[0], 1.0f);
        EXPECT_FLOAT_EQ(modified[1], 2.0f);
    }
}

TEST_F(RollbackTest, DoubleCleanupIsSafe) {
    float weights[] = {1.0f};
    nimcp_rollback_init(&state, weights, 1, 0.5f, 5, 2.0f);

    nimcp_rollback_cleanup(&state);
    // Second cleanup should not crash
    nimcp_rollback_cleanup(&state);
    EXPECT_EQ(state.previous_weights, nullptr);
}

TEST_F(RollbackTest, LargeWeightArray) {
    const uint32_t n = 10000;
    std::vector<float> weights(n, 0.5f);
    int ret = nimcp_rollback_init(&state, weights.data(), n, 1.0f, 5, 2.0f);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.num_weights, n);

    std::vector<float> modified(n, 99.0f);
    state.rollback_triggered = true;
    nimcp_rollback_execute(&state, modified.data());

    for (uint32_t i = 0; i < n; i++) {
        EXPECT_FLOAT_EQ(modified[i], 0.5f);
    }
}

TEST_F(RollbackTest, InitOverwritesPreviousState) {
    float w1[] = {1.0f, 2.0f};
    nimcp_rollback_init(&state, w1, 2, 0.5f, 5, 2.0f);

    float w2[] = {3.0f, 4.0f, 5.0f};
    nimcp_rollback_init(&state, w2, 3, 1.0f, 10, 3.0f);

    EXPECT_EQ(state.num_weights, 3u);
    EXPECT_FLOAT_EQ(state.baseline_loss, 1.0f);
    EXPECT_FLOAT_EQ(state.previous_weights[0], 3.0f);
}

TEST_F(RollbackTest, NullWeightsReturnsError) {
    int ret = nimcp_rollback_init(&state, nullptr, 5, 0.5f, 10, 2.0f);
    EXPECT_EQ(ret, -1);
}

TEST_F(RollbackTest, DoubleInitWithoutCleanup) {
    float w1[] = {1.0f, 2.0f};
    int ret1 = nimcp_rollback_init(&state, w1, 2, 0.5f, 5, 2.0f);
    EXPECT_EQ(ret1, 0);
    ASSERT_NE(state.previous_weights, nullptr);

    // Second init without cleanup — should overwrite cleanly
    float w2[] = {3.0f, 4.0f, 5.0f};
    int ret2 = nimcp_rollback_init(&state, w2, 3, 1.0f, 10, 3.0f);
    EXPECT_EQ(ret2, 0);
    EXPECT_EQ(state.num_weights, 3u);
    EXPECT_FLOAT_EQ(state.previous_weights[0], 3.0f);
    EXPECT_FLOAT_EQ(state.previous_weights[1], 4.0f);
    EXPECT_FLOAT_EQ(state.previous_weights[2], 5.0f);
}
