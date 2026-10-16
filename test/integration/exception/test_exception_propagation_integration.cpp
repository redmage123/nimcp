/**
 * @file test_exception_propagation_integration.cpp
 * @brief Integration tests for cross-module exception propagation
 *
 * WHAT: Test exception propagation between different NIMCP modules
 * WHY:  Verify exceptions properly flow across module boundaries with
 *       context, cause chains, and aggregation preserved
 * HOW:  Create exceptions in different module contexts, propagate them,
 *       and verify all information is preserved through the pipeline
 *
 * TEST SCENARIOS:
 * - Memory exceptions propagating to cognitive layer
 * - Brain exceptions propagating to controller
 * - Exception cause chain preservation
 * - Aggregate exceptions collecting from multiple sources
 * - Context preservation during propagation
 * - Epitope consistency across modules
 *
 * @author NIMCP Development Team
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionPropagationIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<nimcp_exception_category_t> last_category;
    static std::vector<nimcp_error_t> received_exception_codes;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        last_category = EXCEPTION_CATEGORY_GENERIC;
        received_exception_codes.clear();

        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    /**
     * @brief Test handler that tracks received exceptions
     */
    static bool tracking_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_category = ex->category;
        received_exception_codes.push_back(ex->code);
        return false;  // Don't consume - allow other handlers to process
    }

    /**
     * @brief Helper to simulate memory module exception
     *
     * Creates a memory exception as if thrown from the memory subsystem
     */
    static nimcp_memory_exception_t* create_memory_module_exception(
        size_t requested_size,
        const char* allocator_name
    ) {
        nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            requested_size,
            "Memory allocation failed in %s: requested %zu bytes",
            allocator_name, requested_size
        );
        if (ex) {
            ex->allocator_name = allocator_name;
            ex->available_size = 1024;  // Simulated available memory
            ex->is_heap = true;
        }
        return ex;
    }

    /**
     * @brief Helper to simulate brain module exception
     *
     * Creates a brain exception as if thrown from the neural network layer
     */
    static nimcp_brain_exception_t* create_brain_module_exception(
        uint32_t brain_id,
        const char* region_name,
        bool has_nan
    ) {
        nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
            NIMCP_ERROR_LEARNING_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            brain_id,
            region_name,
            "Learning diverged in brain %u, region %s",
            brain_id, region_name
        );
        if (ex) {
            ex->has_nan_weights = has_nan;
            ex->learning_diverged = true;
            ex->gradient_norm = has_nan ? NAN : 1000.0f;
        }
        return ex;
    }

    /**
     * @brief Helper to simulate cognitive module exception
     */
    static nimcp_cognitive_exception_t* create_cognitive_module_exception(
        uint32_t module_id,
        const char* module_name
    ) {
        // Create cognitive exception manually since there's no direct create function
        nimcp_exception_t* base = nimcp_exception_create(
            NIMCP_ERROR_WORKING_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Cognitive processing failed in module %s (ID: %u)",
            module_name, module_id
        );
        return reinterpret_cast<nimcp_cognitive_exception_t*>(base);
    }
};

std::atomic<int> ExceptionPropagationIntegrationTest::handler_call_count(0);
std::atomic<int> ExceptionPropagationIntegrationTest::last_exception_code(0);
std::atomic<nimcp_exception_category_t> ExceptionPropagationIntegrationTest::last_category(EXCEPTION_CATEGORY_GENERIC);
std::vector<nimcp_error_t> ExceptionPropagationIntegrationTest::received_exception_codes;

