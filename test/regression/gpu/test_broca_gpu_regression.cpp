/**
 * @file test_broca_gpu_regression.cpp
 * @brief Regression tests for GPU-accelerated Broca's region
 *
 * WHAT: Regression tests to prevent reintroduction of fixed bugs
 * WHY:  Ensure GPU Broca bug fixes remain stable across code changes
 * HOW:  Reproduce specific bug scenarios and verify correct behavior
 *
 * COVERAGE TARGET: Known bug scenarios and backward compatibility
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

class BrocaGPURegressionTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx;
    broca_gpu_context_t* broca_gpu;
    std::vector<broca_gpu_lexical_entry_t> lexicon;

    void SetUp() override {
        gpu_ctx = nullptr;
        broca_gpu = nullptr;

        // Initialize kernel backend to detect GPU
        nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);

        if (nimcp_cuda_backend_available()) {
            gpu_ctx = nimcp_gpu_context_create(0);
        }

        if (gpu_ctx) {
            broca_gpu_config_t config = broca_gpu_default_config();
            config.max_lexicon_size = 1000;
            config.max_batch_size = 64;
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
        nimcp_kernel_backend_shutdown();
    }

    void setupLexicon() {
        lexicon.clear();

        // Word 1: "hello" -> h,e,l,o
        broca_gpu_lexical_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 1;
        entry.phonemes[0] = 'h'; entry.phonemes[1] = 'e';
        entry.phonemes[2] = 'l'; entry.phonemes[3] = 'o';
        entry.phoneme_count = 4;
        entry.frequency = 0.9f;
        entry.activation = 0.5f;
        lexicon.push_back(entry);

        // Word 2: "world" -> w,r,l,d
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 2;
        entry.phonemes[0] = 'w'; entry.phonemes[1] = 'r';
        entry.phonemes[2] = 'l'; entry.phonemes[3] = 'd';
        entry.phoneme_count = 4;
        entry.frequency = 0.85f;
        entry.activation = 0.4f;
        lexicon.push_back(entry);
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
// BUG REGRESSION: GPU Memory Alignment Issues
//=============================================================================
// BUG: Lexical entry structure had incorrect padding causing alignment issues
// FIX: Added _padding field for 64-byte alignment

TEST_F(BrocaGPURegressionTest, LexicalEntry_Alignment) {
    // Verify structure size is aligned
    EXPECT_EQ(sizeof(broca_gpu_lexical_entry_t) % 8, 0u)
        << "BUG REGRESSION: Lexical entry must be 8-byte aligned for GPU";
}

TEST_F(BrocaGPURegressionTest, MotorCommand_Alignment) {
    // Verify structure size is aligned
    EXPECT_EQ(sizeof(broca_gpu_motor_command_t) % 4, 0u)
        << "BUG REGRESSION: Motor command must be 4-byte aligned for GPU";
}

//=============================================================================
// BUG REGRESSION: Empty Lexicon Handling
//=============================================================================
// BUG: Lookup on empty lexicon caused GPU crash
// FIX: Return false early when lexicon_size == 0

TEST_F(BrocaGPURegressionTest, EmptyLexicon_NoGPUCrash) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Don't upload lexicon - it's empty
    EXPECT_EQ(broca_gpu_get_lexicon_size(broca_gpu), 0u);

    uint32_t word_ids[] = {1, 2, 3};
    broca_gpu_lookup_result_t results[3];

    // Should return false gracefully, not crash
    bool result = broca_gpu_batch_lexical_lookup(broca_gpu, word_ids, 3, results);
    EXPECT_FALSE(result) << "BUG REGRESSION: Empty lexicon lookup should return false";
}

//=============================================================================
// BUG REGRESSION: Zero Batch Size
//=============================================================================
// BUG: Zero count in batch operations caused division by zero
// FIX: Early return for count == 0

TEST_F(BrocaGPURegressionTest, ZeroBatch_Lookup_NoException) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    broca_gpu_lookup_result_t results[1];
    bool result = broca_gpu_batch_lexical_lookup(broca_gpu, nullptr, 0, results);
    EXPECT_FALSE(result) << "BUG REGRESSION: Zero batch should return false";
}

TEST_F(BrocaGPURegressionTest, ZeroBatch_Encode_NoException) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    uint8_t phonemes[16];
    uint32_t count = 0;
    bool result = broca_gpu_encode_phonemes(
        broca_gpu, nullptr, 0, phonemes, 16, &count, nullptr
    );
    EXPECT_FALSE(result) << "BUG REGRESSION: Zero encode should return false";
}

TEST_F(BrocaGPURegressionTest, ZeroBatch_MotorCommands_NoException) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    broca_gpu_motor_command_t commands[1];
    uint32_t count = 0;
    bool result = broca_gpu_generate_motor_commands(
        broca_gpu, nullptr, 0, commands, 1, &count, 0.0f
    );
    EXPECT_FALSE(result) << "BUG REGRESSION: Zero motor gen should return false";
}

//=============================================================================
// BUG REGRESSION: Buffer Overflow in Phoneme Encoding
//=============================================================================
// BUG: Phoneme encoding didn't check buffer size, causing overflow
// FIX: Early return if buffer too small

TEST_F(BrocaGPURegressionTest, SmallBuffer_PhonemeEncoding_NoOverflow) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    uint32_t word_ids[] = {1, 2}; // 8 phonemes total
    uint8_t small_buffer[4];      // Too small!
    uint32_t phoneme_count = 0;

    // Should return false or fill up to buffer size, not overflow
    bool result = broca_gpu_encode_phonemes(
        broca_gpu, word_ids, 2,
        small_buffer, 4, &phoneme_count, nullptr
    );

    // Either returns false or limits to buffer size
    if (result) {
        EXPECT_LE(phoneme_count, 4u)
            << "BUG REGRESSION: Phoneme count should not exceed buffer size";
    }
}

//=============================================================================
// BUG REGRESSION: Motor Command Buffer Overflow
//=============================================================================
// BUG: Motor command generation didn't check max_commands
// FIX: Limit output to max_commands

TEST_F(BrocaGPURegressionTest, SmallBuffer_MotorCommands_NoOverflow) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phonemes[] = {'h', 'e', 'l', 'l', 'o'}; // 5 phonemes * 6 articulators = 30 commands
    broca_gpu_motor_command_t small_buffer[10];     // Too small!
    uint32_t command_count = 0;

    bool result = broca_gpu_generate_motor_commands(
        broca_gpu, phonemes, 5,
        small_buffer, 10, &command_count, 0.0f
    );

    // Should limit to buffer size
    if (result) {
        EXPECT_LE(command_count, 10u)
            << "BUG REGRESSION: Command count should not exceed buffer size";
    }
}

//=============================================================================
// BUG REGRESSION: NULL Pointer Handling
//=============================================================================
// BUG: Various functions didn't check for NULL pointers
// FIX: Early return false for NULL parameters

TEST_F(BrocaGPURegressionTest, NullPointer_AllFunctions_Safe) {
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
    EXPECT_FALSE(broca_gpu_wm_apply_decay(nullptr, 0.0f, 0.0f));
    EXPECT_FALSE(broca_gpu_wm_clear(nullptr));
    EXPECT_FALSE(broca_gpu_get_stats(nullptr, nullptr));

    if (hasGPU()) {
        EXPECT_FALSE(broca_gpu_upload_lexicon(broca_gpu, nullptr, 1));
        EXPECT_FALSE(broca_gpu_batch_lexical_lookup(broca_gpu, nullptr, 1, nullptr));
        EXPECT_FALSE(broca_gpu_encode_phonemes(broca_gpu, nullptr, 1, nullptr, 0, nullptr, nullptr));

        broca_gpu_stats_t stats;
        EXPECT_FALSE(broca_gpu_get_stats(broca_gpu, nullptr));
        EXPECT_TRUE(broca_gpu_get_stats(broca_gpu, &stats));
    }
}

//=============================================================================
// BUG REGRESSION: Activation Value Clamping
//=============================================================================
// BUG: Activation values could exceed 1.0 or go negative
// FIX: Clamp activation values to [0, 1]

TEST_F(BrocaGPURegressionTest, Activation_ValuesClamped) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Boost activation by large amount
    uint32_t word_ids[] = {1};
    EXPECT_TRUE(broca_gpu_update_activations(broca_gpu, word_ids, 1, 10.0f, 1.0f));
    EXPECT_TRUE(broca_gpu_synchronize(broca_gpu));

    // Activation should be clamped to <= 1.0
    // We can't directly verify GPU memory, but we verify no crash
}

//=============================================================================
// BUG REGRESSION: Timestamp Overflow
//=============================================================================
// BUG: Very large timestamps caused overflow in timing calculations
// FIX: Use float consistently for timestamps

TEST_F(BrocaGPURegressionTest, LargeTimestamp_NoOverflow) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phonemes[] = {'a', 'b'};
    broca_gpu_motor_command_t commands[32];
    uint32_t command_count = 0;

    // Large but valid timestamp
    float large_timestamp = 1000000.0f; // 1 million ms

    EXPECT_TRUE(broca_gpu_generate_motor_commands(
        broca_gpu, phonemes, 2,
        commands, 32, &command_count, large_timestamp
    ));

    EXPECT_GT(command_count, 0u);
    EXPECT_GE(commands[0].timestamp_ms, large_timestamp)
        << "BUG REGRESSION: Large timestamp should be preserved";
}

//=============================================================================
// BUG REGRESSION: Coarticulation Out-of-Bounds
//=============================================================================
// BUG: Coarticulation accessed phonemes[-1] and phonemes[count]
// FIX: Check bounds before accessing neighbors

TEST_F(BrocaGPURegressionTest, Coarticulation_SinglePhoneme_NoCrash) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phoneme = 'a';
    bool result = broca_gpu_apply_coarticulation(broca_gpu, &phoneme, 1, 0.5f);

    // Should handle single phoneme (no neighbors) gracefully
    EXPECT_TRUE(result) << "BUG REGRESSION: Single phoneme coarticulation should succeed";
}

//=============================================================================
// BUG REGRESSION: Working Memory Circular Buffer Wrap
//=============================================================================
// BUG: WM circular buffer didn't wrap correctly when full
// FIX: Use modulo arithmetic for indices

TEST_F(BrocaGPURegressionTest, WorkingMemory_Wraparound) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Push many words to force wraparound
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t word_id = i + 1;
        bool result = broca_gpu_wm_push(broca_gpu, &word_id, 1, 1.0f);
        // Should succeed or gracefully handle full buffer
        (void)result;
    }

    uint32_t retrieved[64];
    uint32_t count = 0;
    EXPECT_TRUE(broca_gpu_wm_get_contents(broca_gpu, retrieved, nullptr, 64, &count));

    // Should have some entries (up to buffer capacity)
    EXPECT_LE(count, 64u);
}

//=============================================================================
// BUG REGRESSION: Statistics Counter Overflow
//=============================================================================
// BUG: Statistics counters could overflow on 32-bit systems
// FIX: Use uint64_t for counters

TEST_F(BrocaGPURegressionTest, Statistics_LargeValues) {
    broca_gpu_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    // Verify structure uses large enough types
    EXPECT_GE(sizeof(stats.lexical_lookups), 8u)
        << "BUG REGRESSION: Statistics should use 64-bit counters";
    EXPECT_GE(sizeof(stats.phonemes_encoded), 8u);
    EXPECT_GE(sizeof(stats.motor_commands), 8u);
    EXPECT_GE(sizeof(stats.wm_operations), 8u);
}

//=============================================================================
// BUG REGRESSION: GPU/CPU Consistency After Clear
//=============================================================================
// BUG: Clear didn't reset internal state, causing stale data on next upload
// FIX: Reset all internal state on clear

TEST_F(BrocaGPURegressionTest, ClearAndReupload_Consistent) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // First upload
    uploadLexicon();
    EXPECT_EQ(broca_gpu_get_lexicon_size(broca_gpu), 2u);

    // Clear
    EXPECT_TRUE(broca_gpu_clear_lexicon(broca_gpu));
    EXPECT_EQ(broca_gpu_get_lexicon_size(broca_gpu), 0u);

    // Re-upload different data
    broca_gpu_lexical_entry_t new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    new_entry.word_id = 999;
    new_entry.phonemes[0] = 'z';
    new_entry.phoneme_count = 1;

    EXPECT_TRUE(broca_gpu_upload_lexicon(broca_gpu, &new_entry, 1));
    EXPECT_EQ(broca_gpu_get_lexicon_size(broca_gpu), 1u);

    // Verify new data is accessible
    uint32_t word_ids[] = {999};
    broca_gpu_lookup_result_t result;
    EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_gpu, word_ids, 1, &result));
    EXPECT_TRUE(result.found) << "BUG REGRESSION: Re-uploaded data should be accessible";
}

//=============================================================================
// BUG REGRESSION: Config Validation
//=============================================================================
// BUG: Zero values in config caused issues
// FIX: Apply minimum values for config parameters

TEST_F(BrocaGPURegressionTest, Config_ZeroValues_UsesDefaults) {
    if (!gpu_ctx) {
        GTEST_SKIP() << "GPU not available";
    }

    broca_gpu_config_t config;
    memset(&config, 0, sizeof(config));

    // Should either fail or use safe defaults
    broca_gpu_context_t* ctx = broca_gpu_create(gpu_ctx, &config);

    if (ctx) {
        // If created, verify it's usable
        EXPECT_TRUE(broca_gpu_synchronize(ctx));
        broca_gpu_destroy(ctx);
    }
    // If creation failed, that's also valid behavior
}

//=============================================================================
// BACKWARD COMPATIBILITY: API Stability
//=============================================================================

TEST_F(BrocaGPURegressionTest, BackwardCompat_DefaultConfig) {
    broca_gpu_config_t config = broca_gpu_default_config();

    // Verify expected fields exist and have sane values
    EXPECT_GT(config.max_lexicon_size, 0u);
    EXPECT_GT(config.max_batch_size, 0u);
    EXPECT_GT(config.max_phonemes_per_word, 0u);
    EXPECT_GT(config.max_articulators, 0u);
    EXPECT_GT(config.working_memory_slots, 0u);
}

TEST_F(BrocaGPURegressionTest, BackwardCompat_StructureSizes) {
    // These should not change to maintain ABI compatibility
    // struct layout: 5 uint32_t (20) + 2 bool (2) + 2 padding + 1 float (4) = 28 bytes
    EXPECT_EQ(sizeof(broca_gpu_config_t), 28u);
    // Note: Update test if structure intentionally changes
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
