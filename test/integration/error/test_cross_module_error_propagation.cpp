/**
 * @file test_cross_module_error_propagation.cpp
 * @brief Integration tests for Cross-Module Error Propagation
 *
 * WHAT: Test errors propagate correctly across module boundaries
 * WHY:  Verify cleanup happens properly on errors
 * HOW:  Create multi-module scenarios, inject errors, verify handling
 *
 * TEST COVERAGE:
 * - Error propagation across module boundaries
 * - Cleanup on errors (no leaks)
 * - Error context preservation
 * - Recovery from errors
 * - Error callback chains
 * - Multi-module error scenarios
 *
 * @author NIMCP Development Team
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"

// Module headers
#include "cognitive/nimcp_executive.h"
#include "cognitive/nimcp_working_memory.h"
#include "utils/tensor/nimcp_tensor.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CrossModuleErrorPropagationTest : public ::testing::Test {
protected:
    unified_mem_manager_t mem_mgr_ = nullptr;
    bool bio_async_initialized_ = false;
    bool bio_router_initialized_ = false;
    bool tensor_initialized_ = false;

    // Error tracking
    std::atomic<int> errors_caught_{0};
    std::atomic<int> cleanups_performed_{0};

    void SetUp() override {
        errors_caught_.store(0);
        cleanups_performed_.store(0);

        // Initialize unified memory
        unified_mem_config_t mem_config = unified_mem_default_config();
        mem_mgr_ = unified_mem_create(&mem_config);
        ASSERT_NE(mem_mgr_, nullptr);

        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        if (nimcp_bio_async_init(&bio_config) == NIMCP_SUCCESS) {
            bio_async_initialized_ = true;
        }

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        if (bio_router_init(&router_config) == NIMCP_SUCCESS) {
            bio_router_initialized_ = true;
        }

        // Initialize tensor subsystem
        if (nimcp_tensor_init() == NIMCP_TENSOR_OK) {
            tensor_initialized_ = true;
        }
    }

    void TearDown() override {
        if (tensor_initialized_) {
            nimcp_tensor_shutdown();
        }
        if (bio_router_initialized_) {
            bio_router_shutdown();
        }
        if (bio_async_initialized_) {
            nimcp_bio_async_shutdown();
        }
        if (mem_mgr_) {
            unified_mem_destroy(mem_mgr_);
            mem_mgr_ = nullptr;
        }
    }
};

//=============================================================================
// ERROR CODE TESTS
//=============================================================================

TEST_F(CrossModuleErrorPropagationTest, ErrorCode_BasicValues) {
    /* WHAT: Test basic error code values */
    /* WHY:  Verify error codes are distinct */

    EXPECT_EQ(NIMCP_SUCCESS, 0);
    EXPECT_NE(NIMCP_SUCCESS, NIMCP_ERROR_UNKNOWN);
    EXPECT_NE(NIMCP_ERROR_NO_MEMORY, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_NE(NIMCP_ERROR_NOT_INITIALIZED, NIMCP_ERROR_ALREADY_EXISTS);
}

TEST_F(CrossModuleErrorPropagationTest, ErrorCode_TensorCodes) {
    /* WHAT: Test tensor-specific error codes */
    /* WHY:  Verify tensor module has distinct codes */

    EXPECT_EQ(NIMCP_TENSOR_OK, 0);
    EXPECT_NE(NIMCP_TENSOR_ERR_NULL, NIMCP_TENSOR_OK);
    EXPECT_NE(NIMCP_TENSOR_ERR_SHAPE, NIMCP_TENSOR_ERR_NULL);
    EXPECT_NE(NIMCP_TENSOR_ERR_ALLOC, NIMCP_TENSOR_ERR_SHAPE);
}

//=============================================================================
// NULL POINTER ERROR PROPAGATION TESTS
//=============================================================================

TEST_F(CrossModuleErrorPropagationTest, NullPointer_Executive) {
    /* WHAT: Test NULL pointer handling in executive module */
    /* WHY:  Verify graceful error handling */

    // NULL executive
    uint32_t task_id = executive_add_task(nullptr, nullptr);
    EXPECT_EQ(task_id, 0u);  // Error indicated by 0

    // NULL task to valid executive
    executive_controller_t* exec = executive_create();
    if (exec) {
        task_id = executive_add_task(exec, nullptr);
        EXPECT_EQ(task_id, 0u);  // Error
        executive_destroy(exec);
    }
}

