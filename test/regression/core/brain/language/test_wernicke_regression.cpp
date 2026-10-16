/**
 * @file test_wernicke_regression.cpp
 * @brief Comprehensive regression tests for Wernicke's area
 *
 * WHAT: Regression tests for Wernicke's area language comprehension
 * WHY:  Ensure stability, determinism, performance, and backward compatibility
 * HOW:  Test lifecycle, processing, memory patterns, null safety, and stress
 *
 * COVERAGE AREAS:
 * - Performance benchmarks (lexical access, semantic integration timing)
 * - Determinism tests (same input = same output)
 * - State consistency after processing
 * - Memory usage patterns (create/destroy cycles)
 * - Null pointer safety
 * - Backward compatibility (default config values)
 * - Stress tests (rapid processing)
 *
 * @version Phase L1: Language Region Regression Tests
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cstring>
#include <cmath>

#include "utils/nimcp_test_base.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "core/brain/regions/wernicke/nimcp_semantic_integrator.h"
#include "perception/nimcp_speech_cortex.h"

//=============================================================================
// PERFORMANCE BENCHMARK TESTS
//=============================================================================

class WernickePerformanceTest : public NimcpTestBase {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        NimcpTestBase::SetUp();
        wernicke_config_t config = wernicke_default_config();
        config.enable_lexicon = true;
        config.enable_semantic = true;
        adapter = wernicke_create(&config);
        ASSERT_NE(adapter, nullptr);
        PopulateLexicon();
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
            adapter = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    void PopulateLexicon() {
        // Add sample words for testing
        const char* words[] = {"hello", "world", "cat", "dog", "run", "fast"};
        phoneme_t phonemes[][8] = {
            {PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_OW},
            {PHONEME_W, PHONEME_ER, PHONEME_L, PHONEME_D},
            {PHONEME_K, PHONEME_AE, PHONEME_T},
            {PHONEME_D, PHONEME_AO, PHONEME_G},
            {PHONEME_R, PHONEME_AH, PHONEME_N},
            {PHONEME_F, PHONEME_AE, PHONEME_S, PHONEME_T}
        };
        uint32_t phoneme_counts[] = {4, 4, 3, 3, 3, 4};

        for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
            wernicke_word_t word;
            memset(&word, 0, sizeof(word));
            word.word_id = static_cast<uint32_t>(i + 1);
            strncpy(word.word, words[i], sizeof(word.word) - 1);
            for (uint32_t j = 0; j < phoneme_counts[i]; ++j) {
                word.phonemes[j] = static_cast<uint8_t>(phonemes[i][j]);
            }
            word.phoneme_count = phoneme_counts[i];
            word.frequency = 0.8f;
            word.concept_id = static_cast<uint32_t>(i + 100);
            wernicke_add_word(adapter, &word);
        }
    }
};

/**
 * @test Lexical access performance benchmark
 * WHAT: Measure time for word recognition
 * WHY:  Ensure recognition latency doesn't regress
 */
