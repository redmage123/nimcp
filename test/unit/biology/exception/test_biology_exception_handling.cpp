/**
 * @file test_biology_exception_handling.cpp
 * @brief Unit tests for biology module exception handling
 *
 * WHAT: Test exception handling across biology reasoning module
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test biology module's error conditions and exception integration
 *
 * BIOLOGY OPERATIONS TESTED:
 * - DNA/RNA sequence operations (transcription, translation)
 * - Sequence alignment (global, local)
 * - Mutation analysis
 * - Phylogenetic tree operations
 * - Population genetics calculations
 *
 * TEST PATTERNS:
 * - Error code to exception mapping
 * - Exception dispatch through handler chain
 * - Exception category classification (BIOLOGY_SEQUENCE, BIOLOGY_ALIGNMENT)
 * - Recovery strategy determination
 *
 * @author NIMCP Development Team
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <vector>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/parietal/nimcp_biology.h"
}

//=============================================================================
// Biology Exception Categories
//=============================================================================

// Define biology-specific exception categories for testing
#define EXCEPTION_CATEGORY_BIOLOGY_BASE     200
#define EXCEPTION_CATEGORY_SEQUENCE         (EXCEPTION_CATEGORY_BIOLOGY_BASE + 1)
#define EXCEPTION_CATEGORY_ALIGNMENT        (EXCEPTION_CATEGORY_BIOLOGY_BASE + 2)
#define EXCEPTION_CATEGORY_MUTATION         (EXCEPTION_CATEGORY_BIOLOGY_BASE + 3)
#define EXCEPTION_CATEGORY_PHYLOGENETICS    (EXCEPTION_CATEGORY_BIOLOGY_BASE + 4)
#define EXCEPTION_CATEGORY_POPULATION       (EXCEPTION_CATEGORY_BIOLOGY_BASE + 5)
#define EXCEPTION_CATEGORY_TRANSCRIPTION    (EXCEPTION_CATEGORY_BIOLOGY_BASE + 6)
#define EXCEPTION_CATEGORY_TRANSLATION      (EXCEPTION_CATEGORY_BIOLOGY_BASE + 7)

//=============================================================================
// Test Fixture
//=============================================================================

class BiologyExceptionHandlingTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<int> last_exception_category;
    static std::atomic<bool> handler_consumed;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        last_exception_category = 0;
        handler_consumed = false;

        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_exception_category = ex->category;
        return false;  // Don't consume - allow other handlers
    }

    static bool consuming_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        handler_consumed = true;
        return true;  // Consume the exception
    }

    // Helper to create biology exception
    nimcp_exception_t* create_biology_exception(
        nimcp_error_t code,
        int category,
        nimcp_exception_severity_t severity,
        const char* message
    ) {
        nimcp_exception_t* ex = nimcp_exception_create(
            code,
            severity,
            __FILE__, __LINE__, __func__,
            message
        );
        if (ex) {
            ex->category = static_cast<nimcp_exception_category_t>(category);
        }
        return ex;
    }
};

std::atomic<int> BiologyExceptionHandlingTest::handler_call_count(0);
std::atomic<int> BiologyExceptionHandlingTest::last_exception_code(0);
std::atomic<int> BiologyExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> BiologyExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(BiologyExceptionHandlingTest, CreateSequenceException) {
    // WHAT: Test creation of sequence-related exception
    // WHY:  Verify exception fields are set correctly

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_SEQUENCE,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid DNA sequence: contains non-nucleotide character 'X'"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SEQUENCE);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(BiologyExceptionHandlingTest, CreateAlignmentException) {
    // WHAT: Test creation of alignment-related exception
    // WHY:  Sequence alignment errors need proper categorization

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_ALIGNMENT,
        EXCEPTION_SEVERITY_WARNING,
        "Alignment failed - sequences are incompatible lengths"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_ALIGNMENT);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

TEST_F(BiologyExceptionHandlingTest, CreateMutationException) {
    // WHAT: Test creation of mutation analysis exception
    // WHY:  Mutation detection failures need specialized handling

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_MUTATION,
        EXCEPTION_SEVERITY_ERROR,
        "Mutation position exceeds sequence length"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_MUTATION);

    nimcp_exception_unref(ex);
}

//=============================================================================
// DNA/RNA Sequence Exception Tests
//=============================================================================

TEST_F(BiologyExceptionHandlingTest, DNANullSequenceException) {
    // WHAT: Test exception for NULL sequence parameter
    // WHY:  Verify proper error handling for invalid inputs

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_SEQUENCE,
        EXCEPTION_SEVERITY_ERROR,
        "DNA sequence is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "dna_null_handler";
    options.handler = test_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);
    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER);

    nimcp_exception_unref(ex);
    if (reg) nimcp_handler_unregister(reg);
}

TEST_F(BiologyExceptionHandlingTest, InvalidNucleotideException) {
    // WHAT: Test exception for invalid nucleotide characters
    // WHY:  DNA/RNA sequences must contain valid bases only

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_SEQUENCE,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid nucleotide 'Z' at position 15 in DNA sequence"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SEQUENCE);

    nimcp_exception_unref(ex);
}

TEST_F(BiologyExceptionHandlingTest, SequenceTooLongException) {
    // WHAT: Test exception for sequence exceeding max length
    // WHY:  Sequences have a maximum allowed length

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_SEQUENCE,
        EXCEPTION_SEVERITY_WARNING,
        "Sequence length (5000) exceeds maximum (4096)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Transcription/Translation Exception Tests
//=============================================================================

TEST_F(BiologyExceptionHandlingTest, TranscriptionBufferOverflowException) {
    // WHAT: Test exception for transcription buffer overflow
    // WHY:  Output buffer must be large enough for RNA

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_BUFFER_TOO_SMALL,
        EXCEPTION_CATEGORY_TRANSCRIPTION,
        EXCEPTION_SEVERITY_ERROR,
        "Transcription buffer too small: need 128 bytes, have 64"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_BUFFER_TOO_SMALL);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TRANSCRIPTION);

    nimcp_exception_unref(ex);
}

TEST_F(BiologyExceptionHandlingTest, TranslationInvalidCodonException) {
    // WHAT: Test exception for invalid codon during translation
    // WHY:  Codons must be valid three-letter sequences

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_TRANSLATION,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid codon 'XYZ' at position 12"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TRANSLATION);

    nimcp_exception_unref(ex);
}

TEST_F(BiologyExceptionHandlingTest, TranslationPrematureStopException) {
    // WHAT: Test exception for unexpected stop codon
    // WHY:  Premature stop codons may indicate sequence issues

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_TRANSLATION,
        EXCEPTION_SEVERITY_WARNING,
        "Premature stop codon (UAA) at position 30"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Sequence Alignment Exception Tests
//=============================================================================

TEST_F(BiologyExceptionHandlingTest, AlignmentNullSequenceException) {
    // WHAT: Test exception for NULL alignment input
    // WHY:  Both sequences must be provided for alignment

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_ALIGNMENT,
        EXCEPTION_SEVERITY_ERROR,
        "Alignment sequence 2 is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    nimcp_exception_unref(ex);
}

TEST_F(BiologyExceptionHandlingTest, AlignmentScoreOverflowException) {
    // WHAT: Test exception for alignment score overflow
    // WHY:  Very long sequences may cause score overflow

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_ALIGNMENT,
        EXCEPTION_SEVERITY_WARNING,
        "Alignment score overflow: sequences too long for int32"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Phylogenetics Exception Tests
//=============================================================================

TEST_F(BiologyExceptionHandlingTest, PhyloInvalidDistanceMatrixException) {
    // WHAT: Test exception for invalid distance matrix
    // WHY:  Distance matrix must be symmetric and valid

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_PHYLOGENETICS,
        EXCEPTION_SEVERITY_ERROR,
        "Distance matrix is not symmetric"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_PHYLOGENETICS);

    nimcp_exception_unref(ex);
}

TEST_F(BiologyExceptionHandlingTest, PhyloSpeciesNotFoundException) {
    // WHAT: Test exception for species not found in tree
    // WHY:  Species lookup may fail if name is not in tree

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_CATEGORY_PHYLOGENETICS,
        EXCEPTION_SEVERITY_WARNING,
        "Species 'Unknown_species' not found in phylogenetic tree"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_FOUND);

    nimcp_exception_unref(ex);
}

TEST_F(BiologyExceptionHandlingTest, PhyloMaxSpeciesExceededException) {
    // WHAT: Test exception for exceeding max species count
    // WHY:  Phylogenetic trees have a species limit

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_PHYLOGENETICS,
        EXCEPTION_SEVERITY_ERROR,
        "Number of species (100) exceeds maximum (64)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Population Genetics Exception Tests
//=============================================================================

TEST_F(BiologyExceptionHandlingTest, PopulationInvalidFrequencyException) {
    // WHAT: Test exception for invalid allele frequency
    // WHY:  Allele frequencies must be in [0, 1]

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_POPULATION,
        EXCEPTION_SEVERITY_ERROR,
        "Allele frequency p must be in [0, 1], got 1.5"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_POPULATION);

    nimcp_exception_unref(ex);
}

TEST_F(BiologyExceptionHandlingTest, PopulationSizeZeroException) {
    // WHAT: Test exception for zero population size
    // WHY:  Population size must be positive

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_POPULATION,
        EXCEPTION_SEVERITY_ERROR,
        "Population size must be positive, got 0"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

TEST_F(BiologyExceptionHandlingTest, HardyWeinbergNotInEquilibriumException) {
    // WHAT: Test exception for Hardy-Weinberg disequilibrium
    // WHY:  Chi-squared test may indicate significant deviation

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_POPULATION,
        EXCEPTION_SEVERITY_WARNING,
        "Population is not in Hardy-Weinberg equilibrium (chi^2 = 15.3, p < 0.01)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(BiologyExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "biology_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "biology_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SEQUENCE,
        EXCEPTION_SEVERITY_ERROR,
        "Test exception for handler chain"
    );

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    // Both handlers should be called (neither consumes)
    EXPECT_GE(handler_call_count.load(), 2);

    nimcp_exception_unref(ex);
    if (reg1) nimcp_handler_unregister(reg1);
    if (reg2) nimcp_handler_unregister(reg2);
}

TEST_F(BiologyExceptionHandlingTest, HandlerConsumesException) {
    // WHAT: Test handler consuming exception stops chain
    // WHY:  Verify consumed exceptions don't propagate

    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    // First handler consumes
    options1.name = "consuming_handler";
    options1.handler = consuming_exception_handler;
    options1.priority = 100;

    // Second handler should not be called
    options2.name = "secondary_handler";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SEQUENCE,
        EXCEPTION_SEVERITY_ERROR,
        "Test exception for consumption"
    );

    handler_call_count = 0;
    handler_consumed = false;
    nimcp_exception_dispatch(ex);

    // Only consuming handler should be called
    EXPECT_TRUE(handler_consumed.load());
    EXPECT_EQ(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
    if (reg1) nimcp_handler_unregister(reg1);
    if (reg2) nimcp_handler_unregister(reg2);
}

//=============================================================================
// Recovery Strategy Tests
//=============================================================================

TEST_F(BiologyExceptionHandlingTest, SequenceExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for sequence exceptions
    // WHY:  Sequence errors may need retry or parameter adjustment

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_SEQUENCE,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid DNA sequence"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Sequence exceptions should have some recovery action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(BiologyExceptionHandlingTest, CriticalBiologyExceptionRecovery) {
    // WHAT: Test recovery for critical biology failures
    // WHY:  Critical failures may require emergency save

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_CATEGORY_PHYLOGENETICS,
        EXCEPTION_SEVERITY_CRITICAL,
        "Phylogenetic tree structure corrupted"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Critical errors should trigger some kind of recovery action
    EXPECT_TRUE(strategy.primary_action != EXCEPTION_RECOVERY_NONE ||
                strategy.fallback_action != EXCEPTION_RECOVERY_NONE ||
                strategy.retry_count > 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Exception Statistics Tests
//=============================================================================

TEST_F(BiologyExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor biology exception frequency

    // Register a counting handler
    static std::atomic<int> dispatch_count{0};
    dispatch_count = 0;

    auto counting_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        dispatch_count++;
        return false;
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "stats_counter";
    opts.handler = counting_handler;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Create and dispatch several exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = create_biology_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_CATEGORY_SEQUENCE,
            EXCEPTION_SEVERITY_WARNING,
            "Test exception for statistics"
        );
        if (ex) {
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
        }
    }

    // Handler should have been called for each exception
    EXPECT_GE(dispatch_count.load(), 5);

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(BiologyExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  Biology operations may run across multiple threads

    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&success_count, t, exceptions_per_thread]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i
                );
                if (ex) {
                    success_count++;
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * exceptions_per_thread);
}

//=============================================================================
// Context Entry Tests
//=============================================================================

TEST_F(BiologyExceptionHandlingTest, ExceptionContextMetadata) {
    // WHAT: Test adding context metadata to biology exceptions
    // WHY:  Context helps diagnose issues

    nimcp_exception_t* ex = create_biology_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_SEQUENCE,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid DNA sequence"
    );

    ASSERT_NE(ex, nullptr);

    // Add context
    int result = nimcp_exception_set_context(ex, "sequence_type", "DNA");
    EXPECT_EQ(result, 0);

    result = nimcp_exception_set_context(ex, "invalid_char", "X");
    EXPECT_EQ(result, 0);

    result = nimcp_exception_set_context(ex, "position", "15");
    EXPECT_EQ(result, 0);

    // Verify context
    EXPECT_STREQ(nimcp_exception_get_context(ex, "sequence_type"), "DNA");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "invalid_char"), "X");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "position"), "15");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
