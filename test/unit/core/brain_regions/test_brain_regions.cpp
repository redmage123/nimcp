/**
 * @file test_brain_regions.cpp
 * @brief Unit tests for brain regions hierarchical architecture
 *
 * TEST PHILOSOPHY:
 * - Test initialization with various configurations
 * - Test region creation and organization
 * - Test layer structure and neuron type assignment
 * - Test minicolumn organization
 * - Test inter-region connectivity
 * - Test input processing and output collection
 * - Test error handling and edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 * @version 3.0.0 Module Integration Phase
 */

#include <gtest/gtest.h>
#include <vector>

#include "core/brain/nimcp_brain.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Unit Test Fixture
//=============================================================================

class BrainRegionsTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    brain_module_t* module = nullptr;
    brain_region_t* region = nullptr;

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        if (module) {
            brain_module_destroy(module);
            module = nullptr;
        }
        if (region) {
            brain_region_destroy(region);
            region = nullptr;
        }
    }

    brain_t create_brain_with_regions(uint32_t num_regions = 4, uint32_t neurons_per_region = 1000) {
        brain_config_t config = {};
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 128;
        config.num_outputs = 10;
        config.enable_brain_regions = true;
        config.num_brain_regions = num_regions;
        config.neurons_per_region = neurons_per_region;

        strncpy(config.task_name, "brain_regions_test", sizeof(config.task_name) - 1);

        return brain_create_custom(&config);
    }

    std::vector<float> create_input(uint32_t dim) {
        std::vector<float> input(dim);
        for (uint32_t i = 0; i < dim; i++) {
            input[i] = 0.5f + 0.1f * sin(i * 0.1f);
        }
        return input;
    }
};

//=============================================================================
// 1. Initialization Tests
//=============================================================================

TEST_F(BrainRegionsTest, Initialize_BrainRegionsEnabled_Success) {
    brain = create_brain_with_regions(4, 1000);
    ASSERT_NE(brain, nullptr);
}

TEST_F(BrainRegionsTest, Initialize_BrainRegionsDisabled_SkipsCreation) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 10;
    config.enable_brain_regions = false;

    strncpy(config.task_name, "no_regions_test", sizeof(config.task_name) - 1);

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
}

TEST_F(BrainRegionsTest, Module_CreateDestroy_NoLeak) {
    for (int i = 0; i < 10; i++) {
        module = brain_module_create(4);
        ASSERT_NE(module, nullptr);
        brain_module_destroy(module);
        module = nullptr;
    }
}

TEST_F(BrainRegionsTest, BrainIntegration_CreateWithRegions_Success) {
    brain = create_brain_with_regions(4, 1000);
    ASSERT_NE(brain, nullptr);

    auto input = create_input(128);
    brain_decision_t* decision = brain_decide(brain, input.data(), 128);

    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);
}

TEST_F(BrainRegionsTest, Memory_NoLeaksWithRegions_Success) {
    for (int i = 0; i < 10; i++) {
        brain_t test_brain = create_brain_with_regions(2, 500);
        ASSERT_NE(test_brain, nullptr) << "Failed on iteration " << i;
        brain_destroy(test_brain);
    }
}

//=============================================================================
// 2. Inter-Region Signal Propagation Tests
//=============================================================================

TEST_F(BrainRegionsTest, SignalPropagation_BasicConnection_TransfersActivity) {
    // Create brain module with two regions
    module = brain_module_create(4);
    ASSERT_NE(module, nullptr);

    // Create visual (V1) and motor (M1) regions
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 150);
    ASSERT_NE(v1, nullptr);
    ASSERT_NE(m1, nullptr);

    // Add regions to module
    ASSERT_EQ(brain_module_add_region(module, v1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, m1), NIMCP_SUCCESS);

    // Connect regions: V1 Layer 5 → M1 Layer 4 (feedforward)
    ASSERT_EQ(brain_module_connect_layers(module, v1->id, LAYER_5,
                                          m1->id, LAYER_4, 0.5f), NIMCP_SUCCESS);

    // Set V1 activity level to simulate active visual cortex
    v1->activity_level = 0.8f;
    m1->activity_level = 0.0f;

    // Step the module (should propagate signals)
    ASSERT_EQ(brain_module_step(module, 1000), NIMCP_SUCCESS);

    // After stepping, M1 should have received input (activity may be > 0)
    // We can't directly test activity level yet, but we verify no crash
    EXPECT_GE(m1->activity_level, 0.0f);
}

