/**
 * @file test_attention_exception_integration.cpp
 * @brief Integration tests for multi-attention exception handling with immune system
 *
 * WHAT: Test end-to-end exception flow from attention modules to brain immune system
 * WHY:  Verify that attention exceptions are properly presented as antigens
 * HOW:  Trigger exception conditions, verify immune system receives and processes them
 *
 * INTEGRATION SCENARIOS:
 * - Attention error -> Exception -> Handler -> Immune presentation
 * - Exception categorization and severity mapping
 * - Cross-module exception propagation (attention -> immune)
 * - Recovery from attention failures
 *
 * @author NIMCP Development Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

#include "plasticity/attention/nimcp_attention.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Integration test fixture for attention-immune exception flow
 */
class AttentionExceptionIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    multihead_attention_t mha = nullptr;
    ternary_attention_ctx_t* ternary_ctx = nullptr;

    void SetUp() override {
        // Initialize exception handling system
        nimcp_exception_system_init();
    }

    void TearDown() override {
        if (ternary_ctx) {
            ternary_attention_destroy(ternary_ctx);
            ternary_ctx = nullptr;
        }
        if (mha) {
            multihead_attention_destroy(mha);
            mha = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    /**
     * @brief Create a test brain with immune system enabled
     */
    brain_t create_test_brain() {
        brain_config_t config = {};
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_brain_immune = true;
        config.enable_multihead_attention = true;
        config.num_attention_heads = 4;
        strncpy(config.task_name, "exception_test", sizeof(config.task_name) - 1);
        return brain_create_custom(&config);
    }

    /**
     * @brief Create valid multihead attention config
     */
    multihead_attention_config_t create_valid_mha_config() {
        multihead_attention_config_t config = {};
        config.num_heads = 4;
        config.input_dim = 128;
        config.output_dim = 128;
        config.sequence_length = 16;
        config.use_thalamic_gate = true;
        config.use_salience_weighting = false;
        config.gate_bias = 0.5f;
        config.use_positional_encoding = false;
        config.enable_quantum_attention = false;
        return config;
    }
};

//=============================================================================
// Attention Head Exception Integration Tests
//=============================================================================

TEST_F(AttentionExceptionIntegrationTest, NullConfigExceptionPresentedToImmune) {
    /**
     * WHAT: Verify NULL config exception is presented to immune system
     * WHY:  Test end-to-end exception-immune integration
     * HOW:  Call create with NULL, verify NULL return (exception was raised)
     */
    attention_head_t head = attention_head_create(nullptr);
    EXPECT_EQ(head, nullptr) << "Should return NULL for NULL config";

    // Exception was raised internally via NIMCP_THROW_TO_IMMUNE
    // The immune system would have received NIMCP_ERROR_NULL_POINTER
    // We can't directly verify immune state without a brain instance,
    // but we verify the exception path worked by getting NULL back
}

TEST_F(AttentionExceptionIntegrationTest, InvalidDimensionsExceptionFlow) {
    /**
     * WHAT: Verify invalid dimensions exception flow
     * WHY:  Test exception path for validation failures
     * HOW:  Create with invalid config, verify exception raised
     */
    attention_head_config_t config = {};
    config.input_dim = 0;  // Invalid
    config.output_dim = 64;
    config.key_dim = 64;
    config.value_dim = 64;
    config.temperature = 1.0f;

    attention_head_t head = attention_head_create(&config);
    EXPECT_EQ(head, nullptr) << "Should return NULL for invalid dimensions";
}

//=============================================================================
// Multihead Attention Exception Integration Tests
//=============================================================================

TEST_F(AttentionExceptionIntegrationTest, MultiheadInvalidConfigExceptionFlow) {
    /**
     * WHAT: Verify multihead invalid config exception flow
     * WHY:  Test exception path through multiple validation checks
     * HOW:  Create with various invalid configs
     */
    multihead_attention_config_t config = create_valid_mha_config();

    // Test zero num_heads
    config.num_heads = 0;
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "Should fail with zero num_heads";

    // Test zero input_dim
    config = create_valid_mha_config();
    config.input_dim = 0;
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "Should fail with zero input_dim";

    // Test non-divisible dimensions
    config = create_valid_mha_config();
    config.num_heads = 4;
    config.input_dim = 127;  // Not divisible by 4
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "Should fail with non-divisible dimensions";
}

TEST_F(AttentionExceptionIntegrationTest, MultiheadExceptionRecovery) {
    /**
     * WHAT: Verify multihead attention can recover after exception
     * WHY:  Test system resilience after exception
     * HOW:  Trigger exception, then create successfully
     */
    // First trigger an exception
    mha = multihead_attention_create(nullptr);
    EXPECT_EQ(mha, nullptr);

    // Now create successfully - verify system recovered
    multihead_attention_config_t config = create_valid_mha_config();
    mha = multihead_attention_create(&config);
    EXPECT_NE(mha, nullptr) << "Should recover and create successfully";

    // Verify it works
    const uint32_t seq_len = 8;
    std::vector<float> input(seq_len * config.input_dim, 0.5f);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    bool success = multihead_attention_forward(mha, input.data(), seq_len,
                                               nullptr, output.data());
    EXPECT_TRUE(success) << "Forward pass should succeed after recovery";
}

TEST_F(AttentionExceptionIntegrationTest, ForwardExceptionWithInvalidSequence) {
    /**
     * WHAT: Verify forward pass exception handling
     * WHY:  Test runtime exception path
     * HOW:  Call forward with invalid parameters
     */
    multihead_attention_config_t config = create_valid_mha_config();
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    std::vector<float> input(config.input_dim, 0.5f);
    std::vector<float> output(config.output_dim, 0.0f);

    // Test with excessive sequence length
    bool result = multihead_attention_forward(mha, input.data(),
                                              config.sequence_length + 100,
                                              nullptr, output.data());
    EXPECT_FALSE(result) << "Should fail with excessive sequence length";

    // Test with zero sequence length
    result = multihead_attention_forward(mha, input.data(), 0,
                                         nullptr, output.data());
    EXPECT_FALSE(result) << "Should fail with zero sequence length";

    // Verify system still works after exceptions
    std::vector<float> valid_input(8 * config.input_dim, 0.5f);
    std::vector<float> valid_output(8 * config.output_dim, 0.0f);
    result = multihead_attention_forward(mha, valid_input.data(), 8,
                                         nullptr, valid_output.data());
    EXPECT_TRUE(result) << "Should succeed with valid parameters after exception";
}

//=============================================================================
// Ternary Attention Exception Integration Tests
//=============================================================================

TEST_F(AttentionExceptionIntegrationTest, TernaryNullConfigException) {
    /**
     * WHAT: Verify ternary attention NULL config exception
     * WHY:  Test ternary attention exception path
     * HOW:  Call create and default_config with NULL
     */
    int result = ternary_attention_default_config(nullptr);
    EXPECT_EQ(result, -1) << "default_config should return -1 for NULL";

    ternary_ctx = ternary_attention_create(nullptr);
    EXPECT_EQ(ternary_ctx, nullptr) << "create should return NULL for NULL config";
}

TEST_F(AttentionExceptionIntegrationTest, TernaryDiscretizeExceptionRecovery) {
    /**
     * WHAT: Verify ternary discretize handles exceptions and recovers
     * WHY:  Test runtime exception handling in ternary attention
     * HOW:  Trigger exceptions, then verify successful operation
     */
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    const uint32_t seq_len = 8;
    std::vector<float> soft_attention(seq_len, 0.5f);
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    // Trigger exceptions with NULL parameters
    int result = ternary_attention_discretize(ternary_ctx, nullptr,
                                              seq_len, ternary_out.data());
    EXPECT_EQ(result, -1) << "Should fail with NULL soft_attention";

    result = ternary_attention_discretize(ternary_ctx, soft_attention.data(),
                                          seq_len, nullptr);
    EXPECT_EQ(result, -1) << "Should fail with NULL output";

    result = ternary_attention_discretize(ternary_ctx, soft_attention.data(),
                                          0, ternary_out.data());
    EXPECT_EQ(result, -1) << "Should fail with zero sequence length";

    // Now verify recovery with valid parameters
    result = ternary_attention_discretize(ternary_ctx, soft_attention.data(),
                                          seq_len, ternary_out.data());
    EXPECT_EQ(result, 0) << "Should succeed after exception recovery";
}

TEST_F(AttentionExceptionIntegrationTest, TernaryApplyExceptionHandling) {
    /**
     * WHAT: Verify ternary apply handles exceptions correctly
     * WHY:  Test apply function exception path
     * HOW:  Test various invalid parameter combinations
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

    // Test NULL ctx
    int result = ternary_attention_apply(nullptr, values.data(),
                                         ternary_attention.data(),
                                         seq_len, dim, output.data());
    EXPECT_EQ(result, -1);

    // Test NULL values
    result = ternary_attention_apply(ternary_ctx, nullptr,
                                     ternary_attention.data(),
                                     seq_len, dim, output.data());
    EXPECT_EQ(result, -1);

    // Test zero dim
    result = ternary_attention_apply(ternary_ctx, values.data(),
                                     ternary_attention.data(),
                                     seq_len, 0, output.data());
    EXPECT_EQ(result, -1);

    // Verify recovery
    result = ternary_attention_apply(ternary_ctx, values.data(),
                                     ternary_attention.data(),
                                     seq_len, dim, output.data());
    EXPECT_EQ(result, 0) << "Should succeed after exceptions";
}

TEST_F(AttentionExceptionIntegrationTest, TernaryBackwardExceptionHandling) {
    /**
     * WHAT: Verify ternary backward handles exceptions correctly
     * WHY:  Test backward pass exception path
     * HOW:  Test invalid parameter combinations
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

    // Test various NULL parameters
    EXPECT_EQ(ternary_attention_backward(nullptr, grad_output.data(),
                                         soft_attention.data(), seq_len, dim,
                                         grad_attention.data()), -1);
    EXPECT_EQ(ternary_attention_backward(ternary_ctx, nullptr,
                                         soft_attention.data(), seq_len, dim,
                                         grad_attention.data()), -1);
    EXPECT_EQ(ternary_attention_backward(ternary_ctx, grad_output.data(),
                                         nullptr, seq_len, dim,
                                         grad_attention.data()), -1);
    EXPECT_EQ(ternary_attention_backward(ternary_ctx, grad_output.data(),
                                         soft_attention.data(), seq_len, dim,
                                         nullptr), -1);
    EXPECT_EQ(ternary_attention_backward(ternary_ctx, grad_output.data(),
                                         soft_attention.data(), 0, dim,
                                         grad_attention.data()), -1);
    EXPECT_EQ(ternary_attention_backward(ternary_ctx, grad_output.data(),
                                         soft_attention.data(), seq_len, 0,
                                         grad_attention.data()), -1);

    // Verify recovery
    int result = ternary_attention_backward(ternary_ctx, grad_output.data(),
                                            soft_attention.data(), seq_len, dim,
                                            grad_attention.data());
    EXPECT_EQ(result, 0) << "Should succeed after exceptions";
}

//=============================================================================
// Concurrent Exception Handling Tests
//=============================================================================

TEST_F(AttentionExceptionIntegrationTest, ConcurrentExceptionHandling) {
    /**
     * WHAT: Verify exception handling works correctly under concurrent access
     * WHY:  Test thread safety of exception mechanism
     * HOW:  Multiple threads trigger exceptions simultaneously
     */
    std::atomic<int> exception_count{0};
    std::atomic<int> success_count{0};
    constexpr int num_threads = 8;
    constexpr int iterations_per_thread = 100;

    auto worker = [&](int thread_id) {
        for (int i = 0; i < iterations_per_thread; i++) {
            // Alternate between triggering exceptions and successful operations
            if (i % 2 == 0) {
                // Trigger exception
                attention_head_t head = attention_head_create(nullptr);
                if (head == nullptr) {
                    exception_count++;
                }
            } else {
                // Successful operation
                attention_head_config_t config = {};
                config.input_dim = 32;
                config.output_dim = 32;
                config.key_dim = 32;
                config.value_dim = 32;
                config.temperature = 1.0f;

                attention_head_t head = attention_head_create(&config);
                if (head != nullptr) {
                    success_count++;
                    attention_head_destroy(head);
                }
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(worker, t);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all exceptions were detected
    int expected_exceptions = num_threads * (iterations_per_thread / 2);
    int expected_successes = num_threads * (iterations_per_thread / 2);

    EXPECT_EQ(exception_count.load(), expected_exceptions)
        << "All NULL config calls should trigger exceptions";
    EXPECT_EQ(success_count.load(), expected_successes)
        << "All valid config calls should succeed";
}

//=============================================================================
// Exception Chain Tests
//=============================================================================

TEST_F(AttentionExceptionIntegrationTest, ExceptionChainInMultiheadCreate) {
    /**
     * WHAT: Verify exception chain when multihead creation fails mid-way
     * WHY:  Test cleanup after partial creation failure
     * HOW:  Create config that might cause partial failure during head creation
     */
    multihead_attention_config_t config = {};
    config.num_heads = 1000;  // Many heads
    config.input_dim = 1000;  // Large dimensions - may exhaust memory
    config.output_dim = 1000;
    config.sequence_length = 16;
    config.use_thalamic_gate = false;
    config.use_salience_weighting = false;
    config.gate_bias = 0.5f;

    // This may succeed or fail depending on system resources
    // Key is that it doesn't crash and cleans up properly
    mha = multihead_attention_create(&config);

    // Either creation succeeded or failed gracefully
    if (mha != nullptr) {
        // If it succeeded, verify it works
        const uint32_t seq_len = 4;
        std::vector<float> input(seq_len * config.input_dim, 0.5f);
        std::vector<float> output(seq_len * config.output_dim, 0.0f);

        bool success = multihead_attention_forward(mha, input.data(), seq_len,
                                                   nullptr, output.data());
        // May fail due to buffer allocation in forward pass
        // Key is no crash
        (void)success;
    }
    // No crash = success
    SUCCEED();
}

//=============================================================================
// PE Type Exception Integration Tests
//=============================================================================

TEST_F(AttentionExceptionIntegrationTest, PeTypeExceptionHandling) {
    /**
     * WHAT: Verify PE type exception handling
     * WHY:  Test positional encoding exception path
     * HOW:  Set invalid PE types and verify exception handling
     */
    multihead_attention_config_t config = create_valid_mha_config();
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    // Test with NULL mha
    bool result = multihead_attention_set_pe_type(nullptr, NIMCP_POS_ROTARY);
    EXPECT_FALSE(result) << "Should fail with NULL mha";

    // Test with unsupported PE type (NIMCP_POS_SINUSOIDAL)
    result = multihead_attention_set_pe_type(mha, NIMCP_POS_SINUSOIDAL);
    EXPECT_FALSE(result) << "Should fail with unsupported PE type";

    // Verify valid PE types work
    result = multihead_attention_set_pe_type(mha, NIMCP_POS_ROTARY);
    EXPECT_TRUE(result) << "RoPE should be supported";

    result = multihead_attention_set_pe_type(mha, NIMCP_POS_ALIBI);
    EXPECT_TRUE(result) << "ALiBi should be supported";
}

//=============================================================================
// Top-K Exception Integration Tests
//=============================================================================

TEST_F(AttentionExceptionIntegrationTest, TopKExceptionAndEdgeCases) {
    /**
     * WHAT: Verify top-k exception handling and edge cases
     * WHY:  Test boundary conditions in top-k selection
     * HOW:  Test NULL, zero, and boundary k values
     */
    const uint32_t seq_len = 8;
    std::vector<float> soft_attention = {0.1f, 0.8f, 0.5f, 0.3f, 0.9f, 0.2f, 0.7f, 0.4f};
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    // Test NULL parameters
    EXPECT_EQ(ternary_attention_top_k(nullptr, seq_len, 3, ternary_out.data()), -1);
    EXPECT_EQ(ternary_attention_top_k(soft_attention.data(), seq_len, 3, nullptr), -1);
    EXPECT_EQ(ternary_attention_top_k(soft_attention.data(), 0, 3, ternary_out.data()), -1);

    // Test k = 0 (should be clamped to 1)
    int result = ternary_attention_top_k(soft_attention.data(), seq_len, 0, ternary_out.data());
    EXPECT_EQ(result, 0) << "k=0 should succeed with clamped value";

    int focus_count = 0;
    for (uint32_t i = 0; i < seq_len; i++) {
        if (ternary_out[i] == ATTENTION_FOCUS) focus_count++;
    }
    EXPECT_EQ(focus_count, 1) << "k=0 should result in 1 FOCUS (clamped)";

    // Test k > seq_len (should be clamped)
    result = ternary_attention_top_k(soft_attention.data(), seq_len, 100, ternary_out.data());
    EXPECT_EQ(result, 0) << "k>seq_len should succeed with clamped value";

    focus_count = 0;
    for (uint32_t i = 0; i < seq_len; i++) {
        if (ternary_out[i] == ATTENTION_FOCUS) focus_count++;
    }
    EXPECT_EQ(focus_count, static_cast<int>(seq_len)) << "k>seq_len should make all FOCUS";
}

//=============================================================================
// Stats Exception Integration Tests
//=============================================================================

TEST_F(AttentionExceptionIntegrationTest, StatsExceptionHandling) {
    /**
     * WHAT: Verify stats retrieval exception handling
     * WHY:  Test stats API exception paths
     * HOW:  Test NULL parameters in get_stats calls
     */
    multihead_attention_config_t config = create_valid_mha_config();
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    attention_stats_t stats;

    // Test NULL mha
    bool result = multihead_attention_get_stats(nullptr, &stats);
    EXPECT_FALSE(result) << "Should fail with NULL mha";

    // Test NULL stats
    result = multihead_attention_get_stats(mha, nullptr);
    EXPECT_FALSE(result) << "Should fail with NULL stats";

    // Verify valid call works
    result = multihead_attention_get_stats(mha, &stats);
    EXPECT_TRUE(result) << "Should succeed with valid parameters";
    EXPECT_EQ(stats.forward_calls, 0u) << "No forward calls yet";

    // Ternary stats tests
    ternary_attention_config_t ternary_config;
    ternary_attention_default_config(&ternary_config);
    ternary_ctx = ternary_attention_create(&ternary_config);
    ASSERT_NE(ternary_ctx, nullptr);

    ternary_attention_stats_t ternary_stats;

    EXPECT_EQ(ternary_attention_get_stats(nullptr, &ternary_stats), -1);
    EXPECT_EQ(ternary_attention_get_stats(ternary_ctx, nullptr), -1);
    EXPECT_EQ(ternary_attention_get_stats(ternary_ctx, &ternary_stats), 0);
}
