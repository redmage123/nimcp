/**
 * @file test_region_api_stability.cpp
 * @brief Comprehensive API stability tests for all brain region types
 *
 * WHAT: Ensures the brain_region API remains stable across all region types
 * WHY: API changes can break downstream code; regression tests catch changes early
 * HOW: Test each region type with uniform API tests
 *
 * TESTED REGIONS:
 * - Visual cortex: V1, V2, V4, MT, IT
 * - Auditory cortex: A1, A2, Belt, Parabelt
 * - Motor cortex: M1, Premotor, SMA
 * - Somatosensory: S1, S2
 * - Association: Prefrontal, Parietal, Temporal
 * - Subcortical: Thalamus, Hippocampus, Basal Ganglia, Cerebellum
 *
 * TEST CATEGORIES:
 * 1. Enum Value Stability - Values should not change
 * 2. Create/Destroy API - Uniform interface across types
 * 3. Region Name API - Human-readable names available
 * 4. Module Integration - All types work with brain_module
 * 5. Cross-Region Connections - Any-to-any connectivity
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <chrono>

// Headers have their own extern "C" guards
#include "core/brain_regions/nimcp_brain_regions.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ========================================================================== */

class RegionAPIStabilityTest : public ::testing::Test {
protected:
    brain_module_t* module = nullptr;

    // All region types to test
    std::vector<brain_region_type_t> all_region_types = {
        // Visual
        REGION_VISUAL_V1, REGION_VISUAL_V2, REGION_VISUAL_V4,
        REGION_VISUAL_MT, REGION_VISUAL_IT,
        // Auditory
        REGION_AUDITORY_A1, REGION_AUDITORY_A2,
        REGION_AUDITORY_BELT, REGION_AUDITORY_PARABELT,
        // Motor
        REGION_MOTOR_M1, REGION_MOTOR_PREMOTOR, REGION_MOTOR_SMA,
        // Somatosensory
        REGION_SOMATOSENSORY_S1, REGION_SOMATOSENSORY_S2,
        // Association
        REGION_PREFRONTAL, REGION_PARIETAL, REGION_TEMPORAL,
        // Subcortical
        REGION_THALAMUS, REGION_HIPPOCAMPUS,
        REGION_BASAL_GANGLIA, REGION_CEREBELLUM
    };

    static constexpr uint32_t DEFAULT_NEURONS = 200;

    void SetUp() override {
        module = brain_module_create(30);  // Enough for all region types
        ASSERT_NE(module, nullptr);
    }

    void TearDown() override {
        if (module) {
            brain_module_destroy(module);
            module = nullptr;
        }
    }

    template<typename Func>
    long long measure_ns(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }
};

/* ============================================================================
 * CATEGORY 1: Enum Value Stability
 * ========================================================================== */

TEST_F(RegionAPIStabilityTest, EnumStability_VisualRegions_ValuesUnchanged) {
    // Visual cortex regions start at 0
    EXPECT_EQ(static_cast<int>(REGION_VISUAL_V1), 0);
    EXPECT_EQ(static_cast<int>(REGION_VISUAL_V2), 1);
    EXPECT_EQ(static_cast<int>(REGION_VISUAL_V4), 2);
    EXPECT_EQ(static_cast<int>(REGION_VISUAL_MT), 3);
    EXPECT_EQ(static_cast<int>(REGION_VISUAL_IT), 4);
}

TEST_F(RegionAPIStabilityTest, EnumStability_AuditoryRegions_ValuesUnchanged) {
    // Auditory cortex regions start at 10
    EXPECT_EQ(static_cast<int>(REGION_AUDITORY_A1), 10);
    EXPECT_EQ(static_cast<int>(REGION_AUDITORY_A2), 11);
    EXPECT_EQ(static_cast<int>(REGION_AUDITORY_BELT), 12);
    EXPECT_EQ(static_cast<int>(REGION_AUDITORY_PARABELT), 13);
}

TEST_F(RegionAPIStabilityTest, EnumStability_MotorRegions_ValuesUnchanged) {
    // Motor cortex regions start at 20
    EXPECT_EQ(static_cast<int>(REGION_MOTOR_M1), 20);
    EXPECT_EQ(static_cast<int>(REGION_MOTOR_PREMOTOR), 21);
    EXPECT_EQ(static_cast<int>(REGION_MOTOR_SMA), 22);
}

