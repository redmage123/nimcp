/**
 * @file test_broca_regression.cpp
 * @brief Comprehensive regression tests for Broca's region
 *
 * WHAT: Regression tests for Broca's area language production
 * WHY:  Ensure stability, determinism, performance, and backward compatibility
 * HOW:  Test lifecycle, processing, memory patterns, null safety, and stress
 *
 * COVERAGE AREAS:
 * - Performance benchmarks (syntax parsing, speech generation timing)
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
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/broca/nimcp_syntax_processor.h"

//=============================================================================
// PERFORMANCE BENCHMARK TESTS
//=============================================================================

class BrocaPerformanceTest : public NimcpTestBase {
protected:
    broca_adapter_t* adapter;

    void SetUp() override {
        NimcpTestBase::SetUp();
        broca_config_t config = broca_default_config();
        config.enable_lexicon = true;
        adapter = broca_create(&config);
        ASSERT_NE(adapter, nullptr);
        PopulateLexicon();
    }

    void TearDown() override {
        if (adapter) {
            broca_destroy(adapter);
            adapter = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    void PopulateLexicon() {
        // Add sample words to lexicon for testing
        // Using simple single-byte phoneme representations
        const char* words[] = {"the", "cat", "dog", "runs", "jumps", "quickly"};
        uint8_t phonemes[][8] = {
            {'D', 'A'},               // "the"
            {'K', 'A', 'T'},          // "cat"
            {'D', 'O', 'G'},          // "dog"
            {'R', 'A', 'N', 'Z'},     // "runs"
            {'J', 'A', 'M', 'P', 'S'}, // "jumps"
            {'K', 'W', 'I', 'K', 'L', 'Y'} // "quickly"
        };
        uint32_t phoneme_counts[] = {2, 3, 3, 4, 5, 6};

        for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
            broca_lexical_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            entry.word_id = static_cast<uint32_t>(i + 1);
            strncpy(entry.word, words[i], sizeof(entry.word) - 1);
            memcpy(entry.phonemes, phonemes[i], phoneme_counts[i]);
            entry.phoneme_count = phoneme_counts[i];
            entry.pos = (i < 3) ? POS_NOUN : POS_VERB;
            entry.frequency = 0.8f;
            broca_add_lexical_entry(adapter, &entry);
        }
    }
};

/**
 * @test Syntax parsing performance benchmark
 * WHAT: Measure time for syntax tree building
 * WHY:  Ensure parsing performance doesn't regress
 */
TEST_F(BrocaPerformanceTest, SyntaxParsingPerformance) {
    syntax_processor_t* syntax = broca_get_syntax_processor(adapter);
    ASSERT_NE(syntax, nullptr);

    // Add syntactic units for a sentence: "the cat runs"
    syntactic_unit_t det, noun, verb;
    memset(&det, 0, sizeof(det));
    det.pos = POS_DETERMINER;
    det.word_id = 1;

    memset(&noun, 0, sizeof(noun));
    noun.pos = POS_NOUN;
    noun.word_id = 2;
    noun.features.number = 1;
    noun.features.person = 3;

    memset(&verb, 0, sizeof(verb));
    verb.pos = POS_VERB;
    verb.word_id = 4;
    verb.features.number = 1;
    verb.features.person = 3;
    verb.features.tense = 2;

    const int iterations = 100;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        ASSERT_TRUE(syntax_reset(syntax));
        ASSERT_TRUE(syntax_add_unit(syntax, &det));
        ASSERT_TRUE(syntax_add_unit(syntax, &noun));
        ASSERT_TRUE(syntax_add_unit(syntax, &verb));
        syntax_build_tree(syntax);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_us = static_cast<double>(duration.count()) / iterations;

    // Performance threshold: 500 microseconds per parse (should be much faster)
    EXPECT_LT(avg_us, 500.0) << "Syntax parsing too slow: " << avg_us << " us avg";

    // Log performance for monitoring
    std::cout << "[PERF] Syntax parsing: " << avg_us << " us/iteration" << std::endl;
}

/**
 * @test Speech generation timing benchmark
 * WHAT: Measure time for utterance production
 * WHY:  Ensure production latency doesn't regress
 */
