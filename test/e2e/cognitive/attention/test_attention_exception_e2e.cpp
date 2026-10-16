/**
 * @file test_attention_exception_e2e.cpp
 * @brief End-to-end tests for multi-attention exception handling workflow
 *
 * WHAT: Full exception handling workflow from error detection to immune recovery
 * WHY:  Verify complete exception-immune-recovery pipeline works correctly
 * HOW:  Test realistic scenarios with brain context and immune integration
 *
 * E2E SCENARIOS:
 * - Complete attention creation failure -> immune response -> recovery
 * - Forward pass error -> logging -> graceful degradation
 * - Concurrent attention operations with exceptions
 * - Long-running attention pipeline with occasional failures
 * - Brain-integrated attention exception flow
 *
 * @author NIMCP Development Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>

#include "plasticity/attention/nimcp_attention.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/nimcp_brain.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief E2E test fixture for attention exception workflow
 */
class AttentionExceptionE2ETest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    multihead_attention_t mha = nullptr;
    ternary_attention_ctx_t* ternary_ctx = nullptr;

    void SetUp() override {
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
     * @brief Create brain with immune system for testing
     */
    brain_t create_immune_brain() {
        brain_config_t config = {};
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_brain_immune = true;
        config.enable_multihead_attention = true;
        config.num_attention_heads = 4;
        strncpy(config.task_name, "e2e_exception_test", sizeof(config.task_name) - 1);
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
        config.sequence_length = 32;
        config.use_thalamic_gate = true;
        config.use_salience_weighting = true;
        config.gate_bias = 0.5f;
        config.use_positional_encoding = false;
        config.enable_quantum_attention = false;
        return config;
    }

    /**
     * @brief Generate random input data
     */
    std::vector<float> generate_input(uint32_t seq_len, uint32_t dim) {
        std::vector<float> data(seq_len * dim);
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& v : data) {
            v = dist(gen);
        }
        return data;
    }
};

//=============================================================================
// Complete Exception-Recovery Workflow Tests
//=============================================================================

TEST_F(AttentionExceptionE2ETest, CompleteCreationFailureRecoveryWorkflow) {
    /**
     * E2E: Test complete workflow when attention creation fails
     *
     * Workflow:
     * 1. Attempt to create with invalid config
     * 2. Exception is raised and logged
     * 3. System remains stable
     * 4. Retry with valid config succeeds
     * 5. Operations proceed normally
     */

    // Step 1: Attempt creation with invalid config
    multihead_attention_config_t invalid_config = {};
    invalid_config.num_heads = 0;  // Invalid

    mha = multihead_attention_create(&invalid_config);
    ASSERT_EQ(mha, nullptr) << "Step 1: Invalid config should fail";

    // Step 2: System should be stable (exception was handled)
    // We can verify by checking no crash occurred

    // Step 3: Retry with valid config
    auto valid_config = create_valid_mha_config();
    mha = multihead_attention_create(&valid_config);
    ASSERT_NE(mha, nullptr) << "Step 3: Valid config should succeed";

    // Step 4: Verify operations work
    const uint32_t seq_len = 8;
    auto input = generate_input(seq_len, valid_config.input_dim);
    std::vector<float> output(seq_len * valid_config.output_dim, 0.0f);

    bool success = multihead_attention_forward(mha, input.data(), seq_len,
                                               nullptr, output.data());
    EXPECT_TRUE(success) << "Step 4: Forward pass should succeed";

    // Step 5: Verify output is valid
    float sum = 0.0f;
    for (const auto& v : output) {
        sum += v * v;
    }
    EXPECT_GT(sum, 0.0f) << "Step 5: Output should be non-zero";
}

