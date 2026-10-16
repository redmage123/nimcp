/**
 * @file test_core_exception_macros.cpp
 * @brief Unit tests for core module exception macro integration
 *
 * WHAT: Tests for exception macros used in core brain/neural network modules
 * WHY:  Verify exception handling for brain creation, network operations, NaN detection
 * HOW:  GoogleTest framework with fixture setup/teardown for exception system
 *
 * TEST FOCUS:
 * - Brain exception macros (NIMCP_THROW_BRAIN)
 * - Neural network error handling
 * - Layer and weight initialization exceptions
 * - Learning divergence detection
 * - KG wiring integration
 *
 * @author NIMCP Development Team
 * @date 2025-01-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <cmath>

extern "C" {
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Globals for Handler Tracking
//=============================================================================

static std::atomic<int> g_handler_call_count{0};
static std::atomic<nimcp_error_t> g_last_error_code{NIMCP_SUCCESS};
static std::atomic<nimcp_exception_severity_t> g_last_severity{EXCEPTION_SEVERITY_DEBUG};
static std::atomic<nimcp_exception_type_t> g_last_exception_type{EXCEPTION_TYPE_BASE};
static std::atomic<bool> g_exception_presented_to_immune{false};

/**
 * @brief Test handler callback tracking exceptions
 */
static bool core_test_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex) {
        g_handler_call_count++;
        g_last_error_code = ex->code;
        g_last_severity = ex->severity;
        g_last_exception_type = ex->type;
        g_exception_presented_to_immune = ex->presented_to_immune;
    }
    return false;  // Don't consume
}

//=============================================================================
// Test Fixture for Core Exception Tests
//=============================================================================

/**
 * WHAT: Fixture for core module exception macro tests
 * WHY:  Setup/teardown exception system with brain-specific handler
 */
class CoreExceptionMacrosTest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg_ = nullptr;

    void SetUp() override {
        // Reset tracking
        g_handler_call_count = 0;
        g_last_error_code = NIMCP_SUCCESS;
        g_last_severity = EXCEPTION_SEVERITY_DEBUG;
        g_last_exception_type = EXCEPTION_TYPE_BASE;
        g_exception_presented_to_immune = false;

        // Initialize exception system
        nimcp_exception_system_init();

        // Register test handler for brain exceptions
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "core_test_handler";
        opts.handler = core_test_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        opts.category_filter = EXCEPTION_CATEGORY_BRAIN;
        handler_reg_ = nimcp_handler_register(&opts);

        // Also register a general handler
        nimcp_handler_options_t general_opts;
        nimcp_handler_default_options(&general_opts);
        general_opts.name = "general_handler";
        general_opts.handler = core_test_handler;
        general_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        nimcp_handler_register(&general_opts);
    }

    void TearDown() override {
        if (handler_reg_) {
            nimcp_handler_unregister(handler_reg_);
            handler_reg_ = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// Brain Exception Macro Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW_BRAIN creates brain-specific exception
 * WHY:  Verify brain exception type and fields
 */
TEST_F(CoreExceptionMacrosTest, ThrowBrainCreatesProperException) {
    uint32_t brain_id = 42;
    const char* region = "hippocampus";

    NIMCP_THROW_BRAIN(NIMCP_ERROR_BRAIN_INVALID, brain_id, region,
                      "Brain %u region '%s' is invalid", brain_id, region);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_BRAIN_INVALID);
    EXPECT_EQ(g_last_exception_type, EXCEPTION_TYPE_BRAIN);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test brain creation failure exception
 * WHY:  Verify proper handling of brain initialization errors
 */
TEST_F(CoreExceptionMacrosTest, BrainCreationFailureException) {
    uint32_t brain_id = 0;
    const char* region = "cortex";

    NIMCP_THROW_BRAIN(NIMCP_ERROR_BRAIN_CREATION, brain_id, region,
                      "Failed to create brain instance");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_BRAIN_CREATION);
    EXPECT_EQ(g_last_exception_type, EXCEPTION_TYPE_BRAIN);
}

/**
 * WHAT: Test neural network creation exception
 * WHY:  Verify network structure error handling
 */
TEST_F(CoreExceptionMacrosTest, NetworkCreationException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_NETWORK_CREATION, 1, "network_layer",
                      "Network creation failed: invalid layer configuration");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NETWORK_CREATION);
}

