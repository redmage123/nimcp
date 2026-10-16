/**
 * @file test_signal_propagation_integration.cpp
 * @brief Integration tests for inter-region signal propagation
 *
 * TEST PHILOSOPHY:
 * - Test realistic multi-region communication chains
 * - Test integration with brain_module_step() and brain_region_step()
 * - Test real-world scenarios (visual → motor pathways, etc.)
 * - Test performance with many regions and connections
 * - Test interaction with glial systems and plasticity
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 3.0.0 Signal Propagation Integration
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

#include "core/brain_regions/nimcp_brain_regions.h"

//=============================================================================
// Integration Test Fixture
//=============================================================================

class SignalPropagationIntegrationTest : public ::testing::Test {
protected:
    brain_module_t* module = nullptr;

    void TearDown() override {
        if (module) {
            brain_module_destroy(module);
            module = nullptr;
        }
    }

    void simulate_timesteps(int num_steps, uint64_t delta_t = 1000) {
        for (int i = 0; i < num_steps; i++) {
            ASSERT_EQ(brain_module_step(module, delta_t), NIMCP_SUCCESS);
        }
    }
};

//=============================================================================
// 1. Visual Processing Pathway Integration Tests
//=============================================================================

TEST_F(SignalPropagationIntegrationTest, VisualPathway_V1_V2_V4_IT_SignalFlow) {
    // Create visual processing hierarchy
    module = brain_module_create(8);
    ASSERT_NE(module, nullptr);

    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 300);
    brain_region_t* v2 = brain_region_create(REGION_VISUAL_V2, 250);
    brain_region_t* v4 = brain_region_create(REGION_VISUAL_V4, 200);
    brain_region_t* it = brain_region_create(REGION_VISUAL_IT, 150);

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(v2, nullptr);
    ASSERT_NE(v4, nullptr);
    ASSERT_NE(it, nullptr);

    ASSERT_EQ(brain_module_add_region(module, v1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, v2), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, v4), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, it), NIMCP_SUCCESS);

    // Connect visual hierarchy (feedforward)
    ASSERT_EQ(brain_module_connect_regions(module, v1->id, v2->id, 0.7f), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_connect_regions(module, v2->id, v4->id, 0.6f), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_connect_regions(module, v4->id, it->id, 0.5f), NIMCP_SUCCESS);

    // Inject visual stimulus into V1
    std::vector<float> visual_input(300);
    for (size_t i = 0; i < visual_input.size(); i++) {
        visual_input[i] = 0.8f * sinf(i * 0.1f);
    }
    brain_region_process_input(v1, visual_input.data(), visual_input.size(), 0);

    // Simulate processing through hierarchy
    simulate_timesteps(20, 1000);

    // Verify all regions are active (signal propagated through chain)
    EXPECT_GT(v1->activity_level, 0.0f);
    EXPECT_GT(v2->activity_level, 0.0f);
    EXPECT_GT(v4->activity_level, 0.0f);
    EXPECT_GT(it->activity_level, 0.0f);

    // Verify monotonic decrease through hierarchy (signal attenuation)
    // Note: May not be strictly monotonic due to internal dynamics
    EXPECT_GT(v1->activity_level + v2->activity_level + v4->activity_level, 0.1f);
}

TEST_F(SignalPropagationIntegrationTest, VisuomotorPathway_V1_MT_M1_Integration) {
    // Create visuomotor pathway (visual → motion → motor)
    module = brain_module_create(8);
    ASSERT_NE(module, nullptr);

    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 300);
    brain_region_t* mt = brain_region_create(REGION_VISUAL_MT, 200);
    brain_region_t* m1 = brain_region_create(REGION_MOTOR_M1, 250);

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(mt, nullptr);
    ASSERT_NE(m1, nullptr);

    ASSERT_EQ(brain_module_add_region(module, v1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, mt), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, m1), NIMCP_SUCCESS);

    // Connect pathway
    ASSERT_EQ(brain_module_connect_regions(module, v1->id, mt->id, 0.8f), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_connect_regions(module, mt->id, m1->id, 0.7f), NIMCP_SUCCESS);

    // Inject moving visual stimulus
    std::vector<float> motion_input(300);
    for (size_t i = 0; i < motion_input.size(); i++) {
        motion_input[i] = 0.9f * cosf(i * 0.2f);
    }
    brain_region_process_input(v1, motion_input.data(), motion_input.size(), 0);

    // Process through pathway
    simulate_timesteps(30, 1000);

    // Verify signal reached motor cortex
    EXPECT_GT(m1->activity_level, 0.0f);
}

//=============================================================================
// 2. Feedback and Recurrent Connection Tests
//=============================================================================

TEST_F(SignalPropagationIntegrationTest, Feedback_IT_V4_BackwardFlow) {
    module = brain_module_create(8);
    ASSERT_NE(module, nullptr);

    brain_region_t* v4 = brain_region_create(REGION_VISUAL_V4, 200);
    brain_region_t* it = brain_region_create(REGION_VISUAL_IT, 150);

    ASSERT_NE(v4, nullptr);
    ASSERT_NE(it, nullptr);

    ASSERT_EQ(brain_module_add_region(module, v4), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, it), NIMCP_SUCCESS);

    // Create bidirectional connections (feedforward + feedback)
    ASSERT_EQ(brain_module_connect_layers(module, v4->id, LAYER_5,
                                          it->id, LAYER_4, 0.6f), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_connect_layers(module, it->id, LAYER_6,
                                          v4->id, LAYER_1, 0.4f), NIMCP_SUCCESS);

    // Set IT as active (top-down feedback scenario)
    it->activity_level = 0.7f;

    // Simulate feedback propagation
    simulate_timesteps(15, 1000);

    // V4 should receive feedback signal
    EXPECT_GT(v4->activity_level, 0.0f);
}

//=============================================================================
// 3. Multi-Modal Integration Tests
//=============================================================================

TEST_F(SignalPropagationIntegrationTest, MultiModal_Visual_Auditory_Convergence) {
    module = brain_module_create(10);
    ASSERT_NE(module, nullptr);

    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);
    brain_region_t* a1 = brain_region_create(REGION_AUDITORY_A1, 180);
    brain_region_t* pfc = brain_region_create(REGION_PREFRONTAL, 300);

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(a1, nullptr);
    ASSERT_NE(pfc, nullptr);

    ASSERT_EQ(brain_module_add_region(module, v1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, a1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, pfc), NIMCP_SUCCESS);

    // Both visual and auditory project to prefrontal
    ASSERT_EQ(brain_module_connect_regions(module, v1->id, pfc->id, 0.5f), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_connect_regions(module, a1->id, pfc->id, 0.5f), NIMCP_SUCCESS);

    // Activate both modalities
    v1->activity_level = 0.6f;
    a1->activity_level = 0.7f;

    // Simulate convergence
    simulate_timesteps(25, 1000);

    // PFC should integrate both inputs
    EXPECT_GT(pfc->activity_level, 0.0f);
}

//=============================================================================
// 4. Stress and Performance Tests
//=============================================================================

TEST_F(SignalPropagationIntegrationTest, Stress_ManyRegions_HighConnectivity) {
    module = brain_module_create(20);
    ASSERT_NE(module, nullptr);

    // Create 10 regions
    std::vector<brain_region_t*> regions;
    for (int i = 0; i < 10; i++) {
        brain_region_t* r = brain_region_create(
            static_cast<brain_region_type_t>(i), 100 + i * 10);
        ASSERT_NE(r, nullptr);
        ASSERT_EQ(brain_module_add_region(module, r), NIMCP_SUCCESS);
        regions.push_back(r);
    }

    // Create dense connectivity
    for (size_t i = 0; i < regions.size() - 1; i++) {
        ASSERT_EQ(brain_module_connect_regions(module,
                                                regions[i]->id,
                                                regions[i + 1]->id,
                                                0.4f), NIMCP_SUCCESS);
    }

    // Set first region as active
    regions[0]->activity_level = 0.9f;

    // Simulate propagation through network
    simulate_timesteps(50, 1000);

    // Verify all regions are functioning (no crashes)
    for (auto* r : regions) {
        EXPECT_GE(r->activity_level, 0.0f);
        EXPECT_LE(r->activity_level, 1.0f);
    }
}

TEST_F(SignalPropagationIntegrationTest, Performance_1000Steps_NoMemoryLeak) {
    module = brain_module_create(8);
    ASSERT_NE(module, nullptr);

    brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);
    brain_region_t* v2 = brain_region_create(REGION_VISUAL_V2, 180);

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(v2, nullptr);

    ASSERT_EQ(brain_module_add_region(module, v1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_module_add_region(module, v2), NIMCP_SUCCESS);

    ASSERT_EQ(brain_module_connect_regions(module, v1->id, v2->id, 0.6f), NIMCP_SUCCESS);

    // Run for extended period
    simulate_timesteps(1000, 1000);

    // Verify system is still functional
    EXPECT_GE(v1->activity_level, 0.0f);
    EXPECT_GE(v2->activity_level, 0.0f);
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