//=============================================================================
// Memory Exception Propagation Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, MemoryExceptionPropagatesUp) {
    // WHAT: Test memory exception propagates from memory module to cognitive layer
    // WHY:  Memory failures in low-level allocators must bubble up to higher layers
    // HOW:  Create memory exception, wrap it as cause of cognitive exception

    // Simulate memory allocation failure in arena allocator
    nimcp_memory_exception_t* mem_ex = create_memory_module_exception(
        4096, "arena_allocator"
    );
    ASSERT_NE(mem_ex, nullptr);

    // Add context about the memory operation
    nimcp_exception_set_context(
        (nimcp_exception_t*)mem_ex, "operation", "tensor_allocation"
    );
    nimcp_exception_set_context(
        (nimcp_exception_t*)mem_ex, "layer", "memory_module"
    );

    // Verify memory exception properties
    EXPECT_EQ(mem_ex->base.code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(mem_ex->base.category, EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(mem_ex->requested_size, 4096u);

    // Create higher-level cognitive exception with memory exception as cause
    nimcp_exception_t* cognitive_ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Working memory update failed due to allocation failure"
    );
    ASSERT_NE(cognitive_ex, nullptr);

    // Chain the exceptions (memory is the cause)
    nimcp_exception_set_cause(cognitive_ex, (nimcp_exception_t*)mem_ex);

    // Verify cause chain
    nimcp_exception_t* cause = nimcp_exception_get_cause(cognitive_ex);
    ASSERT_NE(cause, nullptr);
    EXPECT_EQ(cause->code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(cause->type, EXCEPTION_TYPE_MEMORY);

    // Verify context preserved in cause
    const char* operation = nimcp_exception_get_context(cause, "operation");
    ASSERT_NE(operation, nullptr);
    EXPECT_STREQ(operation, "tensor_allocation");

    // Dispatch through handler chain
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "memory_propagation_handler";
    options.handler = tracking_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;
    nimcp_exception_dispatch(cognitive_ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_WORKING_MEMORY);

    if (reg) nimcp_handler_unregister(reg);
    nimcp_exception_unref(cognitive_ex);
    // Note: mem_ex is now owned by cognitive_ex and will be freed with it
}

//=============================================================================
// Brain Exception Propagation Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, BrainExceptionPropagatesUp) {
    // WHAT: Test brain exception propagates from neural layer to brain controller
    // WHY:  Neural network failures must be reported to controlling layer
    // HOW:  Create brain exception with NaN detection, propagate to controller

    // Simulate NaN weight detection in neural network
    nimcp_brain_exception_t* brain_ex = create_brain_module_exception(
        1, "hippocampus", true
    );
    ASSERT_NE(brain_ex, nullptr);

    // Add neural-specific context
    nimcp_exception_set_context(
        (nimcp_exception_t*)brain_ex, "layer_id", "42"
    );
    nimcp_exception_set_context(
        (nimcp_exception_t*)brain_ex, "operation", "forward_pass"
    );

    // Verify brain exception properties
    EXPECT_EQ(brain_ex->base.code, NIMCP_ERROR_LEARNING_FAILED);
    EXPECT_EQ(brain_ex->base.category, EXCEPTION_CATEGORY_BRAIN);
    EXPECT_TRUE(brain_ex->has_nan_weights);
    EXPECT_TRUE(brain_ex->learning_diverged);

    // Create controller-level exception with brain exception as cause
    nimcp_exception_t* controller_ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_INVALID,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Brain controller detected unstable neural network state"
    );
    ASSERT_NE(controller_ex, nullptr);

    nimcp_exception_set_cause(controller_ex, (nimcp_exception_t*)brain_ex);

    // Verify propagation preserves brain-specific data
    nimcp_exception_t* cause = nimcp_exception_get_cause(controller_ex);
    ASSERT_NE(cause, nullptr);
    EXPECT_EQ(cause->type, EXCEPTION_TYPE_BRAIN);

    nimcp_brain_exception_t* brain_cause = (nimcp_brain_exception_t*)cause;
    EXPECT_TRUE(brain_cause->has_nan_weights);
    EXPECT_EQ(brain_cause->brain_id, 1u);
    EXPECT_STREQ(brain_cause->region_name, "hippocampus");

    // Dispatch and verify handler receives it
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "brain_propagation_handler";
    options.handler = tracking_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;
    nimcp_exception_dispatch(controller_ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_BRAIN_INVALID);

    if (reg) nimcp_handler_unregister(reg);
    nimcp_exception_unref(controller_ex);
}