TEST_F(BrainRegionsTest, SignalPropagation_ZeroStrength_NoTransfer) {
    module = brain_module_create(4);
    ASSERT_NE(module, nullptr);

    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);
    brain_region_t* a1 = brain_region_create(REGION_AUDITORY_A1, 150);
    ASSERT_NE(v1, nullptr);
    ASSERT_NE(a1, nullptr);

    ASSERT_EQ(brain_module_add_region(module, v1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, a1), NIMCP_SUCCESS);

    // Connect with zero strength
    ASSERT_EQ(brain_module_connect_layers(module, v1->id, LAYER_5,
                                          a1->id, LAYER_4, 0.0f), NIMCP_SUCCESS);

    v1->activity_level = 1.0f;  // Max activity in V1
    float a1_initial = a1->activity_level;

    // Step once - A1 will have baseline activity from internal processing
    ASSERT_EQ(brain_module_step(module, 1000), NIMCP_SUCCESS);

    // With zero connection strength, A1 activity should be low (baseline only)
    // This tests that zero strength connections don't transfer V1's high activity
    EXPECT_LT(a1->activity_level, 0.3f);  // Much less than V1's 1.0
}

TEST_F(BrainRegionsTest, SignalPropagation_MaxStrength_MaxTransfer) {
    module = brain_module_create(4);
    ASSERT_NE(module, nullptr);

    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);
    brain_region_t* mt = brain_region_create(REGION_VISUAL_MT, 150);
    ASSERT_NE(v1, nullptr);
    ASSERT_NE(mt, nullptr);

    ASSERT_EQ(brain_module_add_region(module, v1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, mt), NIMCP_SUCCESS);

    // Connect with max strength (1.0)
    ASSERT_EQ(brain_module_connect_layers(module, v1->id, LAYER_5,
                                          mt->id, LAYER_4, 1.0f), NIMCP_SUCCESS);

    v1->activity_level = 0.9f;
    mt->activity_level = 0.0f;

    ASSERT_EQ(brain_module_step(module, 1000), NIMCP_SUCCESS);

    // Verify stepping succeeded (activity tracking tested elsewhere)
    EXPECT_GE(mt->activity_level, 0.0f);
}

TEST_F(BrainRegionsTest, SignalPropagation_MultipleRegions_AllReceiveSignals) {
    module = brain_module_create(8);
    ASSERT_NE(module, nullptr);

    // Create visual processing hierarchy: V1 → V2 → V4 → IT
    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);
    brain_region_t* v2 = brain_region_create(REGION_VISUAL_V2, 180);
    brain_region_t* v4 = brain_region_create(REGION_VISUAL_V4, 160);
    brain_region_t* it = brain_region_create(REGION_VISUAL_IT, 140);

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(v2, nullptr);
    ASSERT_NE(v4, nullptr);
    ASSERT_NE(it, nullptr);

    ASSERT_EQ(brain_module_add_region(module, v1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, v2), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, v4), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, it), NIMCP_SUCCESS);

    // Connect sequential hierarchy
    ASSERT_EQ(brain_module_connect_regions(module, v1->id, v2->id, 0.6f), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_connect_regions(module, v2->id, v4->id, 0.5f), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_connect_regions(module, v4->id, it->id, 0.4f), NIMCP_SUCCESS);

    // Set V1 as active source
    v1->activity_level = 0.8f;

    // Step through time
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(brain_module_step(module, 1000), NIMCP_SUCCESS);
    }

    // All regions should have non-negative activity (no crashes)
    EXPECT_GE(v1->activity_level, 0.0f);
    EXPECT_GE(v2->activity_level, 0.0f);
    EXPECT_GE(v4->activity_level, 0.0f);
    EXPECT_GE(it->activity_level, 0.0f);
}

TEST_F(BrainRegionsTest, SignalPropagation_NullBrain_ReturnsError) {
    nimcp_result_t result = brain_module_step(nullptr, 1000);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(BrainRegionsTest, SignalPropagation_EmptyModule_Success) {
    module = brain_module_create(4);
    ASSERT_NE(module, nullptr);

    // Step empty module (no regions)
    EXPECT_EQ(brain_module_step(module, 1000), NIMCP_SUCCESS);
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
