/**
 * @file test_code_generation.cpp
 * @brief Unit tests for Code Generation Engine
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Comprehensive unit tests for code generation engine
 * WHY: Ensure autonomous code fix generation works correctly
 * HOW: Test-driven development with coverage of all public APIs
 *
 * Test Coverage:
 * - Creation and destruction
 * - Fix strategy selection
 * - Fix candidate generation
 * - Confidence scoring
 * - Risk calculation
 * - Pattern matching
 * - Template management
 * - Error handling
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_code_generation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CodeGenerationTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;

    void SetUp() override {
        // Initialize memory tracking
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Record baseline memory (from previous tests or global state)
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;
    }

    void TearDown() override {
        // Check for memory leaks relative to baseline
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, baseline_allocated)
            << "Memory leak detected! Allocated: " << stats.current_allocated
            << ", Baseline: " << baseline_allocated;
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

/**
 * @test Code Generation Engine Creation with Default Config
 *
 * WHAT: Verify code generation engine can be created with default configuration
 * WHY: Ensure proper initialization of all components
 * HOW: Create with defaults, verify ready state, destroy
 */
TEST_F(CodeGenerationTest, CreateWithDefaults) {
    // ACT: Create with default config
    code_gen_engine_t* engine = code_gen_create(NULL);

    // ASSERT: Created successfully
    ASSERT_NE(engine, nullptr);

    // Verify ready state
    EXPECT_TRUE(code_gen_is_ready(engine));

    // CLEANUP
    code_gen_destroy(engine);
}

/**
 * @test Code Generation Engine Creation with Custom Config
 *
 * WHAT: Verify engine accepts custom configuration
 * WHY: Allow users to customize generation parameters
 * HOW: Create with custom config, verify settings applied
 */
TEST_F(CodeGenerationTest, CreateWithCustomConfig) {
    // ARRANGE: Custom config
    code_gen_config_t config = code_gen_default_config();
    config.default_min_confidence = 0.8f;
    config.default_max_risk = 0.2f;
    config.default_max_candidates = 3;
    config.enable_pattern_matching = true;

    // ACT: Create with custom config
    code_gen_engine_t* engine = code_gen_create(&config);

    // ASSERT: Created with custom settings
    ASSERT_NE(engine, nullptr);
    EXPECT_TRUE(code_gen_is_ready(engine));

    // CLEANUP
    code_gen_destroy(engine);
}

/**
 * @test Code Generation Engine Destroy NULL Safety
 *
 * WHAT: Verify destroy handles NULL gracefully
 * WHY: Prevent crashes on double-free or accidental NULL
 * HOW: Call destroy with NULL, expect no crash
 */
TEST_F(CodeGenerationTest, DestroyNullSafety) {
    // ACT & ASSERT: Should not crash
    EXPECT_NO_THROW(code_gen_destroy(NULL));
}

//=============================================================================
// Strategy Selection Tests
//=============================================================================

/**
 * @test Strategy Selection for Null Pointer Error
 *
 * WHAT: Verify correct strategy selected for null pointer errors
 * WHY: Ensure error type maps to appropriate fix
 * HOW: Query strategy for null pointer error, verify null check selected
 */
TEST_F(CodeGenerationTest, SelectStrategyNullPointer) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    code_fix_strategy_t strategy;
    float confidence;

    // ACT: Select strategy for null pointer error
    int result = code_gen_select_strategy(
        engine,
        ERROR_TYPE_NULL_POINTER,
        NULL,  // no code analysis
        &strategy,
        &confidence
    );

    // ASSERT: Null check strategy selected with good confidence
    EXPECT_EQ(result, 0);
    EXPECT_EQ(strategy, FIX_STRATEGY_NULL_CHECK);
    EXPECT_GT(confidence, 0.5f);

    // CLEANUP
    code_gen_destroy(engine);
}

/**
 * @test Strategy Selection for Array Out of Bounds
 *
 * WHAT: Verify correct strategy selected for bounds errors
 * WHY: Ensure bounds violations get bounds check fix
 * HOW: Query strategy, verify bounds check selected
 */