/**
 * WHAT: Test dimension mismatch exception
 * WHY:  Verify matrix/tensor dimension error handling
 */
TEST_F(CoreExceptionMacrosTest, DimensionMismatchException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_DIMENSION_MISMATCH, 1, "linear_layer",
                      "Dimension mismatch: expected %d, got %d", 256, 128);

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_DIMENSION_MISMATCH);
}

/**
 * WHAT: Test weight initialization exception
 * WHY:  Verify weight init error handling
 */
TEST_F(CoreExceptionMacrosTest, WeightInitializationException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_WEIGHT_INIT, 1, "conv_layer",
                      "Weight initialization failed: Xavier init returned NaN");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_WEIGHT_INIT);
}

/**
 * WHAT: Test forward pass exception
 * WHY:  Verify inference error handling
 */
TEST_F(CoreExceptionMacrosTest, ForwardPassException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_FORWARD_PASS, 2, "attention_head",
                      "Forward pass failed at layer %d", 3);

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_FORWARD_PASS);
}

/**
 * WHAT: Test backward pass exception
 * WHY:  Verify gradient computation error handling
 */
TEST_F(CoreExceptionMacrosTest, BackwardPassException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_BACKWARD_PASS, 2, "loss_layer",
                      "Backward pass failed: gradient overflow");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_BACKWARD_PASS);
}

/**
 * WHAT: Test learning failure exception
 * WHY:  Verify training step error handling
 */
TEST_F(CoreExceptionMacrosTest, LearningFailedException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_LEARNING_FAILED, 1, "optimizer",
                      "Learning step failed: loss = inf");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_LEARNING_FAILED);
}

/**
 * WHAT: Test inference failure exception
 * WHY:  Verify inference-specific error handling
 */
TEST_F(CoreExceptionMacrosTest, InferenceFailedException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_INFERENCE_FAILED, 3, "output_layer",
                      "Inference failed: invalid input shape");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INFERENCE_FAILED);
}

//=============================================================================
// Brain Region Error Macro Tests
//=============================================================================

/**
 * WHAT: Test hippocampus-specific exception
 * WHY:  Verify brain region error handling
 */
TEST_F(CoreExceptionMacrosTest, HippocampusErrorException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_HIPPOCAMPUS_ENCODING, 1, "hippocampus",
                      "Memory encoding failed");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_HIPPOCAMPUS_ENCODING);
}

/**
 * WHAT: Test motor cortex exception
 * WHY:  Verify motor region error handling
 */
TEST_F(CoreExceptionMacrosTest, MotorCortexException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_MOTOR_PLANNING, 1, "motor_cortex",
                      "Motor planning failed: trajectory infeasible");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_MOTOR_PLANNING);
}

/**
 * WHAT: Test prefrontal cortex exception
 * WHY:  Verify executive function error handling
 */
TEST_F(CoreExceptionMacrosTest, PrefrontalCortexException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_PREFRONTAL_WORKING_MEMORY, 1, "prefrontal",
                      "Working memory capacity exceeded");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_PREFRONTAL_WORKING_MEMORY);
}

/**
 * WHAT: Test amygdala exception
 * WHY:  Verify emotional processing error handling
 */
TEST_F(CoreExceptionMacrosTest, AmygdalaException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_AMYGDALA_FEAR_PROCESSING, 1, "amygdala",
                      "Fear response processing failed");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_AMYGDALA_FEAR_PROCESSING);
}