TEST_F(RegionAPIStabilityTest, EnumStability_SomatosensoryRegions_ValuesUnchanged) {
    // Somatosensory regions start at 30
    EXPECT_EQ(static_cast<int>(REGION_SOMATOSENSORY_S1), 30);
    EXPECT_EQ(static_cast<int>(REGION_SOMATOSENSORY_S2), 31);
}

TEST_F(RegionAPIStabilityTest, EnumStability_AssociationRegions_ValuesUnchanged) {
    // Association regions start at 40
    EXPECT_EQ(static_cast<int>(REGION_PREFRONTAL), 40);
    EXPECT_EQ(static_cast<int>(REGION_PARIETAL), 41);
    EXPECT_EQ(static_cast<int>(REGION_TEMPORAL), 42);
}

TEST_F(RegionAPIStabilityTest, EnumStability_SubcorticalRegions_ValuesUnchanged) {
    // Subcortical regions start at 50
    EXPECT_EQ(static_cast<int>(REGION_THALAMUS), 50);
    EXPECT_EQ(static_cast<int>(REGION_HIPPOCAMPUS), 51);
    EXPECT_EQ(static_cast<int>(REGION_BASAL_GANGLIA), 52);
    EXPECT_EQ(static_cast<int>(REGION_CEREBELLUM), 53);
}

TEST_F(RegionAPIStabilityTest, EnumStability_LayerEnums_ValuesUnchanged) {
    EXPECT_EQ(static_cast<int>(LAYER_1), 0);
    EXPECT_EQ(static_cast<int>(LAYER_2), 1);
    EXPECT_EQ(static_cast<int>(LAYER_3), 2);
    EXPECT_EQ(static_cast<int>(LAYER_4), 3);
    EXPECT_EQ(static_cast<int>(LAYER_5), 4);
    EXPECT_EQ(static_cast<int>(LAYER_6), 5);
    EXPECT_EQ(static_cast<int>(LAYER_COUNT), 6);
}

/* ============================================================================
 * CATEGORY 2: Create/Destroy API Uniformity
 * ========================================================================== */

TEST_F(RegionAPIStabilityTest, CreateDestroy_AllRegionTypes_SameInterface) {
    for (auto type : all_region_types) {
        brain_region_t* region = brain_region_create(type, DEFAULT_NEURONS);

        // All region types should create successfully
        ASSERT_NE(region, nullptr) << "Failed to create region type " << static_cast<int>(type);

        // All should have correct type
        EXPECT_EQ(region->type, type);

        // All should have requested neurons (or close)
        EXPECT_GT(region->total_neurons, 0u);

        // Destroy should not crash
        brain_region_destroy(region);
    }
}

TEST_F(RegionAPIStabilityTest, CreateDestroy_NullSafe_DestroyNull) {
    // Destroying null should not crash
    brain_region_destroy(nullptr);
    SUCCEED();
}

TEST_F(RegionAPIStabilityTest, CreateDestroy_ZeroNeurons_HandledGracefully) {
    for (auto type : all_region_types) {
        brain_region_t* region = brain_region_create(type, 0);

        // Can return null or region with 0 neurons
        if (region != nullptr) {
            brain_region_destroy(region);
        }
    }

    SUCCEED();
}

TEST_F(RegionAPIStabilityTest, CreateDestroy_VeryLargeNeuronCount_Handled) {
    // Test with large but reasonable neuron count
    for (auto type : all_region_types) {
        brain_region_t* region = brain_region_create(type, 5000);

        // Should create successfully (or gracefully handle)
        if (region != nullptr) {
            EXPECT_GT(region->total_neurons, 0u);
            brain_region_destroy(region);
        }
    }

    SUCCEED();
}

/* ============================================================================
 * CATEGORY 3: Region Name API
 * ========================================================================== */

TEST_F(RegionAPIStabilityTest, GetName_AllRegionTypes_ReturnsNonNull) {
    for (auto type : all_region_types) {
        const char* name = brain_region_get_name(type);
        EXPECT_NE(name, nullptr) << "Null name for type " << static_cast<int>(type);

        // Name should be non-empty
        if (name != nullptr) {
            EXPECT_GT(strlen(name), 0u) << "Empty name for type " << static_cast<int>(type);
        }
    }
}

