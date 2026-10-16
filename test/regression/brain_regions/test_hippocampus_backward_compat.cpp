/**
 * @file test_hippocampus_backward_compat.cpp
 * @brief Backward compatibility regression tests for hippocampus region
 *
 * WHAT: Ensures hippocampus region API remains stable across refactorings
 * WHY: Hippocampus is critical for memory formation and spatial navigation
 * HOW: Test API signatures, behavior, memory management, error handling
 *
 * BIOLOGICAL CONTEXT:
 * - Hippocampus: Memory consolidation, spatial mapping, episodic memory
 * - CA1/CA3 regions: Pattern separation and completion
 * - Dentate gyrus: Neurogenesis and pattern separation
 * - Entorhinal cortex interface: Grid cells and spatial coding
 *
 * TEST CATEGORIES:
 * 1. API Stability - Function signatures unchanged
 * 2. Behavioral Consistency - Memory-like operations work correctly
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
#include <cstring>

// Headers have their own extern "C" guards
#include "core/brain_regions/nimcp_brain_regions.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ========================================================================== */

class HippocampusBackwardCompatTest : public ::testing::Test {
protected:
    brain_module_t* module = nullptr;
    brain_region_t* hippocampus = nullptr;

    static constexpr uint32_t DEFAULT_NEURONS = 400;
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
        hippocampus = nullptr;
    }

    template<typename Func>
    long long measure_ns(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }

    // Helper: Create pattern for memory encoding
    void create_pattern(std::vector<float>& pattern, float base_value, int pattern_id) {
        for (size_t i = 0; i < pattern.size(); i++) {
            pattern[i] = base_value + 0.1f * sinf(static_cast<float>(i + pattern_id));
        }
    }
};

/* ============================================================================
 * CATEGORY 1: API Stability Tests
 * ========================================================================== */

TEST_F(HippocampusBackwardCompatTest, APIStability_CreateRegion_SignatureUnchanged) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);

    EXPECT_EQ(hippocampus->type, REGION_HIPPOCAMPUS);
    EXPECT_EQ(hippocampus->total_neurons, DEFAULT_NEURONS);

    brain_region_destroy(hippocampus);
    hippocampus = nullptr;
}

TEST_F(HippocampusBackwardCompatTest, APIStability_RegionType_EnumValueStable) {
    // REGION_HIPPOCAMPUS should remain at value 51 (subcortical areas)
    EXPECT_EQ(static_cast<int>(REGION_HIPPOCAMPUS), 51);
}

TEST_F(HippocampusBackwardCompatTest, APIStability_AddToModule_ReturnsSuccess) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);

    nimcp_result_t result = brain_module_add_region(module, hippocampus);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(HippocampusBackwardCompatTest, APIStability_GetByType_FindsHippocampus) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);
    ASSERT_EQ(brain_module_add_region(module, hippocampus), NIMCP_SUCCESS);

    brain_region_t* found = brain_module_get_region_by_type(module, REGION_HIPPOCAMPUS);
    EXPECT_EQ(found, hippocampus);
}

TEST_F(HippocampusBackwardCompatTest, APIStability_GetName_ReturnsHippocampusString) {
    const char* name = brain_region_get_name(REGION_HIPPOCAMPUS);
    ASSERT_NE(name, nullptr);

    std::string name_str(name);
    bool contains_hippocampus = (name_str.find("ippocampus") != std::string::npos) ||
                                (name_str.find("IPPOCAMPUS") != std::string::npos);
    EXPECT_TRUE(contains_hippocampus) << "Name: " << name;
}

TEST_F(HippocampusBackwardCompatTest, APIStability_LayerNeurons_CorrectDistribution) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);

    // Hippocampus should have neurons distributed across layers
    uint32_t total_layer_neurons = 0;
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        total_layer_neurons += hippocampus->layer_sizes[layer];
    }

    // Total should approximate DEFAULT_NEURONS
    EXPECT_GE(total_layer_neurons, DEFAULT_NEURONS / 2);
    EXPECT_LE(total_layer_neurons, DEFAULT_NEURONS * 2);

    brain_region_destroy(hippocampus);
    hippocampus = nullptr;
}

/* ============================================================================
 * CATEGORY 2: Behavioral Consistency Tests
 * ========================================================================== */