//=============================================================================
// NaN Detection and Learning Divergence Tests
//=============================================================================

/**
 * Helper function simulating NaN weight detection
 */
static nimcp_error_t check_weights_for_nan(float* weights, size_t count, uint32_t brain_id) {
    for (size_t i = 0; i < count; i++) {
        if (std::isnan(weights[i])) {
            NIMCP_THROW_BRAIN(NIMCP_ERROR_WEIGHT_INIT, brain_id, "weight_check",
                              "NaN detected in weights at index %zu", i);
            return NIMCP_ERROR_WEIGHT_INIT;
        }
    }
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test NaN weight detection exception
 * WHY:  Verify NaN detection triggers proper exception
 */
TEST_F(CoreExceptionMacrosTest, NaNWeightDetectionException) {
    float weights[] = {1.0f, 2.0f, NAN, 4.0f};
    uint32_t brain_id = 1;

    nimcp_error_t result = check_weights_for_nan(weights, 4, brain_id);

    EXPECT_EQ(result, NIMCP_ERROR_WEIGHT_INIT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_exception_type, EXCEPTION_TYPE_BRAIN);
}

/**
 * WHAT: Test valid weights pass without exception
 * WHY:  Verify no exception when weights are valid
 */
TEST_F(CoreExceptionMacrosTest, ValidWeightsNoException) {
    float weights[] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t brain_id = 1;

    nimcp_error_t result = check_weights_for_nan(weights, 4, brain_id);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * Helper function simulating gradient norm check
 */
static nimcp_error_t check_gradient_norm(float gradient_norm, float threshold, uint32_t brain_id) {
    if (std::isinf(gradient_norm)) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_BACKWARD_PASS, brain_id, "gradient_check",
                          "Gradient overflow: norm = inf");
        return NIMCP_ERROR_BACKWARD_PASS;
    }
    if (gradient_norm > threshold) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_LEARNING_FAILED, brain_id, "gradient_check",
                          "Gradient exploding: norm = %.2f > %.2f", gradient_norm, threshold);
        return NIMCP_ERROR_LEARNING_FAILED;
    }
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test gradient explosion detection
 * WHY:  Verify learning divergence is caught
 */
TEST_F(CoreExceptionMacrosTest, GradientExplosionDetection) {
    float gradient_norm = 1000.0f;
    float threshold = 100.0f;
    uint32_t brain_id = 1;

    nimcp_error_t result = check_gradient_norm(gradient_norm, threshold, brain_id);

    EXPECT_EQ(result, NIMCP_ERROR_LEARNING_FAILED);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test gradient overflow detection
 * WHY:  Verify infinite gradient is caught
 */
TEST_F(CoreExceptionMacrosTest, GradientOverflowDetection) {
    float gradient_norm = INFINITY;
    float threshold = 100.0f;
    uint32_t brain_id = 1;

    nimcp_error_t result = check_gradient_norm(gradient_norm, threshold, brain_id);

    EXPECT_EQ(result, NIMCP_ERROR_BACKWARD_PASS);
    EXPECT_EQ(g_handler_call_count, 1);
}

//=============================================================================
// Copy-on-Write and Clone Exception Tests
//=============================================================================

/**
 * WHAT: Test COW failure exception
 * WHY:  Verify copy-on-write error handling
 */
TEST_F(CoreExceptionMacrosTest, CopyOnWriteFailureException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_COW_FAILED, 1, "weights",
                      "Copy-on-write failed: insufficient memory");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_COW_FAILED);
}

/**
 * WHAT: Test brain clone failure exception
 * WHY:  Verify clone operation error handling
 */
TEST_F(CoreExceptionMacrosTest, BrainCloneFailureException) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_CLONE_FAILED, 1, "brain_clone",
                      "Brain clone failed: network state corrupted");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_CLONE_FAILED);
}

//=============================================================================
// KG Wiring Exception Tests
//=============================================================================

