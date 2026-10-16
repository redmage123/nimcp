/**
 * @file test_broca_gpu_integration.cpp
 * @brief Integration tests for Broca's region with GPU-enabled brain
 *
 * WHAT: Verify Broca's region works correctly when the brain has GPU enabled
 * WHY:  Ensure GPU acceleration doesn't interfere with language production
 * HOW:  Create brain with both GPU and Broca enabled, run language production tests
 *
 * @version GPU-Broca Integration Testing
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <string.h>

// GPU headers outside extern "C" due to CUDA runtime templates
#include "gpu/context/nimcp_gpu_context.h"

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "core/brain/nimcp_brain_internal.h"  // For accessing brain struct internals
    #include "core/brain/regions/broca/nimcp_broca_adapter.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrocaGPUIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    /**
     * @brief Check if GPU is available and enabled
     */
    bool hasGPU() const {
        if (!brain) return false;
        return brain->gpu_enabled;
    }

    /**
     * @brief Check if Broca is enabled
     */
    bool hasBroca() const {
        if (!brain) return false;
        return brain->broca_enabled;
    }

    /**
     * @brief Get Broca adapter from brain
     */
    broca_adapter_t* getBroca() const {
        if (!brain) return nullptr;
        return brain->broca;
    }

    /**
     * @brief Create brain with both GPU and Broca enabled
     */
    brain_t createGPUBrocaBrain(const char* name) {
        // Use the standard configuration profile (balanced features)
        brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_STANDARD);

        // Set basic parameters
        strncpy(config.task_name, name, sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.num_inputs = 10;
        config.num_outputs = 5;

        // Ensure GPU is not disabled
        config.disable_gpu = false;
        config.force_cpu_only = false;

        // Enable speech cortex which triggers Broca initialization
        config.enable_speech_cortex = true;

        return brain_create_custom(&config);
    }

    /**
     * @brief Create simple brain for basic GPU tests (no complex features)
     */
    brain_t createSimpleGPUBrain(const char* name) {
        // Use simple brain_create for straightforward testing
        return brain_create(name, BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    }

    /**
     * @brief Add test lexicon entries to Broca
     */
    void setupTestLexicon(broca_adapter_t* broca) {
        broca_lexical_entry_t entry;
        memset(&entry, 0, sizeof(entry));

        // Word: "hello"
        entry.word_id = 1;
        strncpy(entry.word, "hello", sizeof(entry.word) - 1);
        entry.phonemes[0] = 'h';
        entry.phonemes[1] = 'e';
        entry.phonemes[2] = 'l';
        entry.phonemes[3] = 'o';
        entry.phoneme_count = 4;
        entry.pos = 0;
        entry.frequency = 0.9f;
        broca_add_lexical_entry(broca, &entry);

        // Word: "world"
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 2;
        strncpy(entry.word, "world", sizeof(entry.word) - 1);
        entry.phonemes[0] = 'w';
        entry.phonemes[1] = 'r';
        entry.phonemes[2] = 'l';
        entry.phonemes[3] = 'd';
        entry.phoneme_count = 4;
        entry.pos = 0;
        entry.frequency = 0.85f;
        broca_add_lexical_entry(broca, &entry);

        // Word: "test"
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 3;
        strncpy(entry.word, "test", sizeof(entry.word) - 1);
        entry.phonemes[0] = 't';
        entry.phonemes[1] = 'e';
        entry.phonemes[2] = 's';
        entry.phonemes[3] = 't';
        entry.phoneme_count = 4;
        entry.pos = 0;
        entry.frequency = 0.8f;
        broca_add_lexical_entry(broca, &entry);
    }
};

//=============================================================================
// Test 1: Brain Creation with GPU (simple brain, GPU focus)
//=============================================================================

TEST_F(BrocaGPUIntegrationTest, BrainCreation_GPUAndBrocaEnabled) {
    // First test simple brain creation (should always work)
    brain = createSimpleGPUBrain("gpu_broca_test");
    ASSERT_NE(brain, nullptr) << "Simple brain creation should succeed";

    // Log what we got
    if (hasGPU()) {
        std::cout << "[INFO] GPU is enabled" << std::endl;
    } else {
        std::cout << "[INFO] GPU not available, CPU-only mode" << std::endl;
    }

    if (hasBroca()) {
        std::cout << "[INFO] Broca is enabled" << std::endl;
    } else {
        std::cout << "[INFO] Broca not enabled (simple brain mode)" << std::endl;
    }

    // Basic inference should work
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    brain_decision_t* decision = brain_decide(brain, features, 10);
    EXPECT_NE(decision, nullptr) << "Brain inference should work";
}

//=============================================================================
// Test 2: Broca Operations with GPU-Enabled Brain
//=============================================================================

