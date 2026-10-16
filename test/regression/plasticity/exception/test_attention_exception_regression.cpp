/**
 * @file test_attention_exception_regression.cpp
 * @brief Regression tests for multi-attention exception handling stability
 *
 * WHAT: Verify exception handling API contracts remain stable across versions
 * WHY:  Prevent regressions in error handling behavior and return value contracts
 * HOW:  Test API stability, error code consistency, boundary conditions
 *
 * REGRESSION COVERAGE:
 * - API contract verification for all exception-throwing functions
 * - Error code consistency (NULL returns, -1 returns)
 * - Return value contract enforcement
 * - Boundary condition stability
 * - Long-running stability under repeated exceptions
 *
 * @author NIMCP Development Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>

#include "plasticity/attention/nimcp_attention.h"
#include "utils/exception/nimcp_exception.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Regression test fixture for attention exception stability
 */
class AttentionExceptionRegressionTest : public ::testing::Test {
protected:
    multihead_attention_t mha = nullptr;
    ternary_attention_ctx_t* ternary_ctx = nullptr;

    void TearDown() override {
        if (ternary_ctx) {
            ternary_attention_destroy(ternary_ctx);
            ternary_ctx = nullptr;
        }
        if (mha) {
            multihead_attention_destroy(mha);
            mha = nullptr;
        }
    }

    multihead_attention_config_t create_valid_config() {
        multihead_attention_config_t config = {};
        config.num_heads = 4;
        config.input_dim = 128;
        config.output_dim = 128;
        config.sequence_length = 16;
        config.use_thalamic_gate = true;
        config.gate_bias = 0.5f;
        return config;
    }
};

//=============================================================================
// API Contract Regression Tests - Attention Head
//=============================================================================

TEST_F(AttentionExceptionRegressionTest, AttentionHeadCreateNullContractStability) {
    /**
     * REGRESSION: attention_head_create(NULL) MUST return NULL
     * Contract: NULL config -> NULL return
     */
    attention_head_t head = attention_head_create(nullptr);
    EXPECT_EQ(head, nullptr) << "REGRESSION: NULL config must return NULL";
}

TEST_F(AttentionExceptionRegressionTest, AttentionHeadCreateZeroDimContractStability) {
    /**
     * REGRESSION: Zero dimensions MUST return NULL
     * Contract: input_dim == 0 OR output_dim == 0 -> NULL return
     */
    attention_head_config_t config = {};
    config.key_dim = 64;
    config.value_dim = 64;
    config.temperature = 1.0f;

    // Test input_dim = 0
    config.input_dim = 0;
    config.output_dim = 64;
    attention_head_t head = attention_head_create(&config);
    EXPECT_EQ(head, nullptr) << "REGRESSION: input_dim=0 must return NULL";

    // Test output_dim = 0
    config.input_dim = 64;
    config.output_dim = 0;
    head = attention_head_create(&config);
    EXPECT_EQ(head, nullptr) << "REGRESSION: output_dim=0 must return NULL";
}

TEST_F(AttentionExceptionRegressionTest, AttentionHeadDestroyNullContractStability) {
    /**
     * REGRESSION: attention_head_destroy(NULL) MUST NOT crash
     * Contract: NULL head -> no-op, no crash
     */
    attention_head_destroy(nullptr);
    SUCCEED() << "REGRESSION: destroy(NULL) must not crash";
}

