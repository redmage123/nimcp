//=============================================================================
// test_myelin_sheath.cpp - Unit Tests for Myelin Sheath Module
//=============================================================================
/**
 * @file test_myelin_sheath.cpp
 * @brief Comprehensive unit tests for myelin sheath functionality
 *
 * WHAT: Unit tests for myelin sheath module
 * WHY:  Ensure correctness of all myelin sheath operations
 * HOW:  GoogleTest framework with fixtures for isolation
 *
 * TEST CATEGORIES:
 * 1. Memory Pool Tests - Sheath and segment pool allocation
 * 2. Sheath Creation Tests - Basic and advanced creation
 * 3. Segment Management Tests - Add, remove, find segments
 * 4. Structural Properties Tests - Lamellae, compaction, g-ratio
 * 5. Conduction Properties Tests - Velocity, delay calculations
 * 6. Health/Integrity Tests - Damage, repair, health states
 * 7. Paranodal Junction Tests - State management
 * 8. Metabolic Support Tests - ATP, lactate, trophic support
 * 9. Dynamics Tests - Step simulation, myelination/demyelination
 * 10. Copy-on-Write Tests - CoW functionality
 * 11. Network Tests - Network management operations
 * 12. Integration Helper Tests - Axon integration functions
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MyelinSheathTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_init();
    }

    void TearDown() override {
        nimcp_shutdown();
        nimcp_memory_cleanup();
    }
};

//=============================================================================
// 1. Memory Pool Tests - Sheath Pool
//=============================================================================

TEST_F(MyelinSheathTest, SheathPool_CreateDestroy) {
    myelin_sheath_pool_t* pool = myelin_sheath_pool_create(64);
    ASSERT_NE(pool, nullptr);
    EXPECT_GE(pool->capacity, 64u);
    EXPECT_EQ(pool->allocated_count, 0u);
    myelin_sheath_pool_destroy(pool);
}

TEST_F(MyelinSheathTest, SheathPool_AllocFree) {
    myelin_sheath_pool_t* pool = myelin_sheath_pool_create(64);
    ASSERT_NE(pool, nullptr);

    myelin_sheath_t* sheath = myelin_sheath_pool_alloc(pool);
    ASSERT_NE(sheath, nullptr);
    EXPECT_EQ(pool->allocated_count, 1u);

    myelin_sheath_pool_free(pool, sheath);
    EXPECT_EQ(pool->allocated_count, 0u);

    myelin_sheath_pool_destroy(pool);
}

TEST_F(MyelinSheathTest, SheathPool_MultipleAlloc) {
    myelin_sheath_pool_t* pool = myelin_sheath_pool_create(64);
    ASSERT_NE(pool, nullptr);

    myelin_sheath_t* sheaths[10];
    for (int i = 0; i < 10; i++) {
        sheaths[i] = myelin_sheath_pool_alloc(pool);
        ASSERT_NE(sheaths[i], nullptr);
    }
    EXPECT_EQ(pool->allocated_count, 10u);

    for (int i = 0; i < 10; i++) {
        myelin_sheath_pool_free(pool, sheaths[i]);
    }
    EXPECT_EQ(pool->allocated_count, 0u);

    myelin_sheath_pool_destroy(pool);
}

TEST_F(MyelinSheathTest, SheathPool_Exhaustion) {
    myelin_sheath_pool_t* pool = myelin_sheath_pool_create(64);
    ASSERT_NE(pool, nullptr);

    // Allocate all slots
    for (uint32_t i = 0; i < pool->capacity; i++) {
        myelin_sheath_t* sheath = myelin_sheath_pool_alloc(pool);
        ASSERT_NE(sheath, nullptr);
    }

    // Next allocation should fail
    myelin_sheath_t* overflow = myelin_sheath_pool_alloc(pool);
    EXPECT_EQ(overflow, nullptr);

    myelin_sheath_pool_destroy(pool);
}

TEST_F(MyelinSheathTest, SheathPool_NullHandling) {
    EXPECT_EQ(myelin_sheath_pool_create(0), nullptr);
    myelin_sheath_pool_destroy(nullptr);  // Should not crash
    EXPECT_EQ(myelin_sheath_pool_alloc(nullptr), nullptr);
    myelin_sheath_pool_free(nullptr, nullptr);  // Should not crash
}

//=============================================================================
// 1b. Memory Pool Tests - Segment Pool
//=============================================================================

TEST_F(MyelinSheathTest, SegmentPool_CreateDestroy) {
    myelin_segment_pool_t* pool = myelin_segment_pool_create(128);
    ASSERT_NE(pool, nullptr);
    EXPECT_GE(pool->capacity, 128u);
    myelin_segment_pool_destroy(pool);
}

TEST_F(MyelinSheathTest, SegmentPool_AllocFree) {
    myelin_segment_pool_t* pool = myelin_segment_pool_create(128);
    ASSERT_NE(pool, nullptr);

    myelin_segment_t* segment = myelin_segment_pool_alloc(pool);
    ASSERT_NE(segment, nullptr);
    EXPECT_EQ(pool->allocated_count, 1u);

    myelin_segment_pool_free(pool, segment);
    EXPECT_EQ(pool->allocated_count, 0u);

    myelin_segment_pool_destroy(pool);
}

//=============================================================================
// 2. Sheath Creation Tests
//=============================================================================

TEST_F(MyelinSheathTest, SheathCreate_Basic) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    ASSERT_NE(sheath, nullptr);

    EXPECT_EQ(sheath->id, 1u);
    EXPECT_EQ(sheath->axon_id, 100u);
    EXPECT_EQ(sheath->oligodendrocyte_id, 50u);
    EXPECT_EQ(sheath->max_segments, 16u);
    EXPECT_EQ(sheath->num_segments, 0u);
    EXPECT_EQ(sheath->overall_health, MYELIN_HEALTH_INTACT);
    EXPECT_FLOAT_EQ(sheath->mean_integrity, 1.0f);
    EXPECT_EQ(sheath->cow_ref_count, 1u);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, SheathCreate_ForAxon) {
    myelin_sheath_t* sheath = myelin_sheath_create_for_axon(
        1, 100, 50, 1000.0f, 2.0f, 16);
    ASSERT_NE(sheath, nullptr);

    // Should have created segments automatically
    EXPECT_GT(sheath->num_segments, 0u);
    EXPECT_GT(sheath->total_length_um, 0.0f);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, SheathCreate_DefaultSegments) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 0);
    ASSERT_NE(sheath, nullptr);
    EXPECT_EQ(sheath->max_segments, 16u);  // Default
    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, SheathDestroy_NullSafe) {
    myelin_sheath_destroy(nullptr);  // Should not crash
}

//=============================================================================
// 3. Segment Management Tests
//=============================================================================

TEST_F(MyelinSheathTest, Segment_Add) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    ASSERT_NE(sheath, nullptr);

    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);
    ASSERT_NE(segment, nullptr);

    EXPECT_EQ(sheath->num_segments, 1u);
    EXPECT_EQ(segment->id, 0u);
    EXPECT_EQ(segment->sheath_id, 1u);
    EXPECT_EQ(segment->axon_id, 100u);
    EXPECT_FLOAT_EQ(segment->start_position_um, 0.0f);
    EXPECT_FLOAT_EQ(segment->length_um, 200.0f);
    EXPECT_FLOAT_EQ(segment->inner_diameter_um, 2.0f);
    EXPECT_GT(segment->outer_diameter_um, segment->inner_diameter_um);
    EXPECT_GT(segment->g_ratio, 0.0f);
    EXPECT_LT(segment->g_ratio, 1.0f);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Segment_AddMultiple) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    ASSERT_NE(sheath, nullptr);

    for (int i = 0; i < 5; i++) {
        myelin_segment_t* segment = myelin_sheath_add_segment(
            sheath, (float)i * 200.0f, 200.0f, 2.0f);
        ASSERT_NE(segment, nullptr);
    }

    EXPECT_EQ(sheath->num_segments, 5u);
    EXPECT_FLOAT_EQ(sheath->total_length_um, 1000.0f);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Segment_Remove) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_sheath_add_segment(sheath, 0.0f, 200.0f, 2.0f);
    myelin_sheath_add_segment(sheath, 200.0f, 200.0f, 2.0f);

    EXPECT_EQ(sheath->num_segments, 2u);

    nimcp_result_t result = myelin_sheath_remove_segment(sheath, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(sheath->num_segments, 1u);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Segment_Get) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* added = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    myelin_segment_t* retrieved = myelin_sheath_get_segment(sheath, 0);
    EXPECT_EQ(retrieved, added);

    EXPECT_EQ(myelin_sheath_get_segment(sheath, 99), nullptr);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Segment_FindAt) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_sheath_add_segment(sheath, 0.0f, 200.0f, 2.0f);
    myelin_sheath_add_segment(sheath, 201.0f, 200.0f, 2.0f);

    myelin_segment_t* found = myelin_sheath_find_segment_at(sheath, 100.0f);
    ASSERT_NE(found, nullptr);
    EXPECT_FLOAT_EQ(found->start_position_um, 0.0f);

    found = myelin_sheath_find_segment_at(sheath, 300.0f);
    ASSERT_NE(found, nullptr);
    EXPECT_FLOAT_EQ(found->start_position_um, 201.0f);

    EXPECT_EQ(myelin_sheath_find_segment_at(sheath, 500.0f), nullptr);

    myelin_sheath_destroy(sheath);
}

//=============================================================================
// 4. Structural Properties Tests
//=============================================================================

TEST_F(MyelinSheathTest, Structure_SetLamellae) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    nimcp_result_t result = myelin_segment_set_lamellae(segment, 40);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(segment->num_lamellae, 40u);
    EXPECT_GT(segment->thickness_um, 0.0f);

    // Test clamping
    myelin_segment_set_lamellae(segment, 0);
    EXPECT_EQ(segment->num_lamellae, NIMCP_MYELIN_MIN_LAMELLAE);

    myelin_segment_set_lamellae(segment, 1000);
    EXPECT_EQ(segment->num_lamellae, NIMCP_MYELIN_MAX_LAMELLAE);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Structure_SetCompaction) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    nimcp_result_t result = myelin_segment_set_compaction(
        segment, MYELIN_COMPACT_FULL, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(segment->compaction, MYELIN_COMPACT_FULL);
    EXPECT_FLOAT_EQ(segment->compaction_score, 1.0f);
    EXPECT_FLOAT_EQ(segment->mdl_formation, 1.0f);
    EXPECT_FLOAT_EQ(segment->ipl_formation, 1.0f);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Structure_ComputeOptimalLamellae) {
    // For 2um diameter axon with 0.77 g-ratio
    uint32_t lamellae = myelin_compute_optimal_lamellae(2.0f, 0.77f);
    EXPECT_GT(lamellae, 0u);
    EXPECT_LE(lamellae, NIMCP_MYELIN_MAX_LAMELLAE);

    // Verify g-ratio is close to target
    float computed_g = myelin_compute_g_ratio(2.0f, lamellae);
    EXPECT_NEAR(computed_g, 0.77f, 0.1f);
}

TEST_F(MyelinSheathTest, Structure_ComputeGRatio) {
    // More lamellae = lower g-ratio (thicker myelin)
    float g1 = myelin_compute_g_ratio(2.0f, 10);
    float g2 = myelin_compute_g_ratio(2.0f, 50);
    EXPECT_GT(g1, g2);  // More lamellae = lower g-ratio
}

TEST_F(MyelinSheathTest, Structure_ComputeThickness) {
    float t1 = myelin_compute_thickness(10);
    float t2 = myelin_compute_thickness(40);
    EXPECT_GT(t2, t1);  // More lamellae = more thickness
    EXPECT_GT(t1, 0.0f);
}

//=============================================================================
// 5. Conduction Properties Tests
//=============================================================================

TEST_F(MyelinSheathTest, Conduction_ComputeVelocity) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    // Set optimal myelination
    myelin_segment_set_lamellae(segment, NIMCP_MYELIN_OPTIMAL_LAMELLAE);
    segment->compaction_score = 1.0f;
    segment->integrity = 1.0f;

    float velocity = myelin_segment_compute_velocity(segment);
    EXPECT_GT(velocity, NIMCP_MYELIN_BASE_VELOCITY_MS);
    EXPECT_LE(velocity, NIMCP_MYELIN_MAX_VELOCITY_MS);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Conduction_ComputeDelay) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 1000.0f, 2.0f);  // 1mm segment

    float delay = myelin_segment_compute_delay(segment);
    EXPECT_GT(delay, 0.0f);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Conduction_UpdateSheath) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_sheath_add_segment(sheath, 0.0f, 500.0f, 2.0f);
    myelin_sheath_add_segment(sheath, 501.0f, 500.0f, 2.0f);

    myelin_sheath_update_conduction(sheath);

    EXPECT_GT(sheath->effective_velocity_ms, 0.0f);
    EXPECT_GT(sheath->total_delay_ms, 0.0f);
    EXPECT_GT(sheath->velocity_ratio, 0.0f);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Conduction_VelocityRatio) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_sheath_add_segment(sheath, 0.0f, 500.0f, 2.0f);
    myelin_sheath_update_conduction(sheath);

    float ratio = myelin_sheath_get_velocity_ratio(sheath);
    EXPECT_GE(ratio, 1.0f);  // At least as fast as unmyelinated

    myelin_sheath_destroy(sheath);
}

//=============================================================================
// 6. Health/Integrity Tests
//=============================================================================

TEST_F(MyelinSheathTest, Health_ApplyDamage) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    EXPECT_FLOAT_EQ(segment->integrity, 1.0f);

    nimcp_result_t result = myelin_segment_apply_damage(segment, 0.3f, 1000);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(segment->integrity, 0.7f);
    EXPECT_GT(segment->damage_accumulated, 0.0f);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Health_Repair) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    myelin_segment_apply_damage(segment, 0.5f, 1000);
    EXPECT_FLOAT_EQ(segment->integrity, 0.5f);

    nimcp_result_t result = myelin_segment_repair(segment, 0.3f, 2000);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(segment->integrity, 0.8f);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Health_StateTransitions) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    // Start intact
    EXPECT_EQ(segment->health, MYELIN_HEALTH_INTACT);

    // Apply damage - should transition through states
    myelin_segment_apply_damage(segment, 0.2f, 1000);
    myelin_segment_update_health(segment, 1000);
    // Still above damaged threshold

    myelin_segment_apply_damage(segment, 0.5f, 2000);
    myelin_segment_update_health(segment, 2000);
    EXPECT_NE(segment->health, MYELIN_HEALTH_INTACT);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Health_UpdateSheath) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_sheath_add_segment(sheath, 0.0f, 200.0f, 2.0f);
    myelin_sheath_add_segment(sheath, 201.0f, 200.0f, 2.0f);

    myelin_sheath_update_health(sheath);

    EXPECT_EQ(sheath->overall_health, MYELIN_HEALTH_INTACT);
    EXPECT_FLOAT_EQ(sheath->mean_integrity, 1.0f);
    EXPECT_FLOAT_EQ(sheath->min_integrity, 1.0f);
    EXPECT_EQ(sheath->damaged_segment_count, 0u);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Health_StateToString) {
    EXPECT_STREQ(myelin_health_state_to_string(MYELIN_HEALTH_INTACT), "intact");
    EXPECT_STREQ(myelin_health_state_to_string(MYELIN_HEALTH_DAMAGED), "damaged");
    EXPECT_STREQ(myelin_health_state_to_string(MYELIN_HEALTH_DEMYELINATING), "demyelinating");
}

//=============================================================================
// 7. Paranodal Junction Tests
//=============================================================================

TEST_F(MyelinSheathTest, Paranode_SetState) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    nimcp_result_t result = myelin_segment_set_paranodes(
        segment, PARANODE_MATURE, PARANODE_MATURE);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(segment->proximal_paranode, PARANODE_MATURE);
    EXPECT_EQ(segment->distal_paranode, PARANODE_MATURE);
    EXPECT_FLOAT_EQ(segment->paranode_integrity, 1.0f);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Paranode_Functional) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    // Initially forming
    EXPECT_FALSE(myelin_segment_paranodes_functional(segment));

    // Set to mature
    myelin_segment_set_paranodes(segment, PARANODE_MATURE, PARANODE_MATURE);
    EXPECT_TRUE(myelin_segment_paranodes_functional(segment));

    // Disrupt one
    myelin_segment_set_paranodes(segment, PARANODE_DISRUPTED, PARANODE_MATURE);
    EXPECT_FALSE(myelin_segment_paranodes_functional(segment));

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Paranode_UpdateIntegrity) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    myelin_segment_update_paranode_integrity(segment, 0.75f);
    EXPECT_FLOAT_EQ(segment->paranode_integrity, 0.75f);

    // Test clamping
    myelin_segment_update_paranode_integrity(segment, 1.5f);
    EXPECT_FLOAT_EQ(segment->paranode_integrity, 1.0f);

    myelin_sheath_destroy(sheath);
}

//=============================================================================
// 8. Metabolic Support Tests
//=============================================================================

TEST_F(MyelinSheathTest, Metabolic_Update) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    float initial_atp = segment->atp_level;
    myelin_segment_update_metabolism(segment, 1.0f);

    // ATP should decrease due to maintenance
    EXPECT_LT(segment->atp_level, initial_atp);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Metabolic_ReceiveLactate) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    myelin_segment_receive_lactate(segment, 0.5f);
    EXPECT_FLOAT_EQ(segment->lactate_received, 0.5f);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Metabolic_TrophicSupport) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    myelin_segment_set_trophic_support(segment, 0.8f);
    EXPECT_FLOAT_EQ(segment->trophic_support, 0.8f);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Metabolic_HealthCheck) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    segment->atp_level = 1.0f;
    segment->trophic_support = 1.0f;
    EXPECT_TRUE(myelin_segment_metabolically_healthy(segment));

    segment->atp_level = 0.1f;
    EXPECT_FALSE(myelin_segment_metabolically_healthy(segment));

    myelin_sheath_destroy(sheath);
}

//=============================================================================
// 9. Dynamics Tests
//=============================================================================

TEST_F(MyelinSheathTest, Dynamics_SegmentStep) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    uint64_t initial_time = segment->last_update_time;
    myelin_segment_step(segment, 0.1f, 1000000);

    EXPECT_NE(segment->last_update_time, initial_time);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Dynamics_SheathStep) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_sheath_add_segment(sheath, 0.0f, 200.0f, 2.0f);
    myelin_sheath_add_segment(sheath, 201.0f, 200.0f, 2.0f);

    uint64_t initial_time = sheath->last_update_time;
    myelin_sheath_step(sheath, 0.1f, 1000000);

    EXPECT_NE(sheath->last_update_time, initial_time);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Dynamics_Myelinate) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    uint32_t initial_lamellae = segment->num_lamellae;
    myelin_sheath_myelinate(sheath, 10.0f, 1.0f);  // 10 lamellae/sec for 1 sec

    EXPECT_GT(segment->num_lamellae, initial_lamellae);

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinSheathTest, Dynamics_Demyelinate) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_segment_t* segment = myelin_sheath_add_segment(
        sheath, 0.0f, 200.0f, 2.0f);

    // First myelinate
    myelin_segment_set_lamellae(segment, 50);
    uint32_t initial_lamellae = segment->num_lamellae;

    myelin_sheath_demyelinate(sheath, 10.0f, 1.0f);

    EXPECT_LT(segment->num_lamellae, initial_lamellae);

    myelin_sheath_destroy(sheath);
}

//=============================================================================
// 10. Copy-on-Write Tests
//=============================================================================

TEST_F(MyelinSheathTest, CoW_Copy) {
    myelin_sheath_t* original = myelin_sheath_create(1, 100, 50, 16);
    myelin_sheath_add_segment(original, 0.0f, 200.0f, 2.0f);

    myelin_sheath_t* copy = myelin_sheath_cow_copy(original);
    ASSERT_NE(copy, nullptr);

    EXPECT_EQ(copy->id, original->id);
    EXPECT_EQ(copy->axon_id, original->axon_id);
    EXPECT_TRUE(myelin_sheath_is_cow_copy(copy));
    EXPECT_FALSE(myelin_sheath_is_cow_copy(original));
    EXPECT_EQ(original->cow_ref_count, 2u);

    myelin_sheath_cow_release(copy);
    myelin_sheath_destroy(original);
}

TEST_F(MyelinSheathTest, CoW_PrepareWrite) {
    myelin_sheath_t* original = myelin_sheath_create(1, 100, 50, 16);
    myelin_sheath_add_segment(original, 0.0f, 200.0f, 2.0f);

    myelin_sheath_t* copy = myelin_sheath_cow_copy(original);
    ASSERT_NE(copy, nullptr);

    nimcp_result_t result = myelin_sheath_cow_prepare_write(copy);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(myelin_sheath_is_cow_copy(copy));
    EXPECT_TRUE(copy->cow_modified);

    myelin_sheath_destroy(copy);
    myelin_sheath_destroy(original);
}

TEST_F(MyelinSheathTest, CoW_RefCount) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    EXPECT_EQ(myelin_sheath_cow_ref_count(sheath), 1u);

    myelin_sheath_t* copy = myelin_sheath_cow_copy(sheath);
    EXPECT_EQ(myelin_sheath_cow_ref_count(sheath), 2u);

    myelin_sheath_cow_release(copy);
    EXPECT_EQ(myelin_sheath_cow_ref_count(sheath), 1u);

    myelin_sheath_destroy(sheath);
}

//=============================================================================
// 11. Network Tests
//=============================================================================

TEST_F(MyelinSheathTest, Network_Create) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);
    ASSERT_NE(network, nullptr);

    EXPECT_EQ(network->num_sheaths, 0u);
    EXPECT_GE(network->capacity, 100u);

    myelin_network_destroy(network);
}

TEST_F(MyelinSheathTest, Network_AddSheath) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);

    nimcp_result_t result = myelin_network_add_sheath(network, sheath);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(network->num_sheaths, 1u);

    myelin_network_destroy(network);
}

TEST_F(MyelinSheathTest, Network_FindSheath) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);
    myelin_sheath_t* sheath = myelin_sheath_create(42, 100, 50, 16);
    myelin_network_add_sheath(network, sheath);

    myelin_sheath_t* found = myelin_network_find_sheath(network, 42);
    EXPECT_EQ(found, sheath);

    EXPECT_EQ(myelin_network_find_sheath(network, 999), nullptr);

    myelin_network_destroy(network);
}

TEST_F(MyelinSheathTest, Network_FindByAxon) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_network_add_sheath(network, sheath);

    myelin_sheath_t* found = myelin_network_find_by_axon(network, 100);
    EXPECT_EQ(found, sheath);

    EXPECT_EQ(myelin_network_find_by_axon(network, 999), nullptr);

    myelin_network_destroy(network);
}

TEST_F(MyelinSheathTest, Network_FindByOligo) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);

    // Add multiple sheaths from same oligodendrocyte
    for (int i = 0; i < 5; i++) {
        myelin_sheath_t* sheath = myelin_sheath_create(i, i + 100, 50, 16);
        myelin_network_add_sheath(network, sheath);
    }

    myelin_sheath_t* results[10];
    uint32_t count = myelin_network_find_by_oligo(network, 50, results, 10);
    EXPECT_EQ(count, 5u);

    myelin_network_destroy(network);
}

TEST_F(MyelinSheathTest, Network_RemoveSheath) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_network_add_sheath(network, sheath);

    nimcp_result_t result = myelin_network_remove_sheath(network, 1);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(network->num_sheaths, 0u);

    // Clean up the removed sheath
    myelin_sheath_destroy(sheath);
    myelin_network_destroy(network);
}

TEST_F(MyelinSheathTest, Network_Step) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_sheath_add_segment(sheath, 0.0f, 200.0f, 2.0f);
    myelin_network_add_sheath(network, sheath);

    uint64_t initial_time = network->last_step_time;
    myelin_network_step(network, 0.1f, 1000000);

    EXPECT_NE(network->last_step_time, initial_time);

    myelin_network_destroy(network);
}

TEST_F(MyelinSheathTest, Network_GetStats) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 16);
    myelin_sheath_add_segment(sheath, 0.0f, 200.0f, 2.0f);
    myelin_network_add_sheath(network, sheath);

    myelin_network_stats_t stats;
    myelin_network_get_stats(network, &stats);

    EXPECT_EQ(stats.total_sheaths, 1u);
    EXPECT_EQ(stats.total_segments, 1u);
    EXPECT_GT(stats.mean_integrity, 0.0f);

    myelin_network_destroy(network);
}

//=============================================================================
// 12. Integration Helper Tests
//=============================================================================

TEST_F(MyelinSheathTest, Integration_CreateSheathForAxon) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);

    myelin_sheath_t* sheath = myelin_network_create_sheath_for_axon(
        network, 100, 50, 1000.0f, 2.0f, 0.0f);

    ASSERT_NE(sheath, nullptr);
    EXPECT_EQ(sheath->axon_id, 100u);
    EXPECT_GT(sheath->num_segments, 0u);
    EXPECT_EQ(network->num_sheaths, 1u);

    // Try to create again for same axon - should return existing
    myelin_sheath_t* existing = myelin_network_create_sheath_for_axon(
        network, 100, 50, 1000.0f, 2.0f, 0.0f);
    EXPECT_EQ(existing, sheath);
    EXPECT_EQ(network->num_sheaths, 1u);

    myelin_network_destroy(network);
}

TEST_F(MyelinSheathTest, Integration_GetMyelinationFactor) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);
    myelin_network_create_sheath_for_axon(network, 100, 50, 1000.0f, 2.0f, 0.0f);

    float factor = myelin_network_get_myelination_factor(network, 100);
    EXPECT_GE(factor, 0.0f);
    EXPECT_LE(factor, 1.0f);

    // Non-existent axon
    EXPECT_FLOAT_EQ(myelin_network_get_myelination_factor(network, 999), 0.0f);

    myelin_network_destroy(network);
}

TEST_F(MyelinSheathTest, Integration_GetVelocity) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);
    myelin_network_create_sheath_for_axon(network, 100, 50, 1000.0f, 2.0f, 0.0f);

    float velocity = myelin_network_get_velocity(network, 100);
    EXPECT_GT(velocity, 0.0f);

    // Non-existent axon returns base velocity
    EXPECT_FLOAT_EQ(myelin_network_get_velocity(network, 999),
                    NIMCP_MYELIN_BASE_VELOCITY_MS);

    myelin_network_destroy(network);
}

TEST_F(MyelinSheathTest, Integration_GetDelay) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);
    myelin_network_create_sheath_for_axon(network, 100, 50, 1000.0f, 2.0f, 0.0f);

    float delay = myelin_network_get_delay(network, 100);
    EXPECT_GT(delay, 0.0f);

    // Non-existent axon
    EXPECT_FLOAT_EQ(myelin_network_get_delay(network, 999), 0.0f);

    myelin_network_destroy(network);
}

TEST_F(MyelinSheathTest, Integration_ApplyActivity) {
    myelin_sheath_network_t* network = myelin_network_create_default(100);
    myelin_sheath_t* sheath = myelin_network_create_sheath_for_axon(
        network, 100, 50, 1000.0f, 2.0f, 0.0f);

    // Get initial lamellae
    uint32_t initial_lamellae = 0;
    if (sheath->num_segments > 0) {
        initial_lamellae = sheath->segments[0]->num_lamellae;
    }

    // Apply high activity
    myelin_network_apply_activity(network, 100, 10.0f, 1.0f);

    // Should increase myelination
    if (sheath->num_segments > 0) {
        EXPECT_GE(sheath->segments[0]->num_lamellae, initial_lamellae);
    }

    myelin_network_destroy(network);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(MyelinSheathTest, NullSafety_AllFunctions) {
    // Most functions should handle NULL gracefully
    EXPECT_EQ(myelin_segment_set_lamellae(nullptr, 10), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(myelin_segment_set_compaction(nullptr, MYELIN_COMPACT_FULL, 1.0f),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(myelin_segment_apply_damage(nullptr, 0.1f, 1000),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(myelin_segment_repair(nullptr, 0.1f, 1000), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(myelin_segment_set_paranodes(nullptr, PARANODE_MATURE, PARANODE_MATURE),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(myelin_network_add_sheath(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(myelin_network_remove_sheath(nullptr, 0), NIMCP_ERROR_NULL_POINTER);

    // Functions that should not crash with NULL
    myelin_segment_update_health(nullptr, 0);
    myelin_segment_update_metabolism(nullptr, 0.1f);
    myelin_segment_receive_lactate(nullptr, 0.5f);
    myelin_segment_set_trophic_support(nullptr, 0.5f);
    myelin_segment_step(nullptr, 0.1f, 1000);
    myelin_sheath_step(nullptr, 0.1f, 1000);
    myelin_sheath_myelinate(nullptr, 1.0f, 0.1f);
    myelin_sheath_demyelinate(nullptr, 1.0f, 0.1f);
    myelin_sheath_update_conduction(nullptr);
    myelin_sheath_update_health(nullptr);
    myelin_network_step(nullptr, 0.1f, 1000);

    // Query functions with NULL
    EXPECT_FLOAT_EQ(myelin_segment_compute_velocity(nullptr),
                    NIMCP_MYELIN_BASE_VELOCITY_MS);
    EXPECT_FLOAT_EQ(myelin_segment_compute_delay(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(myelin_sheath_get_velocity_ratio(nullptr), 1.0f);
    EXPECT_FALSE(myelin_segment_paranodes_functional(nullptr));
    EXPECT_FALSE(myelin_segment_metabolically_healthy(nullptr));
    EXPECT_FALSE(myelin_sheath_is_cow_copy(nullptr));
    EXPECT_EQ(myelin_sheath_cow_ref_count(nullptr), 0u);
    EXPECT_EQ(myelin_network_find_sheath(nullptr, 0), nullptr);
    EXPECT_EQ(myelin_network_find_by_axon(nullptr, 0), nullptr);
}

//=============================================================================
// Default Configuration Test
//=============================================================================

TEST_F(MyelinSheathTest, DefaultConfig) {
    myelin_network_config_t config = myelin_network_default_config();

    EXPECT_GT(config.max_sheaths, 0u);
    EXPECT_GT(config.max_segments_per_sheath, 0u);
    EXPECT_GT(config.target_g_ratio, 0.0f);
    EXPECT_LT(config.target_g_ratio, 1.0f);
    EXPECT_TRUE(config.use_memory_pools);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