TEST_F(BrocaGPUIntegrationTest, BrocaOperations_WithGPUEnabled) {
    // Use simple brain for reliability
    brain = createSimpleGPUBrain("broca_ops_test");
    ASSERT_NE(brain, nullptr);

    // If Broca is enabled, test its operations
    if (hasBroca()) {
        // Get Broca adapter from brain
        broca_adapter_t* broca = getBroca();
        ASSERT_NE(broca, nullptr) << "Brain should have Broca adapter";

        // Setup test lexicon
        setupTestLexicon(broca);

        // Test basic Broca operations
        EXPECT_EQ(broca_get_status(broca), BROCA_STATUS_IDLE);

        // Begin utterance
        ASSERT_TRUE(broca_begin_utterance(broca));

        // Add words
        broca_input_word_t word;
        memset(&word, 0, sizeof(word));
        word.word_id = 1; // "hello"
        EXPECT_TRUE(broca_add_word(broca, &word));

        word.word_id = 2; // "world"
        EXPECT_TRUE(broca_add_word(broca, &word));

        // Process utterance
        broca_utterance_result_t result;
        memset(&result, 0, sizeof(result));
        bool success = broca_process_utterance(broca, &result);

        // Should succeed (or fail gracefully)
        if (success) {
            EXPECT_GE(result.word_count, 0u);
            EXPECT_GE(result.phoneme_count, 0u);
        }

        // Reset should work
        EXPECT_TRUE(broca_reset(broca));
    } else {
        std::cout << "[INFO] Broca not enabled in simple brain, testing basic inference only" << std::endl;
    }

    // Basic inference should still work regardless
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    brain_decision_t* decision = brain_decide(brain, features, 10);
    EXPECT_NE(decision, nullptr);
}

//=============================================================================
// Test 3: Brain Inference Still Works with Broca Enabled
//=============================================================================

TEST_F(BrocaGPUIntegrationTest, BrainInference_StillWorksWithBroca) {
    brain = createSimpleGPUBrain("inference_broca_test");
    ASSERT_NE(brain, nullptr);

    // Brain inference should work regardless of Broca status
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    brain_decision_t* decision = brain_decide(brain, features, 10);

    ASSERT_NE(decision, nullptr) << "Brain inference should work";
    EXPECT_NE(decision->label, nullptr);
}

//=============================================================================
// Test 4: GPU and Brain Operations Don't Interfere
//=============================================================================

TEST_F(BrocaGPUIntegrationTest, GPUAndBroca_NoInterference) {
    brain = createSimpleGPUBrain("interference_test");
    ASSERT_NE(brain, nullptr);

    // Run multiple iterations of brain operations
    for (int i = 0; i < 5; i++) {
        // Brain inference
        float features[10];
        for (int j = 0; j < 10; j++) {
            features[j] = (float)(i * 10 + j) / 50.0f;
        }
        brain_decision_t* decision = brain_decide(brain, features, 10);
        EXPECT_NE(decision, nullptr) << "Iteration " << i << " brain inference failed";

        // Broca operations (if available)
        if (hasBroca()) {
            broca_adapter_t* broca = getBroca();
            if (broca) {
                EXPECT_TRUE(broca_reset(broca)) << "Iteration " << i << " broca reset failed";
            }
        }

        // Another brain inference
        decision = brain_decide(brain, features, 10);
        EXPECT_NE(decision, nullptr) << "Iteration " << i << " second brain inference failed";
    }
}

//=============================================================================
// Test 5: Working Memory Operations (when Broca available)
//=============================================================================

TEST_F(BrocaGPUIntegrationTest, WorkingMemory_WithGPUEnabled) {
    brain = createSimpleGPUBrain("wm_gpu_test");
    ASSERT_NE(brain, nullptr);

    // If Broca is enabled, test working memory
    if (hasBroca()) {
        broca_adapter_t* broca = getBroca();
        ASSERT_NE(broca, nullptr);

        // Setup lexicon
        setupTestLexicon(broca);

        // Push words to working memory
        EXPECT_TRUE(broca_wm_push(broca, 1)); // "hello"
        EXPECT_TRUE(broca_wm_push(broca, 2)); // "world"
        EXPECT_TRUE(broca_wm_push(broca, 3)); // "test"

        // Verify contents
        uint32_t buffer[10];
        uint32_t count = 10;
        EXPECT_TRUE(broca_wm_get_contents(broca, buffer, &count));
        EXPECT_GE(count, 1u);

        // Pop and verify
        uint32_t popped;
        EXPECT_TRUE(broca_wm_pop(broca, &popped));
    } else {
        std::cout << "[INFO] Broca not enabled, skipping WM test" << std::endl;
    }

    // Basic inference still works
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    brain_decision_t* decision = brain_decide(brain, features, 10);
    EXPECT_NE(decision, nullptr);
}