TEST_F(AttentionExceptionRegressionTest, AttentionHeadForwardNullContractStability) {
    /**
     * REGRESSION: attention_head_forward with NULL params MUST return false
     * Contract: Any NULL required param -> false return
     */
    attention_head_config_t config = {};
    config.input_dim = 64;
    config.output_dim = 64;
    config.key_dim = 64;
    config.value_dim = 64;
    config.temperature = 1.0f;

    attention_head_t head = attention_head_create(&config);
    ASSERT_NE(head, nullptr);

    const uint32_t seq_len = 4;
    std::vector<float> data(seq_len * 64, 0.5f);
    std::vector<float> output(seq_len * 64, 0.0f);

    // NULL head
    EXPECT_FALSE(attention_head_forward(nullptr, data.data(), data.data(),
                                        data.data(), seq_len, output.data(),
                                        nullptr, nullptr, 0))
        << "REGRESSION: NULL head must return false";

    // NULL query
    EXPECT_FALSE(attention_head_forward(head, nullptr, data.data(),
                                        data.data(), seq_len, output.data(),
                                        nullptr, nullptr, 0))
        << "REGRESSION: NULL query must return false";

    // NULL key
    EXPECT_FALSE(attention_head_forward(head, data.data(), nullptr,
                                        data.data(), seq_len, output.data(),
                                        nullptr, nullptr, 0))
        << "REGRESSION: NULL key must return false";

    // NULL value
    EXPECT_FALSE(attention_head_forward(head, data.data(), data.data(),
                                        nullptr, seq_len, output.data(),
                                        nullptr, nullptr, 0))
        << "REGRESSION: NULL value must return false";

    // NULL output
    EXPECT_FALSE(attention_head_forward(head, data.data(), data.data(),
                                        data.data(), seq_len, nullptr,
                                        nullptr, nullptr, 0))
        << "REGRESSION: NULL output must return false";

    // seq_len = 0
    EXPECT_FALSE(attention_head_forward(head, data.data(), data.data(),
                                        data.data(), 0, output.data(),
                                        nullptr, nullptr, 0))
        << "REGRESSION: seq_len=0 must return false";

    attention_head_destroy(head);
}

//=============================================================================
// API Contract Regression Tests - Multihead Attention
//=============================================================================

TEST_F(AttentionExceptionRegressionTest, MultiheadCreateNullContractStability) {
    /**
     * REGRESSION: multihead_attention_create(NULL) MUST return NULL
     * Contract: NULL config -> NULL return
     */
    mha = multihead_attention_create(nullptr);
    EXPECT_EQ(mha, nullptr) << "REGRESSION: NULL config must return NULL";
}

TEST_F(AttentionExceptionRegressionTest, MultiheadCreateInvalidConfigContractStability) {
    /**
     * REGRESSION: Invalid configs MUST return NULL
     * Contract: Various invalid config combinations -> NULL return
     */
    multihead_attention_config_t config = create_valid_config();

    // num_heads = 0
    config.num_heads = 0;
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "REGRESSION: num_heads=0 must return NULL";

    // input_dim = 0
    config = create_valid_config();
    config.input_dim = 0;
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "REGRESSION: input_dim=0 must return NULL";

    // output_dim = 0
    config = create_valid_config();
    config.output_dim = 0;
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "REGRESSION: output_dim=0 must return NULL";

    // sequence_length = 0
    config = create_valid_config();
    config.sequence_length = 0;
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "REGRESSION: sequence_length=0 must return NULL";

    // Non-divisible dimensions
    config = create_valid_config();
    config.num_heads = 4;
    config.input_dim = 127;
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "REGRESSION: non-divisible dims must return NULL";
}

TEST_F(AttentionExceptionRegressionTest, MultiheadDestroyNullContractStability) {
    /**
     * REGRESSION: multihead_attention_destroy(NULL) MUST NOT crash
     * Contract: NULL mha -> no-op, no crash
     */
    multihead_attention_destroy(nullptr);
    SUCCEED() << "REGRESSION: destroy(NULL) must not crash";
}