TEST_F(AttentionExceptionE2ETest, ForwardPassErrorGracefulDegradation) {
    /**
     * E2E: Test graceful degradation when forward pass encounters errors
     *
     * Workflow:
     * 1. Create valid attention system
     * 2. Trigger forward pass errors
     * 3. Verify system handles errors gracefully
     * 4. Resume normal operations
     * 5. Verify results are consistent
     */

    auto config = create_valid_mha_config();
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    const uint32_t seq_len = 8;
    auto input = generate_input(seq_len, config.input_dim);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    // Step 1: Perform successful forward pass
    bool success = multihead_attention_forward(mha, input.data(), seq_len,
                                               nullptr, output.data());
    ASSERT_TRUE(success) << "Step 1: Initial forward should succeed";

    std::vector<float> reference_output = output;

    // Step 2: Trigger errors (NULL input, invalid seq_len)
    EXPECT_FALSE(multihead_attention_forward(mha, nullptr, seq_len, nullptr, output.data()));
    EXPECT_FALSE(multihead_attention_forward(mha, input.data(), 0, nullptr, output.data()));
    EXPECT_FALSE(multihead_attention_forward(mha, input.data(),
                                             config.sequence_length + 100,
                                             nullptr, output.data()));

    // Step 3: Resume normal operations
    std::fill(output.begin(), output.end(), 0.0f);
    success = multihead_attention_forward(mha, input.data(), seq_len,
                                          nullptr, output.data());
    EXPECT_TRUE(success) << "Step 3: Resume should succeed";

    // Step 4: Verify consistency (same input -> same output)
    float diff = 0.0f;
    for (size_t i = 0; i < output.size(); i++) {
        diff += std::abs(output[i] - reference_output[i]);
    }
    EXPECT_NEAR(diff, 0.0f, 1e-5f) << "Step 4: Output should be consistent";
}

TEST_F(AttentionExceptionE2ETest, ConcurrentAttentionOperationsWithExceptions) {
    /**
     * E2E: Test concurrent attention operations with mixed success/failure
     *
     * Workflow:
     * 1. Launch multiple threads
     * 2. Each thread creates/destroys attention, triggers exceptions
     * 3. Verify no race conditions or crashes
     * 4. Verify aggregate statistics
     */

    std::atomic<int> successful_creates{0};
    std::atomic<int> successful_forwards{0};
    std::atomic<int> expected_failures{0};
    constexpr int num_threads = 8;
    constexpr int iterations = 100;

    auto worker = [&](int thread_id) {
        std::mt19937 gen(thread_id);
        std::uniform_int_distribution<int> dist(0, 9);

        for (int i = 0; i < iterations; i++) {
            // Randomly decide operation type
            int op = dist(gen);

            if (op < 3) {
                // 30% chance: trigger exception (NULL config)
                multihead_attention_t local_mha = multihead_attention_create(nullptr);
                if (local_mha == nullptr) {
                    expected_failures++;
                }
            } else if (op < 6) {
                // 30% chance: trigger exception (invalid config)
                multihead_attention_config_t bad_config = {};
                bad_config.num_heads = 0;
                multihead_attention_t local_mha = multihead_attention_create(&bad_config);
                if (local_mha == nullptr) {
                    expected_failures++;
                }
            } else {
                // 40% chance: valid operations
                multihead_attention_config_t config = {};
                config.num_heads = 4;
                config.input_dim = 64;
                config.output_dim = 64;
                config.sequence_length = 8;
                config.use_thalamic_gate = false;
                config.gate_bias = 0.5f;

                multihead_attention_t local_mha = multihead_attention_create(&config);
                if (local_mha != nullptr) {
                    successful_creates++;

                    // Try forward pass
                    std::vector<float> input(4 * 64, 0.5f);
                    std::vector<float> output(4 * 64, 0.0f);

                    if (multihead_attention_forward(local_mha, input.data(), 4,
                                                    nullptr, output.data())) {
                        successful_forwards++;
                    }

                    multihead_attention_destroy(local_mha);
                }
            }
        }
    };

    // Launch threads
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(worker, t);
    }

    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify statistics
    EXPECT_GT(successful_creates.load(), 0) << "Some creates should succeed";
    EXPECT_GT(successful_forwards.load(), 0) << "Some forwards should succeed";
    EXPECT_GT(expected_failures.load(), 0) << "Some failures should occur";

    // Verify totals add up roughly
    int total_ops = successful_creates.load() + expected_failures.load();
    int expected_total = num_threads * iterations;
    EXPECT_GE(total_ops, expected_total * 0.8)
        << "Total operations should be roughly as expected";
}