/**
 * WHAT: Test KG wiring creation exception
 * WHY:  Verify knowledge graph wiring error handling
 */
TEST_F(CoreExceptionMacrosTest, KGWiringCreationException) {
    NIMCP_THROW(NIMCP_ERROR_KG_WIRING_CREATE, "KG wiring creation failed");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_KG_WIRING_CREATE);
}

/**
 * WHAT: Test KG wiring input overflow exception
 * WHY:  Verify max inputs exceeded error handling
 */
TEST_F(CoreExceptionMacrosTest, KGWiringInputsFullException) {
    NIMCP_THROW(NIMCP_ERROR_KG_WIRING_INPUTS_FULL,
                "KG wiring inputs full: max 32 inputs reached");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_KG_WIRING_INPUTS_FULL);
}

/**
 * WHAT: Test KG wiring output overflow exception
 * WHY:  Verify max outputs exceeded error handling
 */
TEST_F(CoreExceptionMacrosTest, KGWiringOutputsFullException) {
    NIMCP_THROW(NIMCP_ERROR_KG_WIRING_OUTPUTS_FULL,
                "KG wiring outputs full: max 32 outputs reached");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_KG_WIRING_OUTPUTS_FULL);
}

/**
 * WHAT: Test KG wiring handler overflow exception
 * WHY:  Verify max handlers exceeded error handling
 */
TEST_F(CoreExceptionMacrosTest, KGWiringHandlersFullException) {
    NIMCP_THROW(NIMCP_ERROR_KG_WIRING_HANDLERS_FULL,
                "KG wiring handlers full: max 64 handlers reached");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_KG_WIRING_HANDLERS_FULL);
}

/**
 * WHAT: Test KG wiring validation exception
 * WHY:  Verify wiring validation error handling
 */
TEST_F(CoreExceptionMacrosTest, KGWiringValidationException) {
    NIMCP_THROW(NIMCP_ERROR_KG_WIRING_VALIDATION,
                "KG wiring validation failed: missing required input");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_KG_WIRING_VALIDATION);
}

//=============================================================================
// Combined Exception Pattern Tests
//=============================================================================

/**
 * Helper function demonstrating brain operation with multiple checks
 */
