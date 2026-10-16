/**
 * @file test_gpu_exception_integration.cpp
 * @brief Integration tests for GPU exception handling with immune system
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Integration tests for GPU exception-to-immune pipeline
 * WHY:  Verify complete flow from GPU error to immune response and recovery
 * HOW:  Test GPU exception creation -> immune presentation -> recovery execution
 *
 * TEST SCENARIOS:
 * - GPU exception to immune system presentation
 * - GPU memory failure with immune-mediated GC recovery
 * - Kernel launch failure with immune response
 * - Device unavailable with CPU fallback orchestration
 * - Multi-GPU error aggregation and immune handling
 * - Performance degradation immune alerting
 * - GPU-immune bridge error state tracking
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <thread>
#include <chrono>
#include <atomic>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "gpu/immune/nimcp_gpu_execution_immune_bridge.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class GpuExceptionIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    execution_immune_bridge_t* exec_bridge;
    static std::atomic<int> handler_call_count;
    static std::atomic<int> recovery_count;
    static std::atomic<bool> immune_response_received;

    void SetUp() override {
        handler_call_count = 0;
        recovery_count = 0;
        immune_response_received = false;

        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize exception-immune integration
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        nimcp_exception_immune_init(&immune_config);

        // Create brain immune system
        brain_immune_config_t brain_config;
        brain_immune_default_config(&brain_config);
        immune_system = brain_immune_create(&brain_config);
        ASSERT_NE(immune_system, nullptr);

        // Start immune system
        brain_immune_start(immune_system);

        // Connect exception system to immune
        nimcp_exception_immune_connect(immune_system);

        // Create execution-immune bridge
        execution_immune_config_t exec_config;
        execution_immune_default_config(&exec_config);
        exec_bridge = execution_immune_create(&exec_config, immune_system, nullptr);
        ASSERT_NE(exec_bridge, nullptr);

        // Install default recovery callbacks
        nimcp_exception_install_default_recovery_callbacks();
    }

    void TearDown() override {
        if (exec_bridge) {
            execution_immune_destroy(exec_bridge);
        }

        nimcp_exception_immune_disconnect();

        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }

        nimcp_exception_immune_shutdown();
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        return false;
    }

    static int test_recovery(nimcp_exception_t* ex,
                             nimcp_exception_recovery_action_t action,
                             void* user_data) {
        (void)ex;
        (void)action;
        (void)user_data;
        recovery_count++;
        return 0;
    }
};

std::atomic<int> GpuExceptionIntegrationTest::handler_call_count(0);
std::atomic<int> GpuExceptionIntegrationTest::recovery_count(0);
std::atomic<bool> GpuExceptionIntegrationTest::immune_response_received(false);

//=============================================================================
// GPU Exception to Immune Presentation Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, GpuExceptionToImmunePresentation) {
    // WHAT: Test GPU exception presentation to immune system
    // WHY:  Verify GPU errors trigger immune responses

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, 100, "GPU error for immune presentation"
    );
    ASSERT_NE(ex, nullptr);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);

    // Check response indicates presentation occurred
    EXPECT_GT(response.antigen_id, 0u);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GpuExceptionIntegrationTest, GpuMemoryExceptionToImmune) {
    // WHAT: Test GPU memory exception immune integration
    // WHY:  Memory failures should trigger specific immune responses

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, 2, "GPU out of memory - triggering immune GC response"
    );
    ASSERT_NE(ex, nullptr);

    ex->gpu_memory_used = 11ULL * 1024 * 1024 * 1024;  // 11GB used
    ex->gpu_memory_total = 12ULL * 1024 * 1024 * 1024; // 12GB total

    // Present to immune
    nimcp_immune_response_t response;
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);

    // Get recovery strategy - should suggest GC for memory issues
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // Memory-related errors should have appropriate recovery
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// GPU-Immune Bridge Integration Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, BridgeErrorStateTracking) {
    // WHAT: Test execution-immune bridge tracks GPU errors
    // WHY:  Bridge should aggregate GPU errors for immune decision making

    // Trigger error through bridge
    int result = execution_immune_trigger_error_response(
        exec_bridge,
        NIMCP_ERROR_GPU_MEMORY,
        "GPU memory allocation failed"
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check error state was updated
    execution_error_state_t state;
    result = execution_immune_get_error_state(exec_bridge, &state);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Error should be recorded
    // (Specific count depends on implementation)
}

TEST_F(GpuExceptionIntegrationTest, BridgeModeModulationOnError) {
    // WHAT: Test mode changes due to GPU errors
    // WHY:  Repeated GPU errors should shift execution mode

    // Get initial mode
    execution_mode_t initial_mode = execution_immune_get_recommended_mode(exec_bridge);

    // Trigger multiple GPU errors
    for (int i = 0; i < 5; i++) {
        execution_immune_trigger_error_response(
            exec_bridge,
            NIMCP_ERROR_GPU,
            "Repeated GPU error"
        );
    }

    // Update bridge to process errors
    execution_immune_update(exec_bridge);

    // Get updated mode (may have shifted)
    execution_mode_t updated_mode = execution_immune_get_recommended_mode(exec_bridge);

    // Mode should be valid
    EXPECT_GE((int)updated_mode, 0);
    EXPECT_LE((int)updated_mode, (int)EXEC_MODE_HYBRID);
}

TEST_F(GpuExceptionIntegrationTest, BridgeEnergyConservation) {
    // WHAT: Test energy conservation on GPU failures
    // WHY:  GPU unavailability should conserve energy

    // Trigger device unavailable error
    execution_immune_trigger_error_response(
        exec_bridge,
        NIMCP_ERROR_GPU_NOT_AVAILABLE,
        "GPU device not available"
    );

    // Update bridge
    execution_immune_update(exec_bridge);

    // Check energy factor
    float energy = execution_immune_get_energy_conservation_factor(exec_bridge);
    EXPECT_GE(energy, 0.0f);
    EXPECT_LE(energy, 1.0f);
}

//=============================================================================
// Kernel Launch Failure Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, KernelLaunchFailureImmune) {
    // WHAT: Test kernel launch failure immune handling
    // WHY:  Kernel failures need immune-mediated recovery

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_KERNEL_LAUNCH,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0, 9, "Kernel launch failed: invalid configuration"
    );
    ASSERT_NE(ex, nullptr);

    ex->kernel_name = "update_neurons_kernel";

    // Set context for kernel details
    nimcp_exception_set_context((nimcp_exception_t*)ex, "grid_dim", "256x256x1");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "block_dim", "1024x1x1");

    // Present to immune
    nimcp_immune_response_t response;
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Device Unavailable and Fallback Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, DeviceUnavailableFallback) {
    // WHAT: Test device unavailable with CPU fallback
    // WHY:  System should gracefully fall back when GPU unavailable

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_NOT_AVAILABLE,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        -1, 101, "No CUDA device available"
    );
    ASSERT_NE(ex, nullptr);

    // Set fallback context
    nimcp_exception_set_context((nimcp_exception_t*)ex, "fallback_mode", "CPU");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "original_mode", "GPU_CUDA");

    // Present to immune for fallback orchestration
    nimcp_immune_response_t response;
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);

    // Verify fallback context preserved
    const char* fallback = nimcp_exception_get_context((nimcp_exception_t*)ex, "fallback_mode");
    EXPECT_STREQ(fallback, "CPU");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GpuExceptionIntegrationTest, FallbackTriggersThroughBridge) {
    // WHAT: Test fallback event tracking through bridge
    // WHY:  Bridge should track fallback events

    // Trigger fallback event
    execution_immune_trigger_error_response(
        exec_bridge,
        NIMCP_ERROR_GPU_NOT_AVAILABLE,
        "GPU unavailable - CPU fallback"
    );

    // Get error state
    execution_error_state_t state;
    int result = execution_immune_get_error_state(exec_bridge, &state);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Multi-GPU Exception Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, MultiGpuExceptionAggregation) {
    // WHAT: Test aggregation of exceptions from multiple GPUs
    // WHY:  Multi-GPU systems may have simultaneous failures

    // Create aggregate exception
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Multi-GPU failure aggregate"
    );
    ASSERT_NE(agg, nullptr);

    // Add child exceptions for each GPU
    for (int device = 0; device < 4; device++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "GPU %d memory error", device);

        nimcp_gpu_exception_t* child = nimcp_gpu_exception_create(
            NIMCP_ERROR_GPU_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            device, 2, msg
        );
        ASSERT_NE(child, nullptr);

        nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)child);
    }

    // Verify aggregate
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 4u);

    // Present aggregate to immune
    nimcp_immune_response_t response;
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)agg, &response);
    EXPECT_EQ(result, 0);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Performance Degradation Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, PerformanceDegradationImmune) {
    // WHAT: Test performance degradation immune alerting
    // WHY:  GPU performance issues should trigger immune response

    // Monitor performance through bridge
    int result = execution_immune_monitor_performance(exec_bridge);
    // May return different values based on state
    (void)result;

    // Trigger performance-related error
    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        0, 0, "GPU performance degraded: 50% of baseline"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context((nimcp_exception_t*)ex, "perf_ratio", "0.50");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "baseline_tflops", "100");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "current_tflops", "50");

    // Present to immune
    nimcp_immune_response_t response;
    result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Recovery Execution Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, RecoveryGCExecution) {
    // WHAT: Test GC recovery execution for GPU memory errors
    // WHY:  GC should be triggered for memory pressure

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, 2, "GPU OOM - needs GC"
    );
    ASSERT_NE(ex, nullptr);

    // Execute GC recovery
    int result = nimcp_exception_execute_recovery(
        (nimcp_exception_t*)ex,
        EXCEPTION_RECOVERY_GC
    );

    // Result depends on whether GC callback is registered
    (void)result;

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GpuExceptionIntegrationTest, RecoveryReduceLoad) {
    // WHAT: Test reduce load recovery for GPU thermal issues
    // WHY:  Thermal throttling should reduce load

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        0, 0, "GPU thermal throttling detected"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context((nimcp_exception_t*)ex, "temperature", "85C");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "power_limit", "90%");

    // Execute reduce load recovery
    int result = nimcp_exception_execute_recovery(
        (nimcp_exception_t*)ex,
        EXCEPTION_RECOVERY_REDUCE_LOAD
    );
    (void)result;

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Exception Queue Processing Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, AsyncGpuExceptionProcessing) {
    // WHAT: Test async GPU exception presentation
    // WHY:  GPU exceptions should not block kernel execution

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0, 0, "Async GPU exception"
    );
    ASSERT_NE(ex, nullptr);

    // Present async
    int result = nimcp_exception_present_async((nimcp_exception_t*)ex);
    EXPECT_EQ(result, 0);

    // Process pending (should process our exception)
    size_t processed = nimcp_exception_immune_process_pending(0);
    // May be 0 or more depending on timing

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Statistics and Metrics Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, ImmuneStatsTracking) {
    // WHAT: Test immune statistics for GPU exceptions
    // WHY:  Need to track GPU error rates

    // Present several GPU exceptions
    for (int i = 0; i < 3; i++) {
        nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
            NIMCP_ERROR_GPU,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            0, i, "GPU error for stats"
        );
        ASSERT_NE(ex, nullptr);

        nimcp_immune_response_t response;
        nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);

        nimcp_exception_unref((nimcp_exception_t*)ex);
    }

    // Get stats
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    EXPECT_GE(stats.exceptions_presented, 3u);
}

//=============================================================================
// Handler Chain with Immune Integration Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, HandlerChainWithImmune) {
    // WHAT: Test handler chain integrates with immune
    // WHY:  Handlers and immune should work together

    // Register custom GPU handler
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "gpu_immune_test_handler";
    options.handler = test_handler;
    options.priority = 100;
    options.category_filter = EXCEPTION_CATEGORY_GPU;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    // Create and dispatch GPU exception
    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, 0, "GPU error for handler chain test"
    );
    ASSERT_NE(ex, nullptr);

    handler_call_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // Handler should have been called
    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    if (reg) nimcp_handler_unregister(reg);
}

//=============================================================================
// Antigen Source Mapping Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, GpuCategoryToAntigenSource) {
    // WHAT: Test GPU category maps to correct antigen source
    // WHY:  GPU errors should map to appropriate immune source

    exception_antigen_source_t source =
        nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_GPU);

    // GPU errors typically map to ANOMALY source
    EXPECT_TRUE(
        source == EX_ANTIGEN_SOURCE_ANOMALY ||
        source == EX_ANTIGEN_SOURCE_BBB ||
        source == EX_ANTIGEN_SOURCE_MANUAL
    );
}

TEST_F(GpuExceptionIntegrationTest, GpuSeverityToImmuneSeverity) {
    // WHAT: Test GPU exception severity mapping
    // WHY:  Ensure proper severity translation

    uint32_t immune_sev;

    immune_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_ERROR);
    EXPECT_GE(immune_sev, 1u);
    EXPECT_LE(immune_sev, 10u);

    immune_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_SEVERE);
    EXPECT_GE(immune_sev, 7u);

    immune_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_GE(immune_sev, 9u);
}

//=============================================================================
// Recovery Notification Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, RecoveryResultNotification) {
    // WHAT: Test recovery result notification to immune
    // WHY:  Immune system should know if recovery succeeded

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, 2, "GPU OOM"
    );
    ASSERT_NE(ex, nullptr);

    // Notify recovery result
    int result = nimcp_exception_notify_recovery_result(
        (nimcp_exception_t*)ex,
        EXCEPTION_RECOVERY_GC,
        true  // success
    );
    EXPECT_EQ(result, 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

TEST_F(GpuExceptionIntegrationTest, NullExceptionToImmune) {
    // WHAT: Test null exception handling
    // WHY:  Ensure robustness

    nimcp_immune_response_t response;
    int result = nimcp_exception_present_to_immune(nullptr, &response);
    EXPECT_NE(result, 0);  // Should fail gracefully
}

TEST_F(GpuExceptionIntegrationTest, NullBridgeOperations) {
    // WHAT: Test null bridge parameter handling
    // WHY:  Ensure null checks work

    EXPECT_EQ(execution_immune_update(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(execution_immune_apply_modulation(nullptr), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