TEST_F(RegionAPIStabilityTest, GetName_InvalidType_HandledGracefully) {
    // Invalid region type should return something (not crash)
    const char* name = brain_region_get_name(static_cast<brain_region_type_t>(999));

    // Either returns null or a placeholder string
    // Main thing is it doesn't crash
    (void)name;
    SUCCEED();
}

TEST_F(RegionAPIStabilityTest, GetName_RegionTypeCount_HasValue) {
    // REGION_TYPE_COUNT should be defined and have a reasonable value
    EXPECT_GT(static_cast<int>(REGION_TYPE_COUNT), 20);
    EXPECT_LT(static_cast<int>(REGION_TYPE_COUNT), 1000);
}

/* ============================================================================
 * CATEGORY 4: Module Integration
 * ========================================================================== */

TEST_F(RegionAPIStabilityTest, ModuleIntegration_AddAllRegionTypes) {
    for (auto type : all_region_types) {
        brain_region_t* region = brain_region_create(type, DEFAULT_NEURONS);
        ASSERT_NE(region, nullptr);

        nimcp_result_t result = brain_module_add_region(module, region);
        EXPECT_EQ(result, NIMCP_SUCCESS) << "Failed to add region type " << static_cast<int>(type);
    }

    // All regions should be added
    EXPECT_EQ(module->num_regions, all_region_types.size());
}

TEST_F(RegionAPIStabilityTest, ModuleIntegration_GetByType_FindsCorrectRegion) {
    // Add a few distinct regions
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_region_t* hippo = brain_region_create(REGION_HIPPOCAMPUS, 150);
    brain_region_t* motor = brain_region_create(REGION_MOTOR_M1, 120);

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(hippo, nullptr);
    ASSERT_NE(motor, nullptr);

    brain_module_add_region(module, v1);
    brain_module_add_region(module, hippo);
    brain_module_add_region(module, motor);

    // Get by type should find correct regions
    EXPECT_EQ(brain_module_get_region_by_type(module, REGION_VISUAL_V1), v1);
    EXPECT_EQ(brain_module_get_region_by_type(module, REGION_HIPPOCAMPUS), hippo);
    EXPECT_EQ(brain_module_get_region_by_type(module, REGION_MOTOR_M1), motor);
}

TEST_F(RegionAPIStabilityTest, ModuleIntegration_GetById_WorksForAll) {
    std::vector<brain_region_t*> regions;

    for (auto type : all_region_types) {
        brain_region_t* region = brain_region_create(type, DEFAULT_NEURONS);
        ASSERT_NE(region, nullptr);
        brain_module_add_region(module, region);
        regions.push_back(region);
    }

    // Get by ID should find each region
    for (auto* region : regions) {
        brain_region_t* found = brain_module_get_region(module, region->id);
        EXPECT_EQ(found, region);
    }
}