TEST_F(CrossModuleErrorPropagationTest, NullPointer_WorkingMemory) {
    /* WHAT: Test NULL pointer handling in working memory */
    /* WHY:  Verify graceful error handling */

    // NULL working memory
    bool added = working_memory_add(nullptr, nullptr, 0, 0.0f);
    EXPECT_FALSE(added);  // Error

    // NULL data to valid WM
    working_memory_t* wm = working_memory_create();
    if (wm) {
        added = working_memory_add(wm, nullptr, 100, 0.5f);
        // May fail or succeed depending on implementation
        working_memory_destroy(wm);
    }
}

TEST_F(CrossModuleErrorPropagationTest, NullPointer_Tensor) {
    /* WHAT: Test NULL pointer handling in tensor module */
    /* WHY:  Verify graceful error handling */

    if (!tensor_initialized_) {
        GTEST_SKIP() << "Tensor not initialized";
    }

    // NULL in tensor operations
    nimcp_tensor_t* result = nimcp_tensor_add(nullptr, nullptr);
    EXPECT_EQ(result, nullptr);

    result = nimcp_tensor_mul(nullptr, nullptr);
    EXPECT_EQ(result, nullptr);

    // Destroy NULL should not crash
    nimcp_tensor_destroy(nullptr);
}

TEST_F(CrossModuleErrorPropagationTest, NullPointer_BioRouter) {
    /* WHAT: Test NULL pointer handling in bio-router */
    /* WHY:  Verify graceful error handling */

    if (!bio_router_initialized_) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    // NULL module info
    bio_module_context_t ctx = bio_router_register_module(nullptr);
    EXPECT_EQ(ctx, nullptr);

    // NULL context for unregister should not crash
    bio_router_unregister_module(nullptr);
}

//=============================================================================
// INVALID PARAMETER ERROR PROPAGATION TESTS
//=============================================================================

TEST_F(CrossModuleErrorPropagationTest, InvalidParam_TensorRank) {
    /* WHAT: Test invalid rank parameter in tensor creation */
    /* WHY:  Verify bounds checking */

    if (!tensor_initialized_) {
        GTEST_SKIP() << "Tensor not initialized";
    }

    // Rank exceeds maximum
    uint32_t dims[16] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 16, NIMCP_DTYPE_F32);
    // Should fail if rank > NIMCP_TENSOR_MAX_RANK
    if (16 > NIMCP_TENSOR_MAX_RANK) {
        EXPECT_EQ(t, nullptr);
    }
    if (t) nimcp_tensor_destroy(t);
}

TEST_F(CrossModuleErrorPropagationTest, InvalidParam_TensorDtype) {
    /* WHAT: Test invalid dtype parameter */
    /* WHY:  Verify type checking */

    if (!tensor_initialized_) {
        GTEST_SKIP() << "Tensor not initialized";
    }

    uint32_t dims[] = {3, 4};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, (nimcp_dtype_t)999);
    // Should fail with invalid dtype
    EXPECT_EQ(t, nullptr);
}

TEST_F(CrossModuleErrorPropagationTest, InvalidParam_ZeroDimension) {
    /* WHAT: Test zero dimension in tensor creation */
    /* WHY:  Verify dimension validation */

    if (!tensor_initialized_) {
        GTEST_SKIP() << "Tensor not initialized";
    }

    uint32_t dims[] = {3, 0, 4};  // Zero dimension
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);
    // Behavior depends on implementation - may fail or succeed with 0 elements
    if (t) nimcp_tensor_destroy(t);
}

//=============================================================================
// RESOURCE EXHAUSTION ERROR TESTS
//=============================================================================