TEST_F(AttentionExceptionRegressionTest, MultiheadForwardNullContractStability) {
    /**
     * REGRESSION: multihead_attention_forward with NULL params MUST return false
     * Contract: Any NULL required param -> false return
     */
    auto config = create_valid_config();
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    const uint32_t seq_len = 8;
    std::vector<float> input(seq_len * config.input_dim, 0.5f);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    // NULL mha
    EXPECT_FALSE(multihead_attention_forward(nullptr, input.data(), seq_len,
                                             nullptr, output.data()))
        << "REGRESSION: NULL mha must return false";

    // NULL input
    EXPECT_FALSE(multihead_attention_forward(mha, nullptr, seq_len,
                                             nullptr, output.data()))
        << "REGRESSION: NULL input must return false";

    // NULL output
    EXPECT_FALSE(multihead_attention_forward(mha, input.data(), seq_len,
                                             nullptr, nullptr))
        << "REGRESSION: NULL output must return false";

    // seq_len = 0
    EXPECT_FALSE(multihead_attention_forward(mha, input.data(), 0,
                                             nullptr, output.data()))
        << "REGRESSION: seq_len=0 must return false";

    // seq_len > max
    EXPECT_FALSE(multihead_attention_forward(mha, input.data(),
                                             config.sequence_length + 1,
                                             nullptr, output.data()))
        << "REGRESSION: seq_len>max must return false";
}

TEST_F(AttentionExceptionRegressionTest, MultiheadSetGateNullContractStability) {
    /**
     * REGRESSION: multihead_attention_set_gate(NULL, ...) MUST return false
     * Contract: NULL mha -> false return
     */
    EXPECT_FALSE(multihead_attention_set_gate(nullptr, 0.5f))
        << "REGRESSION: NULL mha must return false";
}

TEST_F(AttentionExceptionRegressionTest, MultiheadGetStatsNullContractStability) {
    /**
     * REGRESSION: multihead_attention_get_stats with NULL params MUST return false
     * Contract: NULL mha OR NULL stats -> false return
     */
    attention_stats_t stats;

    EXPECT_FALSE(multihead_attention_get_stats(nullptr, &stats))
        << "REGRESSION: NULL mha must return false";

    auto config = create_valid_config();
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    EXPECT_FALSE(multihead_attention_get_stats(mha, nullptr))
        << "REGRESSION: NULL stats must return false";
}

TEST_F(AttentionExceptionRegressionTest, MultiheadResetStatsNullContractStability) {
    /**
     * REGRESSION: multihead_attention_reset_stats(NULL) MUST NOT crash
     * Contract: NULL mha -> no-op, no crash
     */
    multihead_attention_reset_stats(nullptr);
    SUCCEED() << "REGRESSION: reset_stats(NULL) must not crash";
}

TEST_F(AttentionExceptionRegressionTest, MultiheadGetStrengthNullContractStability) {
    /**
     * REGRESSION: multihead_attention_get_strength(NULL) MUST return 0.0f
     * Contract: NULL mha -> 0.0f return
     */
    float strength = multihead_attention_get_strength(nullptr);
    EXPECT_FLOAT_EQ(strength, 0.0f)
        << "REGRESSION: NULL mha must return 0.0f";
}

TEST_F(AttentionExceptionRegressionTest, MultiheadSetPeTypeNullContractStability) {
    /**
     * REGRESSION: multihead_attention_set_pe_type(NULL, ...) MUST return false
     * Contract: NULL mha -> false return
     */
    EXPECT_FALSE(multihead_attention_set_pe_type(nullptr, NIMCP_POS_ROTARY))
        << "REGRESSION: NULL mha must return false";
}

//=============================================================================
// API Contract Regression Tests - Utility Functions
//=============================================================================

TEST_F(AttentionExceptionRegressionTest, ComputeEntropyNullContractStability) {
    /**
     * REGRESSION: attention_compute_entropy with invalid params MUST return 0.0f
     * Contract: NULL weights OR seq_len=0 -> 0.0f return
     */
    EXPECT_FLOAT_EQ(attention_compute_entropy(nullptr, 16), 0.0f)
        << "REGRESSION: NULL weights must return 0.0f";

    std::vector<float> weights(16, 0.0625f);
    EXPECT_FLOAT_EQ(attention_compute_entropy(weights.data(), 0), 0.0f)
        << "REGRESSION: seq_len=0 must return 0.0f";
}