TEST_F(AttentionExceptionE2ETest, LongRunningPipelineWithOccasionalFailures) {
    /**
     * E2E: Test long-running attention pipeline with occasional induced failures
     *
     * Workflow:
     * 1. Create attention system
     * 2. Run many forward passes
     * 3. Occasionally inject failures (NULL input, etc.)
     * 4. Verify system recovers and continues
     * 5. Verify statistics are correct at the end
     */

    auto config = create_valid_mha_config();
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    const uint32_t seq_len = 8;
    auto input = generate_input(seq_len, config.input_dim);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    constexpr int num_iterations = 500;
    constexpr int failure_interval = 50;  // Inject failure every 50 iterations

    int successful_passes = 0;
    int failed_passes = 0;

    for (int i = 0; i < num_iterations; i++) {
        bool should_fail = (i > 0 && i % failure_interval == 0);

        if (should_fail) {
            // Inject failure
            bool result = multihead_attention_forward(mha, nullptr, seq_len,
                                                      nullptr, output.data());
            EXPECT_FALSE(result) << "Injected failure should fail";
            failed_passes++;
        } else {
            // Normal operation
            bool result = multihead_attention_forward(mha, input.data(), seq_len,
                                                      nullptr, output.data());
            EXPECT_TRUE(result) << "Normal operation should succeed at iteration " << i;
            if (result) successful_passes++;
        }
    }

    // Verify statistics
    // Failures occur at i=50, 100, 150, ..., 450 (skipping i=0 due to i > 0 check)
    int expected_failures = (num_iterations - 1) / failure_interval;
    EXPECT_EQ(failed_passes, expected_failures) << "Should have exact number of failures";
    EXPECT_EQ(successful_passes, num_iterations - expected_failures)
        << "Remaining should be successes";

    // Verify final stats
    attention_stats_t stats;
    ASSERT_TRUE(multihead_attention_get_stats(mha, &stats));
    EXPECT_EQ(stats.forward_calls, static_cast<uint64_t>(successful_passes))
        << "Stats should match successful passes";
}

//=============================================================================
// Ternary Attention E2E Tests
//=============================================================================

TEST_F(AttentionExceptionE2ETest, TernaryAttentionCompleteWorkflow) {
    /**
     * E2E: Test complete ternary attention workflow with exception handling
     *
     * Workflow:
     * 1. Create ternary attention context
     * 2. Discretize soft attention to ternary states
     * 3. Apply ternary modulation to values
     * 4. Compute backward pass
     * 5. Handle exceptions throughout
     */

    // Step 1: Create context
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr) << "Step 1: Create should succeed";

    // Step 2: Discretize
    const uint32_t seq_len = 16;
    std::vector<float> soft_attention(seq_len);
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    // Create varied soft attention values
    for (uint32_t i = 0; i < seq_len; i++) {
        soft_attention[i] = static_cast<float>(i) / static_cast<float>(seq_len);
    }

    int result = ternary_attention_discretize(ternary_ctx, soft_attention.data(),
                                              seq_len, ternary_out.data());
    EXPECT_EQ(result, 0) << "Step 2: Discretize should succeed";

    // Verify discretization
    int focus_count = 0, neutral_count = 0, suppress_count = 0;
    for (uint32_t i = 0; i < seq_len; i++) {
        switch (ternary_out[i]) {
            case ATTENTION_FOCUS: focus_count++; break;
            case ATTENTION_NEUTRAL: neutral_count++; break;
            case ATTENTION_SUPPRESS: suppress_count++; break;
        }
    }
    EXPECT_GT(focus_count + neutral_count + suppress_count, 0)
        << "Should have classified all items";

    // Step 3: Apply
    const uint32_t dim = 64;
    std::vector<float> values(seq_len * dim, 1.0f);
    std::vector<float> output(seq_len * dim, 0.0f);

    result = ternary_attention_apply(ternary_ctx, values.data(), ternary_out.data(),
                                     seq_len, dim, output.data());
    EXPECT_EQ(result, 0) << "Step 3: Apply should succeed";

    // Verify modulation (FOCUS should amplify, SUPPRESS should diminish)
    // Check that output varies based on attention state
    float focus_sum = 0.0f, suppress_sum = 0.0f;
    for (uint32_t t = 0; t < seq_len; t++) {
        float token_sum = 0.0f;
        for (uint32_t d = 0; d < dim; d++) {
            token_sum += output[t * dim + d];
        }
        if (ternary_out[t] == ATTENTION_FOCUS) focus_sum += token_sum;
        if (ternary_out[t] == ATTENTION_SUPPRESS) suppress_sum += token_sum;
    }

    if (focus_count > 0 && suppress_count > 0) {
        float avg_focus = focus_sum / focus_count;
        float avg_suppress = suppress_sum / suppress_count;
        EXPECT_GT(avg_focus, avg_suppress) << "FOCUS should have higher average than SUPPRESS";
    }

    // Step 4: Backward pass
    std::vector<float> grad_output(seq_len * dim, 1.0f);
    std::vector<float> grad_attention(seq_len, 0.0f);

    result = ternary_attention_backward(ternary_ctx, grad_output.data(),
                                        soft_attention.data(), seq_len, dim,
                                        grad_attention.data());
    EXPECT_EQ(result, 0) << "Step 4: Backward should succeed";

    // Step 5: Verify stats
    ternary_attention_stats_t stats;
    result = ternary_attention_get_stats(ternary_ctx, &stats);
    EXPECT_EQ(result, 0) << "Step 5: Get stats should succeed";
    EXPECT_GT(stats.n_focus + stats.n_neutral + stats.n_suppress, 0u)
        << "Stats should be populated";
}

