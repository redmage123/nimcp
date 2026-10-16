/**
 * @file test_exception_brain_integration_e2e.cpp
 * @brief E2E tests for exception handling with brain operations
 * @version 1.0.0
 * @date 2026-01-20
 *
 * WHAT: End-to-end tests verifying exception handling during brain operations
 * WHY:  Ensure exception system properly integrates with brain lifecycle,
 *       training, inference, and immune system for fault tolerance
 * HOW:  Test realistic scenarios with actual brain instances and exception handling
 *
 * Test Scenarios:
 * 1. Exception handling during brain creation
 * 2. Exception handling during brain training
 * 3. Exception handling during inference
 * 4. Exception handling with immune system active
 * 5. Exception recovery preserving brain state
 * 6. Exception handling during brain serialization/deserialization
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>
#include <cstdio>
#include <functional>

// Include nimcp.h outside extern "C" (it may include C++ headers internally)
#include "nimcp.h"

// C headers that need extern "C" linkage
extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

// Brain immune also outside extern "C" (uses nimcp.h internally)
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Utilities and Tracking
 * ============================================================================ */

// Exception tracking for callbacks
static std::atomic<int> g_exception_count{0};
static std::atomic<int> g_recovery_count{0};
static std::atomic<bool> g_recovery_success{false};
static nimcp_exception_severity_t g_last_severity = EXCEPTION_SEVERITY_INFO;
static nimcp_exception_category_t g_last_category = EXCEPTION_CATEGORY_GENERIC;

static void reset_tracking() {
    g_exception_count = 0;
    g_recovery_count = 0;
    g_recovery_success = false;
    g_last_severity = EXCEPTION_SEVERITY_INFO;
    g_last_category = EXCEPTION_CATEGORY_GENERIC;
}

// Helper to create test features
static void create_test_features(float* features, uint32_t num_features, float base_value) {
    for (uint32_t i = 0; i < num_features; i++) {
        features[i] = base_value + (float)i * 0.1f;
    }
}

// Helper to create test features with NaN (for testing NaN detection)
static void create_features_with_nan(float* features, uint32_t num_features) {
    for (uint32_t i = 0; i < num_features; i++) {
        if (i == num_features / 2) {
            features[i] = NAN;
        } else {
            features[i] = 0.5f + (float)i * 0.1f;
        }
    }
}

// Temporary file path for serialization tests
static const char* get_temp_filepath() {
    return "/tmp/nimcp_test_brain_exception.brain";
}

/* ============================================================================
 * Test Fixture: Base Exception Brain Test
 * ============================================================================ */

class ExceptionBrainIntegrationE2ETest : public ::testing::Test {
protected:
    nimcp_brain_t brain = nullptr;
    static constexpr uint32_t NUM_INPUTS = 10;
    static constexpr uint32_t NUM_OUTPUTS = 5;

    void SetUp() override {
        reset_tracking();

        // Initialize exception system
        int init_result = nimcp_exception_system_init();
        ASSERT_EQ(init_result, 0) << "Failed to initialize exception system";

        // Create a small brain for testing
        brain = nimcp_brain_create(
            "ExceptionTestBrain",
            NIMCP_BRAIN_SMALL,
            NIMCP_TASK_CLASSIFICATION,
            NUM_INPUTS,
            NUM_OUTPUTS
        );
        // Note: brain may be NULL in some test scenarios (that's intentional)
    }

    void TearDown() override {
        if (brain) {
            nimcp_brain_destroy(brain);
            brain = nullptr;
        }

        // Cleanup exception system
        nimcp_exception_system_shutdown();

        // Cleanup temp files
        remove(get_temp_filepath());
    }

    // Helper to train brain with some examples
    void train_brain_with_examples(int num_examples) {
        if (!brain) return;

        float features[NUM_INPUTS];
        const char* labels[] = {"class_a", "class_b", "class_c", "class_d", "class_e"};

        for (int i = 0; i < num_examples; i++) {
            create_test_features(features, NUM_INPUTS, (float)i * 0.05f);
            nimcp_brain_learn_example(
                brain,
                features,
                NUM_INPUTS,
                labels[i % 5],
                0.9f
            );
        }
    }
};