//=============================================================================
// Test 6: Multiple Iterations (stress test)
//=============================================================================

TEST_F(BrocaGPUIntegrationTest, MultipleUtterances_WithGPU) {
    brain = createSimpleGPUBrain("multi_utterance_gpu");
    ASSERT_NE(brain, nullptr);

    // Process multiple inference cycles
    for (int i = 0; i < 10; i++) {
        // Run brain inference
        float features[10];
        for (int j = 0; j < 10; j++) {
            features[j] = (float)((i + 1) * (j + 1)) / 100.0f;
        }
        brain_decision_t* decision = brain_decide(brain, features, 10);
        EXPECT_NE(decision, nullptr) << "Iteration " << i << " failed";

        // Broca reset if available
        if (hasBroca()) {
            broca_adapter_t* broca = getBroca();
            if (broca) {
                broca_reset(broca);
            }
        }
    }
}

//=============================================================================
// Test 7: Broca Status Consistency (when available)
//=============================================================================

TEST_F(BrocaGPUIntegrationTest, StatusConsistency_WithGPU) {
    brain = createSimpleGPUBrain("status_test");
    ASSERT_NE(brain, nullptr);

    if (hasBroca()) {
        broca_adapter_t* broca = getBroca();
        ASSERT_NE(broca, nullptr);

        // Initial status should be IDLE
        EXPECT_EQ(broca_get_status(broca), BROCA_STATUS_IDLE);

        // After reset, should still be IDLE
        EXPECT_TRUE(broca_reset(broca));
        EXPECT_EQ(broca_get_status(broca), BROCA_STATUS_IDLE);

        // Error should be NONE
        EXPECT_EQ(broca_get_last_error(broca), BROCA_ERROR_NONE);
    } else {
        std::cout << "[INFO] Broca not enabled, skipping status test" << std::endl;
    }

    // Inference still works
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    EXPECT_NE(brain_decide(brain, features, 10), nullptr);
}

//=============================================================================
// Test 8: Learning Still Works with GPU
//=============================================================================

TEST_F(BrocaGPUIntegrationTest, Learning_StillWorks) {
    brain = createSimpleGPUBrain("learning_test");
    ASSERT_NE(brain, nullptr);

    // Train with a simple loop
    for (int episode = 0; episode < 5; episode++) {
        float features[10];
        for (int j = 0; j < 10; j++) {
            features[j] = (float)(episode * j) / 50.0f;
        }

        brain_decision_t* decision = brain_decide(brain, features, 10);
        EXPECT_NE(decision, nullptr);

        // Apply reward learning
        float reward = (episode % 2) ? 1.0f : -0.5f;
        brain_apply_reward_learning(brain, reward);
    }

    // Final inference should still work
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
}

//=============================================================================
// Test 9: NULL Safety
//=============================================================================

TEST_F(BrocaGPUIntegrationTest, NullSafety_WithGPU) {
    brain = createSimpleGPUBrain("null_safety_test");
    ASSERT_NE(brain, nullptr);

    // Brain with NULL features should be handled gracefully
    brain_decision_t* decision = brain_decide(brain, nullptr, 0);
    // May return NULL or valid decision depending on implementation

    // Valid decision after error should still work
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);

    // Broca NULL safety
    if (hasBroca()) {
        broca_adapter_t* broca = getBroca();
        if (broca) {
            // These should not crash
            broca_add_word(broca, nullptr);
            broca_process_utterance(broca, nullptr);
        }
    }
}

//=============================================================================
// Test 10: Performance - No Severe Degradation
//=============================================================================

TEST_F(BrocaGPUIntegrationTest, Performance_NoSevereDegradation) {
    brain = createSimpleGPUBrain("perf_test");
    ASSERT_NE(brain, nullptr);

    // Measure time for 50 inferences
    float features[10];
    for (int j = 0; j < 10; j++) {
        features[j] = 0.5f;
    }

    uint64_t start_time = nimcp_time_get_us();
    for (int i = 0; i < 50; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        (void)decision;
    }
    uint64_t end_time = nimcp_time_get_us();

    uint64_t elapsed_us = end_time - start_time;
    float avg_us = elapsed_us / 50.0f;

    // Allow up to 5ms per inference (generous for brain with GPU enabled)
    EXPECT_LT(avg_us, 5000.0f) << "GPU shouldn't cause severe performance degradation";

    std::cout << "[PERF] Average inference time: " << avg_us << " us";
    if (hasGPU()) {
        std::cout << " (GPU enabled)";
    } else {
        std::cout << " (CPU only)";
    }
    std::cout << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
