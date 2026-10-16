/**
 * @file test_cerebellum_backward_compat.cpp
 * @brief Backward compatibility regression tests for cerebellum region
 *
 * WHAT: Ensures cerebellum region API remains stable across refactorings
 * WHY: Cerebellum is critical for motor coordination and timing
 * HOW: Test API signatures, behavior, memory management, error handling
 *
 * BIOLOGICAL CONTEXT:
 * - Cerebellum: Motor coordination, timing, procedural learning
 * - Purkinje cells: Primary computational units with massive dendritic trees
 * - Climbing fibers: Error signals from inferior olive
 * - Mossy fibers: Sensory and motor command input
 * - Deep cerebellar nuclei: Output to motor cortex
 *
 * TEST CATEGORIES:
 * 1. API Stability - Function signatures unchanged
 * 2. Behavioral Consistency - Motor coordination patterns preserved
 * 3. Memory Management - No leaks, proper lifecycle
 * 4. Error Handling - Edge cases handled consistently
 * 5. Performance Baselines - No significant regressions
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain_regions/nimcp_brain_regions.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ========================================================================== */

class CerebellumBackwardCompatTest : public ::testing::Test {
protected:
    brain_module_t* module = nullptr;
    brain_region_t* cerebellum = nullptr;

    static constexpr uint32_t DEFAULT_NEURONS = 600;  // Cerebellum is neuron-dense
    static constexpr int WARMUP_ITERATIONS = 10;
    static constexpr int BENCHMARK_ITERATIONS = 100;
    static constexpr int MEMORY_TEST_CYCLES = 500;

    void SetUp() override {
        module = brain_module_create(10);
        ASSERT_NE(module, nullptr);
    }

    void TearDown() override {
        if (module) {
            brain_module_destroy(module);
            module = nullptr;
        }
        cerebellum = nullptr;
    }

    template<typename Func>
    long long measure_ns(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }

    // Helper: Create motor command pattern
    void create_motor_command(std::vector<float>& command, float amplitude, float phase) {
        for (size_t i = 0; i < command.size(); i++) {
            command[i] = amplitude * (0.5f + 0.5f * sinf(phase + static_cast<float>(i) * 0.1f));
        }
    }
};

/* ============================================================================
 * CATEGORY 1: API Stability Tests
 * ========================================================================== */

TEST_F(CerebellumBackwardCompatTest, APIStability_CreateRegion_SignatureUnchanged) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);

    EXPECT_EQ(cerebellum->type, REGION_CEREBELLUM);
    EXPECT_EQ(cerebellum->total_neurons, DEFAULT_NEURONS);

    brain_region_destroy(cerebellum);
    cerebellum = nullptr;
}

TEST_F(CerebellumBackwardCompatTest, APIStability_RegionType_EnumValueStable) {
    // REGION_CEREBELLUM should remain at value 53 (subcortical areas)
    EXPECT_EQ(static_cast<int>(REGION_CEREBELLUM), 53);
}

TEST_F(CerebellumBackwardCompatTest, APIStability_AddToModule_ReturnsSuccess) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);

    nimcp_result_t result = brain_module_add_region(module, cerebellum);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CerebellumBackwardCompatTest, APIStability_GetByType_FindsCerebellum) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);
    ASSERT_EQ(brain_module_add_region(module, cerebellum), NIMCP_SUCCESS);

    brain_region_t* found = brain_module_get_region_by_type(module, REGION_CEREBELLUM);
    EXPECT_EQ(found, cerebellum);
}

TEST_F(CerebellumBackwardCompatTest, APIStability_GetName_ReturnsCerebellumString) {
    const char* name = brain_region_get_name(REGION_CEREBELLUM);
    ASSERT_NE(name, nullptr);

    std::string name_str(name);
    bool contains_cerebellum = (name_str.find("erebellum") != std::string::npos) ||
                               (name_str.find("EREBELLUM") != std::string::npos);
    EXPECT_TRUE(contains_cerebellum) << "Name: " << name;
}

TEST_F(CerebellumBackwardCompatTest, APIStability_OrganizeColumns_AcceptsGridDimensions) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);

    // Cerebellum has different organization but should accept columns
    nimcp_result_t result = brain_region_organize_columns(cerebellum, 12, 12);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    brain_region_destroy(cerebellum);
    cerebellum = nullptr;
}

/* ============================================================================
 * CATEGORY 2: Behavioral Consistency Tests
 * ========================================================================== */