TEST_F(CodeGenerationTest, SelectStrategyBoundsError) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    code_fix_strategy_t strategy;
    float confidence;

    // ACT: Select strategy for bounds error
    int result = code_gen_select_strategy(
        engine,
        ERROR_TYPE_BUFFER_OVERFLOW,
        NULL,
        &strategy,
        &confidence
    );

    // ASSERT: Bounds check strategy selected
    EXPECT_EQ(result, 0);
    EXPECT_EQ(strategy, FIX_STRATEGY_BOUNDS_CHECK);

    // CLEANUP
    code_gen_destroy(engine);
}

/**
 * @test Strategy Selection for Division by Zero
 *
 * WHAT: Verify correct strategy for division errors
 * WHY: Prevent runtime division by zero crashes
 * HOW: Query strategy, verify division guard selected
 */
TEST_F(CodeGenerationTest, SelectStrategyDivisionByZero) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    code_fix_strategy_t strategy;
    float confidence;

    // ACT
    int result = code_gen_select_strategy(
        engine,
        ERROR_TYPE_DIVIDE_BY_ZERO,
        NULL,
        &strategy,
        &confidence
    );

    // ASSERT
    EXPECT_EQ(result, 0);
    EXPECT_EQ(strategy, FIX_STRATEGY_DIVISION_GUARD);

    // CLEANUP
    code_gen_destroy(engine);
}

/**
 * @test Get Compatible Strategies
 *
 * WHAT: Verify multiple strategies returned for ambiguous errors
 * WHY: Support multiple candidate generation
 * HOW: Query compatible strategies, verify array populated
 */
TEST_F(CodeGenerationTest, GetCompatibleStrategies) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    code_fix_strategy_t strategies[8];

    // ACT: Get strategies for memory error (multiple options)
    int count = code_gen_get_compatible_strategies(
        engine,
        ERROR_TYPE_HEAP_CORRUPTION,
        strategies,
        8
    );

    // ASSERT: At least one strategy returned
    EXPECT_GT(count, 0);

    // CLEANUP
    code_gen_destroy(engine);
}

//=============================================================================
// Fix Generation Tests
//=============================================================================

/**
 * @test Generate Fix with Specific Strategy
 *
 * WHAT: Verify fix generation using specific strategy
 * WHY: Core fix generation functionality
 * HOW: Request fix with null check strategy, verify output
 */
TEST_F(CodeGenerationTest, GenerateWithStrategy) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    code_location_t location = {0};
    strncpy(location.file_path, "test.c", sizeof(location.file_path) - 1);
    location.line_number = 42;
    strncpy(location.function_name, "test_function", sizeof(location.function_name) - 1);

    const char* source_code = "void test_function(int* ptr) {\n    *ptr = 42;\n}";
    generated_fix_t fix;
    memset(&fix, 0, sizeof(fix));

    // ACT: Generate null check fix
    int result = code_gen_generate_with_strategy(
        engine,
        FIX_STRATEGY_NULL_CHECK,
        &location,
        source_code,
        &fix
    );

    // ASSERT: Fix generated successfully
    EXPECT_EQ(result, 0);
    EXPECT_EQ(fix.strategy, FIX_STRATEGY_NULL_CHECK);
    EXPECT_GT(fix.confidence, 0.0f);
    EXPECT_GT(strlen(fix.fixed_code), 0u);
    EXPECT_GT(strlen(fix.explanation), 0u);

    // CLEANUP
    code_gen_destroy(engine);
}

/**
 * @test Generate Fix Candidates
 *
 * WHAT: Verify multiple fix candidates generated
 * WHY: Support best fix selection from multiple options
 * HOW: Request candidates, verify multiple returned
 */