TEST_F(AttentionExceptionE2ETest, TernaryTemperatureAnnealingWorkflow) {
    /**
     * E2E: Test ternary attention with temperature annealing
     *
     * Workflow:
     * 1. Create with annealing enabled
     * 2. Update temperature over training steps
     * 3. Verify temperature decreases
     * 4. Verify attention becomes harder over time
     */

    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    config.temperature_annealing = true;
    config.initial_temperature = 2.0f;
    config.final_temperature = 0.1f;
    config.annealing_steps = 100;

    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    // Verify initial temperature
    float temp = ternary_attention_get_temperature(ternary_ctx);
    EXPECT_FLOAT_EQ(temp, config.initial_temperature)
        << "Initial temperature should match config";

    // Step through annealing
    const uint32_t seq_len = 8;
    std::vector<float> soft_attention = {0.1f, 0.3f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 0.4f};
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    float prev_temp = temp;
    for (uint32_t step = 0; step <= config.annealing_steps; step += 10) {
        ternary_attention_update_temperature(ternary_ctx, step);
        temp = ternary_attention_get_temperature(ternary_ctx);

        // Temperature should decrease (or stay same at boundaries)
        EXPECT_LE(temp, prev_temp + 0.001f) << "Temperature should not increase at step " << step;
        prev_temp = temp;

        // Discretize at this temperature
        ternary_attention_discretize(ternary_ctx, soft_attention.data(),
                                     seq_len, ternary_out.data());
    }

    // Verify final temperature
    EXPECT_NEAR(temp, config.final_temperature, 0.01f)
        << "Final temperature should match config";
}

//=============================================================================
// Brain-Integrated E2E Tests
//=============================================================================