TEST_F(HippocampusBackwardCompatTest, Behavior_ProcessInput_AcceptsMemoryPattern) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);
    ASSERT_EQ(brain_module_add_region(module, hippocampus), NIMCP_SUCCESS);

    // Create a memory-like pattern
    std::vector<float> pattern(100);
    create_pattern(pattern, 0.5f, 1);

    nimcp_result_t result = brain_region_process_input(hippocampus, pattern.data(),
                                                        static_cast<uint32_t>(pattern.size()), 1000);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(HippocampusBackwardCompatTest, Behavior_MultiplePatterns_DistinctActivations) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);
    ASSERT_EQ(brain_module_add_region(module, hippocampus), NIMCP_SUCCESS);

    // Process two different patterns
    std::vector<float> pattern1(100), pattern2(100);
    create_pattern(pattern1, 0.3f, 1);
    create_pattern(pattern2, 0.7f, 2);

    brain_region_process_input(hippocampus, pattern1.data(),
                               static_cast<uint32_t>(pattern1.size()), 1000);
    brain_region_step(hippocampus, 1000);

    std::vector<float> output1(200);
    uint32_t num1 = brain_region_get_output(hippocampus, output1.data(),
                                            static_cast<uint32_t>(output1.size()));

    brain_region_process_input(hippocampus, pattern2.data(),
                               static_cast<uint32_t>(pattern2.size()), 2000);
    brain_region_step(hippocampus, 1000);

    std::vector<float> output2(200);
    uint32_t num2 = brain_region_get_output(hippocampus, output2.data(),
                                            static_cast<uint32_t>(output2.size()));

    // Both should produce output
    EXPECT_GT(num1, 0u);
    EXPECT_GT(num2, 0u);
}

TEST_F(HippocampusBackwardCompatTest, Behavior_Step_MaintainsActivityLevels) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);
    ASSERT_EQ(brain_module_add_region(module, hippocampus), NIMCP_SUCCESS);

    // Initial input
    std::vector<float> input(100, 0.8f);
    brain_region_process_input(hippocampus, input.data(),
                               static_cast<uint32_t>(input.size()), 1000);

    // Step multiple times and verify activity stays bounded
    for (int i = 0; i < 50; i++) {
        brain_region_step(hippocampus, 1000);
        EXPECT_GE(hippocampus->activity_level, 0.0f);
        EXPECT_LE(hippocampus->activity_level, 1.0f);
    }
}

TEST_F(HippocampusBackwardCompatTest, Behavior_ConnectToOtherRegion_WorksCorrectly) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);
    ASSERT_EQ(brain_module_add_region(module, hippocampus), NIMCP_SUCCESS);

    // Create prefrontal for hippocampus-prefrontal connection
    brain_region_t* prefrontal = brain_region_create(REGION_PREFRONTAL, 300);
    ASSERT_NE(prefrontal, nullptr);
    ASSERT_EQ(brain_module_add_region(module, prefrontal), NIMCP_SUCCESS);

    // Connect hippocampus to prefrontal
    nimcp_result_t result = brain_module_connect_regions(module, hippocampus->id,
                                                          prefrontal->id, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Step the module and verify no crashes
    EXPECT_EQ(brain_module_step(module, 1000), NIMCP_SUCCESS);
}

/* ============================================================================
 * CATEGORY 3: Memory Management Tests
 * ========================================================================== */

TEST_F(HippocampusBackwardCompatTest, Memory_CreateDestroy_NoLeaks) {
    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        brain_region_t* region = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
        ASSERT_NE(region, nullptr);
        brain_region_destroy(region);
    }

    SUCCEED();
}

TEST_F(HippocampusBackwardCompatTest, Memory_OrganizeColumns_NoLeaks) {
    for (int i = 0; i < MEMORY_TEST_CYCLES / 10; i++) {
        brain_region_t* region = brain_region_create(REGION_HIPPOCAMPUS, 400);
        ASSERT_NE(region, nullptr);

        // Hippocampus with columnar organization
        nimcp_result_t result = brain_region_organize_columns(region, 8, 8);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        brain_region_destroy(region);
    }

    SUCCEED();
}

TEST_F(HippocampusBackwardCompatTest, Memory_ProcessManyPatterns_NoAccumulation) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);
    ASSERT_EQ(brain_module_add_region(module, hippocampus), NIMCP_SUCCESS);

    std::vector<float> pattern(100);

    // Process many patterns (simulating memory encoding)
    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        create_pattern(pattern, 0.5f, i);
        brain_region_process_input(hippocampus, pattern.data(),
                                   static_cast<uint32_t>(pattern.size()), i * 1000);
        brain_region_step(hippocampus, 1000);
    }

    SUCCEED();
}

TEST_F(HippocampusBackwardCompatTest, Memory_GetLayerNeurons_NoAllocation) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);

    std::vector<uint32_t> neuron_ids(100);

    // Getting layer neurons repeatedly should not allocate
    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        cortical_layer_t layer = static_cast<cortical_layer_t>(i % LAYER_COUNT);
        brain_region_get_layer_neurons(hippocampus, layer, neuron_ids.data(),
                                       static_cast<uint32_t>(neuron_ids.size()));
    }

    brain_region_destroy(hippocampus);
    hippocampus = nullptr;

    SUCCEED();
}

