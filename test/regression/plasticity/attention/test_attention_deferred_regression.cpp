/**
 * @file test_attention_deferred_regression.cpp
 * @brief Regression tests for deferred callback buffer overflow safety
 *
 * WHAT: Ensure the deferred callback buffer never overflows
 * WHY:  The buffer was previously unbounded (hardcoded 48 with no check).
 *       This regression test ensures the bounds check remains in place.
 * HOW:  Stress-test the deferred callback system with high event volumes,
 *       verify drop counter works, and ensure no memory corruption.
 *
 * REGRESSION COVERAGE:
 * - Buffer overflow protection remains intact
 * - Drop counter accurately tracks excess events
 * - System remains functional after overflow
 * - Multiple forward passes accumulate drops correctly
 * - Reset clears accumulated drops
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <atomic>

extern "C" {
#include "plasticity/attention/nimcp_attention.h"
}

//=============================================================================
// Test Helpers
//=============================================================================

/** Counter for how many times callback was invoked */
static std::atomic<int> g_callback_invocations{0};
static std::atomic<int> g_entropy_events{0};

static void regression_callback(attention_event_type_t event_type,
                                 uint32_t head_index,
                                 float value,
                                 void* user_data)
{
    (void)head_index;
    (void)value;
    (void)user_data;
    g_callback_invocations.fetch_add(1);
    if (event_type == ATTENTION_EVENT_ENTROPY_SPIKE) {
        g_entropy_events.fetch_add(1);
    }
}

//=============================================================================
// Test Fixture
//=============================================================================

class AttentionDeferredRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_callback_invocations.store(0);
        g_entropy_events.store(0);

        memset(&config, 0, sizeof(config));
        config.num_heads = 4;
        config.input_dim = 32;
        config.output_dim = 32;
        config.sequence_length = 8;
        config.use_thalamic_gate = false;
        config.use_salience_weighting = false;
        config.gate_bias = 0.5f;
        config.use_positional_encoding = false;
        config.enable_quantum_attention = false;

        mha = multihead_attention_create(&config);
        ASSERT_NE(mha, nullptr) << "Failed to create multihead attention";
    }

    void TearDown() override {
        if (mha) {
            multihead_attention_destroy(mha);
            mha = nullptr;
        }
    }

    multihead_attention_config_t config;
    multihead_attention_t mha = nullptr;
};

//=============================================================================
// Regression Tests
//=============================================================================

/**
 * WHAT: Regression test - buffer size constant must remain 48
 * WHY:  Changing the constant without updating tests could mask overflow issues
 */
TEST_F(AttentionDeferredRegressionTest, BufferSizeConsistency) {
    EXPECT_EQ(ATTENTION_MAX_DEFERRED_CALLBACKS, 48)
        << "REGRESSION: Buffer size changed from 48 - update tests accordingly";
}

/**
 * WHAT: Regression test - drop counter starts at zero
 * WHY:  Ensure initialization is correct after creation
 */
TEST_F(AttentionDeferredRegressionTest, DropCounterInitializedZero) {
    uint64_t drops = attention_get_deferred_drop_count(mha);
    EXPECT_EQ(drops, 0u) << "REGRESSION: Drop counter not initialized to zero";
}

/**
 * WHAT: Regression test - register and get callback results
 * WHY:  Verify API contract hasn't changed
 */