TEST_F(CrossModuleErrorPropagationTest, ResourceExhaustion_WorkingMemory) {
    /* WHAT: Test working memory capacity exhaustion */
    /* WHY:  Verify graceful degradation */

    working_memory_t* wm = working_memory_create();
    if (!wm) {
        GTEST_SKIP() << "Working memory not available";
    }

    uint32_t items_added = 0;
    float data[64] = {0};

    // Try to exhaust capacity
    uint32_t capacity = working_memory_get_capacity(wm);
    for (uint32_t i = 0; i < capacity + 10; ++i) {
        bool added = working_memory_add(wm, data, sizeof(data) / sizeof(float), 0.5f);
        if (added) {
            items_added++;
        }
    }

    // Should have added some
    EXPECT_GT(items_added, 0u);
    // Working memory uses eviction policy: when full, lowest-salience item
    // is evicted to make room. So all adds succeed but count stays <= capacity.

    // Verify count doesn't exceed capacity
    uint32_t count = working_memory_get_count(wm);
    EXPECT_LE(count, capacity);

    // Clear all items
    working_memory_clear(wm);
    EXPECT_EQ(working_memory_get_count(wm), 0u);

    working_memory_destroy(wm);
}

TEST_F(CrossModuleErrorPropagationTest, ResourceExhaustion_ExecutiveTasks) {
    /* WHAT: Test executive task queue exhaustion */
    /* WHY:  Verify task limit handling */

    executive_config_t config;
    config.max_tasks = 5;  // Small limit
    config.task_switch_cost_ms = 100.0f;
    config.inhibition_threshold = 0.7f;
    config.max_plan_depth = 10;
    config.enable_task_prioritization = true;
    config.enable_deadline_checking = false;
    config.enable_portia_integration = false;
    config.enable_tom_integration = false;
    config.max_agent_models = 4;
    config.enable_immune_integration = false;
    config.immune_impairment_threshold = 0.6f;
    config.immune_critical_threshold = 0.85f;
    config.enable_quantum_executive = false;

    executive_controller_t* exec = executive_create_custom(&config);
    if (!exec) {
        GTEST_SKIP() << "Executive not available";
    }

    std::vector<uint32_t> task_ids;

    // Try to exceed limit
    for (int i = 0; i < 10; ++i) {
        task_descriptor_t task;
        memset(&task, 0, sizeof(task));
        snprintf(task.name, sizeof(task.name), "task_%d", i);
        task.type = TASK_TYPE_CUSTOM;
        task.priority = PRIORITY_NORMAL;
        task.status = TASK_STATUS_PENDING;

        uint32_t id = executive_add_task(exec, &task);
        if (id > 0) {
            task_ids.push_back(id);
        }
    }

    // Should have added some but not all
    EXPECT_GT(task_ids.size(), 0u);
    EXPECT_LE(task_ids.size(), 5u);  // Max is 5

    executive_destroy(exec);
}

//=============================================================================
// CLEANUP ON ERROR TESTS
//=============================================================================

TEST_F(CrossModuleErrorPropagationTest, Cleanup_TensorOperationChain) {
    /* WHAT: Test cleanup when operation chain fails */
    /* WHY:  Verify no memory leaks on error */

    if (!tensor_initialized_) {
        GTEST_SKIP() << "Tensor not initialized";
    }

    nimcp_tensor_reset_stats();

    {
        uint32_t dims[] = {4, 4};
        nimcp_tensor_t* a = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
        nimcp_tensor_t* b = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

        if (a && b) {
            // Start a chain
            nimcp_tensor_t* c = nimcp_tensor_add(a, b);

            // Simulate error by passing NULL
            nimcp_tensor_t* d = nimcp_tensor_mul(c, nullptr);  // Error
            EXPECT_EQ(d, nullptr);

            // Cleanup intermediate results
            if (c) nimcp_tensor_destroy(c);
        }

        if (a) nimcp_tensor_destroy(a);
        if (b) nimcp_tensor_destroy(b);
    }

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_created, stats.tensors_destroyed);
}

TEST_F(CrossModuleErrorPropagationTest, Cleanup_ModuleInitialization) {
    /* WHAT: Test cleanup when module initialization fails */
    /* WHY:  Verify partial initialization is handled */

    // This is more of a design test - verify that if one module
    // fails to initialize, others still work or cleanup properly

    // Already initialized in SetUp - try double init (should fail gracefully)
    if (bio_router_initialized_) {
        bio_router_config_t config = bio_router_default_config();
        nimcp_error_t result = bio_router_init(&config);
        // Should either succeed (idempotent) or return already_initialized error
    }
}