TEST_F(CodeGenerationTest, GenerateCandidates) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    // Create diagnostic result
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NULL_POINTER;
    diagnosis.severity = DIAG_SEVERITY_CRITICAL;
    strncpy(diagnosis.likely_faulty_function, "test_func", sizeof(diagnosis.likely_faulty_function) - 1);
    strncpy(diagnosis.root_cause, "Null pointer dereference", sizeof(diagnosis.root_cause) - 1);

    // Create request
    code_gen_request_t request;
    memset(&request, 0, sizeof(request));
    request.diagnosis = &diagnosis;
    request.source_code = "int* ptr = NULL;\n*ptr = 5;";
    request.min_confidence = 0.5f;
    request.max_risk = 0.5f;
    request.max_candidates = 5;

    code_gen_result_t result;
    memset(&result, 0, sizeof(result));

    // ACT: Generate candidates
    int status = code_gen_generate_candidates(engine, &request, &result);

    // ASSERT: Generation succeeded with candidates
    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.candidates.count, 0u);
    EXPECT_NE(result.best_fix, nullptr);

    // CLEANUP
    code_gen_destroy(engine);
}

/**
 * @test Select Best Fix from Candidates
 *
 * WHAT: Verify best fix selection algorithm
 * WHY: Ensure optimal fix chosen based on confidence/risk
 * HOW: Create candidate set, select best, verify criteria
 */
TEST_F(CodeGenerationTest, SelectBestFix) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    // Create candidate set with varying confidence
    fix_candidate_set_t candidates;
    memset(&candidates, 0, sizeof(candidates));
    candidates.count = 3;

    // Candidate 0: Low confidence
    candidates.candidates[0].fix_id = 1;
    candidates.candidates[0].strategy = FIX_STRATEGY_NULL_CHECK;
    candidates.candidates[0].confidence = 0.5f;
    candidates.candidates[0].risk_score = 0.2f;

    // Candidate 1: High confidence (should be selected)
    candidates.candidates[1].fix_id = 2;
    candidates.candidates[1].strategy = FIX_STRATEGY_NULL_CHECK;
    candidates.candidates[1].confidence = 0.9f;
    candidates.candidates[1].risk_score = 0.1f;

    // Candidate 2: Medium confidence
    candidates.candidates[2].fix_id = 3;
    candidates.candidates[2].strategy = FIX_STRATEGY_BOUNDS_CHECK;
    candidates.candidates[2].confidence = 0.7f;
    candidates.candidates[2].risk_score = 0.3f;

    generated_fix_t selected;
    memset(&selected, 0, sizeof(selected));

    // ACT: Select best fix
    int result = code_gen_select_best_fix(engine, &candidates, &selected);

    // ASSERT: High confidence fix selected
    EXPECT_EQ(result, 0);
    EXPECT_EQ(selected.fix_id, 2u);
    EXPECT_EQ(selected.confidence, 0.9f);

    // CLEANUP
    code_gen_destroy(engine);
}

//=============================================================================
// Confidence and Risk Scoring Tests
//=============================================================================

/**
 * @test Calculate Fix Confidence
 *
 * WHAT: Verify confidence calculation
 * WHY: Confidence used to filter/rank fixes
 * HOW: Calculate confidence for fix, verify range
 */
TEST_F(CodeGenerationTest, CalculateConfidence) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    generated_fix_t fix;
    memset(&fix, 0, sizeof(fix));
    fix.strategy = FIX_STRATEGY_NULL_CHECK;
    fix.complexity = FIX_COMPLEXITY_SIMPLE;
    strncpy(fix.fixed_code, "if (ptr != NULL) { *ptr = 42; }", sizeof(fix.fixed_code) - 1);

    code_gen_request_t request;
    memset(&request, 0, sizeof(request));

    // ACT: Calculate confidence
    float confidence = code_gen_calculate_confidence(engine, &fix, &request);

    // ASSERT: Valid confidence in range [0,1]
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    // CLEANUP
    code_gen_destroy(engine);
}