TEST_F(AttentionExceptionRegressionTest, ValidateConfigNullContractStability) {
    /**
     * REGRESSION: attention_validate_config(NULL) MUST return false
     * Contract: NULL config -> false return
     */
    EXPECT_FALSE(attention_validate_config(nullptr))
        << "REGRESSION: NULL config must return false";
}

//=============================================================================
// API Contract Regression Tests - Ternary Attention
//=============================================================================

TEST_F(AttentionExceptionRegressionTest, TernaryDefaultConfigNullContractStability) {
    /**
     * REGRESSION: ternary_attention_default_config(NULL) MUST return -1
     * Contract: NULL config -> -1 return
     */
    EXPECT_EQ(ternary_attention_default_config(nullptr), -1)
        << "REGRESSION: NULL config must return -1";
}

TEST_F(AttentionExceptionRegressionTest, TernaryCreateNullContractStability) {
    /**
     * REGRESSION: ternary_attention_create(NULL) MUST return NULL
     * Contract: NULL config -> NULL return
     */
    ternary_ctx = ternary_attention_create(nullptr);
    EXPECT_EQ(ternary_ctx, nullptr) << "REGRESSION: NULL config must return NULL";
}

TEST_F(AttentionExceptionRegressionTest, TernaryDestroyNullContractStability) {
    /**
     * REGRESSION: ternary_attention_destroy(NULL) MUST NOT crash
     * Contract: NULL ctx -> no-op, no crash
     */
    ternary_attention_destroy(nullptr);
    SUCCEED() << "REGRESSION: destroy(NULL) must not crash";
}

TEST_F(AttentionExceptionRegressionTest, TernaryDiscretizeNullContractStability) {
    /**
     * REGRESSION: ternary_attention_discretize with NULL params MUST return -1
     * Contract: Any NULL required param OR seq_len=0 -> -1 return
     */
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    const uint32_t seq_len = 8;
    std::vector<float> soft_attention(seq_len, 0.5f);
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    // NULL ctx
    EXPECT_EQ(ternary_attention_discretize(nullptr, soft_attention.data(),
                                           seq_len, ternary_out.data()), -1)
        << "REGRESSION: NULL ctx must return -1";

    // NULL soft_attention
    EXPECT_EQ(ternary_attention_discretize(ternary_ctx, nullptr,
                                           seq_len, ternary_out.data()), -1)
        << "REGRESSION: NULL soft_attention must return -1";

    // NULL ternary_out
    EXPECT_EQ(ternary_attention_discretize(ternary_ctx, soft_attention.data(),
                                           seq_len, nullptr), -1)
        << "REGRESSION: NULL ternary_out must return -1";

    // seq_len = 0
    EXPECT_EQ(ternary_attention_discretize(ternary_ctx, soft_attention.data(),
                                           0, ternary_out.data()), -1)
        << "REGRESSION: seq_len=0 must return -1";
}

TEST_F(AttentionExceptionRegressionTest, TernaryApplyNullContractStability) {
    /**
     * REGRESSION: ternary_attention_apply with NULL params MUST return -1
     * Contract: Any NULL required param OR zero dim/seq_len -> -1 return
     */
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    const uint32_t seq_len = 8;
    const uint32_t dim = 64;
    std::vector<float> values(seq_len * dim, 1.0f);
    std::vector<ternary_attention_state_t> ternary_attention(seq_len, ATTENTION_NEUTRAL);
    std::vector<float> output(seq_len * dim, 0.0f);

    // NULL ctx
    EXPECT_EQ(ternary_attention_apply(nullptr, values.data(),
                                      ternary_attention.data(),
                                      seq_len, dim, output.data()), -1)
        << "REGRESSION: NULL ctx must return -1";

    // NULL values
    EXPECT_EQ(ternary_attention_apply(ternary_ctx, nullptr,
                                      ternary_attention.data(),
                                      seq_len, dim, output.data()), -1)
        << "REGRESSION: NULL values must return -1";

    // NULL ternary_attention
    EXPECT_EQ(ternary_attention_apply(ternary_ctx, values.data(),
                                      nullptr,
                                      seq_len, dim, output.data()), -1)
        << "REGRESSION: NULL ternary_attention must return -1";

    // NULL output
    EXPECT_EQ(ternary_attention_apply(ternary_ctx, values.data(),
                                      ternary_attention.data(),
                                      seq_len, dim, nullptr), -1)
        << "REGRESSION: NULL output must return -1";

    // seq_len = 0
    EXPECT_EQ(ternary_attention_apply(ternary_ctx, values.data(),
                                      ternary_attention.data(),
                                      0, dim, output.data()), -1)
        << "REGRESSION: seq_len=0 must return -1";

    // dim = 0
    EXPECT_EQ(ternary_attention_apply(ternary_ctx, values.data(),
                                      ternary_attention.data(),
                                      seq_len, 0, output.data()), -1)
        << "REGRESSION: dim=0 must return -1";
}