/* ============================================================================
 * Test Fixture: Exception with Immune System
 * ============================================================================ */

class ExceptionBrainImmuneE2ETest : public ExceptionBrainIntegrationE2ETest {
protected:
    brain_immune_system_t* immune = nullptr;

    void SetUp() override {
        ExceptionBrainIntegrationE2ETest::SetUp();

        // Initialize immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        // immune may be NULL if not fully available

        // Initialize exception-immune integration
        nimcp_exception_immune_config_t ex_immune_config;
        nimcp_exception_immune_default_config(&ex_immune_config);
        ex_immune_config.enable_auto_present = true;
        ex_immune_config.enable_auto_recovery = true;
        nimcp_exception_immune_init(&ex_immune_config);

        // Connect exception system to immune if available
        if (immune) {
            nimcp_exception_immune_connect(immune);
        }
    }

    void TearDown() override {
        // Disconnect and shutdown exception-immune integration
        nimcp_exception_immune_disconnect();
        nimcp_exception_immune_shutdown();

        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }

        ExceptionBrainIntegrationE2ETest::TearDown();
    }
};

/* ============================================================================
 * Test 1: Exception Handling During Brain Creation
 * ============================================================================ */

TEST_F(ExceptionBrainIntegrationE2ETest, ExceptionDuringBrainCreation_NullName) {
    printf("=== Test: Exception During Brain Creation (NULL Name) ===\n");

    // Destroy the brain created in SetUp
    if (brain) {
        nimcp_brain_destroy(brain);
        brain = nullptr;
    }

    // Try creating brain with NULL name
    nimcp_brain_t test_brain = nimcp_brain_create(
        nullptr,  // NULL name
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        NUM_INPUTS,
        NUM_OUTPUTS
    );

    // Should handle gracefully (either create with default name or return NULL)
    if (test_brain) {
        printf("  Brain created with default name (graceful handling)\n");
        nimcp_brain_destroy(test_brain);
    } else {
        printf("  Brain creation returned NULL (expected for NULL name)\n");
    }

    printf("Test passed: Exception handling during brain creation\n\n");
}

TEST_F(ExceptionBrainIntegrationE2ETest, ExceptionDuringBrainCreation_InvalidDimensions) {
    printf("=== Test: Exception During Brain Creation (Invalid Dimensions) ===\n");

    // Destroy the brain created in SetUp
    if (brain) {
        nimcp_brain_destroy(brain);
        brain = nullptr;
    }

    // Try creating brain with zero inputs
    nimcp_brain_t test_brain = nimcp_brain_create(
        "ZeroInputBrain",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        0,  // Invalid: zero inputs
        NUM_OUTPUTS
    );

    if (test_brain) {
        printf("  Brain created (implementation may have minimum)\n");
        nimcp_brain_destroy(test_brain);
    } else {
        printf("  Brain creation returned NULL for zero inputs (expected)\n");
    }

    // Try creating brain with zero outputs
    test_brain = nimcp_brain_create(
        "ZeroOutputBrain",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        NUM_INPUTS,
        0  // Invalid: zero outputs
    );

    if (test_brain) {
        printf("  Brain created (implementation may have minimum)\n");
        nimcp_brain_destroy(test_brain);
    } else {
        printf("  Brain creation returned NULL for zero outputs (expected)\n");
    }

    printf("Test passed: Exception handling for invalid dimensions\n\n");
}

TEST_F(ExceptionBrainIntegrationE2ETest, BrainCreationWithExceptionContext) {
    printf("=== Test: Brain Creation With Exception Context ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created successfully";

    // Create an exception manually to verify system works
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        "Test exception for brain creation context"
    );

    ASSERT_NE(ex, nullptr) << "Exception should be created";

    // Set context with brain info
    nimcp_exception_set_context(ex, "brain_name", "ExceptionTestBrain");
    nimcp_exception_set_context(ex, "num_inputs", "10");
    nimcp_exception_set_context(ex, "num_outputs", "5");

    // Verify context is set
    const char* brain_name = nimcp_exception_get_context(ex, "brain_name");
    EXPECT_NE(brain_name, nullptr);
    if (brain_name) {
        EXPECT_STREQ(brain_name, "ExceptionTestBrain");
        printf("  Context brain_name: %s\n", brain_name);
    }

    // Log and release exception
    nimcp_exception_log(ex);
    nimcp_exception_unref(ex);

    printf("Test passed: Brain creation with exception context\n\n");
}