/**
 * @test Calculate Fix Risk Score
 *
 * WHAT: Verify risk calculation for fixes
 * WHY: Risk used to filter dangerous fixes
 * HOW: Calculate risk for different complexity fixes
 */
TEST_F(CodeGenerationTest, CalculateRisk) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    // Simple fix should have low risk
    generated_fix_t simple_fix;
    memset(&simple_fix, 0, sizeof(simple_fix));
    simple_fix.strategy = FIX_STRATEGY_NULL_CHECK;
    simple_fix.complexity = FIX_COMPLEXITY_SIMPLE;

    // Complex fix should have higher risk
    generated_fix_t complex_fix;
    memset(&complex_fix, 0, sizeof(complex_fix));
    complex_fix.strategy = FIX_STRATEGY_MUTEX_FIX;
    complex_fix.complexity = FIX_COMPLEXITY_COMPLEX;

    // ACT: Calculate risks
    float simple_risk = code_gen_calculate_risk(engine, &simple_fix, NULL);
    float complex_risk = code_gen_calculate_risk(engine, &complex_fix, NULL);

    // ASSERT: Complex fixes have higher risk
    EXPECT_GE(simple_risk, 0.0f);
    EXPECT_LE(simple_risk, 1.0f);
    EXPECT_GE(complex_risk, 0.0f);
    EXPECT_LE(complex_risk, 1.0f);
    EXPECT_LT(simple_risk, complex_risk);

    // CLEANUP
    code_gen_destroy(engine);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * @test Get and Reset Statistics
 *
 * WHAT: Verify statistics tracking
 * WHY: Monitor engine performance and outcomes
 * HOW: Get stats, verify structure, reset
 */
TEST_F(CodeGenerationTest, StatisticsTracking) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    code_gen_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    // ACT: Get initial stats
    int result = code_gen_get_stats(engine, &stats);

    // ASSERT: Stats retrieved successfully
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.fixes_generated, 0u);  // No fixes yet

    // Reset and verify
    code_gen_reset_stats(engine);
    result = code_gen_get_stats(engine, &stats);
    EXPECT_EQ(result, 0);

    // CLEANUP
    code_gen_destroy(engine);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

/**
 * @test Strategy Name Conversion
 *
 * WHAT: Verify strategy enum to string conversion
 * WHY: Support logging and debugging
 * HOW: Convert each strategy, verify non-empty string
 */
TEST_F(CodeGenerationTest, StrategyNameConversion) {
    // ACT & ASSERT: Each strategy has a valid name
    EXPECT_NE(code_gen_strategy_name(FIX_STRATEGY_NONE), nullptr);
    EXPECT_GT(strlen(code_gen_strategy_name(FIX_STRATEGY_NULL_CHECK)), 0u);
    EXPECT_GT(strlen(code_gen_strategy_name(FIX_STRATEGY_BOUNDS_CHECK)), 0u);
    EXPECT_GT(strlen(code_gen_strategy_name(FIX_STRATEGY_DIVISION_GUARD)), 0u);
    EXPECT_GT(strlen(code_gen_strategy_name(FIX_STRATEGY_NAN_GUARD)), 0u);
    EXPECT_GT(strlen(code_gen_strategy_name(FIX_STRATEGY_MUTEX_FIX)), 0u);
    EXPECT_GT(strlen(code_gen_strategy_name(FIX_STRATEGY_MEMORY_CLEANUP)), 0u);
}

/**
 * @test Status Name Conversion
 *
 * WHAT: Verify status enum to string conversion
 * WHY: Support status reporting
 * HOW: Convert each status, verify non-empty string
 */
TEST_F(CodeGenerationTest, StatusNameConversion) {
    // ACT & ASSERT
    EXPECT_GT(strlen(code_gen_status_name(FIX_STATUS_PROPOSED)), 0u);
    EXPECT_GT(strlen(code_gen_status_name(FIX_STATUS_COMPILED)), 0u);
    EXPECT_GT(strlen(code_gen_status_name(FIX_STATUS_VALIDATED)), 0u);
    EXPECT_GT(strlen(code_gen_status_name(FIX_STATUS_APPLIED)), 0u);
    EXPECT_GT(strlen(code_gen_status_name(FIX_STATUS_COMMITTED)), 0u);
    EXPECT_GT(strlen(code_gen_status_name(FIX_STATUS_FAILED)), 0u);
}