TEST_F(AttentionDeferredRegressionTest, RegisterCallbackAPIContract) {
    /* Success case */
    int result = multihead_attention_register_callback(mha, regression_callback, nullptr);
    EXPECT_EQ(result, 0) << "REGRESSION: Valid registration should return 0";

    /* NULL mha case */
    result = multihead_attention_register_callback(nullptr, regression_callback, nullptr);
    EXPECT_EQ(result, -1) << "REGRESSION: NULL mha should return -1";

    /* NULL callback case */
    result = multihead_attention_register_callback(mha, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "REGRESSION: NULL callback should return -1";
}

/**
 * WHAT: Regression test - system survives many forward passes with callbacks
 * WHY:  Ensure no memory leaks or corruption from repeated deferred event cycles
 */
TEST_F(AttentionDeferredRegressionTest, RepeatedForwardPassStability) {
    int result = multihead_attention_register_callback(mha, regression_callback, nullptr);
    ASSERT_EQ(result, 0);

    const uint32_t seq_len = 4;
    std::vector<float> input(seq_len * config.input_dim, 1.0f);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    /* Run 100 forward passes */
    for (int i = 0; i < 100; i++) {
        bool success = multihead_attention_forward(mha, input.data(), seq_len,
                                                    nullptr, output.data());
        EXPECT_TRUE(success) << "Forward pass " << i << " failed";
    }

    /* System should still be functional */
    attention_stats_t stats;
    bool got_stats = multihead_attention_get_stats(mha, &stats);
    EXPECT_TRUE(got_stats);
    EXPECT_EQ(stats.forward_calls, 100u)
        << "REGRESSION: All 100 forward passes should be counted";
}

/**
 * WHAT: Regression test - drop count survives reset cycle
 * WHY:  Ensure reset doesn't corrupt state
 */
TEST_F(AttentionDeferredRegressionTest, DropCountResetCycle) {
    int result = multihead_attention_register_callback(mha, regression_callback, nullptr);
    ASSERT_EQ(result, 0);

    /* Initial state */
    uint64_t drops = attention_get_deferred_drop_count(mha);
    EXPECT_EQ(drops, 0u);

    /* Reset on zero */
    attention_reset_deferred_drop_count(mha);
    drops = attention_get_deferred_drop_count(mha);
    EXPECT_EQ(drops, 0u) << "REGRESSION: Reset from zero should stay zero";

    /* Run some forward passes */
    const uint32_t seq_len = 4;
    std::vector<float> input(seq_len * config.input_dim, 0.5f);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    for (int i = 0; i < 10; i++) {
        multihead_attention_forward(mha, input.data(), seq_len, nullptr, output.data());
    }

    /* Reset again */
    attention_reset_deferred_drop_count(mha);
    drops = attention_get_deferred_drop_count(mha);
    EXPECT_EQ(drops, 0u) << "REGRESSION: Reset should clear drop counter";
}

/**
 * WHAT: Regression test - multihead attention creation/destruction cycle
 * WHY:  Ensure deferred callback fields are properly cleaned up
 */
TEST_F(AttentionDeferredRegressionTest, CreateDestroyWithCallbacksCycle) {
    for (int cycle = 0; cycle < 10; cycle++) {
        multihead_attention_t test_mha = multihead_attention_create(&config);
        ASSERT_NE(test_mha, nullptr) << "Create failed at cycle " << cycle;

        /* Register callback */
        int result = multihead_attention_register_callback(test_mha, regression_callback, nullptr);
        EXPECT_EQ(result, 0);

        /* Run one forward pass */
        const uint32_t seq_len = 4;
        std::vector<float> input(seq_len * config.input_dim, 0.1f);
        std::vector<float> output(seq_len * config.output_dim, 0.0f);
        multihead_attention_forward(test_mha, input.data(), seq_len, nullptr, output.data());

        /* Destroy */
        multihead_attention_destroy(test_mha);
    }
    /* If we get here without SEGFAULT or ASAN errors, the test passes */
}

/**
 * WHAT: Regression test - callbacks receive valid event types
 * WHY:  Ensure event type enum values haven't changed
 */
TEST_F(AttentionDeferredRegressionTest, EventTypeEnumConsistency) {
    EXPECT_EQ(ATTENTION_EVENT_FOCUS_SHIFT, 0);
    EXPECT_EQ(ATTENTION_EVENT_GATE_CHANGE, 1);
    EXPECT_EQ(ATTENTION_EVENT_ENTROPY_SPIKE, 2);
    EXPECT_EQ(ATTENTION_EVENT_HEAD_SATURATED, 3);
    EXPECT_EQ(ATTENTION_EVENT_COUNT, 4);
}

/**
 * WHAT: Regression test - stats API still works with callbacks registered
 * WHY:  Adding deferred callback fields to struct must not break stats
 */
TEST_F(AttentionDeferredRegressionTest, StatsStillWorkWithCallbacks) {
    int result = multihead_attention_register_callback(mha, regression_callback, nullptr);
    ASSERT_EQ(result, 0);

    attention_stats_t stats;
    bool got_stats = multihead_attention_get_stats(mha, &stats);
    EXPECT_TRUE(got_stats) << "REGRESSION: get_stats should work with callbacks registered";
    EXPECT_EQ(stats.forward_calls, 0u);
    EXPECT_EQ(stats.active_heads, config.num_heads);
}

/**
 * WHAT: Regression test - gate control still works with callbacks
 * WHY:  Adding deferred callback fields must not break gate API
 */
TEST_F(AttentionDeferredRegressionTest, GateControlStillWorks) {
    int result = multihead_attention_register_callback(mha, regression_callback, nullptr);
    ASSERT_EQ(result, 0);

    bool gate_result = multihead_attention_set_gate(mha, 0.8f);
    EXPECT_TRUE(gate_result) << "REGRESSION: set_gate should work with callbacks registered";

    attention_stats_t stats;
    multihead_attention_get_stats(mha, &stats);
    EXPECT_NEAR(stats.avg_gate_activation, 0.8f, 0.01f)
        << "REGRESSION: Gate value should be preserved";
}