TEST_F(AttentionExceptionRegressionTest, TernaryBackwardNullContractStability) {
    /**
     * REGRESSION: ternary_attention_backward with NULL params MUST return -1
     * Contract: Any NULL required param OR zero dim/seq_len -> -1 return
     */
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    const uint32_t seq_len = 8;
    const uint32_t dim = 64;
    std::vector<float> grad_output(seq_len * dim, 1.0f);
    std::vector<float> soft_attention(seq_len, 0.5f);
    std::vector<float> grad_attention(seq_len, 0.0f);

    // NULL ctx
    EXPECT_EQ(ternary_attention_backward(nullptr, grad_output.data(),
                                         soft_attention.data(), seq_len, dim,
                                         grad_attention.data()), -1)
        << "REGRESSION: NULL ctx must return -1";

    // NULL grad_output
    EXPECT_EQ(ternary_attention_backward(ternary_ctx, nullptr,
                                         soft_attention.data(), seq_len, dim,
                                         grad_attention.data()), -1)
        << "REGRESSION: NULL grad_output must return -1";

    // NULL soft_attention
    EXPECT_EQ(ternary_attention_backward(ternary_ctx, grad_output.data(),
                                         nullptr, seq_len, dim,
                                         grad_attention.data()), -1)
        << "REGRESSION: NULL soft_attention must return -1";

    // NULL grad_attention
    EXPECT_EQ(ternary_attention_backward(ternary_ctx, grad_output.data(),
                                         soft_attention.data(), seq_len, dim,
                                         nullptr), -1)
        << "REGRESSION: NULL grad_attention must return -1";

    // seq_len = 0
    EXPECT_EQ(ternary_attention_backward(ternary_ctx, grad_output.data(),
                                         soft_attention.data(), 0, dim,
                                         grad_attention.data()), -1)
        << "REGRESSION: seq_len=0 must return -1";

    // dim = 0
    EXPECT_EQ(ternary_attention_backward(ternary_ctx, grad_output.data(),
                                         soft_attention.data(), seq_len, 0,
                                         grad_attention.data()), -1)
        << "REGRESSION: dim=0 must return -1";
}

TEST_F(AttentionExceptionRegressionTest, TernaryGetTemperatureNullContractStability) {
    /**
     * REGRESSION: ternary_attention_get_temperature(NULL) MUST return 1.0f
     * Contract: NULL ctx -> 1.0f return (default temperature)
     */
    float temp = ternary_attention_get_temperature(nullptr);
    EXPECT_FLOAT_EQ(temp, 1.0f)
        << "REGRESSION: NULL ctx must return 1.0f";
}

TEST_F(AttentionExceptionRegressionTest, TernaryGetStatsNullContractStability) {
    /**
     * REGRESSION: ternary_attention_get_stats with NULL params MUST return -1
     * Contract: NULL ctx OR NULL stats -> -1 return
     */
    ternary_attention_stats_t stats;

    EXPECT_EQ(ternary_attention_get_stats(nullptr, &stats), -1)
        << "REGRESSION: NULL ctx must return -1";

    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    EXPECT_EQ(ternary_attention_get_stats(ternary_ctx, nullptr), -1)
        << "REGRESSION: NULL stats must return -1";
}