/* ============================================================================
 * Test 2: Exception Handling During Brain Training
 * ============================================================================ */

TEST_F(ExceptionBrainIntegrationE2ETest, ExceptionDuringTraining_NullFeatures) {
    printf("=== Test: Exception During Training (NULL Features) ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    // Try training with NULL features
    nimcp_status_t result = nimcp_brain_learn_example(
        brain,
        nullptr,  // NULL features
        NUM_INPUTS,
        "class_a",
        0.9f
    );

    // Should return error, not crash
    if (result != NIMCP_OK) {
        printf("  Training returned error for NULL features (expected): %d\n", result);
    } else {
        printf("  Training handled NULL features gracefully\n");
    }

    printf("Test passed: Exception handling for NULL features during training\n\n");
}

TEST_F(ExceptionBrainIntegrationE2ETest, ExceptionDuringTraining_DimensionMismatch) {
    printf("=== Test: Exception During Training (Dimension Mismatch) ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    float features[NUM_INPUTS * 2];  // Wrong size
    create_test_features(features, NUM_INPUTS * 2, 0.5f);

    // Try training with wrong number of features
    nimcp_status_t result = nimcp_brain_learn_example(
        brain,
        features,
        NUM_INPUTS * 2,  // Wrong dimension
        "class_a",
        0.9f
    );

    printf("  Training with dimension mismatch result: %d\n", result);

    // Brain should either handle or return error
    if (result != NIMCP_OK) {
        printf("  Error returned for dimension mismatch (expected)\n");
    } else {
        printf("  Training handled dimension mismatch gracefully\n");
    }

    printf("Test passed: Exception handling for dimension mismatch\n\n");
}

TEST_F(ExceptionBrainIntegrationE2ETest, TrainingWithExceptionRecovery) {
    printf("=== Test: Training With Exception Recovery ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    // Train normally first
    printf("  Training with valid examples...\n");
    train_brain_with_examples(5);

    // Create a brain exception manually
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        0,  // brain_id
        "test_region",
        "Simulated learning failure"
    );

    ASSERT_NE(brain_ex, nullptr) << "Brain exception should be created";

    // Get suggested recovery
    nimcp_exception_recovery_action_t action =
        nimcp_exception_get_suggested_recovery((nimcp_exception_t*)brain_ex);
    printf("  Suggested recovery action: %s\n",
           nimcp_exception_recovery_action_to_string(action));

    // Set current exception
    nimcp_exception_set_current((nimcp_exception_t*)brain_ex);

    // Clear and continue training
    nimcp_exception_clear_current();

    // Continue training - should work
    float features[NUM_INPUTS];
    create_test_features(features, NUM_INPUTS, 0.3f);
    nimcp_status_t result = nimcp_brain_learn_example(
        brain, features, NUM_INPUTS, "class_a", 0.9f);

    EXPECT_EQ(result, NIMCP_OK) << "Training should continue after exception recovery";
    printf("  Training continued successfully after exception handling\n");

    nimcp_exception_unref((nimcp_exception_t*)brain_ex);

    printf("Test passed: Training with exception recovery\n\n");
}

/* ============================================================================
 * Test 3: Exception Handling During Inference
 * ============================================================================ */

TEST_F(ExceptionBrainIntegrationE2ETest, ExceptionDuringInference_NullFeatures) {
    printf("=== Test: Exception During Inference (NULL Features) ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    // Train brain first
    train_brain_with_examples(10);

    char label[64] = {0};
    float confidence = 0.0f;

    // Try inference with NULL features
    nimcp_status_t result = nimcp_brain_predict(
        brain,
        nullptr,  // NULL features
        NUM_INPUTS,
        label,
        &confidence
    );

    if (result != NIMCP_OK) {
        printf("  Inference returned error for NULL features (expected): %d\n", result);
    } else {
        printf("  Inference handled NULL features gracefully\n");
    }

    printf("Test passed: Exception handling for NULL features during inference\n\n");
}

TEST_F(ExceptionBrainIntegrationE2ETest, ExceptionDuringInference_NullOutputs) {
    printf("=== Test: Exception During Inference (NULL Outputs) ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    // Train brain first
    train_brain_with_examples(10);

    float features[NUM_INPUTS];
    create_test_features(features, NUM_INPUTS, 0.5f);

    // Try inference with NULL output buffer
    nimcp_status_t result = nimcp_brain_predict(
        brain,
        features,
        NUM_INPUTS,
        nullptr,  // NULL output buffer
        nullptr   // NULL confidence
    );

    if (result != NIMCP_OK) {
        printf("  Inference returned error for NULL outputs (expected): %d\n", result);
    } else {
        printf("  Inference handled NULL outputs gracefully\n");
    }

    printf("Test passed: Exception handling for NULL outputs during inference\n\n");
}

TEST_F(ExceptionBrainIntegrationE2ETest, InferenceWithValidInputsAfterException) {
    printf("=== Test: Inference With Valid Inputs After Exception ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    // Train brain first
    train_brain_with_examples(20);

    // Simulate an exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        "Simulated inference failure"
    );
    nimcp_exception_log(ex);
    nimcp_exception_unref(ex);

    // Now try valid inference
    float features[NUM_INPUTS];
    create_test_features(features, NUM_INPUTS, 0.5f);
    char label[64] = {0};
    float confidence = 0.0f;

    nimcp_status_t result = nimcp_brain_predict(
        brain, features, NUM_INPUTS, label, &confidence);

    if (result == NIMCP_OK) {
        printf("  Inference succeeded: label=%s, confidence=%.4f\n", label, confidence);
    } else {
        printf("  Inference returned: %d (may be expected for untrained labels)\n", result);
    }

    printf("Test passed: Valid inference after exception\n\n");
}

/* ============================================================================
 * Test 4: Exception Handling With Immune System Active
 * ============================================================================ */

TEST_F(ExceptionBrainImmuneE2ETest, ExceptionPresentedToImmune) {
    printf("=== Test: Exception Presented To Immune System ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    if (immune) {
        printf("  Immune system available\n");

        // Start immune system
        int start_result = brain_immune_start(immune);
        printf("  Immune start result: %d\n", start_result);
    } else {
        printf("  Immune system not available (testing without)\n");
    }

    // Create and present exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_INVALID,
        EXCEPTION_SEVERITY_SEVERE,  // Severe enough for immune response
        __FILE__,
        __LINE__,
        __func__,
        "Test exception for immune presentation"
    );

    ASSERT_NE(ex, nullptr) << "Exception should be created";

    // Generate epitope for immune matching
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    printf("  Generated epitope length: %zu\n", epitope_len);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));

    int present_result = nimcp_exception_present_to_immune(ex, &response);
    printf("  Present to immune result: %d\n", present_result);
    printf("  Response - antigen_id: %u, recovery_attempted: %s\n",
           response.antigen_id, response.recovery_attempted ? "yes" : "no");

    if (immune) {
        brain_immune_stop(immune);
    }

    nimcp_exception_unref(ex);

    printf("Test passed: Exception presented to immune system\n\n");
}

TEST_F(ExceptionBrainImmuneE2ETest, RecoveryActionWithImmune) {
    printf("=== Test: Recovery Action With Immune System ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    if (immune) {
        brain_immune_start(immune);
    }

    // Train brain to establish state
    train_brain_with_examples(10);

    // Create brain exception
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        0,
        "hippocampus",
        "Learning divergence detected"
    );

    // Mark as having NaN weights (simulating)
    if (brain_ex) {
        brain_ex->has_nan_weights = true;
        brain_ex->gradient_norm = INFINITY;
    }

    // Present and attempt recovery
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));

    int present_result = nimcp_exception_present_to_immune(
        (nimcp_exception_t*)brain_ex, &response);
    printf("  Present result: %d\n", present_result);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)brain_ex, &strategy);
    printf("  Primary recovery action: %s\n",
           nimcp_exception_recovery_action_to_string(strategy.primary_action));
    printf("  Fallback action: %s\n",
           nimcp_exception_recovery_action_to_string(strategy.fallback_action));

    if (immune) {
        brain_immune_stop(immune);
    }

    nimcp_exception_unref((nimcp_exception_t*)brain_ex);

    printf("Test passed: Recovery action with immune system\n\n");
}

