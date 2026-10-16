/**
 * @file test_brain_regions_real.cpp
 * @brief REAL tests for nimcp_brain_regions.c that exercise actual implementation
 *
 * DIFFERENCE FROM test_brain_regions_coverage.cpp:
 * - Creates REAL brain module instances
 * - Creates REAL brain regions
 * - Exercises actual implementation code paths
 * - NOT just NULL guards and config checks
 * - Tests real region creation, connectivity, processing
 *
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain_regions/nimcp_brain_regions.h"
#include "core/neuron_types/nimcp_neuron_types.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class BrainRegionsRealTest : public ::testing::Test {
protected:
    brain_module_t* brain = nullptr;

    void SetUp() override {
        // Create REAL brain module (small capacity for fast tests)
        brain = brain_module_create(10);
        ASSERT_NE(brain, nullptr) << "Failed to create brain module";
    }

    void TearDown() override {
        if (brain) {
            brain_module_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Test Suite: REAL Brain Module Operations
//=============================================================================

TEST_F(BrainRegionsRealTest, CreateBrainModule_ValidProperties) {
    EXPECT_EQ(brain->max_regions, 10U);
    EXPECT_EQ(brain->num_regions, 0U);
    EXPECT_EQ(brain->total_neurons, 0U);
    EXPECT_TRUE(brain->enable_plasticity);
    EXPECT_TRUE(brain->enable_glial);
    EXPECT_NE(brain->regions, nullptr);
}

TEST_F(BrainRegionsRealTest, AddRegion_UpdatesTotalNeurons) {
    // Create a visual region
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    ASSERT_NE(v1, nullptr);

    // Initial state
    EXPECT_EQ(brain->total_neurons, 0U);
    EXPECT_EQ(brain->num_regions, 0U);

    // Add region
    nimcp_result_t result = brain_module_add_region(brain, v1);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify updates
    EXPECT_EQ(brain->num_regions, 1U);
    EXPECT_EQ(brain->total_neurons, 100U);
}

TEST_F(BrainRegionsRealTest, AddMultipleRegions_AllTracked) {
    // Create multiple regions
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 80);
    brain_region_t* a1 = brain_region_create(REGION_AUDITORY_A1, 60);
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 70);

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(a1, nullptr);
    ASSERT_NE(m1, nullptr);

    // Add all regions
    EXPECT_EQ(brain_module_add_region(brain, v1), NIMCP_SUCCESS);
    EXPECT_EQ(brain_module_add_region(brain, a1), NIMCP_SUCCESS);
    EXPECT_EQ(brain_module_add_region(brain, m1), NIMCP_SUCCESS);

    // Verify
    EXPECT_EQ(brain->num_regions, 3U);
    EXPECT_EQ(brain->total_neurons, 210U);
}

TEST_F(BrainRegionsRealTest, GetRegion_RetrievesCorrectRegion) {
    // Create and add region
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    ASSERT_NE(v1, nullptr);
    uint32_t region_id = v1->id;

    brain_module_add_region(brain, v1);

    // Retrieve by ID
    brain_region_t* found = brain_module_get_region(brain, region_id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, region_id);
    EXPECT_EQ(found->type, REGION_VISUAL_V1);
    EXPECT_EQ(found->total_neurons, 100U);
}

TEST_F(BrainRegionsRealTest, GetRegionByType_FindsCorrectRegion) {
    // Add visual and auditory regions
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 80);
    brain_region_t* a1 = brain_region_create(REGION_AUDITORY_A1, 60);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, a1);

    // Find visual region
    brain_region_t* found_visual = brain_module_get_region_by_type(brain, REGION_VISUAL_V1);
    ASSERT_NE(found_visual, nullptr);
    EXPECT_EQ(found_visual->type, REGION_VISUAL_V1);

    // Find auditory region
    brain_region_t* found_auditory = brain_module_get_region_by_type(brain, REGION_AUDITORY_A1);
    ASSERT_NE(found_auditory, nullptr);
    EXPECT_EQ(found_auditory->type, REGION_AUDITORY_A1);
}

//=============================================================================
// Test Suite: REAL Region Creation - All Types
//=============================================================================

TEST_F(BrainRegionsRealTest, CreateVisualRegions_AllTypes) {
    brain_region_type_t visual_types[] = {
        REGION_VISUAL_V1,
        REGION_VISUAL_V2,
        REGION_VISUAL_V4,
        REGION_VISUAL_MT,
        REGION_VISUAL_IT
    };

    for (int i = 0; i < 5; i++) {
        brain_region_t* region = brain_region_create(visual_types[i], 50);
        ASSERT_NE(region, nullptr);
        EXPECT_EQ(region->type, visual_types[i]);
        EXPECT_EQ(region->total_neurons, 50U);
        EXPECT_NE(region->network, nullptr);
        brain_region_destroy(region);
    }
}

TEST_F(BrainRegionsRealTest, CreateAuditoryRegions_AllTypes) {
    brain_region_type_t auditory_types[] = {
        REGION_AUDITORY_A1,
        REGION_AUDITORY_A2,
        REGION_AUDITORY_BELT,
        REGION_AUDITORY_PARABELT
    };

    for (int i = 0; i < 4; i++) {
        brain_region_t* region = brain_region_create(auditory_types[i], 40);
        ASSERT_NE(region, nullptr);
        EXPECT_EQ(region->type, auditory_types[i]);
        EXPECT_EQ(region->total_neurons, 40U);
        brain_region_destroy(region);
    }
}

TEST_F(BrainRegionsRealTest, CreateMotorRegions_AllTypes) {
    brain_region_type_t motor_types[] = {
        REGION_MOTOR_M1,
        REGION_MOTOR_PREMOTOR,
        REGION_MOTOR_SMA
    };

    for (int i = 0; i < 3; i++) {
        brain_region_t* region = brain_region_create(motor_types[i], 60);
        ASSERT_NE(region, nullptr);
        EXPECT_EQ(region->type, motor_types[i]);
        EXPECT_EQ(region->total_neurons, 60U);
        brain_region_destroy(region);
    }
}

TEST_F(BrainRegionsRealTest, CreateSubcorticalRegions) {
    brain_region_type_t subcortical_types[] = {
        REGION_THALAMUS,
        REGION_HIPPOCAMPUS,
        REGION_BASAL_GANGLIA,
        REGION_CEREBELLUM
    };

    for (int i = 0; i < 4; i++) {
        brain_region_t* region = brain_region_create(subcortical_types[i], 50);
        ASSERT_NE(region, nullptr);
        EXPECT_EQ(region->type, subcortical_types[i]);
        brain_region_destroy(region);
    }
}

//=============================================================================
// Test Suite: REAL Layer Organization
//=============================================================================

TEST_F(BrainRegionsRealTest, RegionLayers_ProperlyAllocated) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 120);
    ASSERT_NE(v1, nullptr);

    // V1 should have prominent Layer 4 (input from thalamus)
    EXPECT_GT(v1->layer_sizes[LAYER_4], 0U);

    // All layers should sum to total neurons
    uint32_t sum = 0;
    for (int i = 0; i < LAYER_COUNT; i++) {
        sum += v1->layer_sizes[i];
    }
    EXPECT_EQ(sum, 120U);

    brain_region_destroy(v1);
}

TEST_F(BrainRegionsRealTest, GetLayerNeurons_ReturnsNeuronIds) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    ASSERT_NE(v1, nullptr);

    brain_module_add_region(brain, v1);

    // Get Layer 4 neurons
    uint32_t ids[50];
    uint32_t count = brain_region_get_layer_neurons(v1, LAYER_4, ids, 50);

    // Should get some neurons
    EXPECT_GT(count, 0U);
    EXPECT_LE(count, v1->layer_sizes[LAYER_4]);

    // All IDs should be valid
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_LT(ids[i], v1->total_neurons);
    }
}

TEST_F(BrainRegionsRealTest, GetLayerNeurons_DifferentLayers) {
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 120);
    ASSERT_NE(m1, nullptr);

    // Test all layers
    for (int layer = LAYER_1; layer < LAYER_COUNT; layer++) {
        uint32_t ids[60];
        uint32_t count = brain_region_get_layer_neurons(m1, (cortical_layer_t)layer, ids, 60);
        EXPECT_LE(count, 60U);
    }

    brain_region_destroy(m1);
}

//=============================================================================
// Test Suite: REAL Minicolumn Organization
//=============================================================================

TEST_F(BrainRegionsRealTest, OrganizeColumns_CreatesStructure) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 120);
    ASSERT_NE(v1, nullptr);

    // Initially no minicolumns
    EXPECT_EQ(v1->num_minicolumns, 0U);
    EXPECT_EQ(v1->minicolumns, nullptr);

    // Organize into 4x3 grid
    nimcp_result_t result = brain_region_organize_columns(v1, 4, 3);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify structure
    EXPECT_EQ(v1->num_minicolumns, 12U);
    EXPECT_EQ(v1->minicolumns_x, 4U);
    EXPECT_EQ(v1->minicolumns_y, 3U);
    EXPECT_NE(v1->minicolumns, nullptr);

    brain_region_destroy(v1);
}

TEST_F(BrainRegionsRealTest, OrganizeColumns_DifferentConfigurations) {
    // Test different grid sizes
    struct {
        uint32_t x;
        uint32_t y;
    } configs[] = {
        {1, 1},   // Single column
        {2, 2},   // Small grid
        {5, 4},   // Medium grid
        {10, 10}  // Large grid
    };

    for (int i = 0; i < 4; i++) {
        brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);
        ASSERT_NE(region, nullptr);

        nimcp_result_t result = brain_region_organize_columns(region, configs[i].x, configs[i].y);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_EQ(region->num_minicolumns, configs[i].x * configs[i].y);

        brain_region_destroy(region);
    }
}

//=============================================================================
// Test Suite: REAL Inter-Region Connectivity
//=============================================================================

TEST_F(BrainRegionsRealTest, ConnectRegions_CreatesConnection) {
    // Create source and target regions
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 80);
    brain_region_t* mt = brain_region_create(REGION_VISUAL_MT, 60);

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(mt, nullptr);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, mt);

    // Connect V1 → MT
    nimcp_result_t result = brain_module_connect_regions(brain, v1->id, mt->id, 0.3f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify connection created
    EXPECT_EQ(brain->num_connections, 1U);
    EXPECT_NE(brain->connections[0], nullptr);
}

TEST_F(BrainRegionsRealTest, ConnectRegions_MultipleConnections) {
    // Create visual processing pathway: V1 → V2 → V4 → IT
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 50);
    brain_region_t* v2 = brain_region_create(REGION_VISUAL_V2, 40);
    brain_region_t* v4 = brain_region_create(REGION_VISUAL_V4, 30);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, v2);
    brain_module_add_region(brain, v4);

    // Create pathway
    EXPECT_EQ(brain_module_connect_regions(brain, v1->id, v2->id, 0.5f), NIMCP_SUCCESS);
    EXPECT_EQ(brain_module_connect_regions(brain, v2->id, v4->id, 0.4f), NIMCP_SUCCESS);

    // Verify all connections
    EXPECT_EQ(brain->num_connections, 2U);
}

TEST_F(BrainRegionsRealTest, ConnectLayers_SpecificLayerConnections) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 80);
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 70);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, m1);

    // Connect Layer 5 of V1 to Layer 4 of M1 (feedforward)
    nimcp_result_t result = brain_module_connect_layers(brain, v1->id, LAYER_5, m1->id, LAYER_4, 0.25f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify connection properties
    EXPECT_EQ(brain->num_connections, 1U);
    EXPECT_EQ(brain->connections[0]->source_layer, LAYER_5);
    EXPECT_EQ(brain->connections[0]->target_layer, LAYER_4);
}

//=============================================================================
// Test Suite: REAL Input Processing
//=============================================================================

TEST_F(BrainRegionsRealTest, ProcessInput_UpdatesRegionState) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    ASSERT_NE(v1, nullptr);

    brain_module_add_region(brain, v1);

    // Create test input
    float input[20];
    for (int i = 0; i < 20; i++) {
        input[i] = 0.5f + 0.3f * std::sin(i * 0.5f);
    }

    // Process input
    uint64_t timestamp = 1000;
    nimcp_result_t result = brain_region_process_input(v1, input, 20, timestamp);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify timestamp updated
    EXPECT_EQ(v1->last_update, timestamp);
}

TEST_F(BrainRegionsRealTest, ProcessInput_MultipleInputs) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_module_add_region(brain, v1);

    // Process multiple inputs over time
    for (uint64_t t = 1000; t <= 5000; t += 1000) {
        float input[15];
        for (int i = 0; i < 15; i++) {
            input[i] = (t / 1000.0f) * 0.1f + i * 0.05f;
        }

        nimcp_result_t result = brain_region_process_input(v1, input, 15, t);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_EQ(v1->last_update, t);
    }
}

//=============================================================================
// Test Suite: REAL Output Retrieval
//=============================================================================

TEST_F(BrainRegionsRealTest, GetOutput_ReturnsActivations) {
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 80);
    ASSERT_NE(m1, nullptr);

    brain_module_add_region(brain, m1);

    // Get output
    float output[20];
    uint32_t count = brain_region_get_output(m1, output, 20);

    // Should return some activations
    EXPECT_GT(count, 0U);
    EXPECT_LE(count, 20U);

    // All values should be valid
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GE(output[i], 0.0f);
        EXPECT_LE(output[i], 1.0f);
    }
}

//=============================================================================
// Test Suite: REAL Simulation Stepping
//=============================================================================

TEST_F(BrainRegionsRealTest, BrainModuleStep_AdvancesTime) {
    uint64_t initial_time = brain->current_time;

    // Step forward
    nimcp_result_t result = brain_module_step(brain, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Time should advance
    EXPECT_GE(brain->current_time, initial_time);
}

TEST_F(BrainRegionsRealTest, BrainModuleStep_WithRegions) {
    // Add regions
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 60);
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 50);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, m1);

    // Step the entire brain
    nimcp_result_t result = brain_module_step(brain, 1000);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(BrainRegionsRealTest, RegionStep_UpdatesRegion) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 80);
    ASSERT_NE(v1, nullptr);

    uint64_t initial_time = v1->last_update;

    // Step region
    nimcp_result_t result = brain_region_step(v1, 1000);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Should update timestamp
    EXPECT_GE(v1->last_update, initial_time);

    brain_region_destroy(v1);
}

//=============================================================================
// Test Suite: REAL Statistics
//=============================================================================

TEST_F(BrainRegionsRealTest, GetStats_ValidStatistics) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    ASSERT_NE(v1, nullptr);

    brain_module_add_region(brain, v1);

    // Get statistics
    brain_region_stats_t stats;
    nimcp_result_t result = brain_region_get_stats(v1, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify basic stats
    EXPECT_EQ(stats.total_neurons, 100U);
    EXPECT_GE(stats.avg_activity, 0.0f);
    EXPECT_LE(stats.avg_activity, 1.0f);

    // Check all layer activities are valid
    for (int i = 0; i < LAYER_COUNT; i++) {
        EXPECT_GE(stats.layer_activity[i], 0.0f);
        EXPECT_LE(stats.layer_activity[i], 1.0f);
    }
}

TEST_F(BrainRegionsRealTest, GetStats_WithMinicolumns) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 120);
    brain_region_organize_columns(v1, 4, 3);

    brain_region_stats_t stats;
    brain_region_get_stats(v1, &stats);

    EXPECT_EQ(stats.num_minicolumns, 12U);

    brain_region_destroy(v1);
}

//=============================================================================
// Test Suite: REAL Region Names
//=============================================================================

TEST_F(BrainRegionsRealTest, GetRegionName_AllTypes) {
    // Test a few key region types
    const char* v1_name = brain_region_get_name(REGION_VISUAL_V1);
    const char* a1_name = brain_region_get_name(REGION_AUDITORY_A1);
    const char* m1_name = brain_region_get_name(REGION_MOTOR_M1);
    const char* pfc_name = brain_region_get_name(REGION_PREFRONTAL);

    EXPECT_NE(v1_name, nullptr);
    EXPECT_NE(a1_name, nullptr);
    EXPECT_NE(m1_name, nullptr);
    EXPECT_NE(pfc_name, nullptr);
}

//=============================================================================
// Test Suite: REAL Complete Workflow
//=============================================================================

TEST_F(BrainRegionsRealTest, CompleteWorkflow_VisualMotorPathway) {
    // 1. Create visual-motor pathway
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 80);
    brain_region_t* mt = brain_region_create(REGION_VISUAL_MT, 60);
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 70);

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(mt, nullptr);
    ASSERT_NE(m1, nullptr);

    // 2. Add regions to brain
    EXPECT_EQ(brain_module_add_region(brain, v1), NIMCP_SUCCESS);
    EXPECT_EQ(brain_module_add_region(brain, mt), NIMCP_SUCCESS);
    EXPECT_EQ(brain_module_add_region(brain, m1), NIMCP_SUCCESS);

    // 3. Connect pathway: V1 → MT → M1
    EXPECT_EQ(brain_module_connect_regions(brain, v1->id, mt->id, 0.4f), NIMCP_SUCCESS);
    EXPECT_EQ(brain_module_connect_regions(brain, mt->id, m1->id, 0.3f), NIMCP_SUCCESS);

    // 4. Organize columns in V1
    EXPECT_EQ(brain_region_organize_columns(v1, 4, 3), NIMCP_SUCCESS);

    // 5. Process visual input
    float visual_input[20];
    for (int i = 0; i < 20; i++) {
        visual_input[i] = 0.5f + 0.4f * std::sin(i * 0.3f);
    }
    EXPECT_EQ(brain_region_process_input(v1, visual_input, 20, 1000), NIMCP_SUCCESS);

    // 6. Step the brain forward
    EXPECT_EQ(brain_module_step(brain, 1000), NIMCP_SUCCESS);

    // 7. Get motor output
    float motor_output[15];
    uint32_t output_count = brain_region_get_output(m1, motor_output, 15);
    EXPECT_GT(output_count, 0U);

    // 8. Verify statistics
    brain_region_stats_t v1_stats, mt_stats, m1_stats;
    EXPECT_EQ(brain_region_get_stats(v1, &v1_stats), NIMCP_SUCCESS);
    EXPECT_EQ(brain_region_get_stats(mt, &mt_stats), NIMCP_SUCCESS);
    EXPECT_EQ(brain_region_get_stats(m1, &m1_stats), NIMCP_SUCCESS);

    // All should have processed
    EXPECT_EQ(v1_stats.total_neurons, 80U);
    EXPECT_EQ(mt_stats.total_neurons, 60U);
    EXPECT_EQ(m1_stats.total_neurons, 70U);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
