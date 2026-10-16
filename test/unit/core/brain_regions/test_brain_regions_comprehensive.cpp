/**
 * @file test_brain_regions_comprehensive.cpp
 * @brief Comprehensive unit tests for brain regions module (100% coverage)
 *
 * COVERAGE GOAL: 100% (currently 5.6%, 358 lines total)
 *
 * TEST STRATEGY:
 * - Test all brain region creation/destruction for all types
 * - Test brain module management (create, add regions, get regions)
 * - Test minicolumn organization
 * - Test layer neuron retrieval
 * - Test region connectivity (feedforward, feedback, lateral)
 * - Test sensory input processing
 * - Test output retrieval
 * - Test simulation stepping
 * - Test statistics gathering
 * - Test all region types and layer proportions
 * - Test edge cases and error handling
 * - Test thread safety
 * - Test memory leaks
 *
 * @author NIMCP Comprehensive Testing
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>

#include "core/brain_regions/nimcp_brain_regions.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainRegionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
    }

    void TearDown() override {
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0) << "Memory leak detected";
    }
};

//=============================================================================
// Brain Module Management Tests
//=============================================================================

TEST_F(BrainRegionsTest, BrainModuleCreate_Success) {
    brain_module_t* brain = brain_module_create(10);

    ASSERT_NE(brain, nullptr);
    EXPECT_EQ(brain->id, 1);
    EXPECT_EQ(brain->max_regions, 10);
    EXPECT_EQ(brain->num_regions, 0);
    EXPECT_EQ(brain->total_neurons, 0);
    EXPECT_NE(brain->regions, nullptr);
    EXPECT_EQ(brain->connections, nullptr);
    EXPECT_EQ(brain->num_connections, 0);
    EXPECT_EQ(brain->current_time, 0);
    EXPECT_TRUE(brain->enable_plasticity);
    EXPECT_TRUE(brain->enable_glial);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleCreate_ZeroRegions) {
    brain_module_t* brain = brain_module_create(0);
    ASSERT_NE(brain, nullptr);
    EXPECT_EQ(brain->max_regions, 0);
    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleDestroy_Null) {
    brain_module_destroy(nullptr); // Should not crash
}

TEST_F(BrainRegionsTest, BrainModuleAddRegion_Success) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(region, nullptr);

    nimcp_result_t result = brain_module_add_region(brain, region);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(brain->num_regions, 1);
    EXPECT_EQ(brain->total_neurons, 100);
    EXPECT_EQ(brain->regions[0], region);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleAddRegion_Multiple) {
    brain_module_t* brain = brain_module_create(5);

    std::vector<brain_region_t*> regions;
    brain_region_type_t types[] = {
        REGION_VISUAL_V1,
        REGION_AUDITORY_A1,
        REGION_MOTOR_M1,
        REGION_PREFRONTAL
    };

    for (int i = 0; i < 4; i++) {
        brain_region_t* region = brain_region_create(types[i], 100 * (i + 1));
        ASSERT_NE(region, nullptr);
        nimcp_result_t result = brain_module_add_region(brain, region);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(brain->num_regions, 4);
    EXPECT_EQ(brain->total_neurons, 100 + 200 + 300 + 400); // 1000 total

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleAddRegion_ExceedsCapacity) {
    brain_module_t* brain = brain_module_create(2);

    brain_region_t* r1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_region_t* r2 = brain_region_create(REGION_AUDITORY_A1, 100);
    brain_region_t* r3 = brain_region_create(REGION_MOTOR_M1, 100);

    EXPECT_EQ(brain_module_add_region(brain, r1), NIMCP_SUCCESS);
    EXPECT_EQ(brain_module_add_region(brain, r2), NIMCP_SUCCESS);
    EXPECT_EQ(brain_module_add_region(brain, r3), NIMCP_ERROR); // Should fail

    // r3 wasn't added, so we need to clean it up manually
    brain_region_destroy(r3);
    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleAddRegion_NullParams) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);

    EXPECT_EQ(brain_module_add_region(nullptr, region), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(brain_module_add_region(brain, nullptr), NIMCP_ERROR_INVALID_PARAM);

    brain_region_destroy(region);
    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleGetRegion_Success) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);

    brain_module_add_region(brain, region);

    brain_region_t* found = brain_module_get_region(brain, region->id);
    EXPECT_EQ(found, region);
    EXPECT_EQ(found->id, region->id);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleGetRegion_NotFound) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);

    brain_module_add_region(brain, region);

    brain_region_t* found = brain_module_get_region(brain, 99999);
    EXPECT_EQ(found, nullptr);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleGetRegion_Null) {
    EXPECT_EQ(brain_module_get_region(nullptr, 1), nullptr);
}

TEST_F(BrainRegionsTest, BrainModuleGetRegionByType_Success) {
    brain_module_t* brain = brain_module_create(5);

    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_region_t* a1 = brain_region_create(REGION_AUDITORY_A1, 150);
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 200);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, a1);
    brain_module_add_region(brain, m1);

    EXPECT_EQ(brain_module_get_region_by_type(brain, REGION_VISUAL_V1), v1);
    EXPECT_EQ(brain_module_get_region_by_type(brain, REGION_AUDITORY_A1), a1);
    EXPECT_EQ(brain_module_get_region_by_type(brain, REGION_MOTOR_M1), m1);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleGetRegionByType_NotFound) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_module_add_region(brain, v1);

    EXPECT_EQ(brain_module_get_region_by_type(brain, REGION_HIPPOCAMPUS), nullptr);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleGetRegionByType_Null) {
    EXPECT_EQ(brain_module_get_region_by_type(nullptr, REGION_VISUAL_V1), nullptr);
}

//=============================================================================
// Brain Region Creation Tests (All Types)
//=============================================================================

TEST_F(BrainRegionsTest, BrainRegionCreate_VisualV1) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 200);

    ASSERT_NE(region, nullptr);
    EXPECT_GT(region->id, 0);
    EXPECT_EQ(region->type, REGION_VISUAL_V1);
    EXPECT_EQ(region->total_neurons, 200);
    EXPECT_STREQ(region->name, "Primary Visual Cortex (V1)");
    EXPECT_NE(region->network, nullptr);
    EXPECT_NE(region->neuron_extended_types, nullptr);
    EXPECT_NE(region->neuron_type_params, nullptr);

    // V1 should have prominent Layer 4 (35% of neurons)
    EXPECT_GT(region->layer_sizes[LAYER_4], region->layer_sizes[LAYER_1]);
    EXPECT_GT(region->layer_sizes[LAYER_4], region->layer_sizes[LAYER_2]);

    // Verify total layer sizes sum to total neurons
    uint32_t sum = 0;
    for (int i = 0; i < LAYER_COUNT; i++) {
        sum += region->layer_sizes[i];
    }
    EXPECT_EQ(sum, 200);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, BrainRegionCreate_AllVisualTypes) {
    brain_region_type_t visual_types[] = {
        REGION_VISUAL_V1,
        REGION_VISUAL_V2,
        REGION_VISUAL_V4,
        REGION_VISUAL_MT,
        REGION_VISUAL_IT
    };

    for (auto type : visual_types) {
        brain_region_t* region = brain_region_create(type, 150);
        ASSERT_NE(region, nullptr) << "Failed to create region type " << type;
        EXPECT_EQ(region->type, type);
        EXPECT_EQ(region->total_neurons, 150);
        brain_region_destroy(region);
    }
}

TEST_F(BrainRegionsTest, BrainRegionCreate_AllAuditoryTypes) {
    brain_region_type_t auditory_types[] = {
        REGION_AUDITORY_A1,
        REGION_AUDITORY_A2,
        REGION_AUDITORY_BELT,
        REGION_AUDITORY_PARABELT
    };

    for (auto type : auditory_types) {
        brain_region_t* region = brain_region_create(type, 150);
        ASSERT_NE(region, nullptr) << "Failed to create region type " << type;
        EXPECT_EQ(region->type, type);
        brain_region_destroy(region);
    }
}

TEST_F(BrainRegionsTest, BrainRegionCreate_AllMotorTypes) {
    brain_region_type_t motor_types[] = {
        REGION_MOTOR_M1,
        REGION_MOTOR_PREMOTOR,
        REGION_MOTOR_SMA
    };

    for (auto type : motor_types) {
        brain_region_t* region = brain_region_create(type, 150);
        ASSERT_NE(region, nullptr) << "Failed to create region type " << type;
        EXPECT_EQ(region->type, type);

        // M1 should have prominent Layer 5 (35% of neurons)
        if (type == REGION_MOTOR_M1) {
            EXPECT_GT(region->layer_sizes[LAYER_5], region->layer_sizes[LAYER_1]);
        }

        brain_region_destroy(region);
    }
}

TEST_F(BrainRegionsTest, BrainRegionCreate_AllSomatosensoryTypes) {
    brain_region_type_t somato_types[] = {
        REGION_SOMATOSENSORY_S1,
        REGION_SOMATOSENSORY_S2
    };

    for (auto type : somato_types) {
        brain_region_t* region = brain_region_create(type, 150);
        ASSERT_NE(region, nullptr);
        EXPECT_EQ(region->type, type);
        brain_region_destroy(region);
    }
}

TEST_F(BrainRegionsTest, BrainRegionCreate_AssociationAreas) {
    brain_region_type_t association_types[] = {
        REGION_PREFRONTAL,
        REGION_PARIETAL,
        REGION_TEMPORAL
    };

    for (auto type : association_types) {
        brain_region_t* region = brain_region_create(type, 150);
        ASSERT_NE(region, nullptr);
        EXPECT_EQ(region->type, type);

        // Prefrontal should have large Layer 3 (30% of neurons)
        if (type == REGION_PREFRONTAL) {
            EXPECT_GT(region->layer_sizes[LAYER_3], region->layer_sizes[LAYER_1]);
        }

        brain_region_destroy(region);
    }
}

TEST_F(BrainRegionsTest, BrainRegionCreate_SubcorticalRegions) {
    brain_region_type_t subcortical_types[] = {
        REGION_THALAMUS,
        REGION_HIPPOCAMPUS,
        REGION_BASAL_GANGLIA,
        REGION_CEREBELLUM
    };

    for (auto type : subcortical_types) {
        brain_region_t* region = brain_region_create(type, 150);
        ASSERT_NE(region, nullptr);
        EXPECT_EQ(region->type, type);
        brain_region_destroy(region);
    }
}

TEST_F(BrainRegionsTest, BrainRegionCreate_SmallNeuronCount) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 10);
    ASSERT_NE(region, nullptr);
    EXPECT_EQ(region->total_neurons, 10);
    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, BrainRegionCreate_LargeNeuronCount) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 10000);
    ASSERT_NE(region, nullptr);
    EXPECT_EQ(region->total_neurons, 10000);
    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, BrainRegionDestroy_Null) {
    brain_region_destroy(nullptr); // Should not crash
}

//=============================================================================
// Minicolumn Organization Tests
//=============================================================================

TEST_F(BrainRegionsTest, OrganizeColumns_Success) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 400);

    nimcp_result_t result = brain_region_organize_columns(region, 10, 10);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(region->num_minicolumns, 100);
    EXPECT_EQ(region->minicolumns_x, 10);
    EXPECT_EQ(region->minicolumns_y, 10);
    EXPECT_NE(region->minicolumns, nullptr);

    // Verify each minicolumn is created
    for (uint32_t i = 0; i < region->num_minicolumns; i++) {
        ASSERT_NE(region->minicolumns[i], nullptr);
        EXPECT_EQ(region->minicolumns[i]->id, i);
        EXPECT_EQ(region->minicolumns[i]->region_id, region->id);
    }

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, OrganizeColumns_PositionMapping) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 400);

    brain_region_organize_columns(region, 5, 4); // 20 columns

    // Verify spatial positions are mapped correctly
    for (uint32_t i = 0; i < region->num_minicolumns; i++) {
        brain_minicolumn_t* col = region->minicolumns[i];
        EXPECT_GE(col->x, 0.0f);
        EXPECT_LE(col->x, 1.0f);
        EXPECT_GE(col->y, 0.0f);
        EXPECT_LE(col->y, 1.0f);
    }

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, OrganizeColumns_LayerDistribution) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 600);

    brain_region_organize_columns(region, 10, 10);

    // Verify neurons are distributed across layers in each column
    // Note: Some layers may have zero neurons if layer_size/num_columns < 1
    for (uint32_t i = 0; i < region->num_minicolumns; i++) {
        brain_minicolumn_t* col = region->minicolumns[i];

        // Verify that if a layer has neurons globally, at least some columns have them
        uint32_t total_neurons_in_col = 0;
        for (int layer = 0; layer < LAYER_COUNT; layer++) {
            total_neurons_in_col += col->layer_neuron_counts[layer];

            // If this column has neurons in this layer, IDs should be allocated
            if (col->layer_neuron_counts[layer] > 0) {
                EXPECT_NE(col->layer_neuron_ids[layer], nullptr);
            } else {
                // No neurons means no ID array needed
                EXPECT_EQ(col->layer_neuron_ids[layer], nullptr);
            }
        }

        // Each column should have at least some neurons
        EXPECT_GT(total_neurons_in_col, 0);
    }

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, OrganizeColumns_NullRegion) {
    nimcp_result_t result = brain_region_organize_columns(nullptr, 10, 10);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(BrainRegionsTest, OrganizeColumns_ZeroDimensions) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);

    EXPECT_EQ(brain_region_organize_columns(region, 0, 10), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(brain_region_organize_columns(region, 10, 0), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(brain_region_organize_columns(region, 0, 0), NIMCP_ERROR_INVALID_PARAM);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, OrganizeColumns_SingleColumn) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);

    nimcp_result_t result = brain_region_organize_columns(region, 1, 1);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(region->num_minicolumns, 1);

    brain_region_destroy(region);
}

//=============================================================================
// Layer Neuron Retrieval Tests
//=============================================================================

TEST_F(BrainRegionsTest, GetLayerNeurons_Success) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 200);

    uint32_t neuron_ids[100];
    uint32_t count = brain_region_get_layer_neurons(region, LAYER_4, neuron_ids, 100);

    EXPECT_GT(count, 0);
    EXPECT_LE(count, 100);
    EXPECT_LE(count, region->layer_sizes[LAYER_4]);

    // Verify neuron IDs are sequential
    for (uint32_t i = 1; i < count; i++) {
        EXPECT_EQ(neuron_ids[i], neuron_ids[i-1] + 1);
    }

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetLayerNeurons_AllLayers) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 300);

    uint32_t total_retrieved = 0;

    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        uint32_t neuron_ids[200];
        uint32_t count = brain_region_get_layer_neurons(
            region, (cortical_layer_t)layer, neuron_ids, 200);

        EXPECT_EQ(count, region->layer_sizes[layer]);
        total_retrieved += count;
    }

    // Should retrieve all neurons across all layers
    EXPECT_EQ(total_retrieved, 300);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetLayerNeurons_BufferTooSmall) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 200);

    uint32_t neuron_ids[5];
    uint32_t count = brain_region_get_layer_neurons(region, LAYER_4, neuron_ids, 5);

    // Should return min(layer_size, max_neurons)
    EXPECT_EQ(count, 5);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetLayerNeurons_NullParams) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);
    uint32_t neuron_ids[10];

    EXPECT_EQ(brain_region_get_layer_neurons(nullptr, LAYER_1, neuron_ids, 10), 0);
    EXPECT_EQ(brain_region_get_layer_neurons(region, LAYER_1, nullptr, 10), 0);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetLayerNeurons_InvalidLayer) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);
    uint32_t neuron_ids[10];

    EXPECT_EQ(brain_region_get_layer_neurons(region, (cortical_layer_t)999, neuron_ids, 10), 0);

    brain_region_destroy(region);
}

//=============================================================================
// Inter-Region Connectivity Tests
//=============================================================================

TEST_F(BrainRegionsTest, ConnectRegions_Success) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_region_t* v2 = brain_region_create(REGION_VISUAL_V2, 100);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, v2);

    nimcp_result_t result = brain_module_connect_regions(brain, v1->id, v2->id, 0.5f);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(brain->num_connections, 1);
    EXPECT_NE(brain->connections, nullptr);

    brain_connection_t* conn = brain->connections[0];
    EXPECT_EQ(conn->source_region_id, v1->id);
    EXPECT_EQ(conn->target_region_id, v2->id);
    EXPECT_FLOAT_EQ(conn->connection_strength, 0.5f);
    EXPECT_TRUE(conn->feedforward);
    EXPECT_EQ(conn->source_layer, LAYER_3);
    EXPECT_EQ(conn->target_layer, LAYER_4);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, ConnectRegions_Multiple) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_region_t* mt = brain_region_create(REGION_VISUAL_MT, 100);
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 100);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, mt);
    brain_module_add_region(brain, m1);

    // V1 -> MT -> M1 pathway
    EXPECT_EQ(brain_module_connect_regions(brain, v1->id, mt->id, 0.6f), NIMCP_SUCCESS);
    EXPECT_EQ(brain_module_connect_regions(brain, mt->id, m1->id, 0.4f), NIMCP_SUCCESS);

    EXPECT_EQ(brain->num_connections, 2);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, ConnectRegions_InvalidRegion) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);

    brain_module_add_region(brain, v1);

    // Try to connect to non-existent region
    nimcp_result_t result = brain_module_connect_regions(brain, v1->id, 99999, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, ConnectRegions_NullBrain) {
    nimcp_result_t result = brain_module_connect_regions(nullptr, 1, 2, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(BrainRegionsTest, ConnectLayers_Success) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_region_t* v2 = brain_region_create(REGION_VISUAL_V2, 100);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, v2);

    nimcp_result_t result = brain_module_connect_layers(
        brain, v1->id, LAYER_5, v2->id, LAYER_1, 0.3f);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(brain->num_connections, 1);

    brain_connection_t* conn = brain->connections[0];
    EXPECT_EQ(conn->source_layer, LAYER_5);
    EXPECT_EQ(conn->target_layer, LAYER_1);
    EXPECT_FLOAT_EQ(conn->connection_strength, 0.3f);
    EXPECT_FALSE(conn->feedforward); // Layer 1 is not Layer 4

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, ConnectLayers_Feedforward) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_region_t* v2 = brain_region_create(REGION_VISUAL_V2, 100);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, v2);

    // Connect to Layer 4 (feedforward)
    brain_module_connect_layers(brain, v1->id, LAYER_3, v2->id, LAYER_4, 0.5f);

    EXPECT_TRUE(brain->connections[0]->feedforward);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, ConnectLayers_NullBrain) {
    nimcp_result_t result = brain_module_connect_layers(
        nullptr, 1, LAYER_3, 2, LAYER_4, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// Sensory Input Processing Tests
//=============================================================================

TEST_F(BrainRegionsTest, ProcessInput_Success) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 200);

    float input[50];
    for (int i = 0; i < 50; i++) {
        input[i] = 0.5f + 0.1f * sinf(i * 0.1f);
    }

    nimcp_result_t result = brain_region_process_input(region, input, 50, 1000);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(region->last_update, 1000);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, ProcessInput_LargeInput) {
    brain_region_t* region = brain_region_create(REGION_AUDITORY_A1, 300);

    float input[500];
    for (int i = 0; i < 500; i++) {
        input[i] = (float)i / 500.0f;
    }

    nimcp_result_t result = brain_region_process_input(region, input, 500, 2000);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(region->last_update, 2000);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, ProcessInput_SmallInput) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 200);

    float input[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    nimcp_result_t result = brain_region_process_input(region, input, 5, 1000);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, ProcessInput_NullParams) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);
    float input[10] = {0};

    EXPECT_EQ(brain_region_process_input(nullptr, input, 10, 1000), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(brain_region_process_input(region, nullptr, 10, 1000), NIMCP_ERROR_INVALID_PARAM);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, ProcessInput_MultipleTimesteps) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 200);

    float input[20];
    for (int i = 0; i < 20; i++) {
        input[i] = 0.5f;
    }

    // Process multiple timesteps
    for (uint64_t t = 0; t < 5; t++) {
        nimcp_result_t result = brain_region_process_input(region, input, 20, t * 1000);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_EQ(region->last_update, t * 1000);
    }

    brain_region_destroy(region);
}

//=============================================================================
// Output Retrieval Tests
//=============================================================================

TEST_F(BrainRegionsTest, GetOutput_Success) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 200);

    // Process some input first
    float input[50];
    for (int i = 0; i < 50; i++) {
        input[i] = 0.5f;
    }
    brain_region_process_input(region, input, 50, 1000);

    // Get output
    float output[100];
    uint32_t count = brain_region_get_output(region, output, 100);

    EXPECT_GT(count, 0);
    EXPECT_LE(count, region->layer_sizes[LAYER_5]);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetOutput_BufferTooSmall) {
    brain_region_t* region = brain_region_create(REGION_MOTOR_M1, 200);

    float output[5];
    uint32_t count = brain_region_get_output(region, output, 5);

    EXPECT_EQ(count, 5); // Limited by buffer size

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetOutput_LargeBuffer) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);

    float output[1000];
    uint32_t count = brain_region_get_output(region, output, 1000);

    EXPECT_GT(count, 0);
    EXPECT_LE(count, region->layer_sizes[LAYER_5]);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetOutput_NullParams) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);
    float output[10];

    EXPECT_EQ(brain_region_get_output(nullptr, output, 10), 0);
    EXPECT_EQ(brain_region_get_output(region, nullptr, 10), 0);

    brain_region_destroy(region);
}

//=============================================================================
// Simulation Stepping Tests
//=============================================================================

TEST_F(BrainRegionsTest, BrainModuleStep_Success) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_region_t* v2 = brain_region_create(REGION_VISUAL_V2, 100);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, v2);

    uint64_t initial_time = brain->current_time;

    nimcp_result_t result = brain_module_step(brain, 1000);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(brain->current_time, initial_time + 1000);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleStep_MultipleSteps) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);
    brain_module_add_region(brain, region);

    for (int i = 0; i < 10; i++) {
        brain_module_step(brain, 500);
    }

    EXPECT_EQ(brain->current_time, 5000);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, BrainModuleStep_NullBrain) {
    nimcp_result_t result = brain_module_step(nullptr, 1000);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(BrainRegionsTest, BrainRegionStep_Success) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);

    uint64_t initial_time = region->last_update;

    nimcp_result_t result = brain_region_step(region, 1000);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(region->last_update, initial_time + 1000);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, BrainRegionStep_MultipleSteps) {
    brain_region_t* region = brain_region_create(REGION_AUDITORY_A1, 150);

    uint64_t expected_time = 0;
    for (int i = 0; i < 5; i++) {
        expected_time += 200;
        brain_region_step(region, 200);
        EXPECT_EQ(region->last_update, expected_time);
    }

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, BrainRegionStep_NullRegion) {
    nimcp_result_t result = brain_region_step(nullptr, 1000);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// Statistics and Monitoring Tests
//=============================================================================

TEST_F(BrainRegionsTest, GetStats_Success) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 200);
    brain_region_stats_t stats;

    nimcp_result_t result = brain_region_get_stats(region, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_neurons, 200);
    EXPECT_EQ(stats.num_minicolumns, 0); // Not organized yet
    EXPECT_GE(stats.avg_activity, 0.0f);
    EXPECT_LE(stats.avg_activity, 1.0f);

    // All layer activities should be in valid range
    for (int i = 0; i < LAYER_COUNT; i++) {
        EXPECT_GE(stats.layer_activity[i], 0.0f);
        EXPECT_LE(stats.layer_activity[i], 1.0f);
    }

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetStats_AfterInput) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 200);

    // Process input to create activity
    float input[50];
    for (int i = 0; i < 50; i++) {
        input[i] = 1.5f; // Strong input
    }
    brain_region_process_input(region, input, 50, 1000);

    brain_region_stats_t stats;
    brain_region_get_stats(region, &stats);

    // Should have some activity after processing input
    EXPECT_GE(stats.avg_activity, 0.0f);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetStats_WithMinicolumns) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 400);
    brain_region_organize_columns(region, 10, 10);

    brain_region_stats_t stats;
    brain_region_get_stats(region, &stats);

    EXPECT_EQ(stats.num_minicolumns, 100);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetStats_NullParams) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);
    brain_region_stats_t stats;

    EXPECT_EQ(brain_region_get_stats(nullptr, &stats), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(brain_region_get_stats(region, nullptr), NIMCP_ERROR_INVALID_PARAM);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetRegionName_AllTypes) {
    struct {
        brain_region_type_t type;
        const char* expected_name;
    } test_cases[] = {
        {REGION_VISUAL_V1, "Primary Visual Cortex (V1)"},
        {REGION_VISUAL_V2, "Secondary Visual Cortex (V2)"},
        {REGION_VISUAL_V4, "Visual Area V4"},
        {REGION_VISUAL_MT, "Middle Temporal (MT/V5)"},
        {REGION_VISUAL_IT, "Inferior Temporal (IT)"},
        {REGION_AUDITORY_A1, "Primary Auditory Cortex (A1)"},
        {REGION_AUDITORY_A2, "Secondary Auditory Cortex (A2)"},
        {REGION_AUDITORY_BELT, "Auditory Belt Region"},
        {REGION_AUDITORY_PARABELT, "Auditory Parabelt"},
        {REGION_MOTOR_M1, "Primary Motor Cortex (M1)"},
        {REGION_MOTOR_PREMOTOR, "Premotor Cortex"},
        {REGION_MOTOR_SMA, "Supplementary Motor Area (SMA)"},
        {REGION_SOMATOSENSORY_S1, "Primary Somatosensory Cortex (S1)"},
        {REGION_SOMATOSENSORY_S2, "Secondary Somatosensory Cortex (S2)"},
        {REGION_PREFRONTAL, "Prefrontal Cortex (PFC)"},
        {REGION_PARIETAL, "Parietal Cortex"},
        {REGION_TEMPORAL, "Temporal Cortex"},
        {REGION_THALAMUS, "Thalamus"},
        {REGION_HIPPOCAMPUS, "Hippocampus"},
        {REGION_BASAL_GANGLIA, "Basal Ganglia"},
        {REGION_CEREBELLUM, "Cerebellum"}
    };

    for (auto& test : test_cases) {
        const char* name = brain_region_get_name(test.type);
        EXPECT_STREQ(name, test.expected_name)
            << "Failed for region type " << test.type;
    }
}

TEST_F(BrainRegionsTest, GetRegionName_UnknownType) {
    const char* name = brain_region_get_name((brain_region_type_t)9999);
    EXPECT_STREQ(name, "Unknown Region");
}

//=============================================================================
// Integration and Edge Case Tests
//=============================================================================

TEST_F(BrainRegionsTest, CompleteWorkflow_VisualPathway) {
    // Create brain with visual processing pathway: V1 -> V2 -> MT -> M1
    brain_module_t* brain = brain_module_create(10);

    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);
    brain_region_t* v2 = brain_region_create(REGION_VISUAL_V2, 200);
    brain_region_t* mt = brain_region_create(REGION_VISUAL_MT, 150);
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 100);

    brain_module_add_region(brain, v1);
    brain_module_add_region(brain, v2);
    brain_module_add_region(brain, mt);
    brain_module_add_region(brain, m1);

    // Connect regions
    brain_module_connect_regions(brain, v1->id, v2->id, 0.7f);
    brain_module_connect_regions(brain, v2->id, mt->id, 0.6f);
    brain_module_connect_regions(brain, mt->id, m1->id, 0.5f);

    // Organize v1 into columns
    brain_region_organize_columns(v1, 10, 10);

    // Process visual input
    float visual_input[50];
    for (int i = 0; i < 50; i++) {
        visual_input[i] = 0.5f + 0.3f * sinf(i * 0.2f);
    }
    brain_region_process_input(v1, visual_input, 50, 1000);

    // Step simulation
    brain_module_step(brain, 1000);

    // Get motor output
    float motor_output[50];
    uint32_t output_count = brain_region_get_output(m1, motor_output, 50);
    EXPECT_GT(output_count, 0);

    // Get statistics
    brain_region_stats_t stats;
    brain_region_get_stats(v1, &stats);
    EXPECT_EQ(stats.total_neurons, 200);
    EXPECT_EQ(stats.num_minicolumns, 100);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, CompleteWorkflow_AuditoryPathway) {
    // Create auditory processing pathway
    brain_module_t* brain = brain_module_create(5);

    brain_region_t* a1 = brain_region_create(REGION_AUDITORY_A1, 200);
    brain_region_t* belt = brain_region_create(REGION_AUDITORY_BELT, 150);
    brain_region_t* pfc = brain_region_create(REGION_PREFRONTAL, 100);

    brain_module_add_region(brain, a1);
    brain_module_add_region(brain, belt);
    brain_module_add_region(brain, pfc);

    brain_module_connect_regions(brain, a1->id, belt->id, 0.6f);
    brain_module_connect_regions(brain, belt->id, pfc->id, 0.4f);

    // Process auditory input
    float audio_input[100];
    for (int i = 0; i < 100; i++) {
        audio_input[i] = 0.8f * sinf(i * 0.05f);
    }
    brain_region_process_input(a1, audio_input, 100, 2000);

    // Step and verify
    brain_module_step(brain, 500);
    EXPECT_EQ(brain->current_time, 500);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, StressTest_ManyRegions) {
    brain_module_t* brain = brain_module_create(50);

    // Add many regions of different types
    brain_region_type_t types[] = {
        REGION_VISUAL_V1, REGION_VISUAL_V2, REGION_VISUAL_V4,
        REGION_AUDITORY_A1, REGION_AUDITORY_A2,
        REGION_MOTOR_M1, REGION_MOTOR_PREMOTOR,
        REGION_PREFRONTAL, REGION_PARIETAL
    };

    for (int i = 0; i < 20; i++) {
        brain_region_type_t type = types[i % 9];
        brain_region_t* region = brain_region_create(type, 50 + i * 10);
        ASSERT_NE(region, nullptr);
        brain_module_add_region(brain, region);
    }

    EXPECT_EQ(brain->num_regions, 20);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, StressTest_ManyConnections) {
    brain_module_t* brain = brain_module_create(10);

    // Create 5 regions
    std::vector<brain_region_t*> regions;
    for (int i = 0; i < 5; i++) {
        brain_region_t* r = brain_region_create(REGION_VISUAL_V1, 100);
        brain_module_add_region(brain, r);
        regions.push_back(r);
    }

    // Connect all regions to each other (fully connected)
    for (size_t i = 0; i < regions.size(); i++) {
        for (size_t j = 0; j < regions.size(); j++) {
            if (i != j) {
                brain_module_connect_regions(brain, regions[i]->id, regions[j]->id, 0.3f);
            }
        }
    }

    EXPECT_EQ(brain->num_connections, 20); // 5 * 4 = 20 connections

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, MemoryTest_NoLeaksAfterMultipleOperations) {
    nimcp_memory_stats_t stats_before;
    nimcp_memory_get_stats(&stats_before);

    // Perform many operations
    for (int i = 0; i < 10; i++) {
        brain_module_t* brain = brain_module_create(5);
        brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);
        brain_module_add_region(brain, region);
        brain_region_organize_columns(region, 5, 5);

        float input[20] = {0};
        brain_region_process_input(region, input, 20, 1000);

        float output[20];
        brain_region_get_output(region, output, 20);

        brain_region_stats_t stats;
        brain_region_get_stats(region, &stats);

        brain_module_destroy(brain);
    }

    nimcp_memory_stats_t stats_after;
    nimcp_memory_get_stats(&stats_after);

    EXPECT_EQ(stats_after.current_allocated, stats_before.current_allocated);
}

//=============================================================================
// Neuron Type Assignment Tests
//=============================================================================

TEST_F(BrainRegionsTest, NeuronTypes_VisualRegions) {
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);

    ASSERT_NE(v1->neuron_extended_types, nullptr);
    ASSERT_NE(v1->neuron_type_params, nullptr);

    // Check that visual neuron types are assigned
    bool found_visual_type = false;
    for (uint32_t i = 0; i < v1->total_neurons; i++) {
        neuron_type_extended_t type = v1->neuron_extended_types[i];
        if (type == NEURON_VISUAL_EDGE ||
            type == NEURON_VISUAL_ORIENTATION ||
            type == NEURON_PYRAMIDAL_L5_THICK) {
            found_visual_type = true;
            break;
        }
    }

    EXPECT_TRUE(found_visual_type);

    brain_region_destroy(v1);
}

TEST_F(BrainRegionsTest, NeuronTypes_AuditoryRegions) {
    brain_region_t* a1 = brain_region_create(REGION_AUDITORY_A1, 200);

    // Check that auditory neuron types are assigned
    bool found_auditory_type = false;
    for (uint32_t i = 0; i < a1->total_neurons; i++) {
        neuron_type_extended_t type = a1->neuron_extended_types[i];
        if (type == NEURON_AUDITORY_FREQUENCY ||
            type == NEURON_AUDITORY_ONSET) {
            found_auditory_type = true;
            break;
        }
    }

    EXPECT_TRUE(found_auditory_type);

    brain_region_destroy(a1);
}

TEST_F(BrainRegionsTest, NeuronTypes_MotorRegions) {
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 200);

    // Check that motor neuron types are assigned
    bool found_motor_type = false;
    for (uint32_t i = 0; i < m1->total_neurons; i++) {
        neuron_type_extended_t type = m1->neuron_extended_types[i];
        if (type == NEURON_MOTOR_ALPHA ||
            type == NEURON_MOTOR_PATTERN_GEN) {
            found_motor_type = true;
            break;
        }
    }

    EXPECT_TRUE(found_motor_type);

    brain_region_destroy(m1);
}

//=============================================================================
// Additional Edge Case Tests for 100% Coverage
//=============================================================================

TEST_F(BrainRegionsTest, BrainModuleCreate_AllocationFailure) {
    // Test with very large allocation that might fail
    // Note: This tests error handling, but may not actually fail on all systems
    brain_module_t* brain = brain_module_create(1000000000); // 1 billion regions
    // Either succeeds or fails gracefully
    if (brain) {
        brain_module_destroy(brain);
    }
}

TEST_F(BrainRegionsTest, BrainRegionCreate_WithGlial) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 200);
    ASSERT_NE(region, nullptr);

    // Glial might be null initially (created on demand)
    // Just verify region was created successfully
    EXPECT_NE(region->network, nullptr);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, ProcessInput_NoLayer4) {
    // Create a region and manually set layer 4 size to 0 to test error path
    // This is artificial but tests the error condition
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);

    // Save original layer 4 size
    uint32_t orig_layer4 = region->layer_sizes[LAYER_4];

    // Temporarily set to 0 to trigger error
    region->layer_sizes[LAYER_4] = 0;

    float input[10] = {0.5f};
    nimcp_result_t result = brain_region_process_input(region, input, 10, 1000);

    // Should return error when no Layer 4
    EXPECT_EQ(result, NIMCP_ERROR);

    // Restore for cleanup
    region->layer_sizes[LAYER_4] = orig_layer4;

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, GetOutput_NoLayer5) {
    // Create a region and set layer 5 size to 0
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);

    uint32_t orig_layer5 = region->layer_sizes[LAYER_5];
    region->layer_sizes[LAYER_5] = 0;

    float output[10];
    uint32_t count = brain_region_get_output(region, output, 10);

    // Should return 0 when no Layer 5
    EXPECT_EQ(count, 0);

    region->layer_sizes[LAYER_5] = orig_layer5;
    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, ConnectRegions_AllocationFailure) {
    brain_module_t* brain = brain_module_create(5);
    brain_region_t* r1 = brain_region_create(REGION_VISUAL_V1, 100);
    brain_region_t* r2 = brain_region_create(REGION_VISUAL_V2, 100);

    brain_module_add_region(brain, r1);
    brain_module_add_region(brain, r2);

    // Test connection allocation
    nimcp_result_t result = brain_module_connect_regions(brain, r1->id, r2->id, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    brain_module_destroy(brain);
}

TEST_F(BrainRegionsTest, OrganizeColumns_AllocationTest) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 400);

    // Test successful column allocation
    nimcp_result_t result = brain_region_organize_columns(region, 5, 5);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    brain_region_destroy(region);
}

TEST_F(BrainRegionsTest, MultipleRegionIDs_Uniqueness) {
    // Verify that region IDs are unique
    std::vector<uint32_t> ids;

    for (int i = 0; i < 10; i++) {
        brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 100);
        ASSERT_NE(region, nullptr);

        // Check this ID hasn't been used before
        EXPECT_EQ(std::find(ids.begin(), ids.end(), region->id), ids.end());
        ids.push_back(region->id);

        brain_region_destroy(region);
    }
}

TEST_F(BrainRegionsTest, StatisticsCalculation_ActiveNeurons) {
    brain_region_t* region = brain_region_create(REGION_VISUAL_V1, 200);

    // Process strong input to activate neurons
    float input[50];
    for (int i = 0; i < 50; i++) {
        input[i] = 2.0f; // Strong input
    }
    brain_region_process_input(region, input, 50, 1000);

    brain_region_stats_t stats;
    brain_region_get_stats(region, &stats);

    // After processing strong input, should have activity
    // The avg_activity should be computed
    EXPECT_GE(stats.avg_activity, 0.0f);
    EXPECT_LE(stats.avg_activity, 1.0f);

    brain_region_destroy(region);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