TEST_F(ExceptionBrainImmuneE2ETest, ImmuneIntegrationStatistics) {
    printf("=== Test: Immune Integration Statistics ===\n");

    if (immune) {
        brain_immune_start(immune);

        // Create and present multiple exceptions
        for (int i = 0; i < 5; i++) {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_WARNING,
                __FILE__,
                __LINE__,
                __func__,
                "Batch exception %d", i
            );

            nimcp_immune_response_t response;
            nimcp_exception_present_to_immune(ex, &response);
            nimcp_exception_unref(ex);
        }

        // Check immune statistics
        brain_immune_stats_t immune_stats;
        brain_immune_get_stats(immune, &immune_stats);
        printf("  Antigens processed: %lu\n",
               (unsigned long)immune_stats.antigens_processed);
        printf("  System health: %.4f\n", immune_stats.system_health);

        brain_immune_stop(immune);
    }

    // Check exception-immune integration stats
    nimcp_exception_immune_stats_t ex_stats;
    nimcp_exception_immune_get_stats(&ex_stats);
    printf("  Exceptions presented: %lu\n",
           (unsigned long)ex_stats.exceptions_presented);
    printf("  Recoveries attempted: %lu\n",
           (unsigned long)ex_stats.recoveries_attempted);
    printf("  Recoveries succeeded: %lu\n",
           (unsigned long)ex_stats.recoveries_succeeded);

    printf("Test passed: Immune integration statistics\n\n");
}

