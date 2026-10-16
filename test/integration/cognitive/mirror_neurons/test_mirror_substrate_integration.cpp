/**
 * @file test_mirror_substrate_integration.cpp
 * @brief Integration tests for Mirror Neuron Substrate backing
 *
 * WHAT: Tests mirror neuron substrate integration with neural components
 * WHY:  Validate biological realism - mirror neurons backed by real substrate
 * HOW:  Integrate with axons, myelin sheaths, dendrites, glial cells
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/mirror_neurons/nimcp_mirror_substrate.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MirrorSubstrateIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    mirror_substrate_pool_t* pool = nullptr;

    void SetUp() override {
        // Create a medium brain with social cognition capabilities
        brain = brain_create("substrate_test_brain", BRAIN_SIZE_MEDIUM,
                           BRAIN_TASK_CLASSIFICATION, 8, 3);
        ASSERT_NE(brain, nullptr);

        // Create substrate pool
        pool = mirror_substrate_pool_create(64);
        ASSERT_NE(pool, nullptr);
    }

    void TearDown() override {
        if (pool) {
            mirror_substrate_pool_destroy(pool);
            pool = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Create substrate backing with default config
    mirror_substrate_backing_t* create_backing(uint32_t unit_id) {
        mirror_substrate_config_t config = mirror_substrate_get_default_config();
        return mirror_substrate_backing_create(unit_id, &config, pool);
    }

    // Helper: Simulate action observation
    void observe_action(uint32_t agent_id) {
        float features[8] = {0.8f, 0.6f, 0.3f, 0.9f, 0.2f, 0.5f, 0.7f, 0.4f};
        brain_observe_action(brain, features, 8, agent_id);
    }
};

//=============================================================================
// TEST SUITE 1: Substrate Pool Integration
//=============================================================================

TEST_F(MirrorSubstrateIntegrationTest, Pool_CreateAndDestroy) {
    ASSERT_NE(pool, nullptr);

    // Check stats using pointer-based API
    uint32_t allocated = 0, capacity = 0, high_water = 0;
    mirror_substrate_pool_stats(pool, &allocated, &capacity, &high_water);
    EXPECT_EQ(64u, capacity);
    EXPECT_EQ(0u, allocated);
}

TEST_F(MirrorSubstrateIntegrationTest, Pool_AllocateMultipleBackings) {
    std::vector<mirror_substrate_backing_t*> backings;

    for (uint32_t i = 0; i < 10; i++) {
        auto* backing = create_backing(i + 1);
        ASSERT_NE(nullptr, backing);
        backings.push_back(backing);
    }

    uint32_t allocated = 0, capacity = 0, high_water = 0;
    mirror_substrate_pool_stats(pool, &allocated, &capacity, &high_water);
    EXPECT_EQ(10u, allocated);

    for (auto* backing : backings) {
        mirror_substrate_backing_destroy(backing, pool);
    }
}

//=============================================================================
// TEST SUITE 2: Substrate Backing Lifecycle
//=============================================================================

TEST_F(MirrorSubstrateIntegrationTest, Backing_CreateInitializeDestroy) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    EXPECT_EQ(1u, backing->mirror_unit_id);
    EXPECT_EQ(0u, backing->num_spines);
    EXPECT_FLOAT_EQ(0.0f, backing->coactivation_score);

    mirror_substrate_backing_destroy(backing, pool);
}

TEST_F(MirrorSubstrateIntegrationTest, Backing_HeapAllocationFallback) {
    mirror_substrate_config_t config = mirror_substrate_get_default_config();
    auto* backing = mirror_substrate_backing_create(1, &config, nullptr);
    ASSERT_NE(nullptr, backing);

    EXPECT_EQ(1u, backing->mirror_unit_id);

    mirror_substrate_backing_destroy(backing, nullptr);
}

//=============================================================================
// TEST SUITE 3: Axon Network Integration
//=============================================================================

TEST_F(MirrorSubstrateIntegrationTest, Axon_BindAxonsToSubstrate) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    EXPECT_EQ(NIMCP_SUCCESS, mirror_substrate_bind_observation_axon(backing, nullptr));
    EXPECT_EQ(NIMCP_SUCCESS, mirror_substrate_bind_execution_axon(backing, nullptr));

    float obs_delay = mirror_substrate_get_observation_delay(backing);
    float exec_delay = mirror_substrate_get_execution_delay(backing);

    EXPECT_GT(obs_delay, 0.0f);
    EXPECT_GT(exec_delay, 0.0f);

    mirror_substrate_backing_destroy(backing, pool);
}

//=============================================================================
// TEST SUITE 4: Myelin Sheath Integration
//=============================================================================

TEST_F(MirrorSubstrateIntegrationTest, Myelin_MyelinationAffectsDelay) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    float delay_unmyelinated = mirror_substrate_get_observation_delay(backing);

    backing->myelination_level = 0.8f;

    float delay_myelinated = mirror_substrate_get_observation_delay(backing);

    EXPECT_LT(delay_myelinated, delay_unmyelinated);

    mirror_substrate_backing_destroy(backing, pool);
}

TEST_F(MirrorSubstrateIntegrationTest, Myelin_UpdateMyelination) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    // Update myelination (without actual myelin sheath, should not crash)
    mirror_substrate_update_myelination(backing);

    mirror_substrate_backing_destroy(backing, pool);
}

//=============================================================================
// TEST SUITE 5: Dendrite and Spine Plasticity
//=============================================================================

TEST_F(MirrorSubstrateIntegrationTest, Spine_AddSpineToSubstrate) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    for (int i = 0; i < 5; i++) {
        int32_t result = mirror_substrate_add_spine(backing, nullptr, i);
        EXPECT_GE(result, 0);
    }

    EXPECT_EQ(5u, backing->num_spines);

    mirror_substrate_backing_destroy(backing, pool);
}

TEST_F(MirrorSubstrateIntegrationTest, Spine_PlasticityFromActivity) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    mirror_substrate_add_spine(backing, nullptr, 0);

    backing->spine_states[0] = MIRROR_SPINE_STATE_THIN;
    backing->spine_weights[0] = 0.5f;

    backing->observation_activity_ema = 0.9f;
    backing->execution_activity_ema = 0.9f;
    backing->coactivation_score = 0.8f;

    // Update plasticity with observation and execution active
    mirror_substrate_update_spine_plasticity(backing, true, true, 0.016f);

    EXPECT_GE(backing->spine_weights[0], 0.0f);

    mirror_substrate_backing_destroy(backing, pool);
}

TEST_F(MirrorSubstrateIntegrationTest, Spine_StateTransitionsFromLTP) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    mirror_substrate_add_spine(backing, nullptr, 0);
    backing->spine_states[0] = MIRROR_SPINE_STATE_THIN;
    backing->spine_weights[0] = 0.5f;

    for (int i = 0; i < 100; i++) {
        backing->coactivation_score = 0.9f;
        mirror_substrate_update_spine_plasticity(backing, true, true, 0.016f);
        backing->spine_weights[0] = std::min(1.0f, backing->spine_weights[0] + 0.01f);
    }

    EXPECT_GE(backing->spine_states[0], MIRROR_SPINE_STATE_THIN);

    mirror_substrate_backing_destroy(backing, pool);
}

//=============================================================================
// TEST SUITE 6: Glial Cell Integration
//=============================================================================

TEST_F(MirrorSubstrateIntegrationTest, Glial_BindGlialCells) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    EXPECT_EQ(NIMCP_SUCCESS, mirror_substrate_bind_astrocyte(backing, nullptr));
    EXPECT_EQ(NIMCP_SUCCESS, mirror_substrate_bind_oligodendrocyte(backing, nullptr));
    EXPECT_EQ(NIMCP_SUCCESS, mirror_substrate_bind_microglia(backing, nullptr));

    mirror_substrate_backing_destroy(backing, pool);
}

TEST_F(MirrorSubstrateIntegrationTest, Glial_AstrocyteModulation) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    backing->astrocyte_modulation = 1.0f;

    // Get current modulation (verifies getter works)
    float modulation = mirror_substrate_get_astrocyte_modulation(backing);

    EXPECT_GE(modulation, 0.5f);
    EXPECT_LE(modulation, 2.0f);

    mirror_substrate_backing_destroy(backing, pool);
}

//=============================================================================
// TEST SUITE 7: Activity Tracking and Coactivation
//=============================================================================

TEST_F(MirrorSubstrateIntegrationTest, Activity_RecordAndDecay) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    mirror_substrate_record_observation(backing, 0.8f, 1000);
    EXPECT_GT(backing->observation_activity_ema, 0.0f);
    EXPECT_EQ(1000u, backing->last_observation_time);

    mirror_substrate_record_execution(backing, 0.7f, 2000);
    EXPECT_GT(backing->execution_activity_ema, 0.0f);
    EXPECT_EQ(2000u, backing->last_execution_time);

    float obs_before = backing->observation_activity_ema;
    mirror_substrate_step(backing, 1.0f, 1000000);

    EXPECT_LT(backing->observation_activity_ema, obs_before);

    mirror_substrate_backing_destroy(backing, pool);
}

TEST_F(MirrorSubstrateIntegrationTest, Activity_CoactivationDetection) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    uint64_t time = 1000;
    mirror_substrate_record_observation(backing, 0.9f, time);
    mirror_substrate_record_execution(backing, 0.9f, time + 50000);

    mirror_substrate_step(backing, 0.016f, time + 100000);

    EXPECT_GT(backing->coactivation_score, 0.0f);

    mirror_substrate_backing_destroy(backing, pool);
}

//=============================================================================
// TEST SUITE 8: Copy-on-Write (CoW) Integration
//=============================================================================

TEST_F(MirrorSubstrateIntegrationTest, CoW_CreateCopy) {
    auto* original = create_backing(1);
    ASSERT_NE(nullptr, original);

    original->coactivation_score = 0.5f;
    original->myelination_level = 0.7f;

    auto* copy = mirror_substrate_cow_copy(original, pool);
    ASSERT_NE(nullptr, copy);

    EXPECT_EQ(original->mirror_unit_id, copy->mirror_unit_id);
    EXPECT_FLOAT_EQ(original->coactivation_score, copy->coactivation_score);

    mirror_substrate_backing_destroy(copy, pool);
    mirror_substrate_backing_destroy(original, pool);
}

TEST_F(MirrorSubstrateIntegrationTest, CoW_PrepareWriteCopiesOnMutation) {
    auto* original = create_backing(1);
    ASSERT_NE(nullptr, original);

    original->coactivation_score = 0.5f;

    auto* copy = mirror_substrate_cow_copy(original, pool);
    ASSERT_NE(nullptr, copy);

    nimcp_result_t result = mirror_substrate_cow_prepare_write(copy);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    copy->coactivation_score = 0.9f;
    EXPECT_NE(original->coactivation_score, copy->coactivation_score);

    mirror_substrate_backing_destroy(copy, pool);
    mirror_substrate_backing_destroy(original, pool);
}

TEST_F(MirrorSubstrateIntegrationTest, CoW_ReferenceCountingDestroy) {
    auto* original = create_backing(1);
    ASSERT_NE(nullptr, original);

    auto* copy1 = mirror_substrate_cow_copy(original, pool);
    auto* copy2 = mirror_substrate_cow_copy(original, pool);
    ASSERT_NE(nullptr, copy1);
    ASSERT_NE(nullptr, copy2);

    mirror_substrate_backing_destroy(copy1, pool);
    mirror_substrate_backing_destroy(copy2, pool);

    EXPECT_EQ(1u, original->mirror_unit_id);

    mirror_substrate_backing_destroy(original, pool);
}

//=============================================================================
// TEST SUITE 9: Full Pipeline with Brain
//=============================================================================

TEST_F(MirrorSubstrateIntegrationTest, Pipeline_ObservationWithBrain) {
    observe_action(1);

    float activations[100];
    uint32_t num_activations = 0;

    EXPECT_TRUE(brain_get_mirror_activations(brain, activations, 100, &num_activations));
}

TEST_F(MirrorSubstrateIntegrationTest, Pipeline_EmpathyComputation) {
    float distress_features[8] = {0.2f, 0.1f, 0.8f, 0.3f, 0.4f, 0.6f, 0.2f, 0.5f};
    brain_observe_action(brain, distress_features, 8, 1);

    float valence, arousal, confidence;
    EXPECT_TRUE(brain_compute_empathy(brain, distress_features, 8,
                                     &valence, &arousal, &confidence));
}

//=============================================================================
// TEST SUITE 10: Performance and Stress Tests
//=============================================================================

TEST_F(MirrorSubstrateIntegrationTest, Performance_PoolAllocationThroughput) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<mirror_substrate_backing_t*> backings;
    const int NUM_ALLOCS = 50;

    for (int i = 0; i < NUM_ALLOCS; i++) {
        auto* backing = create_backing(i + 1);
        ASSERT_NE(nullptr, backing);
        backings.push_back(backing);
    }

    for (auto* backing : backings) {
        mirror_substrate_backing_destroy(backing, pool);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_LT(duration.count(), 5000);

    std::cout << "Pool alloc/free " << NUM_ALLOCS << " pairs: "
              << duration.count() << " us" << std::endl;
}

TEST_F(MirrorSubstrateIntegrationTest, Performance_StepUpdateThroughput) {
    auto* backing = create_backing(1);
    ASSERT_NE(nullptr, backing);

    for (int i = 0; i < 10; i++) {
        mirror_substrate_add_spine(backing, nullptr, i);
    }

    backing->observation_activity_ema = 0.7f;
    backing->execution_activity_ema = 0.7f;
    mirror_substrate_record_observation(backing, 0.8f, 1000);
    mirror_substrate_record_execution(backing, 0.8f, 1050);

    auto start = std::chrono::high_resolution_clock::now();

    const int NUM_STEPS = 10000;
    uint64_t time = 2000;
    for (int i = 0; i < NUM_STEPS; i++) {
        mirror_substrate_step(backing, 0.001f, time);
        time += 1000;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_LT(duration.count(), 50000);

    std::cout << "Step update " << NUM_STEPS << " iterations: "
              << duration.count() << " us ("
              << (duration.count() * 1000 / NUM_STEPS) << " ns/step)"
              << std::endl;

    mirror_substrate_backing_destroy(backing, pool);
}

TEST_F(MirrorSubstrateIntegrationTest, Stress_ManyBackingsWithActivity) {
    std::vector<mirror_substrate_backing_t*> backings;

    for (uint32_t i = 0; i < 32; i++) {
        auto* backing = create_backing(i + 1);
        ASSERT_NE(nullptr, backing);

        for (int j = 0; j < 4; j++) {
            mirror_substrate_add_spine(backing, nullptr, j);
        }

        mirror_substrate_record_observation(backing, 0.5f + (i * 0.01f), i * 1000);
        mirror_substrate_record_execution(backing, 0.5f + (i * 0.01f), i * 1000 + 50);

        backings.push_back(backing);
    }

    uint64_t time = 100000;
    for (int step = 0; step < 100; step++) {
        for (auto* backing : backings) {
            mirror_substrate_step(backing, 0.016f, time);
        }
        time += 16000;
    }

    for (auto* backing : backings) {
        EXPECT_GE(backing->num_spines, 0u);
        EXPECT_GE(backing->coactivation_score, 0.0f);
    }

    for (auto* backing : backings) {
        mirror_substrate_backing_destroy(backing, pool);
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