//=============================================================================
// Exception Cause Chain Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, ExceptionCauseChainPreserved) {
    // WHAT: Test multi-level exception cause chains are preserved
    // WHY:  Deep call stacks can produce chains of exceptions
    // HOW:  Create 3-level chain: memory -> brain -> controller

    // Level 1: Root cause - memory allocation failure
    nimcp_memory_exception_t* mem_ex = create_memory_module_exception(
        8192, "weight_allocator"
    );
    ASSERT_NE(mem_ex, nullptr);
    nimcp_exception_set_context((nimcp_exception_t*)mem_ex, "level", "1");

    // Level 2: Brain exception caused by memory failure
    nimcp_brain_exception_t* brain_ex = create_brain_module_exception(
        2, "prefrontal_cortex", false
    );
    ASSERT_NE(brain_ex, nullptr);
    nimcp_exception_set_context((nimcp_exception_t*)brain_ex, "level", "2");
    nimcp_exception_set_cause((nimcp_exception_t*)brain_ex, (nimcp_exception_t*)mem_ex);

    // Level 3: Top-level controller exception
    nimcp_exception_t* controller_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Brain operation failed with nested causes"
    );
    ASSERT_NE(controller_ex, nullptr);
    nimcp_exception_set_context(controller_ex, "level", "3");
    nimcp_exception_set_cause(controller_ex, (nimcp_exception_t*)brain_ex);

    // Walk the cause chain and verify each level
    nimcp_exception_t* current = controller_ex;
    int chain_depth = 0;

    while (current != nullptr) {
        chain_depth++;
        const char* level = nimcp_exception_get_context(current, "level");
        ASSERT_NE(level, nullptr);

        switch (chain_depth) {
            case 1:
                EXPECT_STREQ(level, "3");
                EXPECT_EQ(current->code, NIMCP_ERROR_OPERATION_FAILED);
                break;
            case 2:
                EXPECT_STREQ(level, "2");
                EXPECT_EQ(current->code, NIMCP_ERROR_LEARNING_FAILED);
                EXPECT_EQ(current->type, EXCEPTION_TYPE_BRAIN);
                break;
            case 3:
                EXPECT_STREQ(level, "1");
                EXPECT_EQ(current->code, NIMCP_ERROR_NO_MEMORY);
                EXPECT_EQ(current->type, EXCEPTION_TYPE_MEMORY);
                break;
        }

        current = nimcp_exception_get_cause(current);
    }

    EXPECT_EQ(chain_depth, 3);

    nimcp_exception_unref(controller_ex);
}

//=============================================================================
// Aggregate Exception Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, AggregateExceptionCollectsFromMultipleSources) {
    // WHAT: Test aggregate exceptions collect errors from multiple sources
    // WHY:  Batch operations may fail in multiple ways simultaneously
    // HOW:  Create aggregate exception, add children from different modules

    // Create aggregate exception
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Multiple failures during batch neural update"
    );
    ASSERT_NE(agg, nullptr);
    EXPECT_EQ(agg->base.type, EXCEPTION_TYPE_AGGREGATE);

    // Add memory exception as child
    nimcp_memory_exception_t* mem_ex = create_memory_module_exception(
        2048, "batch_allocator"
    );
    ASSERT_NE(mem_ex, nullptr);
    nimcp_exception_set_context((nimcp_exception_t*)mem_ex, "source", "memory_module");
    int result = nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)mem_ex);
    EXPECT_EQ(result, 0);

    // Add brain exception as child
    nimcp_brain_exception_t* brain_ex = create_brain_module_exception(
        3, "amygdala", true
    );
    ASSERT_NE(brain_ex, nullptr);
    nimcp_exception_set_context((nimcp_exception_t*)brain_ex, "source", "brain_module");
    result = nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)brain_ex);
    EXPECT_EQ(result, 0);

    // Add threading exception as child
    nimcp_threading_exception_t* thread_ex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        12345,
        "Deadlock detected in worker thread"
    );
    ASSERT_NE(thread_ex, nullptr);
    nimcp_exception_set_context((nimcp_exception_t*)thread_ex, "source", "threading_module");
    result = nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)thread_ex);
    EXPECT_EQ(result, 0);

    // Verify aggregate contains all children
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 3u);

    // Verify each child is retrievable with correct properties
    nimcp_exception_t* child0 = nimcp_aggregate_exception_get(agg, 0);
    ASSERT_NE(child0, nullptr);
    EXPECT_EQ(child0->type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(child0->code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_STREQ(nimcp_exception_get_context(child0, "source"), "memory_module");

    nimcp_exception_t* child1 = nimcp_aggregate_exception_get(agg, 1);
    ASSERT_NE(child1, nullptr);
    EXPECT_EQ(child1->type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(child1->code, NIMCP_ERROR_LEARNING_FAILED);
    EXPECT_STREQ(nimcp_exception_get_context(child1, "source"), "brain_module");

    nimcp_exception_t* child2 = nimcp_aggregate_exception_get(agg, 2);
    ASSERT_NE(child2, nullptr);
    EXPECT_EQ(child2->type, EXCEPTION_TYPE_THREADING);
    EXPECT_EQ(child2->code, NIMCP_ERROR_DEADLOCK);
    EXPECT_STREQ(nimcp_exception_get_context(child2, "source"), "threading_module");

    // Out of bounds returns NULL
    nimcp_exception_t* child3 = nimcp_aggregate_exception_get(agg, 3);
    EXPECT_EQ(child3, nullptr);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Exception Context Preservation Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, ExceptionContextPreservedDuringPropagation) {
    // WHAT: Test context key-value pairs survive propagation
    // WHY:  Debugging requires full context at all levels
    // HOW:  Add multiple context entries, propagate, verify all preserved

    // Create base exception with rich context
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Forward pass computation error"
    );
    ASSERT_NE(ex, nullptr);

    // Add multiple context entries
    EXPECT_EQ(nimcp_exception_set_context(ex, "module", "neural_network"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "layer_index", "7"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "operation", "matrix_multiply"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "input_shape", "[32, 512]"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "weight_shape", "[512, 256]"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "activation", "relu"), 0);

    // Verify context count
    EXPECT_EQ(nimcp_exception_context_count(ex), 6u);

    // Create wrapper exception
    nimcp_exception_t* wrapper = nimcp_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Inference pipeline failed"
    );
    ASSERT_NE(wrapper, nullptr);
    nimcp_exception_set_cause(wrapper, ex);

    // Verify all context preserved in cause
    nimcp_exception_t* cause = nimcp_exception_get_cause(wrapper);
    ASSERT_NE(cause, nullptr);

    EXPECT_STREQ(nimcp_exception_get_context(cause, "module"), "neural_network");
    EXPECT_STREQ(nimcp_exception_get_context(cause, "layer_index"), "7");
    EXPECT_STREQ(nimcp_exception_get_context(cause, "operation"), "matrix_multiply");
    EXPECT_STREQ(nimcp_exception_get_context(cause, "input_shape"), "[32, 512]");
    EXPECT_STREQ(nimcp_exception_get_context(cause, "weight_shape"), "[512, 256]");
    EXPECT_STREQ(nimcp_exception_get_context(cause, "activation"), "relu");

    // Non-existent key returns NULL
    EXPECT_EQ(nimcp_exception_get_context(cause, "nonexistent"), nullptr);

    nimcp_exception_unref(wrapper);
}