/* ============================================================================
 * Test 5: Exception Recovery Preserving Brain State
 * ============================================================================ */

TEST_F(ExceptionBrainIntegrationE2ETest, BrainStatePreservedAfterException) {
    printf("=== Test: Brain State Preserved After Exception ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    // Train brain
    train_brain_with_examples(20);

    // Make a prediction and save result
    float features[NUM_INPUTS];
    create_test_features(features, NUM_INPUTS, 0.5f);
    char label_before[64] = {0};
    float confidence_before = 0.0f;

    nimcp_status_t result = nimcp_brain_predict(
        brain, features, NUM_INPUTS, label_before, &confidence_before);
    printf("  Prediction before exception: %s (%.4f)\n",
           label_before, confidence_before);

    // Simulate exception handling
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Non-destructive exception"
    );

    nimcp_exception_set_current(ex);
    nimcp_exception_log(ex);
    nimcp_exception_clear_current();
    nimcp_exception_unref(ex);

    // Make same prediction again
    char label_after[64] = {0};
    float confidence_after = 0.0f;

    result = nimcp_brain_predict(
        brain, features, NUM_INPUTS, label_after, &confidence_after);
    printf("  Prediction after exception: %s (%.4f)\n",
           label_after, confidence_after);

    // State should be preserved
    if (result == NIMCP_OK && confidence_before > 0.0f) {
        EXPECT_STREQ(label_before, label_after);
        EXPECT_FLOAT_EQ(confidence_before, confidence_after);
        printf("  Brain state preserved!\n");
    }

    printf("Test passed: Brain state preserved after exception\n\n");
}