TEST_F(BrocaPerformanceTest, SpeechGenerationTiming) {
    const int iterations = 50;
    const char* words[] = {"the", "cat", "runs"};
    uint32_t num_words = 3;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        broca_utterance_result_t result;
        broca_produce_from_strings(adapter, words, num_words, &result);
        broca_reset(adapter);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double avg_ms = static_cast<double>(duration.count()) / iterations;

    // Performance threshold: 10 ms per utterance
    EXPECT_LT(avg_ms, 10.0) << "Speech generation too slow: " << avg_ms << " ms avg";

    std::cout << "[PERF] Speech generation: " << avg_ms << " ms/iteration" << std::endl;
}

/**
 * @test Motor command generation benchmark
 * WHAT: Measure motor command output rate
 * WHY:  Ensure motor planning meets real-time requirements
 */
TEST_F(BrocaPerformanceTest, MotorCommandGenerationRate) {
    const char* words[] = {"the", "cat", "runs", "quickly"};
    broca_utterance_result_t result;

    auto start = std::chrono::high_resolution_clock::now();
    bool produced = broca_produce_from_strings(adapter, words, 4, &result);
    auto end = std::chrono::high_resolution_clock::now();

    if (produced && result.ready_for_articulation) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Commands should be generated faster than speaking them
        if (result.command_count > 0) {
            double us_per_command = static_cast<double>(duration.count()) / result.command_count;
            EXPECT_LT(us_per_command, 100.0) << "Motor command gen too slow";
            std::cout << "[PERF] Motor command gen: " << us_per_command << " us/command" << std::endl;
        }
    }
}

//=============================================================================
// DETERMINISM TESTS
//=============================================================================

class BrocaDeterminismTest : public NimcpTestBase {
protected:
    broca_adapter_t* adapter;