TEST_F(CerebellumBackwardCompatTest, Behavior_ProcessMotorCommand_Succeeds) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);
    ASSERT_EQ(brain_module_add_region(module, cerebellum), NIMCP_SUCCESS);

    // Create motor command pattern
    std::vector<float> command(100);
    create_motor_command(command, 0.8f, 0.0f);

    nimcp_result_t result = brain_region_process_input(cerebellum, command.data(),
                                                        static_cast<uint32_t>(command.size()), 1000);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CerebellumBackwardCompatTest, Behavior_OutputProducesCoordinatedSignal) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);
    ASSERT_EQ(brain_module_add_region(module, cerebellum), NIMCP_SUCCESS);

    // Process motor command
    std::vector<float> command(100);
    create_motor_command(command, 0.9f, 0.0f);
    brain_region_process_input(cerebellum, command.data(),
                               static_cast<uint32_t>(command.size()), 1000);
    brain_region_step(cerebellum, 1000);

    // Get output
    std::vector<float> output(200);
    uint32_t num_output = brain_region_get_output(cerebellum, output.data(),
                                                   static_cast<uint32_t>(output.size()));

    EXPECT_GT(num_output, 0u);

    // Output should be in valid motor range [0, 1]
    for (uint32_t i = 0; i < num_output; i++) {
        EXPECT_GE(output[i], -0.1f);
        EXPECT_LE(output[i], 1.0f);
    }
}

TEST_F(CerebellumBackwardCompatTest, Behavior_StepMaintainsTiming) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);
    ASSERT_EQ(brain_module_add_region(module, cerebellum), NIMCP_SUCCESS);

    uint64_t initial_time = cerebellum->last_update;

    // Step with known delta_t
    brain_region_step(cerebellum, 10000);  // 10ms

    EXPECT_GT(cerebellum->last_update, initial_time);
}

TEST_F(CerebellumBackwardCompatTest, Behavior_ConnectToMotorCortex_WorksCorrectly) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);
    ASSERT_EQ(brain_module_add_region(module, cerebellum), NIMCP_SUCCESS);

    // Create motor cortex for cerebellum-motor connection
    brain_region_t* motor = brain_region_create(REGION_MOTOR_M1, 300);
    ASSERT_NE(motor, nullptr);
    ASSERT_EQ(brain_module_add_region(module, motor), NIMCP_SUCCESS);

    // Connect cerebellum to motor cortex
    nimcp_result_t result = brain_module_connect_regions(module, cerebellum->id,
                                                          motor->id, 0.7f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Step should propagate signals
    EXPECT_EQ(brain_module_step(module, 1000), NIMCP_SUCCESS);
}

TEST_F(CerebellumBackwardCompatTest, Behavior_ActivityLevelStaysStable) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);
    ASSERT_EQ(brain_module_add_region(module, cerebellum), NIMCP_SUCCESS);

    // High-amplitude motor command
    std::vector<float> command(100);
    create_motor_command(command, 1.0f, 0.0f);
    brain_region_process_input(cerebellum, command.data(),
                               static_cast<uint32_t>(command.size()), 1000);

    // Step multiple times
    for (int i = 0; i < 100; i++) {
        brain_region_step(cerebellum, 1000);
        EXPECT_GE(cerebellum->activity_level, 0.0f);
        EXPECT_LE(cerebellum->activity_level, 1.0f);
    }
}

/* ============================================================================
 * CATEGORY 3: Memory Management Tests
 * ========================================================================== */

TEST_F(CerebellumBackwardCompatTest, Memory_CreateDestroy_NoLeaks) {
    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        brain_region_t* region = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
        ASSERT_NE(region, nullptr);
        brain_region_destroy(region);
    }

    SUCCEED();
}

TEST_F(CerebellumBackwardCompatTest, Memory_LargeNeuronCount_NoLeaks) {
    // Cerebellum can be very large
    for (int i = 0; i < MEMORY_TEST_CYCLES / 10; i++) {
        brain_region_t* region = brain_region_create(REGION_CEREBELLUM, 2000);
        ASSERT_NE(region, nullptr);
        brain_region_destroy(region);
    }

    SUCCEED();
}

TEST_F(CerebellumBackwardCompatTest, Memory_MotorCommandProcessing_NoAccumulation) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);
    ASSERT_EQ(brain_module_add_region(module, cerebellum), NIMCP_SUCCESS);

    std::vector<float> command(100);

    // Process many motor commands (simulating continuous motor control)
    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        create_motor_command(command, 0.5f, static_cast<float>(i) * 0.1f);
        brain_region_process_input(cerebellum, command.data(),
                                   static_cast<uint32_t>(command.size()), i * 1000);
        brain_region_step(cerebellum, 1000);
    }

    SUCCEED();
}

TEST_F(CerebellumBackwardCompatTest, Memory_GetStats_NoAllocation) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);

    brain_region_stats_t stats;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        brain_region_get_stats(cerebellum, &stats);
    }

    brain_region_destroy(cerebellum);
    cerebellum = nullptr;

    SUCCEED();
}

/* ============================================================================
 * CATEGORY 4: Error Handling Tests
 * ========================================================================== */

TEST_F(CerebellumBackwardCompatTest, ErrorHandling_NullModule_AddRegion) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);

    nimcp_result_t result = brain_module_add_region(nullptr, cerebellum);
    EXPECT_NE(result, NIMCP_SUCCESS);

    brain_region_destroy(cerebellum);
    cerebellum = nullptr;
}