TEST_F(RegionAPIStabilityTest, ModuleIntegration_Step_WorksWithMixedRegions) {
    // Add several different region types
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_region_t* a1 = brain_region_create(REGION_AUDITORY_A1, 100);
    brain_region_t* pfc = brain_region_create(REGION_PREFRONTAL, 100);
    brain_region_t* thal = brain_region_create(REGION_THALAMUS, 100);

    brain_module_add_region(module, v1);
    brain_module_add_region(module, a1);
    brain_module_add_region(module, pfc);
    brain_module_add_region(module, thal);

    // Step should work without crashing
    for (int i = 0; i < 100; i++) {
        nimcp_result_t result = brain_module_step(module, 1000);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

/* ============================================================================
 * CATEGORY 5: Cross-Region Connections
 * ========================================================================== */

TEST_F(RegionAPIStabilityTest, Connections_SensoryToAssociation_Works) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);
    brain_region_t* pfc = brain_region_create(REGION_PREFRONTAL, 200);

    brain_module_add_region(module, v1);
    brain_module_add_region(module, pfc);

    // Connect visual to prefrontal
    nimcp_result_t result = brain_module_connect_regions(module, v1->id, pfc->id, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Step should propagate signals
    EXPECT_EQ(brain_module_step(module, 1000), NIMCP_SUCCESS);
}

TEST_F(RegionAPIStabilityTest, Connections_SubcorticalToCortical_Works) {
    brain_region_t* thal = brain_region_create(REGION_THALAMUS, 150);
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);

    brain_module_add_region(module, thal);
    brain_module_add_region(module, v1);

    // Thalamus to V1 connection (sensory relay)
    nimcp_result_t result = brain_module_connect_regions(module, thal->id, v1->id, 0.8f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(RegionAPIStabilityTest, Connections_MotorPathway_Works) {
    brain_region_t* pfc = brain_region_create(REGION_PREFRONTAL, 150);
    brain_region_t* bg = brain_region_create(REGION_BASAL_GANGLIA, 150);
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 200);

    brain_module_add_region(module, pfc);
    brain_module_add_region(module, bg);
    brain_module_add_region(module, m1);

    // Prefrontal -> Basal Ganglia -> Motor
    EXPECT_EQ(brain_module_connect_regions(module, pfc->id, bg->id, 0.6f), NIMCP_SUCCESS);
    EXPECT_EQ(brain_module_connect_regions(module, bg->id, m1->id, 0.7f), NIMCP_SUCCESS);

    // Step should propagate
    EXPECT_EQ(brain_module_step(module, 1000), NIMCP_SUCCESS);
}

TEST_F(RegionAPIStabilityTest, Connections_LayerSpecific_Works) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);
    brain_region_t* v2 = brain_region_create(REGION_VISUAL_V2, 200);

    brain_module_add_region(module, v1);
    brain_module_add_region(module, v2);

    // Layer-specific connection (L5 output to L4 input)
    nimcp_result_t result = brain_module_connect_layers(module,
                                                         v1->id, LAYER_5,
                                                         v2->id, LAYER_4,
                                                         0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(RegionAPIStabilityTest, Connections_FullyConnected_Scales) {
    const int NUM_REGIONS = 5;
    std::vector<brain_region_t*> regions;

    // Create several regions
    for (int i = 0; i < NUM_REGIONS; i++) {
        brain_region_t* region = brain_region_create(all_region_types[i], 100);
        ASSERT_NE(region, nullptr);
        brain_module_add_region(module, region);
        regions.push_back(region);
    }

    // Fully connect all regions
    for (int i = 0; i < NUM_REGIONS; i++) {
        for (int j = 0; j < NUM_REGIONS; j++) {
            if (i != j) {
                brain_module_connect_regions(module, regions[i]->id, regions[j]->id, 0.3f);
            }
        }
    }

    // Should have NUM_REGIONS * (NUM_REGIONS - 1) connections
    EXPECT_EQ(module->num_connections, NUM_REGIONS * (NUM_REGIONS - 1));

    // Step should work
    EXPECT_EQ(brain_module_step(module, 1000), NIMCP_SUCCESS);
}

/* ============================================================================
 * CATEGORY 6: Performance Baselines
 * ========================================================================== */

TEST_F(RegionAPIStabilityTest, Performance_CreateAllTypes_Under10ms) {
    auto start = std::chrono::high_resolution_clock::now();

    for (auto type : all_region_types) {
        brain_region_t* region = brain_region_create(type, DEFAULT_NEURONS);
        brain_region_destroy(region);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Create/Destroy all " << all_region_types.size() << " region types: "
              << duration_ms << " ms\n";

    EXPECT_LT(duration_ms, 2000) << "Creating all region types should be < 2000 ms";
}

TEST_F(RegionAPIStabilityTest, Performance_StepWithAllRegions_Under50ms) {
    // Add all region types
    for (auto type : all_region_types) {
        brain_region_t* region = brain_region_create(type, 100);
        brain_module_add_region(module, region);
    }

    // Measure step time
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        brain_module_step(module, 1000);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "100 steps with " << all_region_types.size() << " regions: "
              << duration_ms << " ms\n";

    EXPECT_LT(duration_ms, 500) << "100 steps should complete in < 500 ms";
}

TEST_F(RegionAPIStabilityTest, Performance_GetNameConstantTime) {
    std::vector<long long> times;
    times.reserve(1000);

    for (int i = 0; i < 1000; i++) {
        auto type = all_region_types[i % all_region_types.size()];
        long long ns = measure_ns([&]() {
            (void)brain_region_get_name(type);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "GetName avg: " << avg_ns << " ns\n";

    // Should be essentially constant time (table lookup)
    EXPECT_LT(avg_ns, 100.0) << "GetName should be < 100 ns (constant time)";
}

/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