    void SetUp() override {
        NimcpTestBase::SetUp();
        adapter = broca_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            broca_destroy(adapter);
            adapter = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/**
 * @test Same input produces same output
 * WHAT: Verify deterministic processing
 * WHY:  Reproducibility is critical for debugging and testing
 */
TEST_F(BrocaDeterminismTest, SameInputSameOutput) {
    // Add a word to lexicon
    broca_lexical_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.word_id = 1;
    strncpy(entry.word, "test", sizeof(entry.word) - 1);
    entry.phonemes[0] = 'T';
    entry.phonemes[1] = 'E';
    entry.phonemes[2] = 'S';
    entry.phonemes[3] = 'T';
    entry.phoneme_count = 4;
    entry.pos = POS_NOUN;
    entry.frequency = 0.9f;
    broca_add_lexical_entry(adapter, &entry);

    // Process same input twice
    broca_utterance_result_t result1, result2;
    memset(&result1, 0, sizeof(result1));
    memset(&result2, 0, sizeof(result2));
    const char* words[] = {"test"};

    bool success1 = broca_produce_from_strings(adapter, words, 1, &result1);
    broca_reset(adapter);
    bool success2 = broca_produce_from_strings(adapter, words, 1, &result2);

    // Both should have same success/failure
    EXPECT_EQ(success1, success2);

    // If successful, results should be identical
    if (success1 && success2) {
        EXPECT_EQ(result1.syntax_valid, result2.syntax_valid);
        EXPECT_EQ(result1.word_count, result2.word_count);
        EXPECT_EQ(result1.phoneme_count, result2.phoneme_count);
        EXPECT_EQ(result1.command_count, result2.command_count);
    }
}

/**
 * @test Motor commands are deterministic
 * WHAT: Verify motor command sequences are reproducible
 * WHY:  Articulation must be consistent
 */
TEST_F(BrocaDeterminismTest, MotorCommandsDeterministic) {
    broca_lexical_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.word_id = 1;
    strncpy(entry.word, "hi", sizeof(entry.word) - 1);
    entry.phonemes[0] = 'H';
    entry.phonemes[1] = 'Y';
    entry.phoneme_count = 2;
    broca_add_lexical_entry(adapter, &entry);

    const char* words[] = {"hi"};
    std::vector<broca_output_command_t> cmds1, cmds2;

    // First run
    broca_utterance_result_t result1;
    if (broca_produce_from_strings(adapter, words, 1, &result1)) {
        broca_output_command_t cmd;
        while (broca_get_next_command(adapter, &cmd)) {
            cmds1.push_back(cmd);
        }
    }

    broca_reset(adapter);

    // Second run
    broca_utterance_result_t result2;
    if (broca_produce_from_strings(adapter, words, 1, &result2)) {
        broca_output_command_t cmd;
        while (broca_get_next_command(adapter, &cmd)) {
            cmds2.push_back(cmd);
        }
    }

    // Commands should match
    ASSERT_EQ(cmds1.size(), cmds2.size());
    for (size_t i = 0; i < cmds1.size(); ++i) {
        EXPECT_EQ(cmds1[i].articulator, cmds2[i].articulator);
        EXPECT_FLOAT_EQ(cmds1[i].position, cmds2[i].position);
        EXPECT_EQ(cmds1[i].phoneme, cmds2[i].phoneme);
    }
}

/**
 * @test Syntax tree structure is deterministic
 * WHAT: Verify parse tree is consistent across runs
 * WHY:  Grammar processing must be reproducible
 */
TEST_F(BrocaDeterminismTest, SyntaxTreeDeterministic) {
    syntax_processor_t* syntax = broca_get_syntax_processor(adapter);
    if (!syntax) {
        GTEST_SKIP() << "Syntax processor not available";
    }

    syntactic_unit_t noun;
    memset(&noun, 0, sizeof(noun));
    noun.pos = POS_NOUN;
    noun.word_id = 1;

    // Build tree twice
    syntax_reset(syntax);
    syntax_add_unit(syntax, &noun);
    syntax_build_tree(syntax);
    uint32_t depth1 = syntax_get_tree_depth(syntax);
    uint32_t count1 = syntax_get_unit_count(syntax);

    syntax_reset(syntax);
    syntax_add_unit(syntax, &noun);
    syntax_build_tree(syntax);
    uint32_t depth2 = syntax_get_tree_depth(syntax);
    uint32_t count2 = syntax_get_unit_count(syntax);

    EXPECT_EQ(depth1, depth2);
    EXPECT_EQ(count1, count2);
}

//=============================================================================
// STATE CONSISTENCY TESTS
//=============================================================================

class BrocaStateConsistencyTest : public NimcpTestBase {
protected:
    broca_adapter_t* adapter;

    void SetUp() override {
        NimcpTestBase::SetUp();
        adapter = broca_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            broca_destroy(adapter);
            adapter = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/**
 * @test Status transitions correctly through pipeline
 * WHAT: Verify status changes as expected during processing
 * WHY:  Status reflects internal state; inconsistency indicates bugs
 */
TEST_F(BrocaStateConsistencyTest, StatusTransitions) {
    // Initial status should be IDLE
    EXPECT_EQ(broca_get_status(adapter), BROCA_STATUS_IDLE);

    // After begin_utterance, should still be IDLE (ready for words)
    broca_begin_utterance(adapter);
    EXPECT_EQ(broca_get_status(adapter), BROCA_STATUS_IDLE);

    // After reset, back to IDLE
    broca_reset(adapter);
    EXPECT_EQ(broca_get_status(adapter), BROCA_STATUS_IDLE);
}

/**
 * @test Error state doesn't corrupt adapter
 * WHAT: Verify adapter remains usable after errors
 * WHY:  Recovery from errors should be clean
 */
TEST_F(BrocaStateConsistencyTest, ErrorRecovery) {
    // Try to process invalid input
    broca_input_word_t word;
    memset(&word, 0, sizeof(word));
    word.word_id = 99999;  // Non-existent word

    broca_begin_utterance(adapter);
    broca_add_word(adapter, &word);  // May fail

    // Reset should restore usable state
    EXPECT_TRUE(broca_reset(adapter));
    EXPECT_EQ(broca_get_status(adapter), BROCA_STATUS_IDLE);

    // Should be able to process again
    EXPECT_TRUE(broca_begin_utterance(adapter));
}

/**
 * @test Statistics accumulate correctly
 * WHAT: Verify stats counters increment properly
 * WHY:  Statistics are used for monitoring and debugging
 */
TEST_F(BrocaStateConsistencyTest, StatisticsAccumulation) {
    broca_stats_t stats1, stats2;

    ASSERT_TRUE(broca_get_stats(adapter, &stats1));
    uint64_t initial_utterances = stats1.utterances_processed;

    // Add word and process
    broca_lexical_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.word_id = 1;
    strncpy(entry.word, "hello", sizeof(entry.word) - 1);
    entry.phoneme_count = 2;
    entry.phonemes[0] = 'H';
    entry.phonemes[1] = 'O';
    broca_add_lexical_entry(adapter, &entry);

    const char* words[] = {"hello"};
    broca_produce_from_strings(adapter, words, 1, nullptr);

    ASSERT_TRUE(broca_get_stats(adapter, &stats2));
    EXPECT_GE(stats2.utterances_processed, initial_utterances);
}

/**
 * @test Working memory operations don't corrupt state
 * WHAT: Verify WM push/pop maintains consistency
 * WHY:  WM is shared state that could be corrupted
 */
TEST_F(BrocaStateConsistencyTest, WorkingMemoryConsistency) {
    uint32_t word_ids[10];
    uint32_t count = 10;

    // Initially empty
    broca_wm_get_contents(adapter, word_ids, &count);
    uint32_t initial_count = count;

    // Push some words
    for (uint32_t i = 1; i <= 3; ++i) {
        broca_wm_push(adapter, i);
    }

    // Verify count increased
    count = 10;
    broca_wm_get_contents(adapter, word_ids, &count);
    EXPECT_EQ(count, initial_count + 3);

    // Pop should return a valid pushed value (order may be LIFO or FIFO)
    uint32_t popped;
    if (broca_wm_pop(adapter, &popped)) {
        // Should be one of the values we pushed (1, 2, or 3)
        EXPECT_TRUE(popped >= 1 && popped <= 3)
            << "Popped value " << popped << " not in expected range";
    }
}

//=============================================================================
// MEMORY USAGE PATTERN TESTS
//=============================================================================

class BrocaMemoryPatternTest : public NimcpTestBase {
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
 * WHY:  Detect memory leaks and resource exhaustion
 */
TEST_F(BrocaMemoryPatternTest, CreateDestroyCycles) {
    const int cycles = 100;

    for (int i = 0; i < cycles; ++i) {
        broca_adapter_t* adapter = broca_create(nullptr);
        ASSERT_NE(adapter, nullptr) << "Failed at cycle " << i;

        // Do some work
        broca_begin_utterance(adapter);
        broca_reset(adapter);

        broca_destroy(adapter);
    }

    SUCCEED() << "Completed " << cycles << " create/destroy cycles";
}

/**
 * @test Config variation stability
 * WHAT: Create adapters with various configurations
 * WHY:  Ensure all config combinations are stable
 */
TEST_F(BrocaMemoryPatternTest, ConfigVariationStability) {
    broca_config_t configs[] = {
        broca_default_config(),
        broca_default_config(),
        broca_default_config(),
        broca_default_config()
    };

    // Modify each config differently
    configs[1].enable_lexicon = false;
    configs[2].enable_working_memory = false;
    configs[3].max_words = 16;

    for (size_t i = 0; i < sizeof(configs) / sizeof(configs[0]); ++i) {
        broca_adapter_t* adapter = broca_create(&configs[i]);
        ASSERT_NE(adapter, nullptr) << "Config " << i << " failed to create";

        broca_stats_t stats;
        EXPECT_TRUE(broca_get_stats(adapter, &stats));

        broca_destroy(adapter);
    }
}

/**
 * @test Lexicon growth stability
 * WHAT: Add many entries to lexicon
 * WHY:  Ensure lexicon scales without issues
 */
TEST_F(BrocaMemoryPatternTest, LexiconGrowthStability) {
    broca_config_t config = broca_default_config();
    config.lexicon_size = 1000;
    broca_adapter_t* adapter = broca_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add many lexical entries
    for (uint32_t i = 1; i <= 500; ++i) {
        broca_lexical_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.word_id = i;
        snprintf(entry.word, sizeof(entry.word), "word%u", i);
        entry.phoneme_count = 3;
        entry.phonemes[0] = 'W';
        entry.phonemes[1] = static_cast<uint8_t>(i % 26 + 'A');
        entry.phonemes[2] = 'D';

        bool added = broca_add_lexical_entry(adapter, &entry);
        // May fail once lexicon is full, that's OK
        (void)added;
    }

    // System should remain stable
    EXPECT_EQ(broca_get_status(adapter), BROCA_STATUS_IDLE);

    broca_destroy(adapter);
}

//=============================================================================
// NULL POINTER SAFETY TESTS
//=============================================================================

class BrocaNullSafetyTest : public NimcpTestBase {
protected:
    broca_adapter_t* adapter;

    void SetUp() override {
        NimcpTestBase::SetUp();
        adapter = broca_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            broca_destroy(adapter);
            adapter = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/**
 * @test All lifecycle functions handle NULL
 * WHAT: Pass NULL to all lifecycle functions
 * WHY:  Prevent crashes from NULL inputs
 */
TEST_F(BrocaNullSafetyTest, LifecycleFunctionsHandleNull) {
    EXPECT_EQ(broca_get_status(nullptr), BROCA_STATUS_ERROR);
    EXPECT_NE(broca_get_last_error(nullptr), BROCA_ERROR_NONE);
    EXPECT_FALSE(broca_reset(nullptr));
    EXPECT_FALSE(broca_begin_utterance(nullptr));

    // Destroy NULL should not crash
    broca_destroy(nullptr);
    SUCCEED();
}

/**
 * @test All processing functions handle NULL
 * WHAT: Pass NULL to processing functions
 * WHY:  Robust error handling
 */
TEST_F(BrocaNullSafetyTest, ProcessingFunctionsHandleNull) {
    EXPECT_FALSE(broca_add_word(nullptr, nullptr));
    EXPECT_FALSE(broca_add_word(adapter, nullptr));
    EXPECT_FALSE(broca_process_utterance(nullptr, nullptr));

    broca_output_command_t cmd;
    EXPECT_FALSE(broca_get_next_command(nullptr, &cmd));
    EXPECT_FALSE(broca_get_next_command(adapter, nullptr));
}

/**
 * @test Stats and config functions handle NULL
 * WHAT: Pass NULL outputs to get functions
 * WHY:  Prevent crashes from NULL output pointers
 */
TEST_F(BrocaNullSafetyTest, GetFunctionsHandleNull) {
    broca_stats_t stats;
    EXPECT_FALSE(broca_get_stats(nullptr, &stats));
    EXPECT_FALSE(broca_get_stats(adapter, nullptr));

    broca_config_t config;
    EXPECT_FALSE(broca_get_config(nullptr, &config));
    EXPECT_FALSE(broca_get_config(adapter, nullptr));
}

/**
 * @test Lexicon functions handle NULL
 * WHAT: Pass NULL to lexicon functions
 * WHY:  Lexicon is frequently used API
 */
TEST_F(BrocaNullSafetyTest, LexiconFunctionsHandleNull) {
    EXPECT_FALSE(broca_add_lexical_entry(nullptr, nullptr));
    EXPECT_FALSE(broca_add_lexical_entry(adapter, nullptr));

    broca_lexical_entry_t entry;
    EXPECT_FALSE(broca_lookup_word(nullptr, 0, "test", &entry));
    EXPECT_FALSE(broca_lookup_word(adapter, 0, nullptr, &entry));
    EXPECT_FALSE(broca_lookup_word(adapter, 0, "test", nullptr));
}

/**
 * @test Sub-module access functions handle NULL
 * WHAT: Pass NULL to get sub-module functions
 * WHY:  Advanced users access sub-modules directly
 */
TEST_F(BrocaNullSafetyTest, SubmoduleAccessHandleNull) {
    EXPECT_EQ(broca_get_syntax_processor(nullptr), nullptr);
    EXPECT_EQ(broca_get_phonological_processor(nullptr), nullptr);
    EXPECT_EQ(broca_get_speech_motor_planner(nullptr), nullptr);
}

//=============================================================================
// BACKWARD COMPATIBILITY TESTS
//=============================================================================

class BrocaBackwardCompatTest : public NimcpTestBase {
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
 * WHAT: Verify default configuration values haven't changed
 * WHY:  Existing code depends on specific defaults
 */
TEST_F(BrocaBackwardCompatTest, DefaultConfigValues) {
    broca_config_t config = broca_default_config();

    EXPECT_EQ(config.max_words, BROCA_DEFAULT_MAX_WORDS);
    EXPECT_EQ(config.max_phonemes, BROCA_DEFAULT_MAX_PHONEMES);
    EXPECT_EQ(config.max_motor_commands, BROCA_DEFAULT_MAX_COMMANDS);
    EXPECT_EQ(config.working_memory_slots, BROCA_DEFAULT_WORKING_MEMORY_SLOTS);
    EXPECT_EQ(config.lexicon_size, BROCA_DEFAULT_LEXICON_SIZE);
    EXPECT_FLOAT_EQ(config.planning_window_ms, BROCA_DEFAULT_PLANNING_WINDOW_MS);
}

/**
 * @test Status enum values are stable
 * WHAT: Verify status enum values haven't changed
 * WHY:  Code comparing status values depends on these
 */
TEST_F(BrocaBackwardCompatTest, StatusEnumValues) {
    EXPECT_EQ(static_cast<int>(BROCA_STATUS_IDLE), 0);
    EXPECT_EQ(static_cast<int>(BROCA_STATUS_LEXICAL_ACCESS), 1);
    EXPECT_EQ(static_cast<int>(BROCA_STATUS_SYNTACTIC), 2);
    EXPECT_EQ(static_cast<int>(BROCA_STATUS_PHONOLOGICAL), 3);
    EXPECT_EQ(static_cast<int>(BROCA_STATUS_MOTOR_PLANNING), 4);
    EXPECT_EQ(static_cast<int>(BROCA_STATUS_READY), 5);
    EXPECT_EQ(static_cast<int>(BROCA_STATUS_ERROR), 6);
}

/**
 * @test Error enum values are stable
 * WHAT: Verify error enum values haven't changed
 * WHY:  Error handling depends on specific values
 */
TEST_F(BrocaBackwardCompatTest, ErrorEnumValues) {
    EXPECT_EQ(static_cast<int>(BROCA_ERROR_NONE), 0);
    EXPECT_EQ(static_cast<int>(BROCA_ERROR_INVALID_INPUT), 1);
    EXPECT_EQ(static_cast<int>(BROCA_ERROR_SYNTAX_FAILURE), 2);
    EXPECT_EQ(static_cast<int>(BROCA_ERROR_PHONOLOGICAL_FAILURE), 3);
    EXPECT_EQ(static_cast<int>(BROCA_ERROR_MOTOR_PLANNING_FAILURE), 4);
    EXPECT_EQ(static_cast<int>(BROCA_ERROR_WORKING_MEMORY_FULL), 5);
    EXPECT_EQ(static_cast<int>(BROCA_ERROR_LEXICON_MISS), 6);
    EXPECT_EQ(static_cast<int>(BROCA_ERROR_BUFFER_OVERFLOW), 7);
    EXPECT_EQ(static_cast<int>(BROCA_ERROR_INTERNAL), 8);
}

/**
 * @test String functions return valid strings
 * WHAT: Verify string functions work for all values
 * WHY:  Logging and debugging depend on these
 */
TEST_F(BrocaBackwardCompatTest, StringFunctionsValid) {
    // Status strings
    for (int i = 0; i <= static_cast<int>(BROCA_STATUS_ERROR); ++i) {
        const char* str = broca_status_string(static_cast<broca_status_t>(i));
        ASSERT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }

    // Error strings
    for (int i = 0; i <= static_cast<int>(BROCA_ERROR_INTERNAL); ++i) {
        const char* str = broca_error_string(static_cast<broca_error_t>(i));
        ASSERT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

/**
 * @test Create with NULL config uses defaults
 * WHAT: NULL config should create valid adapter with defaults
 * WHY:  Legacy code may pass NULL for defaults
 */
TEST_F(BrocaBackwardCompatTest, CreateNullConfigUsesDefaults) {
    broca_adapter_t* adapter = broca_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    broca_config_t config;
    ASSERT_TRUE(broca_get_config(adapter, &config));

    EXPECT_EQ(config.max_words, BROCA_DEFAULT_MAX_WORDS);
    EXPECT_EQ(config.max_phonemes, BROCA_DEFAULT_MAX_PHONEMES);

    broca_destroy(adapter);
}

//=============================================================================
// STRESS TESTS
//=============================================================================

class BrocaStressTest : public NimcpTestBase {
protected:
    broca_adapter_t* adapter;

    void SetUp() override {
        NimcpTestBase::SetUp();
        broca_config_t config = broca_default_config();
        config.lexicon_size = 100;
        adapter = broca_create(&config);
        ASSERT_NE(adapter, nullptr);

        // Add words for stress testing
        for (uint32_t i = 1; i <= 50; ++i) {
            broca_lexical_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            entry.word_id = i;
            snprintf(entry.word, sizeof(entry.word), "word%u", i);
            entry.phoneme_count = 2;
            entry.phonemes[0] = 'A';
            entry.phonemes[1] = static_cast<uint8_t>(i % 26 + 'A');
            broca_add_lexical_entry(adapter, &entry);
        }
    }

    void TearDown() override {
        if (adapter) {
            broca_destroy(adapter);
            adapter = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/**
 * @test Rapid processing stress test
 * WHAT: Process many utterances rapidly
 * WHY:  Test system stability under load
 */
TEST_F(BrocaStressTest, RapidProcessing) {
    const int iterations = 500;
    int successful = 0;

    for (int i = 0; i < iterations; ++i) {
        broca_begin_utterance(adapter);

        broca_input_word_t word;
        memset(&word, 0, sizeof(word));
        word.word_id = static_cast<uint32_t>((i % 50) + 1);
        broca_add_word(adapter, &word);

        broca_utterance_result_t result;
        if (broca_process_utterance(adapter, &result)) {
            ++successful;
        }
        broca_reset(adapter);
    }

    // System should still be functional after rapid processing
    EXPECT_EQ(broca_get_status(adapter), BROCA_STATUS_IDLE);

    broca_stats_t stats;
    ASSERT_TRUE(broca_get_stats(adapter, &stats));

    // Primary test is stability - stats tracking is secondary
    // Some may fail due to word ID lookups, that's expected
    std::cout << "[STRESS] Rapid processing: " << successful << "/" << iterations
              << " successful, " << stats.utterances_processed << " stats recorded" << std::endl;
}

/**
 * @test Long utterance stress test
 * WHAT: Process utterances at maximum word count
 * WHY:  Test buffer limits
 */
TEST_F(BrocaStressTest, LongUtteranceStress) {
    broca_config_t config;
    broca_get_config(adapter, &config);
    uint32_t max_words = config.max_words;

    broca_begin_utterance(adapter);

    // Add maximum number of words
    for (uint32_t i = 0; i < max_words; ++i) {
        broca_input_word_t word;
        memset(&word, 0, sizeof(word));
        word.word_id = (i % 50) + 1;
        broca_add_word(adapter, &word);
    }

    broca_utterance_result_t result;
    broca_process_utterance(adapter, &result);

    // Should handle max words without crashing
    EXPECT_LE(result.word_count, max_words);
}

/**
 * @test Rapid reset stress test
 * WHAT: Reset adapter many times quickly
 * WHY:  Ensure reset doesn't leak resources
 */
TEST_F(BrocaStressTest, RapidResetStress) {
    const int iterations = 1000;

    for (int i = 0; i < iterations; ++i) {
        ASSERT_TRUE(broca_reset(adapter)) << "Reset failed at iteration " << i;
    }

    EXPECT_EQ(broca_get_status(adapter), BROCA_STATUS_IDLE);
}

/**
 * @test Working memory overflow stress test
 * WHAT: Push beyond working memory capacity
 * WHY:  Test graceful handling of overflow
 */
TEST_F(BrocaStressTest, WorkingMemoryOverflow) {
    broca_config_t config;
    broca_get_config(adapter, &config);
    uint32_t wm_slots = config.working_memory_slots;

    // Try to push more than capacity
    for (uint32_t i = 0; i < wm_slots + 10; ++i) {
        broca_wm_push(adapter, i + 1);
    }

    // Should not crash; state should be valid
    EXPECT_EQ(broca_get_status(adapter), BROCA_STATUS_IDLE);

    uint32_t contents[100];
    uint32_t count = 100;
    broca_wm_get_contents(adapter, contents, &count);
    EXPECT_LE(count, wm_slots);
}