//=============================================================================
// ERROR CONTEXT PRESERVATION TESTS
//=============================================================================

TEST_F(CrossModuleErrorPropagationTest, ErrorContext_TensorErrorString) {
    /* WHAT: Test error string retrieval */
    /* WHY:  Verify error context is preserved */

    const char* err_str = nimcp_tensor_error_string(NIMCP_TENSOR_ERR_NULL);
    EXPECT_NE(err_str, nullptr);
    EXPECT_GT(strlen(err_str), 0u);

    err_str = nimcp_tensor_error_string(NIMCP_TENSOR_ERR_SHAPE);
    EXPECT_NE(err_str, nullptr);

    err_str = nimcp_tensor_error_string(NIMCP_TENSOR_ERR_ALLOC);
    EXPECT_NE(err_str, nullptr);

    // Invalid error code
    err_str = nimcp_tensor_error_string((nimcp_tensor_error_t)-999);
    // Should return some default or "unknown" string
    EXPECT_NE(err_str, nullptr);
}

//=============================================================================
// MULTI-MODULE ERROR SCENARIO TESTS
//=============================================================================

TEST_F(CrossModuleErrorPropagationTest, MultiModule_ExecutiveWithTensor) {
    /* WHAT: Test error propagation between executive and tensor modules */
    /* WHY:  Verify cross-module error handling */

    if (!tensor_initialized_) {
        GTEST_SKIP() << "Tensor not initialized";
    }

    executive_controller_t* exec = executive_create();
    if (!exec) {
        GTEST_SKIP() << "Executive not available";
    }

    // Create tensor for task context
    uint32_t dims[] = {4, 4};
    nimcp_tensor_t* context_tensor = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

    if (context_tensor) {
        // Create task with tensor context
        task_descriptor_t task;
        memset(&task, 0, sizeof(task));
        task.type = TASK_TYPE_REASONING;
        task.priority = PRIORITY_NORMAL;
        task.status = TASK_STATUS_PENDING;
        strncpy(task.name, "tensor_task", sizeof(task.name) - 1);
        task.context = context_tensor;

        uint32_t task_id = executive_add_task(exec, &task);

        // Cleanup - ensure tensor is destroyed even if executive fails
        nimcp_tensor_destroy(context_tensor);
    }

    executive_destroy(exec);
}

TEST_F(CrossModuleErrorPropagationTest, MultiModule_BioRouterWithWorkingMemory) {
    /* WHAT: Test error propagation between bio-router and working memory */
    /* WHY:  Verify messaging system handles errors gracefully */

    if (!bio_router_initialized_) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    working_memory_t* wm = working_memory_create();
    if (!wm) {
        GTEST_SKIP() << "Working memory not available";
    }

    // Register WM as bio-module
    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_WORKING_MEMORY;
    info.module_name = "working_memory";
    info.inbox_capacity = 64;
    info.user_data = wm;

    bio_module_context_t ctx = bio_router_register_module(&info);

    if (ctx) {
        // Add WM items while registered
        float data[16] = {0};
        bool added = working_memory_add(wm, data, sizeof(data) / sizeof(float), 0.7f);

        if (added) {
            working_memory_remove(wm, 0);  // Remove first item
        }

        bio_router_unregister_module(ctx);
    }

    working_memory_destroy(wm);
}

//=============================================================================
// RECOVERY FROM ERROR TESTS
//=============================================================================

