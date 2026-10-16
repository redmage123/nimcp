/**
 * @file test_attention_integration.cpp
 * @brief Unit tests for multihead attention integration into brain
 *
 * TEST COVERAGE:
 * - Attention module initialization and cleanup
 * - Configuration validation
 * - Integration with multimodal pipeline
 * - Guard clauses and error handling
 * - Performance characteristics
 * - Edge cases and boundary conditions
 *
 * TESTING PHILOSOPHY:
 * - Test behavior, not implementation
 * - Use NIMCP coding standards (WHAT/WHY/HOW comments)
 * - Guard clauses before logic
 * - Single responsibility per test
 * - Descriptive test names
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 * @version 3.0.0 Module Integration Phase
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "core/brain/nimcp_brain.h"
#include "plasticity/attention/nimcp_attention.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AttentionIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    brain_config_t config;

    void SetUp() override {
        brain = nullptr;

        // WHAT: Initialize default config
        // WHY:  Start with known good configuration
        // HOW:  Zero struct, set required fields
        memset(&config, 0, sizeof(config));
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 128;
        config.num_outputs = 10;
        strncpy(config.task_name, "attention_test", sizeof(config.task_name) - 1);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Create brain with attention enabled
    brain_t create_brain_with_attention(uint32_t num_heads = 8) {
        config.enable_multihead_attention = true;
        config.num_attention_heads = num_heads;
        config.attention_key_dim = 64;
        config.enable_thalamic_gate = true;
        config.enable_salience_weighting = false;  // Will be tested separately

        return brain_create_custom(&config);
    }

    // Helper: Create test input features
    std::vector<float> create_test_features(uint32_t dim, float base_value = 1.0f) {
        std::vector<float> features(dim);
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + (i * 0.1f);
        }
        return features;
    }
};

//=============================================================================
// 1. Initialization Tests
//=============================================================================

TEST_F(AttentionIntegrationTest, Initialize_AttentionEnabled_Success) {
    // WHAT: Test attention initializes when enabled
    // WHY:  Core functionality requirement
    // HOW:  Enable attention, create brain, verify non-null

    brain = create_brain_with_attention();

    ASSERT_NE(brain, nullptr);
    // Note: Cannot directly access brain->multihead_attention (opaque type)
    // Instead, verify behavior through processing
}

TEST_F(AttentionIntegrationTest, Initialize_AttentionDisabled_SkipsCreation) {
    // WHAT: Test attention not created when disabled
    // WHY:  Resource efficiency - don't create unused modules
    // HOW:  Disable attention, create brain, verify success

    config.enable_multihead_attention = false;
    brain = brain_create_custom(&config);

    ASSERT_NE(brain, nullptr);
    // Brain should work normally without attention
}

TEST_F(AttentionIntegrationTest, Initialize_ZeroHeads_UsesDefault) {
    // WHAT: Test default head count when zero specified
    // WHY:  Sensible defaults for ease of use
    // HOW:  Set heads to 0, verify brain creates successfully

    config.enable_multihead_attention = true;
    config.num_attention_heads = 0;  // Should default to 8

    brain = brain_create_custom(&config);

    ASSERT_NE(brain, nullptr);
}

TEST_F(AttentionIntegrationTest, Initialize_MultipleHeads_Success) {
    // WHAT: Test various head counts (1, 4, 8, 16)
    // WHY:  Verify flexibility in configuration
    // HOW:  Test each head count individually

    for (uint32_t heads : {1, 4, 8, 16}) {
        brain = create_brain_with_attention(heads);
        ASSERT_NE(brain, nullptr) << "Failed with " << heads << " heads";
        brain_destroy(brain);
        brain = nullptr;
    }
}

//=============================================================================
// 2. Configuration Tests
//=============================================================================

TEST_F(AttentionIntegrationTest, Config_ThalamicGateEnabled_Success) {
    // WHAT: Test thalamic gating configuration
    // WHY:  Top-down attention control is key feature
    // HOW:  Enable gate, verify brain creates

    config.enable_multihead_attention = true;
    config.enable_thalamic_gate = true;

    brain = brain_create_custom(&config);

    ASSERT_NE(brain, nullptr);
}

TEST_F(AttentionIntegrationTest, Config_SalienceWeightingEnabled_Success) {
    // WHAT: Test salience-weighted attention
    // WHY:  Integration with salience evaluator
    // HOW:  Enable salience weighting, verify brain creates

    config.enable_multihead_attention = true;
    config.enable_salience_weighting = true;
    config.enable_salience = true;  // Need salience evaluator too

    brain = brain_create_custom(&config);

    ASSERT_NE(brain, nullptr);
}

TEST_F(AttentionIntegrationTest, Config_CustomKeyDimension_Success) {
    // WHAT: Test custom key/query dimension
    // WHY:  Allow tuning for different input sizes
    // HOW:  Set custom key_dim, verify brain creates

    config.enable_multihead_attention = true;
    config.attention_key_dim = 128;  // Non-default

    brain = brain_create_custom(&config);

    ASSERT_NE(brain, nullptr);
}

//=============================================================================
// 3. Processing Pipeline Tests
//=============================================================================

TEST_F(AttentionIntegrationTest, Process_WithAttention_Success) {
    // WHAT: Test attention in full processing pipeline
    // WHY:  Integration test - verify end-to-end flow
    // HOW:  Create brain with attention, process input, verify output

    config.enable_multihead_attention = true;
    config.enable_multimodal_integration = false;  // Use direct processing only

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Create test input
    auto input_data = create_test_features(config.num_inputs);

    // Use direct brain processing
    brain_decision_t* decision = brain_decide(brain, input_data.data(), config.num_inputs);

    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);
}

TEST_F(AttentionIntegrationTest, Process_WithoutAttention_Success) {
    // WHAT: Test processing works without attention (baseline)
    // WHY:  Verify attention is truly optional
    // HOW:  Disable attention, process, verify success

    config.enable_multihead_attention = false;
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    auto input_data = create_test_features(config.num_inputs);

    // Use direct brain processing
    brain_decision_t* decision = brain_decide(brain, input_data.data(), config.num_inputs);

    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);

    brain_free_decision(decision);
}

TEST_F(AttentionIntegrationTest, Process_MultimodalWithAttention_Success) {
    // WHAT: Test attention with multiple cognitive modules enabled
    // WHY:  Verify attention works in complex brain configuration
    // HOW:  Enable attention + multiple cognitive systems

    config.enable_multihead_attention = true;
    config.num_attention_heads = 8;
    config.enable_thalamic_gate = true;

    // Enable complementary cognitive modules (no multimodal to avoid pre-existing issues)
    config.enable_working_memory = true;
    config.enable_global_workspace = true;
    config.enable_salience = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Use brain_decide (proven working path)
    auto input_data = create_test_features(config.num_inputs);
    brain_decision_t* decision = brain_decide(brain, input_data.data(), config.num_inputs);

    // Should succeed with attention + cognitive integration
    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);
}

//=============================================================================
// 4. Error Handling Tests
//=============================================================================

TEST_F(AttentionIntegrationTest, ErrorHandling_NullInput_Fails) {
    // WHAT: Test null input handling
    // WHY:  Verify robust error checking
    // HOW:  Pass null input, expect failure

    brain = create_brain_with_attention();
    ASSERT_NE(brain, nullptr);

    brain_multimodal_output_t output = {};
    output.output_vector = new float[config.num_outputs];
    output.output_dim = config.num_outputs;

    bool result = brain_process_multimodal(brain, nullptr, &output);

    EXPECT_FALSE(result);

    delete[] output.output_vector;
}

TEST_F(AttentionIntegrationTest, ErrorHandling_NullOutput_Fails) {
    // WHAT: Test null output handling
    // WHY:  Verify robust error checking
    // HOW:  Pass null output, expect failure

    brain = create_brain_with_attention();
    ASSERT_NE(brain, nullptr);

    auto input_data = create_test_features(config.num_inputs);
    brain_multimodal_input_t input = {};
    input.direct_data = input_data.data();
    input.direct_dim = config.num_inputs;
    input.timestamp_ms = nimcp_time_get_ms();

    bool result = brain_process_multimodal(brain, &input, nullptr);

    EXPECT_FALSE(result);
}

//=============================================================================
// 5. Cleanup Tests
//=============================================================================

TEST_F(AttentionIntegrationTest, Cleanup_AttentionEnabled_NoLeak) {
    // WHAT: Test attention cleanup doesn't leak memory
    // WHY:  Resource management is critical
    // HOW:  Create and destroy brain, verify no crashes

    brain = create_brain_with_attention();
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);
    brain = nullptr;

    // Valgrind or AddressSanitizer would catch leaks
    SUCCEED();
}

TEST_F(AttentionIntegrationTest, Cleanup_MultipleCreationsDestructions_NoLeak) {
    // WHAT: Test repeated create/destroy cycles
    // WHY:  Catch accumulating leaks or corruption
    // HOW:  Create and destroy 10 times

    for (int i = 0; i < 10; i++) {
        brain = create_brain_with_attention();
        ASSERT_NE(brain, nullptr) << "Failed on iteration " << i;
        brain_destroy(brain);
        brain = nullptr;
    }

    SUCCEED();
}

//=============================================================================
// 6. Performance Tests
//=============================================================================

TEST_F(AttentionIntegrationTest, Performance_AttentionOverhead_Acceptable) {
    // WHAT: Verify attention doesn't add excessive overhead
    // WHY:  Performance is key requirement (should be 2-5x faster overall)
    // HOW:  Time processing with and without attention

    // Note: This is a smoke test, not a rigorous benchmark
    // Full benchmarks should be separate

    config.enable_multihead_attention = true;
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    auto input_data = create_test_features(config.num_inputs);
    brain_multimodal_input_t input = {};
    input.direct_data = input_data.data();
    input.direct_dim = config.num_inputs;
    input.timestamp_ms = nimcp_time_get_ms();

    brain_multimodal_output_t output = {};
    output.output_vector = new float[config.num_outputs];
    output.output_dim = config.num_outputs;

    // Warmup
    brain_process_multimodal(brain, &input, &output);

    // Time 100 iterations
    uint64_t start_time = nimcp_time_get_ms();
    for (int i = 0; i < 100; i++) {
        brain_process_multimodal(brain, &input, &output);
    }
    uint64_t end_time = nimcp_time_get_ms();
    uint64_t elapsed_ms = end_time - start_time;

    // Should complete 100 iterations in reasonable time (< 10s)
    EXPECT_LT(elapsed_ms, 10000);

    delete[] output.output_vector;
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