//=============================================================================
// Epitope Consistency Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, ExceptionEpitopeConsistentAcrossModules) {
    // WHAT: Test exception epitopes are consistent for same error patterns
    // WHY:  Immune system relies on consistent epitopes for pattern matching
    // HOW:  Create similar exceptions, verify epitopes match appropriately

    // Create two similar memory exceptions
    nimcp_memory_exception_t* mem_ex1 = create_memory_module_exception(
        4096, "test_allocator"
    );
    ASSERT_NE(mem_ex1, nullptr);

    nimcp_memory_exception_t* mem_ex2 = create_memory_module_exception(
        4096, "test_allocator"
    );
    ASSERT_NE(mem_ex2, nullptr);

    // Generate epitopes
    size_t len1 = nimcp_exception_generate_epitope((nimcp_exception_t*)mem_ex1);
    size_t len2 = nimcp_exception_generate_epitope((nimcp_exception_t*)mem_ex2);

    EXPECT_GT(len1, 0u);
    EXPECT_EQ(len1, len2);

    // Epitopes for same error type/code should be similar
    // (exact match depends on implementation - at minimum they should both be valid)
    EXPECT_EQ(mem_ex1->base.epitope_len, mem_ex2->base.epitope_len);

    // Verify epitope is used for immune presentation
    uint8_t epitope_copy[NIMCP_EXCEPTION_EPITOPE_SIZE];
    memcpy(epitope_copy, mem_ex1->base.epitope, mem_ex1->base.epitope_len);

    // Present to immune and verify epitope consistency
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)mem_ex1, &response);
    EXPECT_EQ(result, 0);

    // Epitope should not change after presentation
    EXPECT_EQ(memcmp(epitope_copy, mem_ex1->base.epitope, mem_ex1->base.epitope_len), 0);

    nimcp_exception_unref((nimcp_exception_t*)mem_ex1);
    nimcp_exception_unref((nimcp_exception_t*)mem_ex2);
}