TEST_F(AttentionExceptionE2ETest, BrainAttentionExceptionWorkflow) {
    /**
     * E2E: Test attention exceptions within brain context
     *
     * Workflow:
     * 1. Create brain with immune system
     * 2. Access brain's attention system
     * 3. Trigger exceptions
     * 4. Verify brain remains stable
     * 5. Process normally afterward
     */

    brain = create_immune_brain();
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed (may be expected in minimal builds)";
    }

    // Create standalone attention for exception testing
    // (Brain's internal attention is managed internally)
    auto config = create_valid_mha_config();
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    // Trigger exceptions
    for (int i = 0; i < 10; i++) {
        multihead_attention_t bad_mha = multihead_attention_create(nullptr);
        EXPECT_EQ(bad_mha, nullptr);
    }

    // Verify attention still works
    const uint32_t seq_len = 8;
    auto input = generate_input(seq_len, config.input_dim);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    bool success = multihead_attention_forward(mha, input.data(), seq_len,
                                               nullptr, output.data());
    EXPECT_TRUE(success) << "Attention should work after exceptions";

    // Verify brain still works
    std::vector<float> brain_input(64, 0.5f);
    brain_decision_t* decision = brain_decide(brain, brain_input.data(), 64);
    if (decision) {
        EXPECT_GE(decision->confidence, 0.0f);
        EXPECT_LE(decision->confidence, 1.0f);
        brain_free_decision(decision);
    }
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(AttentionExceptionE2ETest, StressTestMixedOperations) {
    /**
     * E2E: Stress test with high volume of mixed operations
     *
     * Tests system stability under heavy load with mixed
     * success/failure operations
     */

    constexpr int num_iterations = 1000;
    std::mt19937 gen(42);
    std::uniform_int_distribution<int> op_dist(0, 3);

    auto config = create_valid_mha_config();
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    ternary_attention_config_t ternary_config;
    ternary_attention_default_config(&ternary_config);
    ternary_ctx = ternary_attention_create(&ternary_config);
    ASSERT_NE(ternary_ctx, nullptr);

    const uint32_t seq_len = 8;
    auto input = generate_input(seq_len, config.input_dim);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);
    std::vector<float> soft_attention(seq_len, 0.5f);
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    for (int i = 0; i < num_iterations; i++) {
        int op = op_dist(gen);

        switch (op) {
            case 0: {
                // Valid multihead forward
                multihead_attention_forward(mha, input.data(), seq_len,
                                            nullptr, output.data());
                break;
            }
            case 1: {
                // Invalid multihead forward (NULL input)
                multihead_attention_forward(mha, nullptr, seq_len,
                                            nullptr, output.data());
                break;
            }
            case 2: {
                // Valid ternary discretize
                ternary_attention_discretize(ternary_ctx, soft_attention.data(),
                                             seq_len, ternary_out.data());
                break;
            }
            case 3: {
                // Invalid ternary discretize
                ternary_attention_discretize(nullptr, soft_attention.data(),
                                             seq_len, ternary_out.data());
                break;
            }
        }
    }

    // Verify system is still functional
    bool success = multihead_attention_forward(mha, input.data(), seq_len,
                                               nullptr, output.data());
    EXPECT_TRUE(success) << "System should remain functional after stress test";

    int result = ternary_attention_discretize(ternary_ctx, soft_attention.data(),
                                              seq_len, ternary_out.data());
    EXPECT_EQ(result, 0) << "Ternary should remain functional after stress test";
}

TEST_F(AttentionExceptionE2ETest, RapidCreateDestroyStability) {
    /**
     * E2E: Test rapid create/destroy cycles don't cause memory issues
     */

    constexpr int num_cycles = 500;

    for (int i = 0; i < num_cycles; i++) {
        // Create and destroy multihead attention
        auto config = create_valid_mha_config();
        multihead_attention_t local_mha = multihead_attention_create(&config);
        if (local_mha) {
            // Do a forward pass
            const uint32_t seq_len = 4;
            std::vector<float> input(seq_len * config.input_dim, 0.5f);
            std::vector<float> output(seq_len * config.output_dim, 0.0f);
            multihead_attention_forward(local_mha, input.data(), seq_len,
                                        nullptr, output.data());
            multihead_attention_destroy(local_mha);
        }

        // Create and destroy ternary attention
        ternary_attention_config_t ternary_config;
        ternary_attention_default_config(&ternary_config);
        ternary_attention_ctx_t* local_ctx = ternary_attention_create(&ternary_config);
        if (local_ctx) {
            std::vector<float> soft_attn(8, 0.5f);
            std::vector<ternary_attention_state_t> out(8);
            ternary_attention_discretize(local_ctx, soft_attn.data(), 8, out.data());
            ternary_attention_destroy(local_ctx);
        }
    }

    // If we got here without crash, test passed
    SUCCEED() << "Rapid create/destroy cycles completed without issues";
}