TEST_F(AttentionExceptionRegressionTest, TernaryTopKNullContractStability) {
    /**
     * REGRESSION: ternary_attention_top_k with NULL params MUST return -1
     * Contract: NULL soft_attention OR NULL ternary_out OR seq_len=0 -> -1 return
     */
    const uint32_t seq_len = 8;
    std::vector<float> soft_attention(seq_len, 0.5f);
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    // NULL soft_attention
    EXPECT_EQ(ternary_attention_top_k(nullptr, seq_len, 3, ternary_out.data()), -1)
        << "REGRESSION: NULL soft_attention must return -1";

    // NULL ternary_out
    EXPECT_EQ(ternary_attention_top_k(soft_attention.data(), seq_len, 3, nullptr), -1)
        << "REGRESSION: NULL ternary_out must return -1";

    // seq_len = 0
    EXPECT_EQ(ternary_attention_top_k(soft_attention.data(), 0, 3, ternary_out.data()), -1)
        << "REGRESSION: seq_len=0 must return -1";
}

//=============================================================================
// Long-Running Stability Tests
//=============================================================================

TEST_F(AttentionExceptionRegressionTest, RepeatedExceptionStability) {
    /**
     * REGRESSION: System must remain stable after many repeated exceptions
     * Contract: Exceptions should not cause memory leaks or state corruption
     */
    constexpr int num_iterations = 1000;

    for (int i = 0; i < num_iterations; i++) {
        // Trigger exception
        attention_head_t head = attention_head_create(nullptr);
        EXPECT_EQ(head, nullptr);

        // Trigger multihead exception
        mha = multihead_attention_create(nullptr);
        EXPECT_EQ(mha, nullptr);

        // Trigger ternary exception
        ternary_ctx = ternary_attention_create(nullptr);
        EXPECT_EQ(ternary_ctx, nullptr);
    }

    // After many exceptions, verify normal operations still work
    attention_head_config_t head_config = {};
    head_config.input_dim = 32;
    head_config.output_dim = 32;
    head_config.key_dim = 32;
    head_config.value_dim = 32;
    head_config.temperature = 1.0f;

    attention_head_t head = attention_head_create(&head_config);
    ASSERT_NE(head, nullptr) << "REGRESSION: Must still create valid head after many exceptions";
    attention_head_destroy(head);

    auto config = create_valid_config();
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr) << "REGRESSION: Must still create valid mha after many exceptions";

    ternary_attention_config_t ternary_config;
    ternary_attention_default_config(&ternary_config);
    ternary_ctx = ternary_attention_create(&ternary_config);
    ASSERT_NE(ternary_ctx, nullptr) << "REGRESSION: Must still create valid ternary after many exceptions";
}