//=============================================================================
// Cross-Module Propagation with Immune Response Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, CrossModulePropagationTriggersImmuneResponse) {
    // WHAT: Test cross-module propagation properly triggers immune response
    // WHY:  Immune system should respond based on highest severity in chain
    // HOW:  Create exception chain, present to immune, verify response

    // Create low-level I/O exception (root cause)
    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/data/model.bin",
        "Failed to read model weights from disk"
    );
    ASSERT_NE(io_ex, nullptr);

    // Create brain exception caused by I/O failure
    nimcp_brain_exception_t* brain_ex = create_brain_module_exception(
        4, "cerebellum", false
    );
    ASSERT_NE(brain_ex, nullptr);
    nimcp_exception_set_cause((nimcp_exception_t*)brain_ex, (nimcp_exception_t*)io_ex);

    // Create top-level critical exception
    nimcp_exception_t* critical_ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Brain initialization failed - unable to load model"
    );
    ASSERT_NE(critical_ex, nullptr);
    nimcp_exception_set_cause(critical_ex, (nimcp_exception_t*)brain_ex);

    // Get recovery strategy for the top-level exception
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(critical_ex, &strategy);

    // Critical brain exception should suggest appropriate recovery
    // (actual values depend on implementation)
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(critical_ex, &response);
    EXPECT_EQ(result, 0);

    // Verify exception is marked as presented
    EXPECT_TRUE(critical_ex->presented_to_immune);

    nimcp_exception_unref(critical_ex);
}

//=============================================================================
// Exception Type Polymorphism Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, ExceptionTypePolymorphismWorks) {
    // WHAT: Test C polymorphism works correctly for exception types
    // WHY:  All exception types should be castable to/from base type
    // HOW:  Create derived types, cast to base, verify type field preserved

    // Create various exception types
    nimcp_memory_exception_t* mem = create_memory_module_exception(1024, "test");
    nimcp_brain_exception_t* brain = create_brain_module_exception(1, "test", false);
    nimcp_io_exception_t* io = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_OPEN, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "/test/path", "Test I/O error"
    );
    nimcp_threading_exception_t* thread = nimcp_threading_exception_create(
        NIMCP_ERROR_THREAD_CREATE, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 999, "Test threading error"
    );

    ASSERT_NE(mem, nullptr);
    ASSERT_NE(brain, nullptr);
    ASSERT_NE(io, nullptr);
    ASSERT_NE(thread, nullptr);

    // Cast to base pointers
    nimcp_exception_t* base_mem = (nimcp_exception_t*)mem;
    nimcp_exception_t* base_brain = (nimcp_exception_t*)brain;
    nimcp_exception_t* base_io = (nimcp_exception_t*)io;
    nimcp_exception_t* base_thread = (nimcp_exception_t*)thread;

    // Verify type field allows correct identification
    EXPECT_EQ(base_mem->type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(base_brain->type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(base_io->type, EXCEPTION_TYPE_IO);
    EXPECT_EQ(base_thread->type, EXCEPTION_TYPE_THREADING);

    // Verify categories match
    EXPECT_EQ(base_mem->category, EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(base_brain->category, EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(base_io->category, EXCEPTION_CATEGORY_IO);
    EXPECT_EQ(base_thread->category, EXCEPTION_CATEGORY_THREADING);

    // Cast back to derived and verify derived-specific fields
    nimcp_memory_exception_t* mem_back = (nimcp_memory_exception_t*)base_mem;
    EXPECT_EQ(mem_back->requested_size, 1024u);

    nimcp_brain_exception_t* brain_back = (nimcp_brain_exception_t*)base_brain;
    EXPECT_EQ(brain_back->brain_id, 1u);

    nimcp_exception_unref(base_mem);
    nimcp_exception_unref(base_brain);
    nimcp_exception_unref(base_io);
    nimcp_exception_unref(base_thread);
}

//=============================================================================
// Reference Counting During Propagation Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, ReferenceCountingDuringPropagation) {
    // WHAT: Test reference counting works correctly during exception chaining
    // WHY:  Exceptions may be shared across multiple consumers
    // HOW:  Create exception, add reference, verify lifecycle

    // Create exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception for reference counting"
    );
    ASSERT_NE(ex, nullptr);
    // Initial ref count should be 1

    // Add reference
    nimcp_exception_t* ref = nimcp_exception_ref(ex);
    EXPECT_EQ(ref, ex);  // Should return same pointer
    // Ref count should now be 2

    // Release one reference - exception should still be valid
    nimcp_exception_unref(ex);
    // Ref count should now be 1

    // Should still be able to access
    EXPECT_EQ(ref->code, NIMCP_ERROR_OPERATION_FAILED);

    // Final release
    nimcp_exception_unref(ref);
    // Exception is now freed
}

//=============================================================================
// Thread-Local Exception Context Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, ThreadLocalExceptionContext) {
    // WHAT: Test thread-local current exception works correctly
    // WHY:  Different threads should have independent exception contexts
    // HOW:  Set exception in main thread, verify it's accessible

    // Initially no current exception
    nimcp_exception_t* current = nimcp_exception_get_current();
    EXPECT_EQ(current, nullptr);

    // Create and set current exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_THREAD_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Thread synchronization error"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_current(ex);

    // Verify it's set
    current = nimcp_exception_get_current();
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->code, NIMCP_ERROR_THREAD_SYNC);

    // Clear current exception
    nimcp_exception_clear_current();
    current = nimcp_exception_get_current();
    EXPECT_EQ(current, nullptr);

    // Exception should be released by clear_current
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, ExceptionStringConversion) {
    // WHAT: Test exception to string conversion
    // WHY:  Logging and debugging require string representations
    // HOW:  Create exception, convert to string, verify content

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_DIMENSION_MISMATCH,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Tensor dimensions [32, 64] incompatible with [128, 64]"
    );
    ASSERT_NE(ex, nullptr);

    char buffer[1024];
    size_t len = nimcp_exception_to_string(ex, buffer, sizeof(buffer));

    EXPECT_GT(len, 0u);
    EXPECT_LT(len, sizeof(buffer));

    // Buffer should contain the error code or message
    std::string str(buffer);
    EXPECT_TRUE(str.find("DIMENSION") != std::string::npos ||
                str.find("3004") != std::string::npos ||
                str.find("dimension") != std::string::npos);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Category and Severity Mapping Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, CategoryFromCodeMapping) {
    // WHAT: Test error code to category mapping
    // WHY:  Categories drive immune system routing
    // HOW:  Check mapping for various error codes

    // Memory errors (2000-2999)
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NO_MEMORY),
              EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_BUFFER_OVERFLOW),
              EXCEPTION_CATEGORY_MEMORY);

    // Brain errors (3000-3999)
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_BRAIN_CREATION),
              EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_LEARNING_FAILED),
              EXCEPTION_CATEGORY_BRAIN);

    // I/O errors (4000-4999)
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_FILE_NOT_FOUND),
              EXCEPTION_CATEGORY_IO);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NETWORK_IO),
              EXCEPTION_CATEGORY_IO);

    // Threading errors (6000-6999)
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_DEADLOCK),
              EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_RACE_CONDITION),
              EXCEPTION_CATEGORY_THREADING);

    // Signal errors (7000-7999)
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGSEGV),
              EXCEPTION_CATEGORY_SIGNAL);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGFPE),
              EXCEPTION_CATEGORY_SIGNAL);

    // Cognitive errors (8000-8999)
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_WORKING_MEMORY),
              EXCEPTION_CATEGORY_COGNITIVE);
}

