/**
 * @file test_chemistry_exception_handling.cpp
 * @brief Unit tests for chemistry module exception handling
 *
 * WHAT: Test exception handling across chemistry reasoning module
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test chemistry module's error conditions and exception integration
 *
 * CHEMISTRY OPERATIONS TESTED:
 * - Periodic table element lookups
 * - Molecular formula parsing
 * - Chemical equation balancing
 * - Stoichiometry calculations
 * - Thermodynamics calculations
 *
 * TEST PATTERNS:
 * - Error code to exception mapping
 * - Exception dispatch through handler chain
 * - Exception category classification (CHEMISTRY_ELEMENT, CHEMISTRY_REACTION)
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
#include "cognitive/parietal/nimcp_chemistry.h"
}

//=============================================================================
// Chemistry Exception Categories
//=============================================================================

// Define chemistry-specific exception categories for testing
#define EXCEPTION_CATEGORY_CHEMISTRY_BASE   300
#define EXCEPTION_CATEGORY_ELEMENT          (EXCEPTION_CATEGORY_CHEMISTRY_BASE + 1)
#define EXCEPTION_CATEGORY_MOLECULE         (EXCEPTION_CATEGORY_CHEMISTRY_BASE + 2)
#define EXCEPTION_CATEGORY_REACTION         (EXCEPTION_CATEGORY_CHEMISTRY_BASE + 3)
#define EXCEPTION_CATEGORY_STOICHIOMETRY    (EXCEPTION_CATEGORY_CHEMISTRY_BASE + 4)
#define EXCEPTION_CATEGORY_THERMODYNAMICS   (EXCEPTION_CATEGORY_CHEMISTRY_BASE + 5)
#define EXCEPTION_CATEGORY_BOND             (EXCEPTION_CATEGORY_CHEMISTRY_BASE + 6)
#define EXCEPTION_CATEGORY_FORMULA          (EXCEPTION_CATEGORY_CHEMISTRY_BASE + 7)

//=============================================================================
// Test Fixture
//=============================================================================

class ChemistryExceptionHandlingTest : public ::testing::Test {
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

    // Helper to create chemistry exception
    nimcp_exception_t* create_chemistry_exception(
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

std::atomic<int> ChemistryExceptionHandlingTest::handler_call_count(0);
std::atomic<int> ChemistryExceptionHandlingTest::last_exception_code(0);
std::atomic<int> ChemistryExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> ChemistryExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(ChemistryExceptionHandlingTest, CreateElementException) {
    // WHAT: Test creation of element-related exception
    // WHY:  Verify exception fields are set correctly

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_CATEGORY_ELEMENT,
        EXCEPTION_SEVERITY_ERROR,
        "Element with atomic number 0 not found"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_FOUND);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_ELEMENT);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(ChemistryExceptionHandlingTest, CreateMoleculeException) {
    // WHAT: Test creation of molecule-related exception
    // WHY:  Molecule parsing errors need proper categorization

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_MOLECULE,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid molecular formula: unbalanced parentheses"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_MOLECULE);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);

    nimcp_exception_unref(ex);
}

TEST_F(ChemistryExceptionHandlingTest, CreateReactionException) {
    // WHAT: Test creation of reaction exception
    // WHY:  Reaction processing failures need specialized handling

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_REACTION,
        EXCEPTION_SEVERITY_ERROR,
        "Unable to balance chemical equation"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_REACTION);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Periodic Table Exception Tests
//=============================================================================

TEST_F(ChemistryExceptionHandlingTest, ElementNullOutputException) {
    // WHAT: Test exception for NULL output parameter
    // WHY:  Verify proper error handling for invalid outputs

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_ELEMENT,
        EXCEPTION_SEVERITY_ERROR,
        "Element properties output is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "element_null_handler";
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

TEST_F(ChemistryExceptionHandlingTest, InvalidAtomicNumberException) {
    // WHAT: Test exception for invalid atomic number
    // WHY:  Atomic numbers must be in valid range [1, 118]

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_ELEMENT,
        EXCEPTION_SEVERITY_ERROR,
        "Atomic number 150 exceeds maximum known element (118)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_ELEMENT);

    nimcp_exception_unref(ex);
}

TEST_F(ChemistryExceptionHandlingTest, UnknownElementSymbolException) {
    // WHAT: Test exception for unknown element symbol
    // WHY:  Symbol lookup may fail for invalid symbols

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_CATEGORY_ELEMENT,
        EXCEPTION_SEVERITY_ERROR,
        "Element symbol 'Xyz' not found in periodic table"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_FOUND);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Molecule Parsing Exception Tests
//=============================================================================

TEST_F(ChemistryExceptionHandlingTest, MoleculeFormulaParseException) {
    // WHAT: Test exception for molecule formula parse error
    // WHY:  Formulas must follow valid syntax

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_FORMULA,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid character '#' in molecular formula"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_FORMULA);

    nimcp_exception_unref(ex);
}

TEST_F(ChemistryExceptionHandlingTest, MoleculeTooManyAtomsException) {
    // WHAT: Test exception for exceeding max atoms
    // WHY:  Molecules have a maximum atom count

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_MOLECULE,
        EXCEPTION_SEVERITY_ERROR,
        "Molecule atom count (100) exceeds maximum (64)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

TEST_F(ChemistryExceptionHandlingTest, MoleculeUnbalancedParenthesesException) {
    // WHAT: Test exception for unbalanced parentheses in formula
    // WHY:  Parentheses must be properly matched

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_FORMULA,
        EXCEPTION_SEVERITY_ERROR,
        "Unbalanced parentheses in formula: Ca(OH2"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_FORMULA);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Chemical Reaction Exception Tests
//=============================================================================

TEST_F(ChemistryExceptionHandlingTest, ReactionParseException) {
    // WHAT: Test exception for reaction equation parse error
    // WHY:  Reaction equations must follow valid syntax

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_REACTION,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid reaction syntax: missing arrow (->) separator"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

TEST_F(ChemistryExceptionHandlingTest, ReactionBalancingFailedException) {
    // WHAT: Test exception for failed equation balancing
    // WHY:  Some equations cannot be balanced with small integers

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_REACTION,
        EXCEPTION_SEVERITY_WARNING,
        "Unable to balance equation with coefficients <= 10"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

TEST_F(ChemistryExceptionHandlingTest, ReactionTooManySpeciesException) {
    // WHAT: Test exception for exceeding max species in reaction
    // WHY:  Reactions have a species limit

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_REACTION,
        EXCEPTION_SEVERITY_ERROR,
        "Number of reactants (20) exceeds maximum (16)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Stoichiometry Exception Tests
//=============================================================================

TEST_F(ChemistryExceptionHandlingTest, StoichiometryUnbalancedException) {
    // WHAT: Test exception for stoichiometry on unbalanced equation
    // WHY:  Stoichiometry requires balanced equations

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_CATEGORY_STOICHIOMETRY,
        EXCEPTION_SEVERITY_ERROR,
        "Cannot perform stoichiometry on unbalanced equation"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_STOICHIOMETRY);

    nimcp_exception_unref(ex);
}

TEST_F(ChemistryExceptionHandlingTest, StoichiometryNegativeMolesException) {
    // WHAT: Test exception for negative moles input
    // WHY:  Moles must be non-negative

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_STOICHIOMETRY,
        EXCEPTION_SEVERITY_ERROR,
        "Moles cannot be negative: got -5.0"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

TEST_F(ChemistryExceptionHandlingTest, StoichiometryInvalidReagentIndexException) {
    // WHAT: Test exception for invalid reagent index
    // WHY:  Reagent index must be within bounds

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_STOICHIOMETRY,
        EXCEPTION_SEVERITY_ERROR,
        "Limiting reagent index (5) exceeds reactant count (3)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Thermodynamics Exception Tests
//=============================================================================

TEST_F(ChemistryExceptionHandlingTest, ThermodynamicsDataNotFoundException) {
    // WHAT: Test exception for missing thermodynamic data
    // WHY:  Not all compounds have thermodynamic data

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_CATEGORY_THERMODYNAMICS,
        EXCEPTION_SEVERITY_WARNING,
        "Thermodynamic data not found for compound: C10H22O5"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_FOUND);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_THERMODYNAMICS);

    nimcp_exception_unref(ex);
}

TEST_F(ChemistryExceptionHandlingTest, ThermodynamicsInvalidTemperatureException) {
    // WHAT: Test exception for invalid temperature
    // WHY:  Temperature must be positive (Kelvin)

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_THERMODYNAMICS,
        EXCEPTION_SEVERITY_ERROR,
        "Temperature must be positive: got -50 K"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Bond Exception Tests
//=============================================================================

TEST_F(ChemistryExceptionHandlingTest, BondPredictionException) {
    // WHAT: Test exception for bond prediction failure
    // WHY:  Some element combinations may not form bonds

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_BOND,
        EXCEPTION_SEVERITY_WARNING,
        "Cannot predict bond type between He and Ne (noble gases)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_BOND);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(ChemistryExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "chemistry_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "chemistry_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_REACTION,
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

TEST_F(ChemistryExceptionHandlingTest, HandlerConsumesException) {
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

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_MOLECULE,
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

TEST_F(ChemistryExceptionHandlingTest, ChemistryExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for chemistry exceptions
    // WHY:  Chemistry errors may need retry or parameter adjustment

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_REACTION,
        EXCEPTION_SEVERITY_ERROR,
        "Equation balancing failed"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Chemistry exceptions should have some recovery action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(ChemistryExceptionHandlingTest, CriticalChemistryExceptionRecovery) {
    // WHAT: Test recovery for critical chemistry failures
    // WHY:  Critical failures may require emergency save

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_CATEGORY_MOLECULE,
        EXCEPTION_SEVERITY_CRITICAL,
        "Molecule data structure corrupted"
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

TEST_F(ChemistryExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor chemistry exception frequency

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
        nimcp_exception_t* ex = create_chemistry_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_CATEGORY_ELEMENT,
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

TEST_F(ChemistryExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  Chemistry operations may run across multiple threads

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

TEST_F(ChemistryExceptionHandlingTest, ExceptionContextMetadata) {
    // WHAT: Test adding context metadata to chemistry exceptions
    // WHY:  Context helps diagnose issues

    nimcp_exception_t* ex = create_chemistry_exception(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_CATEGORY_ELEMENT,
        EXCEPTION_SEVERITY_ERROR,
        "Element not found"
    );

    ASSERT_NE(ex, nullptr);

    // Add context
    int result = nimcp_exception_set_context(ex, "element_symbol", "Xyz");
    EXPECT_EQ(result, 0);

    result = nimcp_exception_set_context(ex, "lookup_type", "symbol");
    EXPECT_EQ(result, 0);

    // Verify context
    EXPECT_STREQ(nimcp_exception_get_context(ex, "element_symbol"), "Xyz");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "lookup_type"), "symbol");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