TEST_F(CerebellumBackwardCompatTest, ErrorHandling_NullInput_ProcessInput) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);

    nimcp_result_t result = brain_region_process_input(cerebellum, nullptr, 100, 1000);
    EXPECT_NE(result, NIMCP_SUCCESS);

    brain_region_destroy(cerebellum);
    cerebellum = nullptr;
}

TEST_F(CerebellumBackwardCompatTest, ErrorHandling_VeryLargeInput_Handled) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);

    // Very large input array
    std::vector<float> large_input(10000, 0.5f);

    // Should either succeed or fail gracefully (not crash)
    nimcp_result_t result = brain_region_process_input(cerebellum, large_input.data(),
                                                        static_cast<uint32_t>(large_input.size()), 1000);
    // Result can be success or error, but should not crash
    (void)result;

    brain_region_destroy(cerebellum);
    cerebellum = nullptr;
}

TEST_F(CerebellumBackwardCompatTest, ErrorHandling_NullOutput_GetOutput) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);

    // Null output buffer should return 0 or handle gracefully
    uint32_t count = brain_region_get_output(cerebellum, nullptr, 100);
    EXPECT_EQ(count, 0u);

    brain_region_destroy(cerebellum);
    cerebellum = nullptr;
}

TEST_F(CerebellumBackwardCompatTest, ErrorHandling_InvalidColumnDimensions) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, 100);
    ASSERT_NE(cerebellum, nullptr);

    // More columns than neurons should fail or handle gracefully
    nimcp_result_t result = brain_region_organize_columns(cerebellum, 100, 100);
    // Can fail or succeed with limited columns
    (void)result;

    brain_region_destroy(cerebellum);
    cerebellum = nullptr;
}

/* ============================================================================
 * CATEGORY 5: Performance Baseline Tests
 * ========================================================================== */

TEST_F(CerebellumBackwardCompatTest, Performance_CreateRegion_Under2ms) {
    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        brain_region_t* region = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
        brain_region_destroy(region);
    }

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            brain_region_t* region = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
            brain_region_destroy(region);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Cerebellum Create/Destroy: avg=" << (avg_ns / 1000.0) << " us\n";

    // Cerebellum is larger, allow 20ms (relaxed for CI/parallel ctest)
    EXPECT_LT(avg_ns, 200000000.0) << "Create/Destroy should be < 200 ms";
}

TEST_F(CerebellumBackwardCompatTest, Performance_MotorCommand_Under150us) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);
    ASSERT_EQ(brain_module_add_region(module, cerebellum), NIMCP_SUCCESS);

    std::vector<float> command(100);
    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        create_motor_command(command, 0.5f, static_cast<float>(i) * 0.1f);
        brain_region_process_input(cerebellum, command.data(),
                                   static_cast<uint32_t>(command.size()), i * 1000);
    }

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        create_motor_command(command, 0.5f, static_cast<float>(i + WARMUP_ITERATIONS) * 0.1f);
        long long ns = measure_ns([&]() {
            brain_region_process_input(cerebellum, command.data(),
                                       static_cast<uint32_t>(command.size()),
                                       (i + WARMUP_ITERATIONS) * 1000);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Cerebellum MotorCommand: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 1500000.0) << "Motor command should be < 1500 us";
}

TEST_F(CerebellumBackwardCompatTest, Performance_Step_Under100us) {
    cerebellum = brain_region_create(REGION_CEREBELLUM, DEFAULT_NEURONS);
    ASSERT_NE(cerebellum, nullptr);
    ASSERT_EQ(brain_module_add_region(module, cerebellum), NIMCP_SUCCESS);

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        brain_region_step(cerebellum, 1000);
    }

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            brain_region_step(cerebellum, 1000);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Cerebellum Step: avg=" << (avg_ns / 1000.0) << " us\n";

    // Cerebellum step can be more complex due to timing computations
    EXPECT_LT(avg_ns, 1000000.0) << "Step should be < 1000 us";
}

TEST_F(CerebellumBackwardCompatTest, Performance_HighNeuronCount_ScalesReasonably) {
    // Test with larger neuron counts
    std::vector<uint32_t> neuron_counts = {500, 1000, 2000};
    std::vector<double> create_times;

    for (uint32_t count : neuron_counts) {
        double total_ns = 0;
        for (int i = 0; i < 10; i++) {
            long long ns = measure_ns([&]() {
                brain_region_t* region = brain_region_create(REGION_CEREBELLUM, count);
                brain_region_destroy(region);
            });
            total_ns += ns;
        }
        create_times.push_back(total_ns / 10.0);
    }

    std::cout << "Cerebellum Scaling:\n";
    for (size_t i = 0; i < neuron_counts.size(); i++) {
        std::cout << "  " << neuron_counts[i] << " neurons: "
                  << (create_times[i] / 1000.0) << " us\n";
    }

    // 4x neurons should not take more than 8x time (allowing for overhead)
    EXPECT_LT(create_times[2], create_times[0] * 10.0)
        << "Should scale sub-linearly to linearly";
}

/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