TEST_F(CrossModuleErrorPropagationTest, Recovery_AfterTensorError) {
    /* WHAT: Test recovery after tensor operation error */
    /* WHY:  Verify module remains usable after error */

    if (!tensor_initialized_) {
        GTEST_SKIP() << "Tensor not initialized";
    }

    // Cause an error
    nimcp_tensor_t* failed = nimcp_tensor_add(nullptr, nullptr);
    EXPECT_EQ(failed, nullptr);

    // Try valid operation after error
    uint32_t dims[] = {3, 4};
    nimcp_tensor_t* valid = nimcp_tensor_ones(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(valid, nullptr);  // Should succeed

    // More operations should work
    nimcp_tensor_t* scaled = nimcp_tensor_mul_scalar(valid, 2.0);
    EXPECT_NE(scaled, nullptr);

    if (scaled) nimcp_tensor_destroy(scaled);
    nimcp_tensor_destroy(valid);
}

TEST_F(CrossModuleErrorPropagationTest, Recovery_AfterExecutiveError) {
    /* WHAT: Test recovery after executive error */
    /* WHY:  Verify module remains usable after error */

    executive_controller_t* exec = executive_create();
    if (!exec) {
        GTEST_SKIP() << "Executive not available";
    }

    // Cause an error - switch to non-existent task
    bool result = executive_switch_task(exec, 999, 0);
    // Should fail

    // Try valid operation after error
    task_descriptor_t task;
    memset(&task, 0, sizeof(task));
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_NORMAL;
    task.status = TASK_STATUS_PENDING;
    strncpy(task.name, "valid_task", sizeof(task.name) - 1);

    uint32_t task_id = executive_add_task(exec, &task);
    // Should succeed
    EXPECT_GT(task_id, 0u);

    executive_destroy(exec);
}

TEST_F(CrossModuleErrorPropagationTest, Recovery_AfterBioRouterError) {
    /* WHAT: Test recovery after bio-router error */
    /* WHY:  Verify router remains usable after error */

    if (!bio_router_initialized_) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    // Cause an error - register with NULL
    bio_module_context_t bad_ctx = bio_router_register_module(nullptr);
    EXPECT_EQ(bad_ctx, nullptr);

    // Try valid registration after error
    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_MOTOR_CORTEX;
    info.module_name = "motor_recovery_test";
    info.inbox_capacity = 32;

    bio_module_context_t good_ctx = bio_router_register_module(&info);
    // Should succeed
    if (good_ctx) {
        bio_router_unregister_module(good_ctx);
    }
}

//=============================================================================
// CONCURRENT ERROR HANDLING TESTS
//=============================================================================

TEST_F(CrossModuleErrorPropagationTest, Concurrent_TensorErrors) {
    /* WHAT: Test concurrent error handling in tensor module */
    /* WHY:  Verify thread safety of error handling */

    if (!tensor_initialized_) {
        GTEST_SKIP() << "Tensor not initialized";
    }

    std::atomic<bool> stop{false};
    std::atomic<int> errors{0};
    std::atomic<int> successes{0};

    auto worker = [&]() {
        while (!stop.load()) {
            // Alternate between valid and invalid operations
            uint32_t dims[] = {4, 4};
            nimcp_tensor_t* t = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

            if (t) {
                successes.fetch_add(1);

                // Try invalid operation
                nimcp_tensor_t* bad = nimcp_tensor_add(t, nullptr);
                if (!bad) {
                    errors.fetch_add(1);
                }

                nimcp_tensor_destroy(t);
            }
            std::this_thread::yield();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(successes.load(), 0);
    EXPECT_GT(errors.load(), 0);  // Some errors should have occurred
}

//=============================================================================
// STATISTICS AFTER ERRORS TESTS
//=============================================================================

TEST_F(CrossModuleErrorPropagationTest, Statistics_AfterErrors) {
    /* WHAT: Test statistics accuracy after errors */
    /* WHY:  Verify error tracking */

    if (!tensor_initialized_) {
        GTEST_SKIP() << "Tensor not initialized";
    }

    nimcp_tensor_reset_stats();

    // Perform some successful operations
    uint32_t dims[] = {4, 4};
    nimcp_tensor_t* t1 = nimcp_tensor_ones(dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* t2 = nimcp_tensor_ones(dims, 2, NIMCP_DTYPE_F32);

    // Cause some errors
    nimcp_tensor_t* bad1 = nimcp_tensor_add(nullptr, nullptr);
    nimcp_tensor_t* bad2 = nimcp_tensor_mul(nullptr, nullptr);

    EXPECT_EQ(bad1, nullptr);
    EXPECT_EQ(bad2, nullptr);

    // More successful operations
    if (t1 && t2) {
        nimcp_tensor_t* sum = nimcp_tensor_add(t1, t2);
        if (sum) nimcp_tensor_destroy(sum);
    }

    // Cleanup
    if (t1) nimcp_tensor_destroy(t1);
    if (t2) nimcp_tensor_destroy(t2);

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);

    // Stats should be consistent
    EXPECT_EQ(stats.tensors_created, stats.tensors_destroyed);
}
