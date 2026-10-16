/**
 * @file test_broca_gpu.cpp
 * @brief Unit tests for GPU-accelerated Broca's region
 *
 * WHAT: Unit tests for GPU Broca kernels and API
 * WHY:  Verify correctness of GPU-accelerated language production
 * HOW:  Test individual operations: lexical lookup, phoneme encoding, motor commands
 *
 * @version 1.0
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// GPU headers outside extern "C"
#include "gpu/cognitive/nimcp_broca_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/backend/nimcp_kernel_backend.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrocaGPUUnitTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx;
    broca_gpu_context_t* broca_ctx;
    std::vector<broca_gpu_lexical_entry_t> test_lexicon;

    void SetUp() override {
        gpu_ctx = nullptr;
        broca_ctx = nullptr;

        // Initialize kernel backend to detect GPU
        nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);

        // Try to create GPU context
        if (nimcp_cuda_backend_available()) {
            gpu_ctx = nimcp_gpu_context_create(0);
        }

        if (gpu_ctx) {
            broca_gpu_config_t config = broca_gpu_default_config();
            config.max_lexicon_size = 1000;
            config.max_batch_size = 64;
            broca_ctx = broca_gpu_create(gpu_ctx, &config);
        }

        // Setup test lexicon
        setupTestLexicon();
    }

    void TearDown() override {
        if (broca_ctx) {
            broca_gpu_destroy(broca_ctx);
            broca_ctx = nullptr;
        }
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
        nimcp_kernel_backend_shutdown();
    }

    void setupTestLexicon() {
        test_lexicon.clear();

        // Word 1: "hello" -> h,e,l,o
        broca_gpu_lexical_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 1;
        entry.phonemes[0] = 'h'; entry.phonemes[1] = 'e';
        entry.phonemes[2] = 'l'; entry.phonemes[3] = 'o';
        entry.phoneme_count = 4;
        entry.pos = 0;
        entry.frequency = 0.9f;
        entry.activation = 0.5f;
        test_lexicon.push_back(entry);

        // Word 2: "world" -> w,r,l,d
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 2;
        entry.phonemes[0] = 'w'; entry.phonemes[1] = 'r';
        entry.phonemes[2] = 'l'; entry.phonemes[3] = 'd';
        entry.phoneme_count = 4;
        entry.pos = 0;
        entry.frequency = 0.85f;
        entry.activation = 0.4f;
        test_lexicon.push_back(entry);

        // Word 3: "test" -> t,e,s,t
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 3;
        entry.phonemes[0] = 't'; entry.phonemes[1] = 'e';
        entry.phonemes[2] = 's'; entry.phonemes[3] = 't';
        entry.phoneme_count = 4;
        entry.pos = 0;
        entry.frequency = 0.8f;
        entry.activation = 0.3f;
        test_lexicon.push_back(entry);

        // Word 4: "cat" -> k,a,t
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 4;
        entry.phonemes[0] = 'k'; entry.phonemes[1] = 'a';
        entry.phonemes[2] = 't';
        entry.phoneme_count = 3;
        entry.pos = 0;
        entry.frequency = 0.75f;
        entry.activation = 0.2f;
        test_lexicon.push_back(entry);

        // Word 5: "dog" -> d,o,g
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 5;
        entry.phonemes[0] = 'd'; entry.phonemes[1] = 'o';
        entry.phonemes[2] = 'g';
        entry.phoneme_count = 3;
        entry.pos = 0;
        entry.frequency = 0.7f;
        entry.activation = 0.1f;
        test_lexicon.push_back(entry);
    }

    bool hasGPU() const {
        return gpu_ctx != nullptr && broca_ctx != nullptr;
    }

    bool uploadLexicon() {
        if (!broca_ctx || test_lexicon.empty()) return false;
        return broca_gpu_upload_lexicon(broca_ctx, test_lexicon.data(), test_lexicon.size());
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, DefaultConfig_HasSaneValues) {
    broca_gpu_config_t config = broca_gpu_default_config();

    EXPECT_GT(config.max_lexicon_size, 0u);
    EXPECT_GT(config.max_batch_size, 0u);
    EXPECT_GT(config.max_phonemes_per_word, 0u);
    EXPECT_GT(config.max_articulators, 0u);
    EXPECT_GT(config.working_memory_slots, 0u);
    EXPECT_GT(config.activation_decay_rate, 0.0f);
    EXPECT_LE(config.activation_decay_rate, 1.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, Create_WithNullContext_ReturnsNull) {
    broca_gpu_context_t* ctx = broca_gpu_create(nullptr, nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(BrocaGPUUnitTest, Create_WithValidContext_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_NE(broca_ctx, nullptr);
}

TEST_F(BrocaGPUUnitTest, Destroy_WithNull_DoesNotCrash) {
    broca_gpu_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Lexicon Management Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, UploadLexicon_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(uploadLexicon());
    EXPECT_EQ(broca_gpu_get_lexicon_size(broca_ctx), test_lexicon.size());
}

TEST_F(BrocaGPUUnitTest, ClearLexicon_ResetsSize) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();
    EXPECT_GT(broca_gpu_get_lexicon_size(broca_ctx), 0u);

    EXPECT_TRUE(broca_gpu_clear_lexicon(broca_ctx));
    EXPECT_EQ(broca_gpu_get_lexicon_size(broca_ctx), 0u);
}

//=============================================================================
// Lexical Lookup Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, BatchLookup_FindsExistingWords) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    uint32_t word_ids[] = {1, 2, 3};
    broca_gpu_lookup_result_t results[3];

    EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_ctx, word_ids, 3, results));

    // All words should be found
    EXPECT_TRUE(results[0].found);
    EXPECT_TRUE(results[1].found);
    EXPECT_TRUE(results[2].found);

    // Word IDs should match
    EXPECT_EQ(results[0].word_id, 1u);
    EXPECT_EQ(results[1].word_id, 2u);
    EXPECT_EQ(results[2].word_id, 3u);

    // Phoneme counts should match
    EXPECT_EQ(results[0].phoneme_count, 4u); // "hello"
    EXPECT_EQ(results[1].phoneme_count, 4u); // "world"
    EXPECT_EQ(results[2].phoneme_count, 4u); // "test"
}

TEST_F(BrocaGPUUnitTest, BatchLookup_HandlesNonExistentWords) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    uint32_t word_ids[] = {1, 999, 3}; // 999 doesn't exist
    broca_gpu_lookup_result_t results[3];

    EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_ctx, word_ids, 3, results));

    EXPECT_TRUE(results[0].found);
    EXPECT_FALSE(results[1].found); // 999 not found
    EXPECT_TRUE(results[2].found);
}

TEST_F(BrocaGPUUnitTest, BatchLookup_MatchesCPU) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    uint32_t word_ids[] = {1, 2, 3, 4, 5};
    broca_gpu_lookup_result_t gpu_results[5];
    broca_gpu_lookup_result_t cpu_results[5];

    // GPU lookup
    EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_ctx, word_ids, 5, gpu_results));

    // CPU lookup (reference)
    EXPECT_TRUE(broca_cpu_batch_lexical_lookup(
        test_lexicon.data(), test_lexicon.size(),
        word_ids, 5, cpu_results
    ));

    // Compare results
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(gpu_results[i].found, cpu_results[i].found) << "Mismatch at index " << i;
        EXPECT_EQ(gpu_results[i].word_id, cpu_results[i].word_id) << "Mismatch at index " << i;
        EXPECT_EQ(gpu_results[i].phoneme_count, cpu_results[i].phoneme_count) << "Mismatch at index " << i;
    }
}

//=============================================================================
// Activation Update Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, UpdateActivations_AppliesDecay) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Apply decay without boosting
    EXPECT_TRUE(broca_gpu_update_activations(broca_ctx, nullptr, 0, 0.0f, 0.9f));

    // Synchronize and verify (activations should be lower)
    EXPECT_TRUE(broca_gpu_synchronize(broca_ctx));
}

TEST_F(BrocaGPUUnitTest, UpdateActivations_BoostsSpecifiedWords) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    uint32_t boost_ids[] = {1}; // Boost "hello"
    EXPECT_TRUE(broca_gpu_update_activations(broca_ctx, boost_ids, 1, 0.5f, 0.95f));
    EXPECT_TRUE(broca_gpu_synchronize(broca_ctx));
}

//=============================================================================
// Phoneme Encoding Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, EncodePhonemes_ProducesCorrectOutput) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    uint32_t word_ids[] = {1, 4}; // "hello" (4 phonemes) + "cat" (3 phonemes)
    uint8_t phoneme_buffer[32];
    uint32_t phoneme_count = 0;
    uint32_t boundaries[2];

    EXPECT_TRUE(broca_gpu_encode_phonemes(
        broca_ctx, word_ids, 2,
        phoneme_buffer, 32, &phoneme_count, boundaries
    ));

    // Should have 7 phonemes total (4 + 3)
    EXPECT_EQ(phoneme_count, 7u);

    // Check boundaries
    EXPECT_EQ(boundaries[0], 4u); // After "hello"
    EXPECT_EQ(boundaries[1], 7u); // After "cat"

    // Check phonemes
    EXPECT_EQ(phoneme_buffer[0], 'h');
    EXPECT_EQ(phoneme_buffer[1], 'e');
    EXPECT_EQ(phoneme_buffer[2], 'l');
    EXPECT_EQ(phoneme_buffer[3], 'o');
    EXPECT_EQ(phoneme_buffer[4], 'k');
    EXPECT_EQ(phoneme_buffer[5], 'a');
    EXPECT_EQ(phoneme_buffer[6], 't');
}

TEST_F(BrocaGPUUnitTest, EncodePhonemes_MatchesCPU) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    uint32_t word_ids[] = {1, 2, 3}; // hello, world, test
    uint8_t gpu_phonemes[64];
    uint8_t cpu_phonemes[64];
    uint32_t gpu_count = 0, cpu_count = 0;
    uint32_t gpu_boundaries[3], cpu_boundaries[3];

    // GPU encode
    EXPECT_TRUE(broca_gpu_encode_phonemes(
        broca_ctx, word_ids, 3,
        gpu_phonemes, 64, &gpu_count, gpu_boundaries
    ));

    // CPU encode (reference)
    EXPECT_TRUE(broca_cpu_encode_phonemes(
        test_lexicon.data(), test_lexicon.size(),
        word_ids, 3,
        cpu_phonemes, 64, &cpu_count, cpu_boundaries
    ));

    // Compare
    EXPECT_EQ(gpu_count, cpu_count);
    for (uint32_t i = 0; i < gpu_count; i++) {
        EXPECT_EQ(gpu_phonemes[i], cpu_phonemes[i]) << "Phoneme mismatch at index " << i;
    }
}

//=============================================================================
// Motor Command Generation Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, GenerateMotorCommands_ProducesCorrectCount) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phonemes[] = {'h', 'e', 'l', 'o'}; // 4 phonemes
    broca_gpu_motor_command_t commands[64];
    uint32_t command_count = 0;

    EXPECT_TRUE(broca_gpu_generate_motor_commands(
        broca_ctx, phonemes, 4,
        commands, 64, &command_count, 0.0f
    ));

    // Should have 4 phonemes * 6 articulators = 24 commands
    EXPECT_EQ(command_count, 24u);
}

TEST_F(BrocaGPUUnitTest, GenerateMotorCommands_HasCorrectTimestamps) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phonemes[] = {'h', 'e', 'l', 'o'};
    broca_gpu_motor_command_t commands[64];
    uint32_t command_count = 0;

    EXPECT_TRUE(broca_gpu_generate_motor_commands(
        broca_ctx, phonemes, 4,
        commands, 64, &command_count, 100.0f
    ));

    // First phoneme commands should have timestamp near base
    EXPECT_NEAR(commands[0].timestamp_ms, 100.0f, 1.0f);

    // Last phoneme commands should have later timestamp
    EXPECT_GT(commands[command_count - 1].timestamp_ms, commands[0].timestamp_ms);
}

TEST_F(BrocaGPUUnitTest, GenerateMotorCommands_MatchesCPU) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phonemes[] = {'t', 'e', 's', 't'};
    broca_gpu_motor_command_t gpu_commands[64];
    broca_gpu_motor_command_t cpu_commands[64];
    uint32_t gpu_count = 0, cpu_count = 0;

    // GPU generate
    EXPECT_TRUE(broca_gpu_generate_motor_commands(
        broca_ctx, phonemes, 4,
        gpu_commands, 64, &gpu_count, 0.0f
    ));

    // CPU generate (reference)
    EXPECT_TRUE(broca_cpu_generate_motor_commands(
        phonemes, 4,
        cpu_commands, 64, &cpu_count, 0.0f, 6
    ));

    // Compare
    EXPECT_EQ(gpu_count, cpu_count);
    for (uint32_t i = 0; i < gpu_count; i++) {
        EXPECT_EQ(gpu_commands[i].articulator, cpu_commands[i].articulator);
        EXPECT_EQ(gpu_commands[i].phoneme, cpu_commands[i].phoneme);
        EXPECT_NEAR(gpu_commands[i].position, cpu_commands[i].position, 0.01f);
        EXPECT_NEAR(gpu_commands[i].velocity, cpu_commands[i].velocity, 0.01f);
        EXPECT_NEAR(gpu_commands[i].timestamp_ms, cpu_commands[i].timestamp_ms, 0.01f);
    }
}

//=============================================================================
// Timing Adjustment Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, AdjustTiming_ScalesCorrectly) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phonemes[] = {'h', 'e'};
    broca_gpu_motor_command_t commands[32];
    uint32_t command_count = 0;

    EXPECT_TRUE(broca_gpu_generate_motor_commands(
        broca_ctx, phonemes, 2,
        commands, 32, &command_count, 0.0f
    ));

    float original_last_ts = commands[command_count - 1].timestamp_ms;

    // Speed up by 2x
    EXPECT_TRUE(broca_gpu_adjust_timing(broca_ctx, commands, command_count, 0.5f));

    // Timestamps should be halved
    EXPECT_NEAR(commands[command_count - 1].timestamp_ms, original_last_ts * 0.5f, 0.1f);
}

//=============================================================================
// Working Memory Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, WMPush_AddsWords) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint32_t word_ids[] = {1, 2, 3};
    EXPECT_TRUE(broca_gpu_wm_push(broca_ctx, word_ids, 3, 1.0f));

    uint32_t retrieved[10];
    float activations[10];
    uint32_t actual_count = 0;

    EXPECT_TRUE(broca_gpu_wm_get_contents(broca_ctx, retrieved, activations, 10, &actual_count));
    EXPECT_EQ(actual_count, 3u);
    EXPECT_EQ(retrieved[0], 1u);
    EXPECT_EQ(retrieved[1], 2u);
    EXPECT_EQ(retrieved[2], 3u);
}

TEST_F(BrocaGPUUnitTest, WMClear_RemovesAll) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint32_t word_ids[] = {1, 2, 3};
    broca_gpu_wm_push(broca_ctx, word_ids, 3, 1.0f);

    EXPECT_TRUE(broca_gpu_wm_clear(broca_ctx));

    uint32_t retrieved[10];
    uint32_t actual_count = 10;
    EXPECT_TRUE(broca_gpu_wm_get_contents(broca_ctx, retrieved, nullptr, 10, &actual_count));
    EXPECT_EQ(actual_count, 0u);
}

TEST_F(BrocaGPUUnitTest, WMDecay_ReducesActivations) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint32_t word_ids[] = {1};
    broca_gpu_wm_push(broca_ctx, word_ids, 1, 1.0f);

    // Apply decay
    EXPECT_TRUE(broca_gpu_wm_apply_decay(broca_ctx, 0.5f, 0.0f));

    uint32_t retrieved[10];
    float activations[10];
    uint32_t actual_count = 0;
    EXPECT_TRUE(broca_gpu_wm_get_contents(broca_ctx, retrieved, activations, 10, &actual_count));

    // Activation should be reduced
    EXPECT_NEAR(activations[0], 0.5f, 0.01f);
}

//=============================================================================
// Full Pipeline Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, ProduceUtterance_RunsFullPipeline) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    uint32_t word_ids[] = {1, 4}; // "hello cat"
    broca_gpu_motor_command_t commands[256];
    uint32_t command_count = 0;

    EXPECT_TRUE(broca_gpu_produce_utterance(
        broca_ctx, word_ids, 2,
        commands, 256, &command_count, 0.0f
    ));

    // Should have commands (7 phonemes * 6 articulators = 42)
    EXPECT_GT(command_count, 0u);
    EXPECT_EQ(command_count, 42u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, Stats_TracksOperations) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Reset stats
    broca_gpu_reset_stats(broca_ctx);

    // Perform some operations
    uint32_t word_ids[] = {1, 2};
    broca_gpu_lookup_result_t results[2];
    broca_gpu_batch_lexical_lookup(broca_ctx, word_ids, 2, results);

    uint8_t phonemes[16];
    uint32_t phoneme_count;
    broca_gpu_encode_phonemes(broca_ctx, word_ids, 2, phonemes, 16, &phoneme_count, nullptr);

    broca_gpu_motor_command_t commands[64];
    uint32_t command_count;
    broca_gpu_generate_motor_commands(broca_ctx, phonemes, phoneme_count, commands, 64, &command_count, 0.0f);

    // Check stats
    broca_gpu_stats_t stats;
    EXPECT_TRUE(broca_gpu_get_stats(broca_ctx, &stats));

    EXPECT_EQ(stats.lexical_lookups, 2u);
    EXPECT_GT(stats.phonemes_encoded, 0u);
    EXPECT_GT(stats.motor_commands, 0u);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(BrocaGPUUnitTest, NullSafety_AllFunctionsHandleNull) {
    // These should not crash
    broca_gpu_destroy(nullptr);
    EXPECT_FALSE(broca_gpu_synchronize(nullptr));
    EXPECT_FALSE(broca_gpu_upload_lexicon(nullptr, nullptr, 0));
    EXPECT_FALSE(broca_gpu_clear_lexicon(nullptr));
    EXPECT_EQ(broca_gpu_get_lexicon_size(nullptr), 0u);
    EXPECT_FALSE(broca_gpu_batch_lexical_lookup(nullptr, nullptr, 0, nullptr));
    EXPECT_FALSE(broca_gpu_encode_phonemes(nullptr, nullptr, 0, nullptr, 0, nullptr, nullptr));
    EXPECT_FALSE(broca_gpu_generate_motor_commands(nullptr, nullptr, 0, nullptr, 0, nullptr, 0.0f));
    EXPECT_FALSE(broca_gpu_wm_push(nullptr, nullptr, 0, 0.0f));
    EXPECT_FALSE(broca_gpu_wm_get_contents(nullptr, nullptr, nullptr, 0, nullptr));
    EXPECT_FALSE(broca_gpu_get_stats(nullptr, nullptr));

    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