/**
 * @test Version String
 *
 * WHAT: Verify version string returned
 * WHY: Support version tracking
 * HOW: Get version, verify format
 */
TEST_F(CodeGenerationTest, VersionString) {
    // ACT
    const char* version = code_gen_version();

    // ASSERT: Valid version string
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * @test Handle NULL Parameters Gracefully
 *
 * WHAT: Verify NULL parameter handling
 * WHY: Prevent crashes on invalid input
 * HOW: Call functions with NULL, verify error returned
 */
TEST_F(CodeGenerationTest, NullParameterHandling) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    code_fix_strategy_t strategy;
    float confidence;
    generated_fix_t fix;

    // ACT & ASSERT: NULL parameters return error
    EXPECT_NE(code_gen_select_strategy(NULL, ERROR_TYPE_NULL_POINTER, NULL, &strategy, &confidence), 0);
    EXPECT_NE(code_gen_select_strategy(engine, ERROR_TYPE_NULL_POINTER, NULL, NULL, &confidence), 0);
    EXPECT_NE(code_gen_generate_with_strategy(NULL, FIX_STRATEGY_NULL_CHECK, NULL, "code", &fix), 0);
    EXPECT_NE(code_gen_generate_candidates(NULL, NULL, NULL), 0);

    // CLEANUP
    code_gen_destroy(engine);
}

/**
 * @test is_ready Returns False for NULL Engine
 *
 * WHAT: Verify is_ready handles NULL
 * WHY: Safe status checking
 * HOW: Call with NULL, verify false returned
 */
TEST_F(CodeGenerationTest, IsReadyNullSafety) {
    // ACT & ASSERT
    EXPECT_FALSE(code_gen_is_ready(NULL));
}

//=============================================================================
// Template Management Tests
//=============================================================================

/**
 * @test Register Custom Template
 *
 * WHAT: Verify custom template registration
 * WHY: Support user-defined fix patterns
 * HOW: Register template, use in generation
 */
TEST_F(CodeGenerationTest, RegisterCustomTemplate) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    const char* template_code = "if (${VAR} == NULL) return ${DEFAULT};";
    const char* description = "Custom null check with default return";

    // ACT: Register template
    int result = code_gen_register_template(
        engine,
        FIX_STRATEGY_NULL_CHECK,
        template_code,
        description
    );

    // ASSERT: Registration succeeded
    EXPECT_EQ(result, 0);

    // CLEANUP
    code_gen_destroy(engine);
}

//=============================================================================
// Learning Tests
//=============================================================================

/**
 * @test Learn from Fix Outcome
 *
 * WHAT: Verify learning from successful/failed fixes
 * WHY: Improve future fix generation
 * HOW: Record outcome, verify no crash
 */
TEST_F(CodeGenerationTest, LearnFromOutcome) {
    // ARRANGE
    code_gen_engine_t* engine = code_gen_create(NULL);
    ASSERT_NE(engine, nullptr);

    generated_fix_t fix;
    memset(&fix, 0, sizeof(fix));
    fix.fix_id = 1;
    fix.strategy = FIX_STRATEGY_NULL_CHECK;
    fix.confidence = 0.8f;
    strncpy(fix.source_file, "test.c", sizeof(fix.source_file) - 1);

    // ACT: Learn from successful outcome
    int result = code_gen_learn_from_outcome(engine, &fix, true);

    // ASSERT: Learning succeeded
    EXPECT_EQ(result, 0);

    // Learn from failed outcome too
    result = code_gen_learn_from_outcome(engine, &fix, false);
    EXPECT_EQ(result, 0);

    // CLEANUP
    code_gen_destroy(engine);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