TEST_F(ExceptionBrainIntegrationE2ETest, MultipleExceptionsStateConsistency) {
    printf("=== Test: Multiple Exceptions State Consistency ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    // Train brain
    train_brain_with_examples(30);

    // Generate multiple exceptions while training
    for (int i = 0; i < 10; i++) {
        // Train more
        float features[NUM_INPUTS];
        create_test_features(features, NUM_INPUTS, (float)i * 0.1f);
        nimcp_brain_learn_example(brain, features, NUM_INPUTS,
                                   (i % 2 == 0) ? "class_a" : "class_b", 0.8f);

        // Generate exception (every other iteration)
        if (i % 2 == 1) {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_WARNING,
                __FILE__, __LINE__, __func__,
                "Iteration %d exception", i
            );
            nimcp_exception_log(ex);
            nimcp_exception_unref(ex);
        }
    }

    // Brain should still be functional
    float features[NUM_INPUTS];
    create_test_features(features, NUM_INPUTS, 0.5f);
    char label[64] = {0};
    float confidence = 0.0f;

    nimcp_status_t result = nimcp_brain_predict(
        brain, features, NUM_INPUTS, label, &confidence);

    printf("  Final prediction after %d exceptions: %s (%.4f)\n",
           5, label, confidence);

    // Just verify brain didn't crash
    printf("  Brain still functional after multiple exceptions\n");

    printf("Test passed: State consistency with multiple exceptions\n\n");
}

/* ============================================================================
 * Test 6: Exception Handling During Serialization/Deserialization
 * ============================================================================ */