TEST_F(AttentionExceptionRegressionTest, ConcurrentExceptionStability) {
    /**
     * REGRESSION: Concurrent exceptions must not cause race conditions
     * Contract: Thread-safe exception handling
     */
    std::atomic<int> failure_count{0};
    constexpr int num_threads = 8;
    constexpr int iterations_per_thread = 500;

    auto worker = [&]() {
        for (int i = 0; i < iterations_per_thread; i++) {
            // All these should return NULL/false consistently
            if (attention_head_create(nullptr) != nullptr) failure_count++;
            if (multihead_attention_create(nullptr) != nullptr) failure_count++;
            if (ternary_attention_create(nullptr) != nullptr) failure_count++;
            if (multihead_attention_set_gate(nullptr, 0.5f) != false) failure_count++;
            if (attention_validate_config(nullptr) != false) failure_count++;
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(worker);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(failure_count.load(), 0)
        << "REGRESSION: Concurrent exceptions must return consistent values";
}

TEST_F(AttentionExceptionRegressionTest, ExceptionRecoveryStability) {
    /**
     * REGRESSION: System must recover fully after exception
     * Contract: Post-exception state must be clean
     */
    // Trigger exception
    mha = multihead_attention_create(nullptr);
    EXPECT_EQ(mha, nullptr);

    // Create valid mha
    auto config = create_valid_config();
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    // Trigger forward exception
    bool result = multihead_attention_forward(mha, nullptr, 8, nullptr, nullptr);
    EXPECT_FALSE(result);

    // Verify forward still works after exception
    const uint32_t seq_len = 8;
    std::vector<float> input(seq_len * config.input_dim, 0.5f);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    result = multihead_attention_forward(mha, input.data(), seq_len, nullptr, output.data());
    EXPECT_TRUE(result) << "REGRESSION: Forward must work after exception recovery";

    // Verify output is non-zero
    float sum = 0.0f;
    for (size_t i = 0; i < output.size(); i++) {
        sum += output[i];
    }
    EXPECT_NE(sum, 0.0f) << "REGRESSION: Output must be non-zero after recovery";
}

//=============================================================================
// Boundary Condition Regression Tests
//=============================================================================

TEST_F(AttentionExceptionRegressionTest, BoundaryDimensionStability) {
    /**
     * REGRESSION: Boundary dimension values must be handled consistently
     * Contract: Very small and very large dimensions behave predictably
     */
    // Single dimension
    attention_head_config_t config = {};
    config.input_dim = 1;
    config.output_dim = 1;
    config.key_dim = 1;
    config.value_dim = 1;
    config.temperature = 1.0f;

    attention_head_t head = attention_head_create(&config);
    EXPECT_NE(head, nullptr) << "REGRESSION: Single dimension must work";
    attention_head_destroy(head);

    // Large power-of-2 dimension
    config.input_dim = 1024;
    config.output_dim = 1024;
    config.key_dim = 1024;
    config.value_dim = 1024;

    head = attention_head_create(&config);
    EXPECT_NE(head, nullptr) << "REGRESSION: Large dimension must work";
    if (head) attention_head_destroy(head);

    // Odd dimension
    config.input_dim = 63;
    config.output_dim = 63;
    config.key_dim = 63;
    config.value_dim = 63;

    head = attention_head_create(&config);
    EXPECT_NE(head, nullptr) << "REGRESSION: Odd dimension must work";
    if (head) attention_head_destroy(head);
}

TEST_F(AttentionExceptionRegressionTest, TernaryTopKBoundaryStability) {
    /**
     * REGRESSION: Top-k boundary values must be handled consistently
     * Contract: k=0 -> clamped to 1, k>seq_len -> clamped to seq_len
     */
    const uint32_t seq_len = 8;
    std::vector<float> soft_attention = {0.1f, 0.8f, 0.5f, 0.3f, 0.9f, 0.2f, 0.7f, 0.4f};
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    // k = 0 -> should be clamped to 1
    int result = ternary_attention_top_k(soft_attention.data(), seq_len, 0, ternary_out.data());
    EXPECT_EQ(result, 0) << "REGRESSION: k=0 must succeed (clamped to 1)";

    int focus_count = 0;
    for (uint32_t i = 0; i < seq_len; i++) {
        if (ternary_out[i] == ATTENTION_FOCUS) focus_count++;
    }
    EXPECT_EQ(focus_count, 1) << "REGRESSION: k=0 must result in 1 FOCUS";

    // k > seq_len -> should be clamped to seq_len
    result = ternary_attention_top_k(soft_attention.data(), seq_len, 100, ternary_out.data());
    EXPECT_EQ(result, 0) << "REGRESSION: k>seq_len must succeed (clamped)";

    focus_count = 0;
    for (uint32_t i = 0; i < seq_len; i++) {
        if (ternary_out[i] == ATTENTION_FOCUS) focus_count++;
    }
    EXPECT_EQ(focus_count, static_cast<int>(seq_len))
        << "REGRESSION: k>seq_len must result in all FOCUS";
}