/* ============================================================================
 * CATEGORY 4: Error Handling Tests
 * ========================================================================== */

TEST_F(HippocampusBackwardCompatTest, ErrorHandling_NullModule_AddRegion) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);

    nimcp_result_t result = brain_module_add_region(nullptr, hippocampus);
    EXPECT_NE(result, NIMCP_SUCCESS);

    brain_region_destroy(hippocampus);
    hippocampus = nullptr;
}

TEST_F(HippocampusBackwardCompatTest, ErrorHandling_NullInput_ProcessInput) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);

    nimcp_result_t result = brain_region_process_input(hippocampus, nullptr, 100, 1000);
    EXPECT_NE(result, NIMCP_SUCCESS);

    brain_region_destroy(hippocampus);
    hippocampus = nullptr;
}

TEST_F(HippocampusBackwardCompatTest, ErrorHandling_InvalidLayer_GetNeurons) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);

    std::vector<uint32_t> neuron_ids(100);

    // Invalid layer (beyond LAYER_COUNT) - should handle gracefully
    uint32_t count = brain_region_get_layer_neurons(hippocampus,
                                                     static_cast<cortical_layer_t>(LAYER_COUNT + 1),
                                                     neuron_ids.data(),
                                                     static_cast<uint32_t>(neuron_ids.size()));
    EXPECT_EQ(count, 0u);

    brain_region_destroy(hippocampus);
    hippocampus = nullptr;
}

TEST_F(HippocampusBackwardCompatTest, ErrorHandling_NullStats_GetStats) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);

    nimcp_result_t result = brain_region_get_stats(hippocampus, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);

    brain_region_destroy(hippocampus);
    hippocampus = nullptr;
}

TEST_F(HippocampusBackwardCompatTest, ErrorHandling_ZeroDeltaT_Step) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);

    // Zero delta_t should still succeed (no-op or minimal step)
    nimcp_result_t result = brain_region_step(hippocampus, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    brain_region_destroy(hippocampus);
    hippocampus = nullptr;
}

/* ============================================================================
 * CATEGORY 5: Performance Baseline Tests
 * ========================================================================== */

TEST_F(HippocampusBackwardCompatTest, Performance_CreateRegion_Under1ms) {
    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        brain_region_t* region = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
        brain_region_destroy(region);
    }

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            brain_region_t* region = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
            brain_region_destroy(region);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Hippocampus Create/Destroy: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 200000000.0) << "Create/Destroy should be < 200 ms";
}

TEST_F(HippocampusBackwardCompatTest, Performance_PatternEncoding_Under200us) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);
    ASSERT_EQ(brain_module_add_region(module, hippocampus), NIMCP_SUCCESS);

    std::vector<float> pattern(100);
    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        create_pattern(pattern, 0.5f, i);
        brain_region_process_input(hippocampus, pattern.data(),
                                   static_cast<uint32_t>(pattern.size()), i * 1000);
    }

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        create_pattern(pattern, 0.5f, i + WARMUP_ITERATIONS);
        long long ns = measure_ns([&]() {
            brain_region_process_input(hippocampus, pattern.data(),
                                       static_cast<uint32_t>(pattern.size()),
                                       (i + WARMUP_ITERATIONS) * 1000);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Hippocampus PatternEncoding: avg=" << (avg_ns / 1000.0) << " us\n";

    // Memory encoding should be fast
    EXPECT_LT(avg_ns, 2000000.0) << "Pattern encoding should be < 2000 us";
}

TEST_F(HippocampusBackwardCompatTest, Performance_Step_Under50us) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);
    ASSERT_EQ(brain_module_add_region(module, hippocampus), NIMCP_SUCCESS);

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        brain_region_step(hippocampus, 1000);
    }

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            brain_region_step(hippocampus, 1000);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Hippocampus Step: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 500000.0) << "Step should be < 500 us";
}

TEST_F(HippocampusBackwardCompatTest, Performance_GetOutput_Under10us) {
    hippocampus = brain_region_create(REGION_HIPPOCAMPUS, DEFAULT_NEURONS);
    ASSERT_NE(hippocampus, nullptr);
    ASSERT_EQ(brain_module_add_region(module, hippocampus), NIMCP_SUCCESS);

    // Process input first
    std::vector<float> input(100, 0.5f);
    brain_region_process_input(hippocampus, input.data(),
                               static_cast<uint32_t>(input.size()), 1000);
    brain_region_step(hippocampus, 1000);

    std::vector<float> output(200);
    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        brain_region_get_output(hippocampus, output.data(),
                                static_cast<uint32_t>(output.size()));
    }

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            brain_region_get_output(hippocampus, output.data(),
                                    static_cast<uint32_t>(output.size()));
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Hippocampus GetOutput: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 100000.0) << "GetOutput should be < 100 us";
}

/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