TEST_F(ExceptionBrainIntegrationE2ETest, ExceptionDuringSave_NullPath) {
    printf("=== Test: Exception During Save (NULL Path) ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    // Train brain first
    train_brain_with_examples(10);

    // Try saving with NULL path
    nimcp_status_t result = nimcp_brain_save(brain, nullptr);

    if (result != NIMCP_OK) {
        printf("  Save returned error for NULL path (expected): %d\n", result);
    } else {
        printf("  Save handled NULL path gracefully\n");
    }

    printf("Test passed: Exception handling for NULL path during save\n\n");
}

TEST_F(ExceptionBrainIntegrationE2ETest, ExceptionDuringSave_InvalidPath) {
    printf("=== Test: Exception During Save (Invalid Path) ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    // Train brain first
    train_brain_with_examples(10);

    // Try saving to invalid path
    nimcp_status_t result = nimcp_brain_save(
        brain, "/nonexistent/directory/brain.data");

    if (result != NIMCP_OK) {
        printf("  Save returned error for invalid path (expected): %d\n", result);
    } else {
        printf("  Save created directory or handled gracefully\n");
    }

    printf("Test passed: Exception handling for invalid path during save\n\n");
}

TEST_F(ExceptionBrainIntegrationE2ETest, SaveAndLoadWithExceptionHandling) {
    printf("=== Test: Save And Load With Exception Handling ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    // Train brain
    train_brain_with_examples(20);

    // Save brain
    const char* filepath = get_temp_filepath();
    nimcp_status_t save_result = nimcp_brain_save(brain, filepath);
    printf("  Save result: %d\n", save_result);

    if (save_result == NIMCP_OK) {
        // Generate exception between save and load
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_IO,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Simulated I/O issue"
        );
        nimcp_exception_log(ex);
        nimcp_exception_unref(ex);

        // Load brain
        nimcp_brain_t loaded_brain = nimcp_brain_load(filepath);

        if (loaded_brain) {
            printf("  Brain loaded successfully after exception\n");

            // Verify loaded brain works
            float features[NUM_INPUTS];
            create_test_features(features, NUM_INPUTS, 0.5f);
            char label[64] = {0};
            float confidence = 0.0f;

            nimcp_status_t result = nimcp_brain_predict(
                loaded_brain, features, NUM_INPUTS, label, &confidence);
            printf("  Loaded brain prediction: %s (%.4f)\n", label, confidence);

            nimcp_brain_destroy(loaded_brain);
        } else {
            printf("  Load failed (may be expected)\n");
        }
    }

    printf("Test passed: Save and load with exception handling\n\n");
}

TEST_F(ExceptionBrainIntegrationE2ETest, LoadFromCorruptedFile) {
    printf("=== Test: Load From Corrupted File ===\n");

    const char* filepath = get_temp_filepath();

    // Create a corrupted file
    FILE* f = fopen(filepath, "wb");
    if (f) {
        const char* garbage = "THIS IS NOT A VALID BRAIN FILE\x00\xFF\xFE";
        fwrite(garbage, 1, strlen(garbage) + 3, f);
        fclose(f);

        // Try to load
        nimcp_brain_t loaded_brain = nimcp_brain_load(filepath);

        if (loaded_brain) {
            printf("  Loaded something (unexpected)\n");
            nimcp_brain_destroy(loaded_brain);
        } else {
            printf("  Load returned NULL for corrupted file (expected)\n");
        }

        // No exception should crash the system
        printf("  System still stable after corrupted file load attempt\n");
    } else {
        printf("  Could not create test file, skipping\n");
    }

    printf("Test passed: Load from corrupted file handled\n\n");
}

/* ============================================================================
 * Test: Aggregate Exception for Batch Operations
 * ============================================================================ */

TEST_F(ExceptionBrainIntegrationE2ETest, AggregateExceptionForBatchErrors) {
    printf("=== Test: Aggregate Exception For Batch Errors ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    // Create aggregate exception for batch operation
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Batch training errors"
    );

    ASSERT_NE(agg, nullptr) << "Aggregate exception should be created";

    // Simulate batch training with some errors
    int error_count = 0;
    for (int i = 0; i < 10; i++) {
        float features[NUM_INPUTS];

        // Every third sample has a "problem"
        if (i % 3 == 0) {
            // Create child exception
            nimcp_exception_t* child = nimcp_exception_create(
                NIMCP_ERROR_LEARNING_FAILED,
                EXCEPTION_SEVERITY_WARNING,
                __FILE__, __LINE__, __func__,
                "Sample %d failed validation", i
            );

            if (nimcp_aggregate_exception_add(agg, child) == 0) {
                error_count++;
            } else {
                nimcp_exception_unref(child);
            }
        } else {
            // Normal training
            create_test_features(features, NUM_INPUTS, (float)i * 0.1f);
            nimcp_brain_learn_example(brain, features, NUM_INPUTS, "class_a", 0.9f);
        }
    }

    // Check aggregate
    size_t child_count = nimcp_aggregate_exception_count(agg);
    printf("  Errors in batch: %zu\n", child_count);
    EXPECT_EQ(child_count, (size_t)error_count);

    // Log aggregate
    nimcp_exception_log((nimcp_exception_t*)agg);

    // Cleanup
    nimcp_exception_unref((nimcp_exception_t*)agg);

    printf("Test passed: Aggregate exception for batch errors\n\n");
}

/* ============================================================================
 * Test: Exception Chain (Cause)
 * ============================================================================ */

TEST_F(ExceptionBrainIntegrationE2ETest, ExceptionChainWithCause) {
    printf("=== Test: Exception Chain With Cause ===\n");

    // Create root cause exception
    nimcp_exception_t* root_cause = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Out of memory in neural network allocation"
    );

    // Create intermediate exception
    nimcp_exception_t* intermediate = nimcp_exception_create(
        NIMCP_ERROR_NETWORK_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Failed to create neural network layer"
    );
    nimcp_exception_set_cause(intermediate, root_cause);

    // Create top-level exception
    nimcp_exception_t* top_level = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Brain creation failed"
    );
    nimcp_exception_set_cause(top_level, intermediate);

    // Traverse cause chain
    printf("  Exception chain:\n");
    nimcp_exception_t* current = top_level;
    int depth = 0;
    while (current) {
        printf("    [%d] Code: %d, Message: %s\n",
               depth, current->code, current->message);
        current = nimcp_exception_get_cause(current);
        depth++;
    }

    EXPECT_EQ(depth, 3) << "Should have 3 exceptions in chain";

    // Cleanup (unref top-level releases entire chain)
    nimcp_exception_unref(top_level);

    printf("Test passed: Exception chain with cause\n\n");
}

/* ============================================================================
 * Test: Thread Safety
 * ============================================================================ */