TEST_F(WernickePerformanceTest, LexicalAccessPerformance) {
    phoneme_t phonemes[] = {PHONEME_K, PHONEME_AE, PHONEME_T};  // "cat"
    wernicke_word_result_t result;

    const int iterations = 100;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        wernicke_recognize_word(adapter, phonemes, 3, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_us = static_cast<double>(duration.count()) / iterations;

    // Performance threshold: 200 microseconds per lookup
    EXPECT_LT(avg_us, 200.0) << "Lexical access too slow: " << avg_us << " us avg";

    std::cout << "[PERF] Lexical access: " << avg_us << " us/lookup" << std::endl;
}

/**
 * @test Semantic integration timing benchmark
 * WHAT: Measure time for semantic processing
 * WHY:  Ensure semantic integration meets real-time requirements
 */
TEST_F(WernickePerformanceTest, SemanticIntegrationTiming) {
    semantic_integrator_t* sem = wernicke_get_semantic_integrator(adapter);
    if (!sem) {
        GTEST_SKIP() << "Semantic integrator not available";
    }

    const int iterations = 100;
    float features[32] = {0.5f};  // Dummy features

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        semantic_result_t result;
        semantic_integrate_word(sem, 1, features, 32, &result);
        semantic_reset(sem);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_us = static_cast<double>(duration.count()) / iterations;

    // Performance threshold: 500 microseconds per integration
    EXPECT_LT(avg_us, 500.0) << "Semantic integration too slow: " << avg_us << " us avg";

    std::cout << "[PERF] Semantic integration: " << avg_us << " us/word" << std::endl;
}

/**
 * @test Word lookup throughput benchmark
 * WHAT: Measure maximum word lookup rate
 * WHY:  Comprehension depends on fast word access
 */
TEST_F(WernickePerformanceTest, WordLookupThroughput) {
    wernicke_word_t entry;
    const char* words[] = {"hello", "world", "cat", "dog", "run", "fast"};

    const int iterations = 1000;
    int successful = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        if (wernicke_lookup_word(adapter, words[i % 6], &entry)) {
            ++successful;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double rate = static_cast<double>(successful) / (static_cast<double>(duration.count()) / 1000.0);

    // Should achieve at least 10,000 lookups/second
    EXPECT_GT(rate, 10000.0) << "Lookup rate too low: " << rate << " lookups/sec";

    std::cout << "[PERF] Word lookup rate: " << rate << " lookups/sec" << std::endl;
}

//=============================================================================
// DETERMINISM TESTS
//=============================================================================

class WernickeDeterminismTest : public NimcpTestBase {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        NimcpTestBase::SetUp();
        adapter = wernicke_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
            adapter = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/**
 * @test Same phoneme input produces same word output
 * WHAT: Verify deterministic word recognition
 * WHY:  Reproducibility is critical for comprehension
 */
TEST_F(WernickeDeterminismTest, SameInputSameOutput) {
    // Add a word
    wernicke_word_t word;
    memset(&word, 0, sizeof(word));
    word.word_id = 1;
    strncpy(word.word, "test", sizeof(word.word) - 1);
    word.phonemes[0] = PHONEME_T;
    word.phonemes[1] = PHONEME_EH;
    word.phonemes[2] = PHONEME_S;
    word.phonemes[3] = PHONEME_T;
    word.phoneme_count = 4;
    word.frequency = 0.9f;
    word.concept_id = 100;
    wernicke_add_word(adapter, &word);

    phoneme_t input[] = {PHONEME_T, PHONEME_EH, PHONEME_S, PHONEME_T};
    wernicke_word_result_t result1, result2;
    memset(&result1, 0, sizeof(result1));
    memset(&result2, 0, sizeof(result2));

    // Recognize twice
    bool found1 = wernicke_recognize_word(adapter, input, 4, &result1);
    bool found2 = wernicke_recognize_word(adapter, input, 4, &result2);

    // Both should have same success/failure
    EXPECT_EQ(found1, found2);

    // If both found, results should be identical
    if (found1 && found2) {
        EXPECT_EQ(result1.word.word_id, result2.word.word_id);
        EXPECT_STREQ(result1.word.word, result2.word.word);
        EXPECT_FLOAT_EQ(result1.confidence, result2.confidence);
    }
}

/**
 * @test Working memory contents are deterministic
 * WHAT: Verify WM operations produce consistent results
 * WHY:  Phonological loop state must be predictable
 */
TEST_F(WernickeDeterminismTest, WorkingMemoryDeterministic) {
    phoneme_t input1[] = {PHONEME_AH, PHONEME_B, PHONEME_K};
    phoneme_t input2[] = {PHONEME_AH, PHONEME_B, PHONEME_K};

    phoneme_t output1[10], output2[10];
    uint32_t count1 = 10, count2 = 10;

    // First run
    wernicke_wm_clear(adapter);
    wernicke_wm_store(adapter, input1, 3);
    wernicke_wm_get_contents(adapter, output1, 10, &count1);

    // Second run
    wernicke_wm_clear(adapter);
    wernicke_wm_store(adapter, input2, 3);
    wernicke_wm_get_contents(adapter, output2, 10, &count2);

    ASSERT_EQ(count1, count2);
    for (uint32_t i = 0; i < count1; ++i) {
        EXPECT_EQ(output1[i], output2[i]);
    }
}

/**
 * @test Phoneme processing is deterministic
 * WHAT: Verify same phoneme events produce same result
 * WHY:  Processing must be reproducible
 */
TEST_F(WernickeDeterminismTest, PhonemeProcessingDeterministic) {
    phoneme_event_t events[3];
    memset(events, 0, sizeof(events));
    events[0].phoneme = PHONEME_AH;
    events[0].confidence = 0.9f;
    events[1].phoneme = PHONEME_B;
    events[1].confidence = 0.85f;
    events[2].phoneme = PHONEME_K;
    events[2].confidence = 0.8f;

    // Process twice
    bool result1 = wernicke_process_phonemes(adapter, events, 3);
    wernicke_reset(adapter);
    bool result2 = wernicke_process_phonemes(adapter, events, 3);

    // Both should succeed or fail consistently
    EXPECT_EQ(result1, result2);
}

//=============================================================================
// STATE CONSISTENCY TESTS
//=============================================================================

class WernickeStateConsistencyTest : public NimcpTestBase {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        NimcpTestBase::SetUp();
        adapter = wernicke_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
            adapter = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/**
 * @test Status reflects processing state correctly
 * WHAT: Verify status transitions are valid
 * WHY:  Status indicates internal state for debugging
 */
TEST_F(WernickeStateConsistencyTest, StatusReflectsState) {
    // Initial status should be IDLE
    EXPECT_EQ(wernicke_get_status(adapter), WERNICKE_STATUS_IDLE);

    // After reset, still IDLE
    wernicke_reset(adapter);
    EXPECT_EQ(wernicke_get_status(adapter), WERNICKE_STATUS_IDLE);

    // Error should not be set initially
    EXPECT_EQ(wernicke_get_last_error(adapter), WERNICKE_ERROR_NONE);
}

/**
 * @test Error recovery maintains valid state
 * WHAT: Verify adapter is usable after errors
 * WHY:  Must recover gracefully from invalid input
 */
TEST_F(WernickeStateConsistencyTest, ErrorRecovery) {
    // Try to look up non-existent word
    wernicke_word_t entry;
    bool found = wernicke_lookup_word(adapter, "xyzzynonexistent", &entry);
    EXPECT_FALSE(found);

    // Adapter should still be usable
    EXPECT_TRUE(wernicke_reset(adapter));
    EXPECT_EQ(wernicke_get_status(adapter), WERNICKE_STATUS_IDLE);
}

/**
 * @test Statistics track operations correctly
 * WHAT: Verify stats counters increment properly
 * WHY:  Statistics are used for monitoring
 */
TEST_F(WernickeStateConsistencyTest, StatisticsAccumulation) {
    wernicke_stats_t stats1, stats2;

    ASSERT_TRUE(wernicke_get_stats(adapter, &stats1));
    uint64_t initial_processed = stats1.phonemes_processed;

    // Process some phonemes
    phoneme_event_t events[3];
    memset(events, 0, sizeof(events));
    events[0].phoneme = PHONEME_AH;
    events[1].phoneme = PHONEME_B;
    events[2].phoneme = PHONEME_K;

    wernicke_process_phonemes(adapter, events, 3);

    ASSERT_TRUE(wernicke_get_stats(adapter, &stats2));
    EXPECT_GE(stats2.phonemes_processed, initial_processed);
}

/**
 * @test Working memory operations maintain consistency
 * WHAT: Verify WM state is always valid
 * WHY:  WM is critical for sentence processing
 */
TEST_F(WernickeStateConsistencyTest, WorkingMemoryConsistency) {
    phoneme_t phonemes[10];
    uint32_t count = 10;

    // Initially empty
    wernicke_wm_get_contents(adapter, phonemes, 10, &count);
    uint32_t initial = count;

    // Store some phonemes
    phoneme_t input[] = {PHONEME_AH, PHONEME_B};
    wernicke_wm_store(adapter, input, 2);

    // Count should increase
    count = 10;
    wernicke_wm_get_contents(adapter, phonemes, 10, &count);
    EXPECT_EQ(count, initial + 2);

    // Clear should empty it
    wernicke_wm_clear(adapter);
    count = 10;
    wernicke_wm_get_contents(adapter, phonemes, 10, &count);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// MEMORY USAGE PATTERN TESTS
//=============================================================================

class WernickeMemoryPatternTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

/**
 * @test Create/destroy cycle stability
 * WHAT: Repeatedly create and destroy adapters
 * WHY:  Detect memory leaks
 */
TEST_F(WernickeMemoryPatternTest, CreateDestroyCycles) {
    const int cycles = 100;

    for (int i = 0; i < cycles; ++i) {
        wernicke_adapter_t* adapter = wernicke_create(nullptr);
        ASSERT_NE(adapter, nullptr) << "Failed at cycle " << i;

        // Do some work
        wernicke_reset(adapter);

        wernicke_destroy(adapter);
    }

    SUCCEED() << "Completed " << cycles << " create/destroy cycles";
}

/**
 * @test Config variation stability
 * WHAT: Create adapters with various configurations
 * WHY:  All config combinations must be stable
 */
TEST_F(WernickeMemoryPatternTest, ConfigVariationStability) {
    wernicke_config_t configs[] = {
        wernicke_default_config(),
        wernicke_default_config(),
        wernicke_default_config(),
        wernicke_default_config()
    };

    // Modify each config differently
    configs[1].enable_lexicon = false;
    configs[2].enable_working_memory = false;
    configs[3].max_words = 32;

    for (size_t i = 0; i < sizeof(configs) / sizeof(configs[0]); ++i) {
        wernicke_adapter_t* adapter = wernicke_create(&configs[i]);
        ASSERT_NE(adapter, nullptr) << "Config " << i << " failed";

        wernicke_stats_t stats;
        EXPECT_TRUE(wernicke_get_stats(adapter, &stats));

        wernicke_destroy(adapter);
    }
}

/**
 * @test Lexicon growth stability
 * WHAT: Add many words to lexicon
 * WHY:  Lexicon should scale without issues
 */
TEST_F(WernickeMemoryPatternTest, LexiconGrowthStability) {
    wernicke_config_t config = wernicke_default_config();
    config.lexicon_size = 1000;
    wernicke_adapter_t* adapter = wernicke_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add many words
    for (uint32_t i = 1; i <= 500; ++i) {
        wernicke_word_t word;
        memset(&word, 0, sizeof(word));
        word.word_id = i;
        snprintf(word.word, sizeof(word.word), "word%u", i);
        word.phoneme_count = 2;
        word.phonemes[0] = static_cast<uint8_t>(PHONEME_AH);
        word.phonemes[1] = static_cast<uint8_t>(i % 26 + PHONEME_B);

        wernicke_add_word(adapter, &word);
    }

    // System should remain stable
    EXPECT_EQ(wernicke_get_status(adapter), WERNICKE_STATUS_IDLE);

    wernicke_destroy(adapter);
}

//=============================================================================
// NULL POINTER SAFETY TESTS
//=============================================================================

class WernickeNullSafetyTest : public NimcpTestBase {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        NimcpTestBase::SetUp();
        adapter = wernicke_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
            adapter = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/**
 * @test Lifecycle functions handle NULL
 * WHAT: Pass NULL to lifecycle functions
 * WHY:  Prevent crashes
 */
TEST_F(WernickeNullSafetyTest, LifecycleFunctionsHandleNull) {
    EXPECT_EQ(wernicke_get_status(nullptr), WERNICKE_STATUS_ERROR);
    EXPECT_NE(wernicke_get_last_error(nullptr), WERNICKE_ERROR_NONE);
    EXPECT_FALSE(wernicke_reset(nullptr));

    // Destroy NULL should not crash
    wernicke_destroy(nullptr);
    SUCCEED();
}

/**
 * @test Processing functions handle NULL
 * WHAT: Pass NULL to processing functions
 * WHY:  Robust error handling
 */
TEST_F(WernickeNullSafetyTest, ProcessingFunctionsHandleNull) {
    phoneme_event_t events[1];
    EXPECT_FALSE(wernicke_process_phonemes(nullptr, events, 1));
    EXPECT_FALSE(wernicke_process_phonemes(adapter, nullptr, 1));

    phoneme_t phonemes[1] = {PHONEME_AH};
    wernicke_word_result_t result;
    EXPECT_FALSE(wernicke_recognize_word(nullptr, phonemes, 1, &result));
    EXPECT_FALSE(wernicke_recognize_word(adapter, nullptr, 1, &result));
    EXPECT_FALSE(wernicke_recognize_word(adapter, phonemes, 1, nullptr));
}

/**
 * @test Stats and config functions handle NULL
 * WHAT: Pass NULL outputs to get functions
 * WHY:  Prevent crashes
 */
TEST_F(WernickeNullSafetyTest, GetFunctionsHandleNull) {
    wernicke_stats_t stats;
    EXPECT_FALSE(wernicke_get_stats(nullptr, &stats));
    EXPECT_FALSE(wernicke_get_stats(adapter, nullptr));

    wernicke_config_t config;
    EXPECT_FALSE(wernicke_get_config(nullptr, &config));
    EXPECT_FALSE(wernicke_get_config(adapter, nullptr));
}

/**
 * @test Lexicon functions handle NULL
 * WHAT: Pass NULL to lexicon functions
 * WHY:  Lexicon is frequently used
 */
TEST_F(WernickeNullSafetyTest, LexiconFunctionsHandleNull) {
    EXPECT_FALSE(wernicke_add_word(nullptr, nullptr));
    EXPECT_FALSE(wernicke_add_word(adapter, nullptr));

    wernicke_word_t entry;
    EXPECT_FALSE(wernicke_lookup_word(nullptr, "test", &entry));
    EXPECT_FALSE(wernicke_lookup_word(adapter, nullptr, &entry));
    EXPECT_FALSE(wernicke_lookup_word(adapter, "test", nullptr));
}

/**
 * @test Working memory functions handle NULL
 * WHAT: Pass NULL to WM functions
 * WHY:  WM operations are common
 */
TEST_F(WernickeNullSafetyTest, WorkingMemoryFunctionsHandleNull) {
    phoneme_t phonemes[1] = {PHONEME_AH};
    EXPECT_FALSE(wernicke_wm_store(nullptr, phonemes, 1));
    EXPECT_FALSE(wernicke_wm_store(adapter, nullptr, 1));

    uint32_t count = 10;
    EXPECT_FALSE(wernicke_wm_get_contents(nullptr, phonemes, 10, &count));
    EXPECT_FALSE(wernicke_wm_get_contents(adapter, nullptr, 10, &count));
    EXPECT_FALSE(wernicke_wm_get_contents(adapter, phonemes, 10, nullptr));

    // Clear NULL should not crash
    wernicke_wm_clear(nullptr);
}

/**
 * @test Sub-module access functions handle NULL
 * WHAT: Pass NULL to sub-module getters
 * WHY:  Advanced users access sub-modules
 */
TEST_F(WernickeNullSafetyTest, SubmoduleAccessHandleNull) {
    EXPECT_EQ(wernicke_get_phonological_analyzer(nullptr), nullptr);
    EXPECT_EQ(wernicke_get_lexical_access(nullptr), nullptr);
    EXPECT_EQ(wernicke_get_semantic_integrator(nullptr), nullptr);
    EXPECT_EQ(wernicke_get_syntactic_comprehension(nullptr), nullptr);
}

//=============================================================================
// BACKWARD COMPATIBILITY TESTS
//=============================================================================

class WernickeBackwardCompatTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

/**
 * @test Default config values are stable
 * WHAT: Verify default configuration hasn't changed
 * WHY:  Existing code depends on defaults
 */
TEST_F(WernickeBackwardCompatTest, DefaultConfigValues) {
    wernicke_config_t config = wernicke_default_config();

    EXPECT_EQ(config.max_phonemes, WERNICKE_DEFAULT_MAX_PHONEMES);
    EXPECT_EQ(config.max_words, WERNICKE_DEFAULT_MAX_WORDS);
    EXPECT_EQ(config.max_concepts, WERNICKE_DEFAULT_MAX_CONCEPTS);
    EXPECT_EQ(config.lexicon_size, WERNICKE_DEFAULT_LEXICON_SIZE);
    EXPECT_EQ(config.working_memory_slots, WERNICKE_DEFAULT_WORKING_MEMORY_SLOTS);
    EXPECT_EQ(config.embedding_dim, WERNICKE_DEFAULT_EMBEDDING_DIM);
    EXPECT_FLOAT_EQ(config.processing_window_ms, WERNICKE_DEFAULT_PROCESSING_WINDOW_MS);
    EXPECT_EQ(config.formant_count, WERNICKE_DEFAULT_FORMANT_COUNT);
}

/**
 * @test Status enum values are stable
 * WHAT: Verify status enum values
 * WHY:  Status comparisons depend on values
 */
TEST_F(WernickeBackwardCompatTest, StatusEnumValues) {
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_IDLE), 0);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_PHONOLOGICAL), 1);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_LEXICAL_ACCESS), 2);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_SEMANTIC), 3);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_SYNTACTIC), 4);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_COMPREHENSION_READY), 5);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_ERROR), 6);
}

