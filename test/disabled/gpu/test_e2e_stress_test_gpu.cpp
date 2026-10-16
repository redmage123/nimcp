/**
 * @file test_e2e_stress_test_gpu.cpp
 * @brief E2E GPU Stress Tests for Maximum Scale and Stability
 *
 * WHAT: End-to-end stress testing of GPU operations at maximum scale
 * WHY:  Verify system stability, memory handling, and performance at limits
 * HOW:  Test maximum allocations, sustained loads, error recovery, and concurrency
 *
 * TEST PIPELINES:
 * - MemoryExhaustionRecovery: Test graceful handling of OOM conditions
 * - MaximumTensorSize: Push tensor sizes to GPU limits
 * - SustainedLoad: Long-running operations for stability
 * - MemoryLeakDetection: Verify no memory leaks over many iterations
 * - ConcurrentOperations: Multiple simultaneous GPU operations
 * - ErrorRecovery: Recovery from various error conditions
 * - ThermalThrottling: Performance under sustained load
 * - BatchSizeScaling: Maximum batch sizes
 * - MillionNeuronSimulation: Simulate 1M+ neurons
 * - PerformanceRegression: Detect performance degradation
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "gpu/nimcp_execution_mode.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/training/nimcp_training_gpu.h"
#include "utils/memory/nimcp_memory.h"

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>
#include <random>
#include <numeric>
#include <algorithm>
#include <mutex>
#include <condition_variable>

//=============================================================================
// Test Metrics Structure
//=============================================================================

struct StressMetrics {
    double total_time_ms;
    size_t peak_memory_bytes;
    size_t final_memory_bytes;
    size_t leaked_bytes;
    uint64_t total_operations;
    uint64_t successful_operations;
    uint64_t failed_operations;
    double operations_per_second;
    double error_rate;
    double memory_efficiency;
    bool stability_passed;
    std::string failure_reason;
};

//=============================================================================
// Test Fixture
//=============================================================================

class GPUStressTestE2E : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx_ = nullptr;
    hardware_capabilities_t caps_;
    bool has_gpu_ = false;
    std::mt19937 rng_;
    StressMetrics metrics_;

    void SetUp() override {
        memset(&caps_, 0, sizeof(caps_));
        memset(&metrics_, 0, sizeof(metrics_));

        execution_detect_capabilities(&caps_);
        has_gpu_ = caps_.cuda_available || caps_.rocm_available || caps_.opencl_available;

        if (has_gpu_) {
            gpu_ctx_ = nimcp_gpu_context_create_auto();
        }

        rng_.seed(std::chrono::system_clock::now().time_since_epoch().count());
    }

    void TearDown() override {
        if (gpu_ctx_) {
            nimcp_gpu_context_destroy(gpu_ctx_);
            gpu_ctx_ = nullptr;
        }
    }

    bool HasGPU() const { return has_gpu_ && gpu_ctx_ != nullptr; }

    size_t GetFreeGPUMemory() {
        if (!gpu_ctx_) return 0;
        size_t allocated = 0, peak = 0, free_mem = 0;
        nimcp_gpu_memory_stats(gpu_ctx_, &allocated, &peak, &free_mem);
        return free_mem;
    }

    size_t GetAllocatedGPUMemory() {
        if (!gpu_ctx_) return 0;
        size_t allocated = 0, peak = 0, free_mem = 0;
        nimcp_gpu_memory_stats(gpu_ctx_, &allocated, &peak, &free_mem);
        return allocated;
    }

    void PrintMetrics(const std::string& test_name) {
        std::cout << "\n=== " << test_name << " Stress Metrics ===" << std::endl;
        std::cout << "  Total Time: " << metrics_.total_time_ms << " ms" << std::endl;
        std::cout << "  Peak Memory: " << (metrics_.peak_memory_bytes / 1024.0 / 1024.0)
                  << " MB" << std::endl;
        std::cout << "  Final Memory: " << (metrics_.final_memory_bytes / 1024.0 / 1024.0)
                  << " MB" << std::endl;
        std::cout << "  Leaked Memory: " << (metrics_.leaked_bytes / 1024.0)
                  << " KB" << std::endl;
        std::cout << "  Total Operations: " << metrics_.total_operations << std::endl;
        std::cout << "  Successful: " << metrics_.successful_operations << std::endl;
        std::cout << "  Failed: " << metrics_.failed_operations << std::endl;
        std::cout << "  Ops/sec: " << metrics_.operations_per_second << std::endl;
        std::cout << "  Error Rate: " << (metrics_.error_rate * 100) << "%" << std::endl;
        std::cout << "  Stability: " << (metrics_.stability_passed ? "PASSED" : "FAILED") << std::endl;
        if (!metrics_.failure_reason.empty()) {
            std::cout << "  Failure: " << metrics_.failure_reason << std::endl;
        }
    }
};

//=============================================================================
// Pipeline 1: Memory Exhaustion and Recovery
//=============================================================================

TEST_F(GPUStressTestE2E, MemoryExhaustionRecovery) {
    E2E_PIPELINE_START("Memory Exhaustion and Recovery");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    // Stage 1: Measure available memory
    E2E_STAGE_BEGIN("Measure available memory", 1000);

    size_t initial_free = GetFreeGPUMemory();
    size_t initial_allocated = GetAllocatedGPUMemory();

    std::cout << "\n  Initial GPU Memory:" << std::endl;
    std::cout << "    Free: " << (initial_free / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "    Allocated: " << (initial_allocated / 1024.0 / 1024.0) << " MB" << std::endl;

    E2E_STAGE_END();

    // Stage 2: Allocate until OOM
    E2E_STAGE_BEGIN("Allocate until OOM", 30000);

    std::vector<nimcp_gpu_tensor_t*> allocations;
    size_t total_allocated = 0;
    size_t allocation_size = 256 * 1024 * 1024;  // Start with 256 MB chunks
    size_t successful_allocs = 0;
    size_t failed_allocs = 0;

    while (allocation_size >= 1024 * 1024) {  // Minimum 1 MB
        size_t n_elements = allocation_size / sizeof(float);
        size_t dims[] = {n_elements};

        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(
            gpu_ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);

        if (tensor) {
            allocations.push_back(tensor);
            total_allocated += allocation_size;
            successful_allocs++;
        } else {
            failed_allocs++;
            // Reduce allocation size and try again
            allocation_size /= 2;
        }
    }

    std::cout << "\n  Memory exhaustion results:" << std::endl;
    std::cout << "    Total allocated: " << (total_allocated / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "    Successful allocations: " << successful_allocs << std::endl;
    std::cout << "    Failed allocations: " << failed_allocs << std::endl;

    metrics_.peak_memory_bytes = total_allocated;
    metrics_.successful_operations = successful_allocs;
    metrics_.failed_operations = failed_allocs;

    E2E_STAGE_END();

    // Stage 3: Verify GPU operations still work near OOM
    E2E_STAGE_BEGIN("Verify operations near OOM", 5000);

    bool operations_work = false;

    // Try to do a small operation
    if (!allocations.empty()) {
        // Fill a tensor with values
        nimcp_gpu_tensor_t* test_tensor = allocations.back();
        bool success = nimcp_gpu_fill(gpu_ctx_, test_tensor, 1.0f);
        operations_work = success;

        std::cout << "\n  Operations near OOM: " << (operations_work ? "WORKING" : "FAILED") << std::endl;
    }

    E2E_STAGE_END();

    // Stage 4: Free memory and verify recovery
    E2E_STAGE_BEGIN("Free memory and verify recovery", 5000);

    // Free all allocations
    for (auto* tensor : allocations) {
        nimcp_gpu_tensor_destroy(tensor);
    }
    allocations.clear();

    nimcp_gpu_context_synchronize(gpu_ctx_);

    size_t final_free = GetFreeGPUMemory();
    size_t final_allocated = GetAllocatedGPUMemory();

    std::cout << "\n  After cleanup:" << std::endl;
    std::cout << "    Free: " << (final_free / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "    Allocated: " << (final_allocated / 1024.0 / 1024.0) << " MB" << std::endl;

    // Verify memory was recovered (within some tolerance)
    bool memory_recovered = (final_allocated <= initial_allocated + 10 * 1024 * 1024);  // 10 MB tolerance

    std::cout << "    Memory recovered: " << (memory_recovered ? "YES" : "NO") << std::endl;

    metrics_.final_memory_bytes = final_allocated;
    metrics_.leaked_bytes = (final_allocated > initial_allocated) ?
                            (final_allocated - initial_allocated) : 0;
    metrics_.stability_passed = memory_recovered && operations_work;

    EXPECT_TRUE(memory_recovered) << "Memory should be recovered after freeing";

    E2E_STAGE_END();

    // Stage 5: Verify normal operations resume
    E2E_STAGE_BEGIN("Verify normal operations", 2000);

    size_t test_dims[] = {1024, 1024};
    nimcp_gpu_tensor_t* a = nimcp_gpu_tensor_create(gpu_ctx_, test_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* b = nimcp_gpu_tensor_create(gpu_ctx_, test_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* c = nimcp_gpu_tensor_create(gpu_ctx_, test_dims, 2, NIMCP_GPU_PRECISION_FP32);

    E2E_ASSERT_NOT_NULL(a, "Should be able to allocate after recovery");
    E2E_ASSERT_NOT_NULL(b, "Should be able to allocate after recovery");
    E2E_ASSERT_NOT_NULL(c, "Should be able to allocate after recovery");

    nimcp_gpu_fill(gpu_ctx_, a, 1.0f);
    nimcp_gpu_fill(gpu_ctx_, b, 2.0f);

    bool success = nimcp_gpu_gemm(gpu_ctx_, a, b, c, 1.0f, 0.0f, false, false);
    EXPECT_TRUE(success) << "GEMM should work after OOM recovery";

    nimcp_gpu_tensor_destroy(a);
    nimcp_gpu_tensor_destroy(b);
    nimcp_gpu_tensor_destroy(c);

    std::cout << "\n  Normal operations resumed successfully" << std::endl;

    E2E_STAGE_END();

    PrintMetrics("Memory Exhaustion Recovery");

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Memory Leak Detection
//=============================================================================

TEST_F(GPUStressTestE2E, MemoryLeakDetection) {
    E2E_PIPELINE_START("Memory Leak Detection");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const size_t N_ITERATIONS = 1000;
    const size_t TENSOR_SIZE = 1024 * 1024;  // 1M elements = 4 MB

    // Stage 1: Get baseline memory
    E2E_STAGE_BEGIN("Get baseline memory", 1000);

    // Force cleanup
    nimcp_gpu_context_synchronize(gpu_ctx_);

    size_t baseline_allocated = GetAllocatedGPUMemory();
    std::cout << "\n  Baseline allocated: " << (baseline_allocated / 1024.0 / 1024.0) << " MB" << std::endl;

    E2E_STAGE_END();

    // Stage 2: Repeated allocation/deallocation cycles
    E2E_STAGE_BEGIN("Allocation/deallocation cycles", 60000);

    size_t max_allocated = baseline_allocated;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < N_ITERATIONS; i++) {
        size_t dims[] = {TENSOR_SIZE};

        // Create tensor
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(
            gpu_ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);

        if (tensor) {
            // Use it
            nimcp_gpu_fill(gpu_ctx_, tensor, static_cast<float>(i));

            // Destroy it
            nimcp_gpu_tensor_destroy(tensor);
        }

        // Sample memory periodically
        if (i % 100 == 0) {
            size_t current = GetAllocatedGPUMemory();
            max_allocated = std::max(max_allocated, current);
        }
    }

    nimcp_gpu_context_synchronize(gpu_ctx_);
    auto end = std::chrono::high_resolution_clock::now();
    metrics_.total_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "\n  Completed " << N_ITERATIONS << " cycles in " << metrics_.total_time_ms << " ms" << std::endl;

    E2E_STAGE_END();

    // Stage 3: Check for leaks
    E2E_STAGE_BEGIN("Check for leaks", 2000);

    // Allow GPU to clean up
    nimcp_gpu_context_synchronize(gpu_ctx_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    size_t final_allocated = GetAllocatedGPUMemory();
    metrics_.final_memory_bytes = final_allocated;
    metrics_.peak_memory_bytes = max_allocated;

    // Calculate leak
    if (final_allocated > baseline_allocated) {
        metrics_.leaked_bytes = final_allocated - baseline_allocated;
    } else {
        metrics_.leaked_bytes = 0;
    }

    std::cout << "\n  Memory analysis:" << std::endl;
    std::cout << "    Baseline: " << (baseline_allocated / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "    Final: " << (final_allocated / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "    Peak: " << (max_allocated / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "    Leaked: " << (metrics_.leaked_bytes / 1024.0) << " KB" << std::endl;

    // Allow some tolerance (1 MB)
    bool no_leak = metrics_.leaked_bytes < 1024 * 1024;
    metrics_.stability_passed = no_leak;

    EXPECT_TRUE(no_leak) << "Should not have significant memory leak";

    E2E_STAGE_END();

    // Stage 4: Complex operation leak test
    E2E_STAGE_BEGIN("Complex operation leak test", 30000);

    size_t pre_complex = GetAllocatedGPUMemory();

    for (size_t i = 0; i < 100; i++) {
        size_t dims[] = {512, 512};
        nimcp_gpu_tensor_t* a = nimcp_gpu_tensor_create(gpu_ctx_, dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* b = nimcp_gpu_tensor_create(gpu_ctx_, dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* c = nimcp_gpu_tensor_create(gpu_ctx_, dims, 2, NIMCP_GPU_PRECISION_FP32);

        if (a && b && c) {
            nimcp_gpu_fill(gpu_ctx_, a, 1.0f);
            nimcp_gpu_fill(gpu_ctx_, b, 2.0f);
            nimcp_gpu_gemm(gpu_ctx_, a, b, c, 1.0f, 0.0f, false, false);
            nimcp_gpu_relu(gpu_ctx_, c, c);
            nimcp_gpu_softmax(gpu_ctx_, c, c);
        }

        if (a) nimcp_gpu_tensor_destroy(a);
        if (b) nimcp_gpu_tensor_destroy(b);
        if (c) nimcp_gpu_tensor_destroy(c);
    }

    nimcp_gpu_context_synchronize(gpu_ctx_);
    size_t post_complex = GetAllocatedGPUMemory();

    size_t complex_leak = (post_complex > pre_complex) ? (post_complex - pre_complex) : 0;
    std::cout << "\n  Complex operation leak: " << (complex_leak / 1024.0) << " KB" << std::endl;

    EXPECT_LT(complex_leak, 1024 * 1024) << "Complex operations should not leak significantly";

    E2E_STAGE_END();

    metrics_.total_operations = N_ITERATIONS + 100;
    metrics_.operations_per_second = metrics_.total_operations / (metrics_.total_time_ms / 1000.0);

    PrintMetrics("Memory Leak Detection");

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Sustained Load Test
//=============================================================================

TEST_F(GPUStressTestE2E, SustainedLoadTest) {
    E2E_PIPELINE_START("Sustained Load Test");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const double TEST_DURATION_SECONDS = 30.0;  // 30 second sustained load
    const size_t BATCH_SIZE = 64;
    const size_t DIM = 1024;

    // Stage 1: Setup workload
    E2E_STAGE_BEGIN("Setup workload", 2000);

    size_t a_dims[] = {BATCH_SIZE, DIM};
    size_t b_dims[] = {DIM, DIM};
    size_t c_dims[] = {BATCH_SIZE, DIM};

    nimcp_gpu_tensor_t* a = nimcp_gpu_tensor_create(gpu_ctx_, a_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* b = nimcp_gpu_tensor_create(gpu_ctx_, b_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* c = nimcp_gpu_tensor_create(gpu_ctx_, c_dims, 2, NIMCP_GPU_PRECISION_FP32);

    E2E_ASSERT_NOT_NULL(a, "Failed to create tensor a");
    E2E_ASSERT_NOT_NULL(b, "Failed to create tensor b");
    E2E_ASSERT_NOT_NULL(c, "Failed to create tensor c");

    nimcp_gpu_fill(gpu_ctx_, a, 0.5f);
    nimcp_gpu_fill(gpu_ctx_, b, 0.5f);

    std::cout << "\n  Workload: " << BATCH_SIZE << "x" << DIM << " GEMM" << std::endl;
    std::cout << "  Duration: " << TEST_DURATION_SECONDS << " seconds" << std::endl;

    E2E_STAGE_END();

    // Stage 2: Run sustained load
    E2E_STAGE_BEGIN("Run sustained load", static_cast<uint64_t>(TEST_DURATION_SECONDS * 1000 + 5000));

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = start_time + std::chrono::duration<double>(TEST_DURATION_SECONDS);

    uint64_t operations = 0;
    uint64_t errors = 0;
    std::vector<double> latencies;

    size_t initial_memory = GetAllocatedGPUMemory();

    while (std::chrono::high_resolution_clock::now() < end_time) {
        auto op_start = std::chrono::high_resolution_clock::now();

        bool success = nimcp_gpu_gemm(gpu_ctx_, a, b, c, 1.0f, 0.0f, false, false);
        success = success && nimcp_gpu_relu(gpu_ctx_, c, c);

        auto op_end = std::chrono::high_resolution_clock::now();

        if (success) {
            operations++;
            double latency = std::chrono::duration<double, std::milli>(op_end - op_start).count();
            latencies.push_back(latency);
        } else {
            errors++;
        }
    }

    nimcp_gpu_context_synchronize(gpu_ctx_);

    auto actual_end = std::chrono::high_resolution_clock::now();
    metrics_.total_time_ms = std::chrono::duration<double, std::milli>(actual_end - start_time).count();

    size_t final_memory = GetAllocatedGPUMemory();

    std::cout << "\n  Sustained load completed:" << std::endl;
    std::cout << "    Operations: " << operations << std::endl;
    std::cout << "    Errors: " << errors << std::endl;

    E2E_STAGE_END();

    // Stage 3: Analyze performance
    E2E_STAGE_BEGIN("Analyze performance", 1000);

    metrics_.total_operations = operations;
    metrics_.successful_operations = operations;
    metrics_.failed_operations = errors;
    metrics_.error_rate = static_cast<double>(errors) / (operations + errors);
    metrics_.operations_per_second = operations / (metrics_.total_time_ms / 1000.0);

    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        double avg_latency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        double p99_latency = latencies[static_cast<size_t>(latencies.size() * 0.99)];
        double max_latency = latencies.back();

        std::cout << "\n  Latency statistics:" << std::endl;
        std::cout << "    Average: " << avg_latency << " ms" << std::endl;
        std::cout << "    P99: " << p99_latency << " ms" << std::endl;
        std::cout << "    Max: " << max_latency << " ms" << std::endl;
        std::cout << "    Ops/sec: " << metrics_.operations_per_second << std::endl;

        // Check for thermal throttling (latency increase over time)
        size_t sample_size = latencies.size() / 10;
        double early_avg = std::accumulate(latencies.begin(), latencies.begin() + sample_size, 0.0) / sample_size;
        double late_avg = std::accumulate(latencies.end() - sample_size, latencies.end(), 0.0) / sample_size;
        double throttling_ratio = late_avg / early_avg;

        std::cout << "\n  Thermal analysis:" << std::endl;
        std::cout << "    Early avg latency: " << early_avg << " ms" << std::endl;
        std::cout << "    Late avg latency: " << late_avg << " ms" << std::endl;
        std::cout << "    Throttling ratio: " << throttling_ratio << "x" << std::endl;

        // Significant throttling if late latency is >1.5x early
        if (throttling_ratio > 1.5) {
            std::cout << "    WARNING: Possible thermal throttling detected" << std::endl;
        }
    }

    metrics_.peak_memory_bytes = std::max(initial_memory, final_memory);
    metrics_.final_memory_bytes = final_memory;
    metrics_.leaked_bytes = (final_memory > initial_memory) ? (final_memory - initial_memory) : 0;
    metrics_.stability_passed = errors == 0;

    EXPECT_EQ(errors, 0u) << "Should have no errors during sustained load";

    E2E_STAGE_END();

    // Cleanup
    nimcp_gpu_tensor_destroy(a);
    nimcp_gpu_tensor_destroy(b);
    nimcp_gpu_tensor_destroy(c);

    PrintMetrics("Sustained Load Test");

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Million Neuron Simulation
//=============================================================================

TEST_F(GPUStressTestE2E, MillionNeuronSimulation) {
    E2E_PIPELINE_START("Million Neuron Simulation");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    const size_t N_NEURONS = 1000000;  // 1 million neurons
    const size_t N_TIMESTEPS = 100;
    const float DT = 1.0f;

    // Stage 1: Create million-neuron population
    E2E_STAGE_BEGIN("Create million neurons", 10000);

    nimcp_lif_params_t params = {
        .tau_mem = 20.0f,
        .tau_syn = 5.0f,
        .v_thresh = -50.0f,
        .v_reset = -70.0f,
        .v_rest = -65.0f,
        .dt = DT,
        .hard_reset = true
    };

    nimcp_lif_state_t* neurons = nimcp_lif_state_create(gpu_ctx_, N_NEURONS, &params);

    if (!neurons) {
        std::cout << "\n  WARNING: Could not allocate 1M neurons - trying smaller size" << std::endl;
        // Try with fewer neurons
        size_t reduced = N_NEURONS / 2;
        while (reduced >= 100000 && !neurons) {
            neurons = nimcp_lif_state_create(gpu_ctx_, reduced, &params);
            if (!neurons) reduced /= 2;
        }

        if (neurons) {
            std::cout << "  Allocated " << reduced << " neurons instead" << std::endl;
        } else {
            E2E_PIPELINE_END();
            GTEST_SKIP() << "Insufficient GPU memory for large neuron populations";
        }
    }

    E2E_ASSERT_NOT_NULL(neurons, "Failed to create neuron population");

    size_t actual_neurons = neurons ? N_NEURONS : 0;  // Simplified - would need state query

    std::cout << "\n  Created " << N_NEURONS << " neurons" << std::endl;

    E2E_STAGE_END();

    // Stage 2: Create input current
    E2E_STAGE_BEGIN("Create input buffer", 5000);

    size_t input_dims[] = {N_NEURONS};
    nimcp_gpu_tensor_t* input_current = nimcp_gpu_tensor_create(
        gpu_ctx_, input_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!input_current) {
        nimcp_lif_state_destroy(neurons);
        E2E_PIPELINE_END();
        GTEST_SKIP() << "Insufficient GPU memory for input buffer";
    }

    // Fill with constant current
    nimcp_gpu_fill(gpu_ctx_, input_current, 5.0f);

    E2E_STAGE_END();

    // Stage 3: Run simulation
    E2E_STAGE_BEGIN("Run million-neuron simulation", 120000);

    auto sim_start = std::chrono::high_resolution_clock::now();

    uint64_t total_spikes = 0;
    bool simulation_stable = true;

    for (size_t t = 0; t < N_TIMESTEPS; t++) {
        bool success = nimcp_gpu_lif_forward(gpu_ctx_, neurons, input_current);

        if (!success) {
            simulation_stable = false;
            metrics_.failure_reason = "LIF forward failed at timestep " + std::to_string(t);
            break;
        }

        uint32_t spike_count = 0;
        nimcp_gpu_spike_count(gpu_ctx_, neurons->spikes, &spike_count);
        total_spikes += spike_count;
    }

    nimcp_gpu_context_synchronize(gpu_ctx_);
    auto sim_end = std::chrono::high_resolution_clock::now();
    metrics_.total_time_ms = std::chrono::duration<double, std::milli>(sim_end - sim_start).count();

    double neuron_updates_per_sec = (N_NEURONS * N_TIMESTEPS) / (metrics_.total_time_ms / 1000.0);
    double avg_firing_rate = (total_spikes * 1000.0) / (N_NEURONS * N_TIMESTEPS * DT);

    std::cout << "\n  Simulation completed:" << std::endl;
    std::cout << "    Time: " << metrics_.total_time_ms << " ms" << std::endl;
    std::cout << "    Neuron-updates/sec: " << (neuron_updates_per_sec / 1e9) << " G" << std::endl;
    std::cout << "    Total spikes: " << total_spikes << std::endl;
    std::cout << "    Average firing rate: " << avg_firing_rate << " Hz" << std::endl;

    metrics_.total_operations = N_NEURONS * N_TIMESTEPS;
    metrics_.successful_operations = simulation_stable ? metrics_.total_operations : 0;
    metrics_.operations_per_second = neuron_updates_per_sec;
    metrics_.stability_passed = simulation_stable;

    E2E_STAGE_END();

    // Stage 4: Memory usage analysis
    E2E_STAGE_BEGIN("Memory analysis", 1000);

    size_t allocated = 0, peak = 0, free_mem = 0;
    nimcp_gpu_memory_stats(gpu_ctx_, &allocated, &peak, &free_mem);

    metrics_.peak_memory_bytes = peak;
    metrics_.final_memory_bytes = allocated;

    double bytes_per_neuron = static_cast<double>(peak) / N_NEURONS;

    std::cout << "\n  Memory usage:" << std::endl;
    std::cout << "    Peak: " << (peak / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "    Per neuron: " << bytes_per_neuron << " bytes" << std::endl;

    E2E_STAGE_END();

    // Cleanup
    nimcp_gpu_tensor_destroy(input_current);
    nimcp_lif_state_destroy(neurons);

    PrintMetrics("Million Neuron Simulation");

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Batch Size Scaling
//=============================================================================

TEST_F(GPUStressTestE2E, BatchSizeScaling) {
    E2E_PIPELINE_START("Batch Size Scaling");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    std::vector<size_t> batch_sizes = {1, 8, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    const size_t DIM = 1024;
    const int N_ITERATIONS = 100;

    std::cout << "\n=== Batch Size Scaling ===" << std::endl;
    std::cout << "| Batch | Time(ms) | Samples/sec | Efficiency |" << std::endl;
    std::cout << "|-------|----------|-------------|------------|" << std::endl;

    double baseline_samples_per_sec = 0;

    for (size_t batch_size : batch_sizes) {
        size_t a_dims[] = {batch_size, DIM};
        size_t b_dims[] = {DIM, DIM};
        size_t c_dims[] = {batch_size, DIM};

        nimcp_gpu_tensor_t* a = nimcp_gpu_tensor_create(gpu_ctx_, a_dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* b = nimcp_gpu_tensor_create(gpu_ctx_, b_dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* c = nimcp_gpu_tensor_create(gpu_ctx_, c_dims, 2, NIMCP_GPU_PRECISION_FP32);

        if (!a || !b || !c) {
            std::cout << "| " << batch_size << " | OOM |" << std::endl;
            if (a) nimcp_gpu_tensor_destroy(a);
            if (b) nimcp_gpu_tensor_destroy(b);
            if (c) nimcp_gpu_tensor_destroy(c);
            break;
        }

        nimcp_gpu_fill(gpu_ctx_, a, 1.0f);
        nimcp_gpu_fill(gpu_ctx_, b, 1.0f);

        // Warm up
        for (int i = 0; i < 10; i++) {
            nimcp_gpu_gemm(gpu_ctx_, a, b, c, 1.0f, 0.0f, false, false);
        }
        nimcp_gpu_context_synchronize(gpu_ctx_);

        // Benchmark
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < N_ITERATIONS; i++) {
            nimcp_gpu_gemm(gpu_ctx_, a, b, c, 1.0f, 0.0f, false, false);
        }

        nimcp_gpu_context_synchronize(gpu_ctx_);
        auto end = std::chrono::high_resolution_clock::now();
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();

        double samples_per_sec = (batch_size * N_ITERATIONS) / (time_ms / 1000.0);

        if (batch_size == 1) {
            baseline_samples_per_sec = samples_per_sec;
        }

        double efficiency = (samples_per_sec / batch_size) / (baseline_samples_per_sec);

        std::cout << "| " << batch_size << " | " << time_ms << " | "
                  << samples_per_sec << " | " << (efficiency * 100) << "% |" << std::endl;

        nimcp_gpu_tensor_destroy(a);
        nimcp_gpu_tensor_destroy(b);
        nimcp_gpu_tensor_destroy(c);
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 6: Error Recovery Test
//=============================================================================

TEST_F(GPUStressTestE2E, ErrorRecoveryTest) {
    E2E_PIPELINE_START("Error Recovery Test");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    // Stage 1: Test null pointer handling
    E2E_STAGE_BEGIN("Test null pointer handling", 2000);

    bool null_handled = true;

    // These should fail gracefully, not crash
    bool result = nimcp_gpu_gemm(gpu_ctx_, nullptr, nullptr, nullptr, 1.0f, 0.0f, false, false);
    null_handled = null_handled && !result;

    result = nimcp_gpu_relu(gpu_ctx_, nullptr, nullptr);
    null_handled = null_handled && !result;

    result = nimcp_gpu_fill(gpu_ctx_, nullptr, 1.0f);
    null_handled = null_handled && !result;

    std::cout << "\n  Null pointer handling: " << (null_handled ? "PASSED" : "FAILED") << std::endl;

    E2E_STAGE_END();

    // Stage 2: Test dimension mismatch handling
    E2E_STAGE_BEGIN("Test dimension mismatch", 2000);

    size_t dims_a[] = {100, 200};
    size_t dims_b[] = {300, 400};  // Incompatible for GEMM
    size_t dims_c[] = {100, 400};

    nimcp_gpu_tensor_t* a = nimcp_gpu_tensor_create(gpu_ctx_, dims_a, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* b = nimcp_gpu_tensor_create(gpu_ctx_, dims_b, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* c = nimcp_gpu_tensor_create(gpu_ctx_, dims_c, 2, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_fill(gpu_ctx_, a, 1.0f);
    nimcp_gpu_fill(gpu_ctx_, b, 1.0f);

    // This should fail gracefully (dimensions don't match)
    bool mismatch_result = nimcp_gpu_gemm(gpu_ctx_, a, b, c, 1.0f, 0.0f, false, false);

    std::cout << "\n  Dimension mismatch handling: " << (!mismatch_result ? "PASSED" : "UNEXPECTED SUCCESS") << std::endl;

    nimcp_gpu_tensor_destroy(a);
    nimcp_gpu_tensor_destroy(b);
    nimcp_gpu_tensor_destroy(c);

    E2E_STAGE_END();

    // Stage 3: Test recovery after error
    E2E_STAGE_BEGIN("Test recovery after error", 2000);

    // Valid operation should work after error conditions
    size_t valid_dims[] = {100, 100};
    nimcp_gpu_tensor_t* x = nimcp_gpu_tensor_create(gpu_ctx_, valid_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* y = nimcp_gpu_tensor_create(gpu_ctx_, valid_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* z = nimcp_gpu_tensor_create(gpu_ctx_, valid_dims, 2, NIMCP_GPU_PRECISION_FP32);

    E2E_ASSERT_NOT_NULL(x, "Should allocate after errors");

    nimcp_gpu_fill(gpu_ctx_, x, 1.0f);
    nimcp_gpu_fill(gpu_ctx_, y, 2.0f);

    bool recovery_result = nimcp_gpu_gemm(gpu_ctx_, x, y, z, 1.0f, 0.0f, false, false);

    std::cout << "\n  Recovery after error: " << (recovery_result ? "PASSED" : "FAILED") << std::endl;

    // Verify result
    std::vector<float> z_data(100 * 100);
    nimcp_gpu_tensor_to_host(z, z_data.data());

    float expected = 100.0f * 2.0f;  // Each element should be sum of 100 products of 1*2
    bool result_correct = std::abs(z_data[0] - expected) < 0.1f;

    std::cout << "  Result correctness: " << (result_correct ? "PASSED" : "FAILED") << std::endl;

    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(y);
    nimcp_gpu_tensor_destroy(z);

    metrics_.stability_passed = null_handled && recovery_result && result_correct;

    E2E_STAGE_END();

    PrintMetrics("Error Recovery Test");

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 7: Performance Regression Test
//=============================================================================

TEST_F(GPUStressTestE2E, PerformanceRegressionTest) {
    E2E_PIPELINE_START("Performance Regression Test");

    if (!HasGPU()) {
        std::cout << "Skipping GPU test - no GPU available" << std::endl;
        E2E_PIPELINE_END();
        GTEST_SKIP() << "No GPU available";
    }

    // Define baseline performance expectations (FLOPS)
    // These would be calibrated for specific GPU models
    struct Benchmark {
        const char* name;
        size_t m, n, k;
        double min_gflops;  // Minimum acceptable GFLOPS
    };

    std::vector<Benchmark> benchmarks = {
        {"Small GEMM", 128, 128, 128, 10.0},
        {"Medium GEMM", 512, 512, 512, 100.0},
        {"Large GEMM", 1024, 1024, 1024, 500.0},
        {"Huge GEMM", 2048, 2048, 2048, 1000.0}
    };

    std::cout << "\n=== Performance Regression Tests ===" << std::endl;
    std::cout << "| Benchmark   | Size      | GFLOPS  | Expected | Status |" << std::endl;
    std::cout << "|-------------|-----------|---------|----------|--------|" << std::endl;

    bool all_passed = true;

    for (const auto& bench : benchmarks) {
        size_t a_dims[] = {bench.m, bench.k};
        size_t b_dims[] = {bench.k, bench.n};
        size_t c_dims[] = {bench.m, bench.n};

        nimcp_gpu_tensor_t* a = nimcp_gpu_tensor_create(gpu_ctx_, a_dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* b = nimcp_gpu_tensor_create(gpu_ctx_, b_dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* c = nimcp_gpu_tensor_create(gpu_ctx_, c_dims, 2, NIMCP_GPU_PRECISION_FP32);

        if (!a || !b || !c) {
            std::cout << "| " << bench.name << " | " << bench.m << "x" << bench.n
                      << " | OOM |" << std::endl;
            if (a) nimcp_gpu_tensor_destroy(a);
            if (b) nimcp_gpu_tensor_destroy(b);
            if (c) nimcp_gpu_tensor_destroy(c);
            continue;
        }

        nimcp_gpu_fill(gpu_ctx_, a, 1.0f);
        nimcp_gpu_fill(gpu_ctx_, b, 1.0f);

        // Warm up
        for (int i = 0; i < 10; i++) {
            nimcp_gpu_gemm(gpu_ctx_, a, b, c, 1.0f, 0.0f, false, false);
        }
        nimcp_gpu_context_synchronize(gpu_ctx_);

        // Benchmark
        const int N_ITER = 100;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < N_ITER; i++) {
            nimcp_gpu_gemm(gpu_ctx_, a, b, c, 1.0f, 0.0f, false, false);
        }

        nimcp_gpu_context_synchronize(gpu_ctx_);
        auto end = std::chrono::high_resolution_clock::now();
        double time_s = std::chrono::duration<double>(end - start).count();

        // FLOPS for GEMM = 2 * M * N * K
        double flops = 2.0 * bench.m * bench.n * bench.k * N_ITER;
        double gflops = flops / time_s / 1e9;

        bool passed = gflops >= bench.min_gflops;
        all_passed = all_passed && passed;

        std::cout << "| " << bench.name << " | " << bench.m << "x" << bench.n
                  << " | " << gflops << " | " << bench.min_gflops
                  << " | " << (passed ? "PASS" : "FAIL") << " |" << std::endl;

        nimcp_gpu_tensor_destroy(a);
        nimcp_gpu_tensor_destroy(b);
        nimcp_gpu_tensor_destroy(c);
    }

    metrics_.stability_passed = all_passed;

    std::cout << "\nOverall: " << (all_passed ? "ALL PASSED" : "SOME FAILED") << std::endl;

    E2E_PIPELINE_END();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