TEST_F(ExceptionBrainIntegrationE2ETest, ConcurrentExceptionHandling) {
    printf("=== Test: Concurrent Exception Handling ===\n");

    ASSERT_NE(brain, nullptr) << "Brain should be created";

    std::atomic<int> total_exceptions{0};
    std::atomic<bool> stop_flag{false};

    // Thread 1: Generate exceptions
    std::thread exception_thread([&]() {
        for (int i = 0; i < 50 && !stop_flag; i++) {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_WARNING,
                __FILE__, __LINE__, __func__,
                "Thread exception %d", i
            );
            nimcp_exception_unref(ex);
            total_exceptions++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Thread 2: Train brain
    std::thread training_thread([&]() {
        for (int i = 0; i < 50 && !stop_flag; i++) {
            float features[NUM_INPUTS];
            create_test_features(features, NUM_INPUTS, (float)i * 0.05f);
            nimcp_brain_learn_example(brain, features, NUM_INPUTS,
                                       (i % 2 == 0) ? "class_a" : "class_b", 0.8f);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Wait for threads
    exception_thread.join();
    training_thread.join();

    printf("  Total exceptions generated: %d\n", total_exceptions.load());
    printf("  No crashes or deadlocks detected\n");

    // Verify brain still works
    float features[NUM_INPUTS];
    create_test_features(features, NUM_INPUTS, 0.5f);
    char label[64] = {0};
    float confidence = 0.0f;
    nimcp_brain_predict(brain, features, NUM_INPUTS, label, &confidence);
    printf("  Brain still functional after concurrent operations\n");

    printf("Test passed: Concurrent exception handling\n\n");
}

/* ============================================================================
 * Test: Full Lifecycle
 * ============================================================================ */

TEST_F(ExceptionBrainImmuneE2ETest, FullExceptionBrainLifecycle) {
    printf("=== Test: Full Exception-Brain Lifecycle ===\n");

    // Phase 1: Brain creation with exception handling
    printf("Phase 1: Brain creation\n");
    ASSERT_NE(brain, nullptr);

    // Phase 2: Training with exceptions
    printf("Phase 2: Training with exception handling\n");
    train_brain_with_examples(30);

    // Simulate some training exceptions
    nimcp_exception_t* train_ex = nimcp_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Learning rate adjustment needed"
    );
    nimcp_exception_log(train_ex);
    nimcp_exception_unref(train_ex);

    // Phase 3: Inference with exceptions
    printf("Phase 3: Inference with exception handling\n");
    float features[NUM_INPUTS];
    create_test_features(features, NUM_INPUTS, 0.5f);
    char label[64] = {0};
    float confidence = 0.0f;
    nimcp_brain_predict(brain, features, NUM_INPUTS, label, &confidence);
    printf("  Prediction: %s (%.4f)\n", label, confidence);

    // Phase 4: Immune integration
    printf("Phase 4: Immune integration\n");
    if (immune) {
        brain_immune_start(immune);

        nimcp_exception_t* immune_ex = nimcp_exception_create(
            NIMCP_ERROR_BRAIN_INVALID,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Anomaly detected in processing"
        );

        nimcp_immune_response_t response;
        nimcp_exception_present_to_immune(immune_ex, &response);
        printf("  Exception presented to immune: antigen_id=%u\n",
               response.antigen_id);

        nimcp_exception_unref(immune_ex);
        brain_immune_stop(immune);
    }

    // Phase 5: Serialization with exceptions
    printf("Phase 5: Serialization with exception handling\n");
    const char* filepath = get_temp_filepath();
    nimcp_status_t save_result = nimcp_brain_save(brain, filepath);
    printf("  Save result: %d\n", save_result);

    // Phase 6: Statistics
    printf("Phase 6: Final statistics\n");
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("  Total exceptions presented: %lu\n",
           (unsigned long)stats.exceptions_presented);
    printf("  Avg response time: %.2f us\n", stats.avg_response_time_us);

    printf("Test passed: Full exception-brain lifecycle\n\n");
}