static nimcp_error_t brain_forward_pass(void* brain, void* input, void* output) {
    NIMCP_CHECK_THROW(brain != nullptr, NIMCP_ERROR_NULL_POINTER, "brain is NULL");
    NIMCP_CHECK_THROW(input != nullptr, NIMCP_ERROR_NULL_POINTER, "input is NULL");
    NIMCP_CHECK_THROW(output != nullptr, NIMCP_ERROR_NULL_POINTER, "output is NULL");

    // Simulate forward pass
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test brain forward pass with NULL brain
 * WHY:  Verify first check failure
 */
TEST_F(CoreExceptionMacrosTest, BrainForwardPassNullBrain) {
    int input = 1, output = 0;
    nimcp_error_t result = brain_forward_pass(nullptr, &input, &output);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test brain forward pass with NULL input
 * WHY:  Verify second check failure
 */
TEST_F(CoreExceptionMacrosTest, BrainForwardPassNullInput) {
    int brain = 1, output = 0;
    nimcp_error_t result = brain_forward_pass(&brain, nullptr, &output);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test brain forward pass with valid arguments
 * WHY:  Verify success path
 */
TEST_F(CoreExceptionMacrosTest, BrainForwardPassSuccess) {
    int brain = 1, input = 1, output = 0;
    nimcp_error_t result = brain_forward_pass(&brain, &input, &output);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Critical Brain Error Tests
//=============================================================================

/**
 * WHAT: Test critical brain error escalation
 * WHY:  Verify critical errors use correct severity
 */
TEST_F(CoreExceptionMacrosTest, CriticalBrainError) {
    NIMCP_THROW_CRITICAL(NIMCP_ERROR_BRAIN_INVALID,
                         "Critical brain state corruption detected");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_BRAIN_INVALID);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test fatal brain error
 * WHY:  Verify fatal errors trigger emergency response
 */
TEST_F(CoreExceptionMacrosTest, FatalBrainError) {
    NIMCP_THROW_FATAL(NIMCP_ERROR_MEMORY_CORRUPTION,
                      "Fatal: brain memory corruption, emergency shutdown");

    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_MEMORY_CORRUPTION);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_FATAL);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

//=============================================================================
// Recovery Pattern Tests for Brain Exceptions
//=============================================================================

static std::atomic<int> g_recovery_callback_count{0};
static std::atomic<nimcp_exception_recovery_action_t> g_last_recovery_action{EXCEPTION_RECOVERY_NONE};

static int brain_recovery_callback(nimcp_exception_t* ex,
                                    nimcp_exception_recovery_action_t action,
                                    void* user_data) {
    (void)ex;
    (void)user_data;
    g_recovery_callback_count++;
    g_last_recovery_action = action;
    return 0;
}

/**
 * WHAT: Fixture for brain recovery tests
 * WHY:  Setup recovery callbacks for brain errors
 */
class BrainRecoveryTest : public CoreExceptionMacrosTest {
protected:
    void SetUp() override {
        CoreExceptionMacrosTest::SetUp();
        g_recovery_callback_count = 0;
        g_last_recovery_action = EXCEPTION_RECOVERY_NONE;

        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, brain_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RESTART_COMPONENT, brain_recovery_callback, nullptr);
    }

    void TearDown() override {
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RESTART_COMPONENT);
        CoreExceptionMacrosTest::TearDown();
    }
};

/**
 * WHAT: Test brain exception with rollback recovery
 * WHY:  Verify brain errors can trigger checkpoint rollback
 */
TEST_F(BrainRecoveryTest, BrainExceptionWithRollback) {
    NIMCP_THROW_AND_RECOVER(NIMCP_ERROR_LEARNING_FAILED, EXCEPTION_RECOVERY_ROLLBACK,
                            "Learning diverged, rolling back to checkpoint");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_recovery_callback_count, 1);
    EXPECT_EQ(g_last_recovery_action, EXCEPTION_RECOVERY_ROLLBACK);
}

/**
 * WHAT: Test brain exception with component restart
 * WHY:  Verify brain module can be restarted on error
 */
TEST_F(BrainRecoveryTest, BrainExceptionWithComponentRestart) {
    NIMCP_THROW_AND_RECOVER(NIMCP_ERROR_BRAIN_INVALID, EXCEPTION_RECOVERY_RESTART_COMPONENT,
                            "Brain state invalid, restarting component");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_recovery_callback_count, 1);
    EXPECT_EQ(g_last_recovery_action, EXCEPTION_RECOVERY_RESTART_COMPONENT);
}

//=============================================================================
// Brain Exception Epitope Tests
//=============================================================================

/**
 * WHAT: Test brain exception epitope generation
 * WHY:  Verify immune system can identify brain error patterns
 */
TEST_F(CoreExceptionMacrosTest, BrainExceptionEpitopeGeneration) {
    nimcp_brain_exception_t* bex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_INVALID,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1,  // brain_id
        "hippocampus",
        "Test brain exception"
    );
    ASSERT_NE(bex, nullptr);

    size_t epitope_len = nimcp_exception_generate_epitope((nimcp_exception_t*)bex);

    EXPECT_GT(epitope_len, 0u);
    EXPECT_LE(epitope_len, NIMCP_EXCEPTION_EPITOPE_SIZE);

    nimcp_exception_unref((nimcp_exception_t*)bex);
}

/**
 * WHAT: Test brain exceptions from same region have similar epitopes
 * WHY:  Verify immune pattern matching works for brain regions
 */