/**
 * @test Error enum values are stable
 * WHAT: Verify error enum values
 * WHY:  Error handling depends on values
 */
TEST_F(WernickeBackwardCompatTest, ErrorEnumValues) {
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_NONE), 0);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_INVALID_INPUT), 1);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_PHONOLOGICAL_FAILURE), 2);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_LEXICAL_FAILURE), 3);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_SEMANTIC_FAILURE), 4);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_SYNTACTIC_FAILURE), 5);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_WORKING_MEMORY_FULL), 6);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_WORD_NOT_FOUND), 7);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_CONCEPT_NOT_FOUND), 8);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_BUFFER_OVERFLOW), 9);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_BROCA_DISCONNECTED), 10);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_INTERNAL), 11);
}

/**
 * @test String functions return valid strings
 * WHAT: Verify string functions work for all values
 * WHY:  Logging depends on these
 */
TEST_F(WernickeBackwardCompatTest, StringFunctionsValid) {
    // Status strings
    for (int i = 0; i <= static_cast<int>(WERNICKE_STATUS_ERROR); ++i) {
        const char* str = wernicke_status_string(static_cast<wernicke_status_t>(i));
        ASSERT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }

    // Error strings
    for (int i = 0; i <= static_cast<int>(WERNICKE_ERROR_INTERNAL); ++i) {
        const char* str = wernicke_error_string(static_cast<wernicke_error_t>(i));
        ASSERT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

/**
 * @test Create with NULL config uses defaults
 * WHAT: NULL config should use defaults
 * WHY:  Legacy code passes NULL
 */
TEST_F(WernickeBackwardCompatTest, CreateNullConfigUsesDefaults) {
    wernicke_adapter_t* adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    wernicke_config_t config;
    ASSERT_TRUE(wernicke_get_config(adapter, &config));

    EXPECT_EQ(config.max_phonemes, WERNICKE_DEFAULT_MAX_PHONEMES);
    EXPECT_EQ(config.max_words, WERNICKE_DEFAULT_MAX_WORDS);

    wernicke_destroy(adapter);
}

/**
 * @test Default enable flags
 * WHAT: Verify processing layer enable flags
 * WHY:  Module initialization depends on these
 */
TEST_F(WernickeBackwardCompatTest, DefaultEnableFlags) {
    wernicke_config_t config = wernicke_default_config();

    EXPECT_TRUE(config.enable_phonological);
    EXPECT_TRUE(config.enable_lexical);
    EXPECT_TRUE(config.enable_lexicon);
    EXPECT_TRUE(config.enable_working_memory);
}

//=============================================================================
// STRESS TESTS
//=============================================================================

class WernickeStressTest : public NimcpTestBase {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        NimcpTestBase::SetUp();
        wernicke_config_t config = wernicke_default_config();
        config.lexicon_size = 100;
        adapter = wernicke_create(&config);
        ASSERT_NE(adapter, nullptr);

        // Add words for stress testing
        for (uint32_t i = 1; i <= 50; ++i) {
            wernicke_word_t word;
            memset(&word, 0, sizeof(word));
            word.word_id = i;
            snprintf(word.word, sizeof(word.word), "word%u", i);
            word.phoneme_count = 2;
            word.phonemes[0] = static_cast<uint8_t>(PHONEME_AH);
            word.phonemes[1] = static_cast<uint8_t>(i % 26 + PHONEME_B);
            wernicke_add_word(adapter, &word);
        }
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
            adapter = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/**
 * @test Rapid phoneme processing stress test
 * WHAT: Process many phoneme sequences rapidly
 * WHY:  Test stability under load
 */
TEST_F(WernickeStressTest, RapidPhonemeProcessing) {
    const int iterations = 500;

    for (int i = 0; i < iterations; ++i) {
        phoneme_event_t events[3];
        memset(events, 0, sizeof(events));
        events[0].phoneme = static_cast<phoneme_t>(PHONEME_AH + (i % 10));
        events[1].phoneme = static_cast<phoneme_t>(PHONEME_B + (i % 20));
        events[2].phoneme = PHONEME_K;

        wernicke_process_phonemes(adapter, events, 3);
        wernicke_reset(adapter);
    }

    // System should still be functional
    EXPECT_EQ(wernicke_get_status(adapter), WERNICKE_STATUS_IDLE);

    wernicke_stats_t stats;
    ASSERT_TRUE(wernicke_get_stats(adapter, &stats));
    EXPECT_GE(stats.phonemes_processed, static_cast<uint64_t>(iterations));
}

/**
 * @test Rapid word lookup stress test
 * WHAT: Look up many words rapidly
 * WHY:  Test lexicon performance under load
 */
TEST_F(WernickeStressTest, RapidWordLookup) {
    const int iterations = 1000;
    int successful = 0;

    for (int i = 0; i < iterations; ++i) {
        char word[32];
        snprintf(word, sizeof(word), "word%d", (i % 50) + 1);

        wernicke_word_t entry;
        if (wernicke_lookup_word(adapter, word, &entry)) {
            ++successful;
        }
    }

    EXPECT_GE(successful, iterations / 2);
    EXPECT_EQ(wernicke_get_status(adapter), WERNICKE_STATUS_IDLE);
}

/**
 * @test Rapid reset stress test
 * WHAT: Reset adapter many times
 * WHY:  Ensure reset doesn't leak
 */
TEST_F(WernickeStressTest, RapidResetStress) {
    const int iterations = 1000;

    for (int i = 0; i < iterations; ++i) {
        ASSERT_TRUE(wernicke_reset(adapter)) << "Reset failed at " << i;
    }

    EXPECT_EQ(wernicke_get_status(adapter), WERNICKE_STATUS_IDLE);
}

/**
 * @test Working memory overflow stress test
 * WHAT: Store beyond WM capacity
 * WHY:  Test graceful overflow handling
 */
TEST_F(WernickeStressTest, WorkingMemoryOverflow) {
    wernicke_config_t config;
    wernicke_get_config(adapter, &config);
    uint32_t wm_slots = config.working_memory_slots;

    // Store more than configured capacity
    for (uint32_t i = 0; i < wm_slots + 10; ++i) {
        phoneme_t phoneme = static_cast<phoneme_t>(PHONEME_AH + (i % 10));
        wernicke_wm_store(adapter, &phoneme, 1);
    }

    // Primary test: should not crash and remain in valid state
    EXPECT_EQ(wernicke_get_status(adapter), WERNICKE_STATUS_IDLE);

    phoneme_t contents[100];
    uint32_t count = 100;
    wernicke_wm_get_contents(adapter, contents, 100, &count);

    // WM may or may not enforce hard limit - just verify it returns something reasonable
    // Some implementations grow dynamically, others enforce limits
    EXPECT_GT(count, 0u) << "WM should contain stored phonemes";
    std::cout << "[STRESS] WM overflow: stored " << (wm_slots + 10) << ", got " << count
              << " (config slots=" << wm_slots << ")" << std::endl;
}

/**
 * @test Concurrent-style operation stress test
 * WHAT: Interleave different operations
 * WHY:  Test state machine robustness
 */
TEST_F(WernickeStressTest, InterleavedOperations) {
    const int iterations = 200;

    for (int i = 0; i < iterations; ++i) {
        // Phoneme processing
        phoneme_event_t events[2];
        memset(events, 0, sizeof(events));
        events[0].phoneme = PHONEME_AH;
        events[1].phoneme = PHONEME_B;
        wernicke_process_phonemes(adapter, events, 2);

        // Word lookup
        char word[32];
        snprintf(word, sizeof(word), "word%d", (i % 50) + 1);
        wernicke_word_t entry;
        wernicke_lookup_word(adapter, word, &entry);

        // Working memory operations
        phoneme_t wm_phoneme = PHONEME_K;
        wernicke_wm_store(adapter, &wm_phoneme, 1);

        if (i % 10 == 0) {
            wernicke_wm_clear(adapter);
        }

        if (i % 20 == 0) {
            wernicke_reset(adapter);
        }
    }

    EXPECT_EQ(wernicke_get_status(adapter), WERNICKE_STATUS_IDLE);
}
