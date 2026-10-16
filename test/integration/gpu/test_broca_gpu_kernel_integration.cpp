/**
 * @file test_broca_gpu_kernel_integration.cpp
 * @brief Integration tests for GPU Broca kernels with brain and GPU systems
 *
 * WHAT: Integration tests for GPU-accelerated Broca's region
 * WHY:  Verify GPU Broca works correctly with other GPU systems
 * HOW:  Test GPU Broca with GPU context, brain integration, and multi-module coordination
 *
 * @version 1.0
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>

// GPU headers outside extern "C"
#include "gpu/cognitive/nimcp_broca_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/backend/nimcp_kernel_backend.h"

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrocaGPUKernelIntegrationTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx;
    broca_gpu_context_t* broca_gpu;
    brain_t brain;
    std::vector<broca_gpu_lexical_entry_t> lexicon;

    void SetUp() override {
        gpu_ctx = nullptr;
        broca_gpu = nullptr;
        brain = nullptr;

        // Initialize kernel backend to detect GPU
        nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);

        // Try to create GPU context
        if (nimcp_cuda_backend_available()) {
            gpu_ctx = nimcp_gpu_context_create(0);
        }

        if (gpu_ctx) {
            broca_gpu_config_t config = broca_gpu_default_config();
            config.max_lexicon_size = 5000;
            config.max_batch_size = 128;
            config.enable_coarticulation = true;
            config.enable_async_transfer = true;
            broca_gpu = broca_gpu_create(gpu_ctx, &config);
        }

        setupLexicon();
    }

    void TearDown() override {
        if (broca_gpu) {
            broca_gpu_destroy(broca_gpu);
            broca_gpu = nullptr;
        }
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        nimcp_kernel_backend_shutdown();
    }

    void setupLexicon() {
        lexicon.clear();

        // Create a reasonable lexicon (100 words)
        const char* words[] = {
            "hello", "world", "test", "cat", "dog", "run", "walk", "eat",
            "sleep", "think", "speak", "read", "write", "learn", "teach",
            "build", "create", "destroy", "help", "save", "load", "process"
        };
        const int word_count = sizeof(words) / sizeof(words[0]);

        for (int i = 0; i < 100; i++) {
            broca_gpu_lexical_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            entry.word_id = i + 1;

            // Use word pattern or index-based phonemes
            const char* word = words[i % word_count];
            size_t len = strlen(word);
            entry.phoneme_count = (len > 16) ? 16 : len;
            for (uint32_t j = 0; j < entry.phoneme_count; j++) {
                entry.phonemes[j] = word[j];
            }

            entry.pos = (i % 4);  // Vary POS
            entry.frequency = 1.0f - (i * 0.009f);  // Decreasing frequency
            entry.activation = 0.5f;

            lexicon.push_back(entry);
        }
    }

    bool hasGPU() const {
        return gpu_ctx != nullptr && broca_gpu != nullptr;
    }

    bool uploadLexicon() {
        if (!broca_gpu || lexicon.empty()) return false;
        return broca_gpu_upload_lexicon(broca_gpu, lexicon.data(), lexicon.size());
    }
};

//=============================================================================
// GPU Context Integration Tests
//=============================================================================

TEST_F(BrocaGPUKernelIntegrationTest, MultipleContexts_IndependentOperation) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Create a second GPU Broca context
    broca_gpu_config_t config = broca_gpu_default_config();
    broca_gpu_context_t* broca_gpu2 = broca_gpu_create(gpu_ctx, &config);
    ASSERT_NE(broca_gpu2, nullptr);

    // Upload different lexicons
    uploadLexicon();
    broca_gpu_lexical_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.word_id = 999;
    entry.phonemes[0] = 'x';
    entry.phoneme_count = 1;
    EXPECT_TRUE(broca_gpu_upload_lexicon(broca_gpu2, &entry, 1));

    // Verify independent operation
    EXPECT_EQ(broca_gpu_get_lexicon_size(broca_gpu), lexicon.size());
    EXPECT_EQ(broca_gpu_get_lexicon_size(broca_gpu2), 1u);

    broca_gpu_destroy(broca_gpu2);
}

TEST_F(BrocaGPUKernelIntegrationTest, SharedGPUContext_ProperSynchronization) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Interleave operations that share GPU context
    uint32_t word_ids1[] = {1, 2, 3};
    uint32_t word_ids2[] = {4, 5, 6};
    broca_gpu_lookup_result_t results1[3], results2[3];

    // Interleaved lookups
    EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_gpu, word_ids1, 3, results1));
    EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_gpu, word_ids2, 3, results2));

    // Verify results
    EXPECT_TRUE(results1[0].found);
    EXPECT_TRUE(results2[0].found);
    EXPECT_EQ(results1[0].word_id, 1u);
    EXPECT_EQ(results2[0].word_id, 4u);
}

//=============================================================================
// Large Batch Processing Tests
//=============================================================================

TEST_F(BrocaGPUKernelIntegrationTest, LargeBatch_LexicalLookup) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Create large batch (all 100 words)
    std::vector<uint32_t> word_ids(100);
    for (int i = 0; i < 100; i++) {
        word_ids[i] = i + 1;
    }

    std::vector<broca_gpu_lookup_result_t> results(100);

    auto start = std::chrono::high_resolution_clock::now();
    EXPECT_TRUE(broca_gpu_batch_lexical_lookup(
        broca_gpu, word_ids.data(), 100, results.data()
    ));
    auto end = std::chrono::high_resolution_clock::now();

    // Verify all found
    int found_count = 0;
    for (int i = 0; i < 100; i++) {
        if (results[i].found) found_count++;
    }
    EXPECT_EQ(found_count, 100);

    // Log performance
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Large batch lookup (100 words): " << duration.count() << " us" << std::endl;
}

TEST_F(BrocaGPUKernelIntegrationTest, LargeBatch_PhonemeEncoding) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Encode 50 words
    std::vector<uint32_t> word_ids(50);
    for (int i = 0; i < 50; i++) {
        word_ids[i] = i + 1;
    }

    uint8_t phonemes[1024];
    uint32_t phoneme_count = 0;
    uint32_t boundaries[50];

    auto start = std::chrono::high_resolution_clock::now();
    EXPECT_TRUE(broca_gpu_encode_phonemes(
        broca_gpu, word_ids.data(), 50,
        phonemes, 1024, &phoneme_count, boundaries
    ));
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_GT(phoneme_count, 0u);

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Large batch phoneme encoding (50 words, " << phoneme_count
              << " phonemes): " << duration.count() << " us" << std::endl;
}

//=============================================================================
// Full Pipeline Integration Tests
//=============================================================================

TEST_F(BrocaGPUKernelIntegrationTest, FullPipeline_WordsToMotorCommands) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Run full pipeline: words -> phonemes -> motor commands
    uint32_t word_ids[] = {1, 2, 3, 4, 5}; // hello, world, test, cat, dog
    broca_gpu_motor_command_t commands[512];
    uint32_t command_count = 0;

    auto start = std::chrono::high_resolution_clock::now();
    EXPECT_TRUE(broca_gpu_produce_utterance(
        broca_gpu, word_ids, 5,
        commands, 512, &command_count, 0.0f
    ));
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_GT(command_count, 0u);

    // Verify command structure
    for (uint32_t i = 0; i < command_count; i++) {
        EXPECT_LT(commands[i].articulator, 6u); // 6 articulators
        EXPECT_GE(commands[i].position, 0.0f);
        EXPECT_LE(commands[i].position, 1.0f);
    }

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Full pipeline (5 words -> " << command_count
              << " motor commands): " << duration.count() << " us" << std::endl;
}

TEST_F(BrocaGPUKernelIntegrationTest, Pipeline_WithTimingAdjustment) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    uint32_t word_ids[] = {1, 2};
    broca_gpu_motor_command_t commands[128];
    uint32_t command_count = 0;

    EXPECT_TRUE(broca_gpu_produce_utterance(
        broca_gpu, word_ids, 2,
        commands, 128, &command_count, 100.0f
    ));

    ASSERT_GT(command_count, 0u);

    float original_duration = commands[command_count - 1].timestamp_ms - commands[0].timestamp_ms;

    // Speed up by 2x
    EXPECT_TRUE(broca_gpu_adjust_timing(broca_gpu, commands, command_count, 0.5f));

    float new_duration = commands[command_count - 1].timestamp_ms - commands[0].timestamp_ms;

    EXPECT_NEAR(new_duration, original_duration * 0.5f, 1.0f);
}

//=============================================================================
// Working Memory Integration Tests
//=============================================================================

TEST_F(BrocaGPUKernelIntegrationTest, WorkingMemory_ProductionLoop) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Push words to working memory
    uint32_t words[] = {1, 2, 3};
    EXPECT_TRUE(broca_gpu_wm_push(broca_gpu, words, 3, 1.0f));

    // Retrieve and produce from WM contents
    uint32_t wm_words[10];
    float activations[10];
    uint32_t wm_count = 0;
    EXPECT_TRUE(broca_gpu_wm_get_contents(broca_gpu, wm_words, activations, 10, &wm_count));
    EXPECT_EQ(wm_count, 3u);

    // Apply decay
    EXPECT_TRUE(broca_gpu_wm_apply_decay(broca_gpu, 0.8f, 0.1f));

    // Re-retrieve and verify decay
    EXPECT_TRUE(broca_gpu_wm_get_contents(broca_gpu, wm_words, activations, 10, &wm_count));
    EXPECT_NEAR(activations[0], 0.8f, 0.01f);

    // Produce utterance from WM
    broca_gpu_motor_command_t commands[256];
    uint32_t command_count = 0;
    EXPECT_TRUE(broca_gpu_produce_utterance(
        broca_gpu, wm_words, wm_count,
        commands, 256, &command_count, 0.0f
    ));
    EXPECT_GT(command_count, 0u);
}

//=============================================================================
// GPU/CPU Equivalence Integration Tests
//=============================================================================

TEST_F(BrocaGPUKernelIntegrationTest, GPUCPUEquivalence_FullBatch) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Test with 20 words
    std::vector<uint32_t> word_ids(20);
    for (int i = 0; i < 20; i++) {
        word_ids[i] = i + 1;
    }

    // GPU lookup
    std::vector<broca_gpu_lookup_result_t> gpu_results(20);
    EXPECT_TRUE(broca_gpu_batch_lexical_lookup(
        broca_gpu, word_ids.data(), 20, gpu_results.data()
    ));

    // CPU lookup
    std::vector<broca_gpu_lookup_result_t> cpu_results(20);
    EXPECT_TRUE(broca_cpu_batch_lexical_lookup(
        lexicon.data(), lexicon.size(),
        word_ids.data(), 20, cpu_results.data()
    ));

    // Compare
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(gpu_results[i].found, cpu_results[i].found)
            << "Mismatch at word " << word_ids[i];
        EXPECT_EQ(gpu_results[i].phoneme_count, cpu_results[i].phoneme_count)
            << "Phoneme count mismatch at word " << word_ids[i];
    }
}

TEST_F(BrocaGPUKernelIntegrationTest, GPUCPUEquivalence_MotorCommands) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phonemes[] = {'h', 'e', 'l', 'l', 'o', 'w', 'o', 'r', 'l', 'd'};
    const uint32_t num_phonemes = 10;

    // GPU generate
    std::vector<broca_gpu_motor_command_t> gpu_commands(num_phonemes * 6);
    uint32_t gpu_count = 0;
    EXPECT_TRUE(broca_gpu_generate_motor_commands(
        broca_gpu, phonemes, num_phonemes,
        gpu_commands.data(), gpu_commands.size(), &gpu_count, 0.0f
    ));

    // CPU generate
    std::vector<broca_gpu_motor_command_t> cpu_commands(num_phonemes * 6);
    uint32_t cpu_count = 0;
    EXPECT_TRUE(broca_cpu_generate_motor_commands(
        phonemes, num_phonemes,
        cpu_commands.data(), cpu_commands.size(), &cpu_count, 0.0f, 6
    ));

    // Compare
    EXPECT_EQ(gpu_count, cpu_count);
    for (uint32_t i = 0; i < gpu_count && i < cpu_count; i++) {
        EXPECT_EQ(gpu_commands[i].articulator, cpu_commands[i].articulator);
        EXPECT_NEAR(gpu_commands[i].position, cpu_commands[i].position, 0.01f);
    }
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(BrocaGPUKernelIntegrationTest, Stress_RepeatedOperations) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Perform many operations
    for (int iter = 0; iter < 100; iter++) {
        uint32_t word_ids[] = {(uint32_t)(iter % 100) + 1};
        broca_gpu_lookup_result_t result;
        EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_gpu, word_ids, 1, &result));
        EXPECT_TRUE(result.found);
    }

    broca_gpu_stats_t stats;
    EXPECT_TRUE(broca_gpu_get_stats(broca_gpu, &stats));
    EXPECT_EQ(stats.lexical_lookups, 100u);
}

TEST_F(BrocaGPUKernelIntegrationTest, Stress_MixedOperations) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    for (int iter = 0; iter < 50; iter++) {
        // Lookup
        uint32_t word_ids[] = {1, 2, 3};
        broca_gpu_lookup_result_t results[3];
        EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_gpu, word_ids, 3, results));

        // Encode
        uint8_t phonemes[64];
        uint32_t phoneme_count;
        EXPECT_TRUE(broca_gpu_encode_phonemes(
            broca_gpu, word_ids, 3, phonemes, 64, &phoneme_count, nullptr
        ));

        // Motor commands
        broca_gpu_motor_command_t commands[128];
        uint32_t command_count;
        EXPECT_TRUE(broca_gpu_generate_motor_commands(
            broca_gpu, phonemes, phoneme_count,
            commands, 128, &command_count, 0.0f
        ));

        // WM push and clear
        EXPECT_TRUE(broca_gpu_wm_push(broca_gpu, word_ids, 3, 1.0f));
        EXPECT_TRUE(broca_gpu_wm_clear(broca_gpu));
    }

    EXPECT_TRUE(broca_gpu_synchronize(broca_gpu));
}

//=============================================================================
// Statistics Verification Tests
//=============================================================================

TEST_F(BrocaGPUKernelIntegrationTest, Statistics_AccumulateCorrectly) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();
    broca_gpu_reset_stats(broca_gpu);

    // 10 lookups of 3 words each
    for (int i = 0; i < 10; i++) {
        uint32_t word_ids[] = {1, 2, 3};
        broca_gpu_lookup_result_t results[3];
        broca_gpu_batch_lexical_lookup(broca_gpu, word_ids, 3, results);
    }

    broca_gpu_stats_t stats;
    EXPECT_TRUE(broca_gpu_get_stats(broca_gpu, &stats));
    EXPECT_EQ(stats.lexical_lookups, 30u); // 10 * 3

    // 5 phoneme encodings
    for (int i = 0; i < 5; i++) {
        uint32_t word_ids[] = {1}; // "hello" has 5 phonemes
        uint8_t phonemes[16];
        uint32_t phoneme_count;
        broca_gpu_encode_phonemes(broca_gpu, word_ids, 1, phonemes, 16, &phoneme_count, nullptr);
    }

    EXPECT_TRUE(broca_gpu_get_stats(broca_gpu, &stats));
    EXPECT_GT(stats.phonemes_encoded, 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