//=============================================================================
// Immune Integration Mapping Tests
//=============================================================================

TEST_F(ExceptionPropagationIntegrationTest, ExceptionToAntigenSourceMapping) {
    // WHAT: Test exception category to antigen source mapping
    // WHY:  Correct mapping ensures proper immune system routing
    // HOW:  Verify each category maps to expected antigen source

    // Memory -> ANOMALY
    EXPECT_EQ(nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_MEMORY),
              EX_ANTIGEN_SOURCE_ANOMALY);

    // Brain -> BBB (brain security)
    EXPECT_EQ(nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_BRAIN),
              EX_ANTIGEN_SOURCE_BBB);

    // Threading -> BFT (byzantine fault tolerance)
    EXPECT_EQ(nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_THREADING),
              EX_ANTIGEN_SOURCE_BFT);

    // Security -> BBB
    EXPECT_EQ(nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_SECURITY),
              EX_ANTIGEN_SOURCE_BBB);

    // I/O -> ANOMALY
    EXPECT_EQ(nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_IO),
              EX_ANTIGEN_SOURCE_ANOMALY);
}

TEST_F(ExceptionPropagationIntegrationTest, SeverityToImmuneMapping) {
    // WHAT: Test exception severity to immune severity mapping
    // WHY:  Immune system uses 1-10 scale
    // HOW:  Verify mapping for each severity level

    // Debug (1) -> 1
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_DEBUG), 1u);

    // Info (2) -> 2
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_INFO), 2u);

    // Warning (3) -> 3
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_WARNING), 3u);

    // Error (5) -> 5
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_ERROR), 5u);

    // Severe (7) -> 7
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_SEVERE), 7u);

    // Critical (9) -> 9
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_CRITICAL), 9u);

    // Fatal (10) -> 10
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_FATAL), 10u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