TEST_F(CoreExceptionMacrosTest, SameBrainRegionSimilarEpitope) {
    nimcp_brain_exception_t* bex1 = nimcp_brain_exception_create(
        NIMCP_ERROR_HIPPOCAMPUS_ENCODING,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1, "hippocampus", "Error 1"
    );
    nimcp_brain_exception_t* bex2 = nimcp_brain_exception_create(
        NIMCP_ERROR_HIPPOCAMPUS_ENCODING,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1, "hippocampus", "Error 2"
    );

    ASSERT_NE(bex1, nullptr);
    ASSERT_NE(bex2, nullptr);

    size_t len1 = nimcp_exception_generate_epitope((nimcp_exception_t*)bex1);
    size_t len2 = nimcp_exception_generate_epitope((nimcp_exception_t*)bex2);

    EXPECT_EQ(len1, len2);

    // Check first bytes match (error code encoded)
    int match_count = 0;
    for (size_t i = 0; i < 8 && i < len1; i++) {
        if (bex1->base.epitope[i] == bex2->base.epitope[i]) {
            match_count++;
        }
    }
    EXPECT_GE(match_count, 4);

    nimcp_exception_unref((nimcp_exception_t*)bex1);
    nimcp_exception_unref((nimcp_exception_t*)bex2);
}

//=============================================================================
// Handler Filtering Tests
//=============================================================================

static std::atomic<int> g_brain_handler_count{0};
static std::atomic<int> g_generic_handler_count{0};

static bool brain_only_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex && ex->category == EXCEPTION_CATEGORY_BRAIN) {
        g_brain_handler_count++;
    }
    return false;
}

static bool generic_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    (void)ex;
    g_generic_handler_count++;
    return false;
}

/**
 * WHAT: Fixture for handler filtering tests
 * WHY:  Test category-specific handler registration
 */
class HandlerFilteringTest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* brain_handler_reg_ = nullptr;
    nimcp_handler_registration_t* generic_handler_reg_ = nullptr;

    void SetUp() override {
        g_brain_handler_count = 0;
        g_generic_handler_count = 0;

        nimcp_exception_system_init();

        // Brain-only handler
        nimcp_handler_options_t brain_opts;
        nimcp_handler_default_options(&brain_opts);
        brain_opts.name = "brain_only_handler";
        brain_opts.handler = brain_only_handler;
        brain_opts.category_filter = EXCEPTION_CATEGORY_BRAIN;
        brain_handler_reg_ = nimcp_handler_register(&brain_opts);

        // Generic handler (all categories)
        nimcp_handler_options_t generic_opts;
        nimcp_handler_default_options(&generic_opts);
        generic_opts.name = "generic_handler";
        generic_opts.handler = generic_handler;
        generic_handler_reg_ = nimcp_handler_register(&generic_opts);
    }

    void TearDown() override {
        if (brain_handler_reg_) nimcp_handler_unregister(brain_handler_reg_);
        if (generic_handler_reg_) nimcp_handler_unregister(generic_handler_reg_);
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

/**
 * WHAT: Test brain exception triggers brain handler
 * WHY:  Verify category filtering works
 */
TEST_F(HandlerFilteringTest, BrainExceptionTriggersBrainHandler) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_BRAIN_INVALID, 1, "test", "Brain error");

    EXPECT_EQ(g_brain_handler_count, 1);
    EXPECT_EQ(g_generic_handler_count, 1);  // Generic also sees it
}

/**
 * WHAT: Test non-brain exception doesn't trigger brain handler
 * WHY:  Verify category filtering excludes wrong categories
 */
TEST_F(HandlerFilteringTest, MemoryExceptionDoesNotTriggerBrainHandler) {
    NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 1024, "Memory error");

    EXPECT_EQ(g_brain_handler_count, 0);  // Brain handler filtered out
    EXPECT_EQ(g_generic_handler_count, 1);  // Generic sees it
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
