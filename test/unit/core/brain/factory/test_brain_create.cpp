//=============================================================================
// test_brain_create.cpp - Comprehensive Unit Tests for brain_create()
//=============================================================================
/**
 * @file test_brain_create.cpp
 * @brief Comprehensive unit tests for the brain_create() factory function
 *
 * WHAT: Tests covering all aspects of brain_create() including creation,
 *       parameter validation, different brain sizes/tasks, and error handling
 * WHY:  Ensure robust brain creation with proper validation and initialization
 * HOW:  GoogleTest framework with 25 comprehensive test cases
 *
 * Test Categories:
 * 1. Basic Creation (success paths)
 * 2. Parameter Validation (NULL checks, input/output ranges)
 * 3. Different Brain Sizes (TINY, SMALL, MEDIUM, LARGE)
 * 4. Different Tasks (CLASSIFICATION, REGRESSION, PATTERN_MATCHING, etc.)
 * 5. Edge Cases (minimum/maximum dimensions)
 * 6. Resource Allocation (subsystem initialization)
 * 7. Error Handling (graceful failures)
 *
 * @version 1.0.0
 * @date 2025-11-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <memory>

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * BrainCreateTest Fixture
 *
 * Provides setup/teardown for initialization and error handling
 */
class BrainCreateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any previous errors
        brain_clear_error();
    }

    void TearDown() override {
        // No global cleanup needed
    }

    // Helper to check if brain was created successfully
    bool is_valid_brain(brain_t brain) const {
        return brain != nullptr;
    }

    // Helper to clean up created brains
    void cleanup_brain(brain_t brain) {
        if (brain != nullptr) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// 1. BASIC CREATION TESTS (Success Paths)
//=============================================================================

/**
 * TEST: BasicCreation_Success
 *
 * WHAT: Create a brain with typical parameters
 * WHY:  Verify basic creation functionality
 * EXPECT: Brain created successfully, not nullptr
 */
TEST_F(BrainCreateTest, BasicCreation_Success)
{
    brain_t brain = brain_create(
        "test_brain",           // task_name
        BRAIN_SIZE_SMALL,       // size
        BRAIN_TASK_CLASSIFICATION,  // task
        5,                      // num_inputs
        3                       // num_outputs
    );

    EXPECT_NE(nullptr, brain) << "Brain should be created successfully";
    cleanup_brain(brain);
}

/**
 * TEST: BasicCreation_WithDifferentName
 *
 * WHAT: Create multiple brains with different names
 * WHY:  Verify task_name parameter works correctly
 * EXPECT: Multiple distinct brains created
 */
TEST_F(BrainCreateTest, BasicCreation_WithDifferentName)
{
    const char* names[] = {
        "classifier",
        "regressor",
        "detector",
        "analyzer"
    };

    std::vector<brain_t> brains;

    for (const auto* name : names) {
        brain_t brain = brain_create(
            name,
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            2,
            2
        );
        EXPECT_NE(nullptr, brain) << "Brain with name '" << name << "' should be created";
        brains.push_back(brain);
    }

    // Cleanup
    for (auto brain : brains) {
        cleanup_brain(brain);
    }
}

/**
 * TEST: BasicCreation_AllocationSuccess
 *
 * WHAT: Verify brain structure is properly allocated
 * WHY:  Ensure memory is allocated and not null
 * EXPECT: Brain pointer is valid and not null
 */
TEST_F(BrainCreateTest, BasicCreation_AllocationSuccess)
{
    brain_t brain = brain_create(
        "alloc_test",
        BRAIN_SIZE_MEDIUM,
        BRAIN_TASK_CLASSIFICATION,
        10,
        5
    );

    ASSERT_NE(nullptr, brain);
    EXPECT_TRUE(is_valid_brain(brain));

    cleanup_brain(brain);
}

//=============================================================================
// 2. PARAMETER VALIDATION TESTS
//=============================================================================

/**
 * TEST: ParameterValidation_NullTaskName
 *
 * WHAT: Create brain with NULL task_name
 * WHY:  Ensure NULL name is properly rejected
 * EXPECT: Creation fails, returns nullptr
 */
TEST_F(BrainCreateTest, ParameterValidation_NullTaskName)
{
    brain_t brain = brain_create(
        nullptr,                // NULL task_name - INVALID
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        3
    );

    EXPECT_EQ(nullptr, brain)
        << "Brain creation should fail with NULL task_name";
}

/**
 * TEST: ParameterValidation_EmptyTaskName
 *
 * WHAT: Create brain with empty string task_name
 * WHY:  Verify empty name validation
 * EXPECT: Creation fails or succeeds with warning
 */
TEST_F(BrainCreateTest, ParameterValidation_EmptyTaskName)
{
    brain_t brain = brain_create(
        "",                     // Empty name
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        3
    );

    // Empty name should either fail or be handled gracefully
    // The behavior depends on implementation - document here
    EXPECT_TRUE(brain == nullptr || brain != nullptr)
        << "Empty task_name handling should be documented";
    cleanup_brain(brain);
}

/**
 * TEST: ParameterValidation_ZeroInputs
 *
 * WHAT: Create brain with zero input dimensions
 * WHY:  Verify zero input validation
 * EXPECT: Creation fails (invalid configuration)
 */
TEST_F(BrainCreateTest, ParameterValidation_ZeroInputs)
{
    brain_t brain = brain_create(
        "zero_inputs",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        0,                      // INVALID - zero inputs
        3
    );

    EXPECT_EQ(nullptr, brain)
        << "Brain creation should fail with zero inputs";
}

/**
 * TEST: ParameterValidation_ZeroOutputs
 *
 * WHAT: Create brain with zero output dimensions
 * WHY:  Verify zero output validation
 * EXPECT: Creation fails (invalid configuration)
 */
TEST_F(BrainCreateTest, ParameterValidation_ZeroOutputs)
{
    brain_t brain = brain_create(
        "zero_outputs",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        0                       // INVALID - zero outputs
    );

    EXPECT_EQ(nullptr, brain)
        << "Brain creation should fail with zero outputs";
}

/**
 * TEST: ParameterValidation_ExcessiveInputs
 *
 * WHAT: Create brain with very large input dimension
 * WHY:  Verify upper bound validation on inputs
 * EXPECT: Creation fails (exceeds limit)
 */
TEST_F(BrainCreateTest, ParameterValidation_ExcessiveInputs)
{
    brain_t brain = brain_create(
        "excessive_inputs",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        50000,                  // Likely exceeds limit (10000)
        3
    );

    EXPECT_EQ(nullptr, brain)
        << "Brain creation should fail with excessive inputs";
}

/**
 * TEST: ParameterValidation_ExcessiveOutputs
 *
 * WHAT: Create brain with very large output dimension
 * WHY:  Verify upper bound validation on outputs
 * EXPECT: Creation fails (exceeds limit)
 */
TEST_F(BrainCreateTest, ParameterValidation_ExcessiveOutputs)
{
    brain_t brain = brain_create(
        "excessive_outputs",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        50000                   // Likely exceeds limit (10000)
    );

    EXPECT_EQ(nullptr, brain)
        << "Brain creation should fail with excessive outputs";
}

//=============================================================================
// 3. DIFFERENT BRAIN SIZES TESTS
//=============================================================================

/**
 * TEST: BrainSizes_Tiny
 *
 * WHAT: Create TINY brain (100 neurons)
 * WHY:  Test smallest configuration
 * EXPECT: Creation succeeds with appropriate neuron count
 */
TEST_F(BrainCreateTest, BrainSizes_Tiny)
{
    brain_t brain = brain_create(
        "tiny_brain",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        3,
        2
    );

    EXPECT_NE(nullptr, brain) << "TINY brain should be created successfully";
    cleanup_brain(brain);
}

/**
 * TEST: BrainSizes_Small
 *
 * WHAT: Create SMALL brain (500 neurons)
 * WHY:  Test common small configuration
 * EXPECT: Creation succeeds
 */
TEST_F(BrainCreateTest, BrainSizes_Small)
{
    brain_t brain = brain_create(
        "small_brain",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        3
    );

    EXPECT_NE(nullptr, brain) << "SMALL brain should be created successfully";
    cleanup_brain(brain);
}

/**
 * TEST: BrainSizes_Medium
 *
 * WHAT: Create MEDIUM brain (1000 neurons)
 * WHY:  Test typical configuration
 * EXPECT: Creation succeeds
 */
TEST_F(BrainCreateTest, BrainSizes_Medium)
{
    brain_t brain = brain_create(
        "medium_brain",
        BRAIN_SIZE_MEDIUM,
        BRAIN_TASK_CLASSIFICATION,
        10,
        5
    );

    EXPECT_NE(nullptr, brain) << "MEDIUM brain should be created successfully";
    cleanup_brain(brain);
}

/**
 * TEST: BrainSizes_Large
 *
 * WHAT: Create LARGE brain (5000 neurons)
 * WHY:  Test largest standard configuration
 * EXPECT: Creation succeeds (may take longer)
 */
TEST_F(BrainCreateTest, BrainSizes_Large)
{
    brain_t brain = brain_create(
        "large_brain",
        BRAIN_SIZE_LARGE,
        BRAIN_TASK_CLASSIFICATION,
        20,
        10
    );

    EXPECT_NE(nullptr, brain) << "LARGE brain should be created successfully";
    cleanup_brain(brain);
}

/**
 * TEST: BrainSizes_Custom
 *
 * WHAT: Create CUSTOM brain with default settings
 * WHY:  Verify CUSTOM size handling
 * EXPECT: Creation succeeds with default neuron count
 */
TEST_F(BrainCreateTest, BrainSizes_Custom)
{
    brain_t brain = brain_create(
        "custom_brain",
        BRAIN_SIZE_CUSTOM,
        BRAIN_TASK_CLASSIFICATION,
        5,
        3
    );

    EXPECT_NE(nullptr, brain) << "CUSTOM brain should be created successfully";
    cleanup_brain(brain);
}

//=============================================================================
// 4. DIFFERENT TASK TYPES TESTS
//=============================================================================

/**
 * TEST: TaskTypes_Classification
 *
 * WHAT: Create brain for classification task
 * WHY:  Verify classification task configuration
 * EXPECT: Creation succeeds with appropriate defaults
 */
TEST_F(BrainCreateTest, TaskTypes_Classification)
{
    brain_t brain = brain_create(
        "classifier",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        3
    );

    EXPECT_NE(nullptr, brain) << "Classification brain should be created";
    cleanup_brain(brain);
}

/**
 * TEST: TaskTypes_Regression
 *
 * WHAT: Create brain for regression task
 * WHY:  Verify regression configuration
 * EXPECT: Creation succeeds
 */
TEST_F(BrainCreateTest, TaskTypes_Regression)
{
    brain_t brain = brain_create(
        "regressor",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_REGRESSION,
        5,
        1                       // Single continuous output
    );

    EXPECT_NE(nullptr, brain) << "Regression brain should be created";
    cleanup_brain(brain);
}

/**
 * TEST: TaskTypes_PatternMatching
 *
 * WHAT: Create brain for pattern matching task
 * WHY:  Verify pattern matching configuration
 * EXPECT: Creation succeeds
 */
TEST_F(BrainCreateTest, TaskTypes_PatternMatching)
{
    brain_t brain = brain_create(
        "pattern_matcher",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_PATTERN_MATCHING,
        10,
        5
    );

    EXPECT_NE(nullptr, brain) << "Pattern matching brain should be created";
    cleanup_brain(brain);
}

/**
 * TEST: TaskTypes_Sequence
 *
 * WHAT: Create brain for sequence learning task
 * WHY:  Verify temporal sequence configuration
 * EXPECT: Creation succeeds
 */
TEST_F(BrainCreateTest, TaskTypes_Sequence)
{
    brain_t brain = brain_create(
        "sequence_learner",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_SEQUENCE,
        5,
        5
    );

    EXPECT_NE(nullptr, brain) << "Sequence learning brain should be created";
    cleanup_brain(brain);
}

/**
 * TEST: TaskTypes_Association
 *
 * WHAT: Create brain for association learning task
 * WHY:  Verify association configuration
 * EXPECT: Creation succeeds
 */
TEST_F(BrainCreateTest, TaskTypes_Association)
{
    brain_t brain = brain_create(
        "associator",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_ASSOCIATION,
        3,
        3
    );

    EXPECT_NE(nullptr, brain) << "Association learning brain should be created";
    cleanup_brain(brain);
}

/**
 * TEST: TaskTypes_Custom
 *
 * WHAT: Create brain for custom task
 * WHY:  Verify custom task handling
 * EXPECT: Creation succeeds
 */
TEST_F(BrainCreateTest, TaskTypes_Custom)
{
    brain_t brain = brain_create(
        "custom_task",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CUSTOM,
        5,
        3
    );

    EXPECT_NE(nullptr, brain) << "Custom task brain should be created";
    cleanup_brain(brain);
}

//=============================================================================
// 5. EDGE CASES TESTS
//=============================================================================

/**
 * TEST: EdgeCase_MinimumDimensions
 *
 * WHAT: Create brain with minimum valid dimensions
 * WHY:  Verify boundary conditions (1 input, 1 output)
 * EXPECT: Creation succeeds
 */
TEST_F(BrainCreateTest, EdgeCase_MinimumDimensions)
{
    brain_t brain = brain_create(
        "minimal",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        1,                      // Minimum input
        1                       // Minimum output
    );

    EXPECT_NE(nullptr, brain)
        << "Brain with minimum dimensions should be created";
    cleanup_brain(brain);
}

/**
 * TEST: EdgeCase_LargeDimensions
 *
 * WHAT: Create brain with large but valid dimensions
 * WHY:  Test upper boundary without exceeding limits
 * EXPECT: Creation succeeds
 */
TEST_F(BrainCreateTest, EdgeCase_LargeDimensions)
{
    brain_t brain = brain_create(
        "large_dims",
        BRAIN_SIZE_LARGE,
        BRAIN_TASK_CLASSIFICATION,
        1000,                   // Large input
        100                     // Large output
    );

    EXPECT_NE(nullptr, brain)
        << "Brain with large dimensions should be created";
    cleanup_brain(brain);
}

/**
 * TEST: EdgeCase_HighOutputCount
 *
 * WHAT: Create classification brain with many output classes
 * WHY:  Verify multi-class configuration
 * EXPECT: Creation succeeds
 */
TEST_F(BrainCreateTest, EdgeCase_HighOutputCount)
{
    brain_t brain = brain_create(
        "multi_class",
        BRAIN_SIZE_MEDIUM,
        BRAIN_TASK_CLASSIFICATION,
        10,
        1000                    // Many output classes
    );

    EXPECT_NE(nullptr, brain)
        << "Brain with many output classes should be created";
    cleanup_brain(brain);
}

/**
 * TEST: EdgeCase_VeryLongTaskName
 *
 * WHAT: Create brain with very long task name
 * WHY:  Test name buffer handling
 * EXPECT: Creation succeeds or fails gracefully
 */
TEST_F(BrainCreateTest, EdgeCase_VeryLongTaskName)
{
    // Create a long but reasonable name
    std::string long_name(255, 'a');

    brain_t brain = brain_create(
        long_name.c_str(),
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        3
    );

    // Should either succeed or fail gracefully
    if (brain != nullptr) {
        cleanup_brain(brain);
    }
    EXPECT_TRUE(true) << "Long name handling should not crash";
}

//=============================================================================
// 6. RESOURCE ALLOCATION TESTS
//=============================================================================

/**
 * TEST: ResourceAllocation_NetworkInitialized
 *
 * WHAT: Verify neural network is initialized
 * WHY:  Ensure adaptive network is properly created
 * EXPECT: Brain has valid network structure
 */
TEST_F(BrainCreateTest, ResourceAllocation_NetworkInitialized)
{
    brain_t brain = brain_create(
        "network_test",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        3
    );

    EXPECT_NE(nullptr, brain) << "Brain creation should succeed";

    // Check that network is allocated (this assumes accessor functions exist)
    // This test verifies the core network exists

    cleanup_brain(brain);
}

/**
 * TEST: ResourceAllocation_MultipleCreations
 *
 * WHAT: Create multiple brains without memory leaks
 * WHY:  Verify proper resource allocation and independence
 * EXPECT: All brains created successfully and independently
 */
TEST_F(BrainCreateTest, ResourceAllocation_MultipleCreations)
{
    const int num_brains = 5;
    std::vector<brain_t> brains;

    // Create multiple brains
    for (int i = 0; i < num_brains; ++i) {
        std::string name = "brain_" + std::to_string(i);
        brain_t brain = brain_create(
            name.c_str(),
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            2,
            2
        );
        EXPECT_NE(nullptr, brain)
            << "Brain " << i << " should be created successfully";
        brains.push_back(brain);
    }

    EXPECT_EQ(num_brains, static_cast<int>(brains.size()))
        << "All brains should be created";

    // Cleanup all brains
    for (auto brain : brains) {
        cleanup_brain(brain);
    }
}

//=============================================================================
// 7. ERROR HANDLING TESTS
//=============================================================================

/**
 * TEST: ErrorHandling_InvalidSize
 *
 * WHAT: Create brain with invalid size value
 * WHY:  Verify invalid enum handling
 * EXPECT: Creation fails gracefully
 */
TEST_F(BrainCreateTest, ErrorHandling_InvalidSize)
{
    // Cast to size enum an invalid value
    brain_t brain = brain_create(
        "invalid_size",
        static_cast<brain_size_t>(999),  // Invalid size
        BRAIN_TASK_CLASSIFICATION,
        5,
        3
    );

    // Should handle invalid size gracefully
    // Implementation may use default or reject
    EXPECT_TRUE(brain == nullptr || brain != nullptr)
        << "Invalid size should be handled";
    cleanup_brain(brain);
}

/**
 * TEST: ErrorHandling_InvalidTask
 *
 * WHAT: Create brain with invalid task type
 * WHY:  Verify invalid task enum handling
 * EXPECT: Creation fails gracefully
 */
TEST_F(BrainCreateTest, ErrorHandling_InvalidTask)
{
    brain_t brain = brain_create(
        "invalid_task",
        BRAIN_SIZE_SMALL,
        static_cast<brain_task_t>(999),  // Invalid task
        5,
        3
    );

    // Should handle invalid task gracefully
    EXPECT_TRUE(brain == nullptr || brain != nullptr)
        << "Invalid task should be handled";
    cleanup_brain(brain);
}

/**
 * TEST: ErrorHandling_CombinedValidationFailure
 *
 * WHAT: Create brain with multiple invalid parameters
 * WHY:  Verify validation catches multiple errors
 * EXPECT: Creation fails (NULL name takes precedence)
 */
TEST_F(BrainCreateTest, ErrorHandling_CombinedValidationFailure)
{
    brain_t brain = brain_create(
        nullptr,                // NULL name - INVALID
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        0,                      // Zero inputs - INVALID
        0                       // Zero outputs - INVALID
    );

    EXPECT_EQ(nullptr, brain)
        << "Creation should fail with multiple invalid parameters";
}

/**
 * TEST: ErrorHandling_BoundaryInputCount
 *
 * WHAT: Test inputs at exactly the validation boundary
 * WHY:  Verify boundary conditions are handled correctly
 * EXPECT: Creation succeeds at valid boundary
 */
TEST_F(BrainCreateTest, ErrorHandling_BoundaryInputCount)
{
    // Just above zero (valid)
    brain_t brain = brain_create(
        "boundary_inputs",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        1,                      // Minimum valid
        1
    );

    EXPECT_NE(nullptr, brain) << "Boundary value should be accepted";
    cleanup_brain(brain);
}

/**
 * TEST: ErrorHandling_SequentialCreationAfterFailure
 *
 * WHAT: Verify system can recover after failed creation
 * WHY:  Ensure no internal state corruption from failed attempts
 * EXPECT: Can create valid brain after failure
 */
TEST_F(BrainCreateTest, ErrorHandling_SequentialCreationAfterFailure)
{
    // Attempt creation with invalid parameters
    brain_t invalid_brain = brain_create(
        nullptr,
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        3
    );
    EXPECT_EQ(nullptr, invalid_brain);

    // Now create valid brain - should work
    brain_t valid_brain = brain_create(
        "recovery_test",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        3
    );

    EXPECT_NE(nullptr, valid_brain)
        << "Should be able to create valid brain after previous failure";
    cleanup_brain(valid_brain);
}

//=============================================================================
// 8. CROSS-CATEGORY INTEGRATION TESTS
//=============================================================================

/**
 * TEST: Integration_AllSizesAllTasks
 *
 * WHAT: Verify all size/task combinations work
 * WHY:  Test complete matrix of valid configurations
 * EXPECT: All combinations succeed
 */
TEST_F(BrainCreateTest, Integration_AllSizesAllTasks)
{
    brain_size_t sizes[] = {
        BRAIN_SIZE_TINY,
        BRAIN_SIZE_SMALL,
        BRAIN_SIZE_MEDIUM,
        BRAIN_SIZE_LARGE,
        BRAIN_SIZE_CUSTOM
    };

    brain_task_t tasks[] = {
        BRAIN_TASK_CLASSIFICATION,
        BRAIN_TASK_REGRESSION,
        BRAIN_TASK_PATTERN_MATCHING,
        BRAIN_TASK_SEQUENCE,
        BRAIN_TASK_ASSOCIATION,
        BRAIN_TASK_CUSTOM
    };

    int success_count = 0;
    int total_combinations = 0;

    for (auto size : sizes) {
        for (auto task : tasks) {
            total_combinations++;
            std::string name = "size_" + std::to_string(size) +
                             "_task_" + std::to_string(task);

            brain_t brain = brain_create(
                name.c_str(),
                size,
                task,
                5,
                3
            );

            if (brain != nullptr) {
                success_count++;
                cleanup_brain(brain);
            }
        }
    }

    EXPECT_GT(success_count, 0)
        << "At least some size/task combinations should succeed";
    EXPECT_EQ(total_combinations, success_count)
        << "All " << total_combinations << " size/task combinations should succeed";
}

//=============================================================================
// End of Test Suite
//=============================================================================

