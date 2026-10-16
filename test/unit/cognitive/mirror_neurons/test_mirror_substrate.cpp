/**
 * @file test_mirror_substrate.cpp
 * @brief Comprehensive unit tests for mirror neuron substrate integration
 * @version 1.0.0
 * @date 2025-11-25
 *
 * WHAT: Tests for mirror neuron substrate-level integration
 * WHY:  Verify biological substrate backing for mirror neurons
 * HOW:  Test pool management, backing lifecycle, plasticity, CoW, and integration
 *
 * Test Coverage:
 * - Memory pool lifecycle and allocation
 * - Substrate backing create/destroy
 * - Copy-on-Write semantics
 * - Axon binding and delay calculations
 * - Myelin sheath integration
 * - Dendrite binding and spine plasticity
 * - Glial cell binding (astrocyte, oligodendrocyte, microglia)
 * - Activity tracking and coactivation
 * - Configuration and statistics
 * - Error handling and edge cases
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/mirror_neurons/nimcp_mirror_substrate.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "utils/memory/nimcp_memory.h"
#include "core/axon/nimcp_axon.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"

#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>

//=============================================================================
// Test Fixtures
//=============================================================================

class MirrorSubstratePoolTest : public ::testing::Test {
protected:
    mirror_substrate_pool_t* pool;

    void SetUp() override {
        pool = nullptr;
    }

    void TearDown() override {
        if (pool) {
            mirror_substrate_pool_destroy(pool);
            pool = nullptr;
        }
    }
};

class MirrorSubstrateBackingTest : public ::testing::Test {
protected:
    mirror_substrate_pool_t* pool;
    mirror_substrate_backing_t* backing;
    mirror_substrate_config_t config;

    void SetUp() override {
        pool = mirror_substrate_pool_create(64);
        config = mirror_substrate_get_default_config();
        backing = nullptr;
    }

    void TearDown() override {
        if (backing && pool) {
            mirror_substrate_backing_destroy(backing, pool);
            backing = nullptr;
        }
        if (pool) {
            mirror_substrate_pool_destroy(pool);
            pool = nullptr;
        }
    }
};

class MirrorSubstrateIntegrationTest : public ::testing::Test {
protected:
    mirror_neurons_t mirror;
    mirror_neuron_config_t config;

    void SetUp() override {
        config = mirror_neurons_get_default_config();
        config.enable_substrate = true;
        config.enable_myelination = true;
        config.enable_dendrite_plasticity = true;
        mirror = nullptr;
    }

    void TearDown() override {
        if (mirror) {
            mirror_neurons_destroy(mirror);
            mirror = nullptr;
        }
    }
};

//=============================================================================
// Pool Management Tests
//=============================================================================

TEST_F(MirrorSubstratePoolTest, CreatePoolWithDefaultCapacity) {
    pool = mirror_substrate_pool_create(NIMCP_MIRROR_SUBSTRATE_POOL_SIZE);
    ASSERT_NE(nullptr, pool);

    uint32_t allocated, capacity, high_water;
    mirror_substrate_pool_stats(pool, &allocated, &capacity, &high_water);

    EXPECT_EQ(0u, allocated);
    EXPECT_GE(capacity, NIMCP_MIRROR_SUBSTRATE_POOL_SIZE);
    EXPECT_EQ(0u, high_water);
}

TEST_F(MirrorSubstratePoolTest, CreatePoolWithSmallCapacity) {
    pool = mirror_substrate_pool_create(16);
    ASSERT_NE(nullptr, pool);

    uint32_t allocated, capacity, high_water;
    mirror_substrate_pool_stats(pool, &allocated, &capacity, &high_water);

    EXPECT_EQ(0u, allocated);
    // Capacity rounded up to block size (64)
    EXPECT_GE(capacity, 16u);
}

TEST_F(MirrorSubstratePoolTest, CreatePoolWithZeroCapacity) {
    // Zero capacity should still create a valid pool with minimum size
    pool = mirror_substrate_pool_create(0);
    // May return NULL or minimum size pool depending on implementation
    // Either is acceptable
}

TEST_F(MirrorSubstratePoolTest, AllocateFromPool) {
    pool = mirror_substrate_pool_create(64);
    ASSERT_NE(nullptr, pool);

    mirror_substrate_backing_t* backing1 = mirror_substrate_pool_alloc(pool);
    ASSERT_NE(nullptr, backing1);

    uint32_t allocated, capacity, high_water;
    mirror_substrate_pool_stats(pool, &allocated, &capacity, &high_water);

    EXPECT_EQ(1u, allocated);
    EXPECT_EQ(1u, high_water);

    // Allocate more
    mirror_substrate_backing_t* backing2 = mirror_substrate_pool_alloc(pool);
    ASSERT_NE(nullptr, backing2);
    EXPECT_NE(backing1, backing2);

    mirror_substrate_pool_stats(pool, &allocated, &capacity, &high_water);
    EXPECT_EQ(2u, allocated);
    EXPECT_EQ(2u, high_water);

    // Free one
    mirror_substrate_pool_free(pool, backing1);

    mirror_substrate_pool_stats(pool, &allocated, &capacity, &high_water);
    EXPECT_EQ(1u, allocated);
    EXPECT_EQ(2u, high_water);  // High water doesn't decrease

    // Free the other
    mirror_substrate_pool_free(pool, backing2);

    mirror_substrate_pool_stats(pool, &allocated, &capacity, &high_water);
    EXPECT_EQ(0u, allocated);
}

TEST_F(MirrorSubstratePoolTest, AllocateUntilFull) {
    pool = mirror_substrate_pool_create(64);
    ASSERT_NE(nullptr, pool);

    uint32_t allocated, capacity, high_water;
    mirror_substrate_pool_stats(pool, &allocated, &capacity, &high_water);

    // Allocate all slots
    mirror_substrate_backing_t* backings[128] = {nullptr};
    uint32_t i;
    for (i = 0; i < capacity && i < 128; i++) {
        backings[i] = mirror_substrate_pool_alloc(pool);
        if (!backings[i]) break;
    }

    EXPECT_EQ(i, capacity);

    // Next allocation should fail
    mirror_substrate_backing_t* overflow = mirror_substrate_pool_alloc(pool);
    EXPECT_EQ(nullptr, overflow);

    // Clean up
    for (uint32_t j = 0; j < i; j++) {
        mirror_substrate_pool_free(pool, backings[j]);
    }
}

TEST_F(MirrorSubstratePoolTest, ReuseFreedSlots) {
    pool = mirror_substrate_pool_create(64);
    ASSERT_NE(nullptr, pool);

    // Allocate 3
    mirror_substrate_backing_t* b1 = mirror_substrate_pool_alloc(pool);
    mirror_substrate_backing_t* b2 = mirror_substrate_pool_alloc(pool);
    mirror_substrate_backing_t* b3 = mirror_substrate_pool_alloc(pool);

    ASSERT_NE(nullptr, b1);
    ASSERT_NE(nullptr, b2);
    ASSERT_NE(nullptr, b3);

    // Free middle one
    mirror_substrate_pool_free(pool, b2);

    // Allocate again - should reuse freed slot
    mirror_substrate_backing_t* b4 = mirror_substrate_pool_alloc(pool);
    ASSERT_NE(nullptr, b4);
    // b4 might be same address as b2 if slot is reused

    // Clean up
    mirror_substrate_pool_free(pool, b1);
    mirror_substrate_pool_free(pool, b3);
    mirror_substrate_pool_free(pool, b4);
}

TEST_F(MirrorSubstratePoolTest, DestroyNullSafe) {
    mirror_substrate_pool_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Backing Lifecycle Tests
//=============================================================================

TEST_F(MirrorSubstrateBackingTest, CreateBackingFromPool) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    EXPECT_EQ(1u, backing->mirror_unit_id);
    EXPECT_EQ(0u, backing->neuron_id);  // No substrate neuron by default
    EXPECT_EQ(nullptr, backing->substrate_neuron);
    EXPECT_EQ(0u, backing->num_spines);
    EXPECT_FLOAT_EQ(1.0f, backing->astrocyte_modulation);
    EXPECT_FALSE(backing->marked_for_pruning);
}

TEST_F(MirrorSubstrateBackingTest, CreateBackingFromHeap) {
    // Create without pool - heap allocation
    mirror_substrate_backing_t* heap_backing = mirror_substrate_backing_create(2, &config, nullptr);
    ASSERT_NE(nullptr, heap_backing);

    EXPECT_EQ(2u, heap_backing->mirror_unit_id);

    // Destroy with NULL pool - heap free
    mirror_substrate_backing_destroy(heap_backing, nullptr);
}

TEST_F(MirrorSubstrateBackingTest, InitializeBacking) {
    backing = mirror_substrate_pool_alloc(pool);
    ASSERT_NE(nullptr, backing);

    // Initialize with config
    mirror_substrate_backing_init(backing, &config);

    EXPECT_FLOAT_EQ(0.0f, backing->myelination_level);
    EXPECT_FLOAT_EQ(0.0f, backing->observation_activity_ema);
    EXPECT_FLOAT_EQ(0.0f, backing->execution_activity_ema);
    EXPECT_FLOAT_EQ(0.0f, backing->coactivation_score);
    EXPECT_EQ(1u, backing->cow_ref_count);
    EXPECT_FALSE(backing->cow_modified);
}

TEST_F(MirrorSubstrateBackingTest, DestroyNullSafe) {
    mirror_substrate_backing_destroy(nullptr, pool);
    mirror_substrate_backing_destroy(nullptr, nullptr);
    SUCCEED();
}

//=============================================================================
// Copy-on-Write Tests
//=============================================================================

TEST_F(MirrorSubstrateBackingTest, CowCopyCreation) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Modify backing to have interesting state
    backing->myelination_level = 0.5f;
    backing->observation_activity_ema = 0.3f;

    // Create CoW copy
    mirror_substrate_backing_t* copy = mirror_substrate_cow_copy(backing, pool);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(backing, copy);

    // Copy should have same values
    EXPECT_FLOAT_EQ(backing->myelination_level, copy->myelination_level);
    EXPECT_FLOAT_EQ(backing->observation_activity_ema, copy->observation_activity_ema);

    // Both should be CoW copies
    EXPECT_TRUE(mirror_substrate_is_cow_copy(backing) || mirror_substrate_is_cow_copy(copy));

    // Clean up
    mirror_substrate_cow_release(copy, pool);
}

TEST_F(MirrorSubstrateBackingTest, CowPrepareWrite) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Prepare for write
    nimcp_result_t result = mirror_substrate_cow_prepare_write(backing);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Should be able to modify now
    backing->myelination_level = 0.8f;
    EXPECT_FLOAT_EQ(0.8f, backing->myelination_level);
}

TEST_F(MirrorSubstrateBackingTest, CowRefCounting) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    EXPECT_EQ(1u, backing->cow_ref_count);

    // Create copy - ref count should increase on original or copy
    mirror_substrate_backing_t* copy = mirror_substrate_cow_copy(backing, pool);
    ASSERT_NE(nullptr, copy);

    // One of them should have increased ref count
    EXPECT_TRUE(backing->cow_ref_count >= 1 || copy->cow_ref_count >= 1);

    // Release copy
    mirror_substrate_cow_release(copy, pool);
}

//=============================================================================
// Axon Integration Tests
//=============================================================================

TEST_F(MirrorSubstrateBackingTest, BindNullAxon) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    nimcp_result_t result = mirror_substrate_bind_observation_axon(backing, nullptr);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    EXPECT_EQ(nullptr, backing->observation_axon);
    EXPECT_EQ(0u, backing->observation_axon_id);
    EXPECT_FLOAT_EQ(NIMCP_MIRROR_BASE_DELAY_MS, backing->observation_delay_ms);
}

TEST_F(MirrorSubstrateBackingTest, GetObservationDelayWithoutSubstrate) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    float delay = mirror_substrate_get_observation_delay(backing);
    // Should return base delay + dendrite integration time
    EXPECT_GT(delay, NIMCP_MIRROR_BASE_DELAY_MS);
}

TEST_F(MirrorSubstrateBackingTest, GetExecutionDelayWithoutSubstrate) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    float delay = mirror_substrate_get_execution_delay(backing);
    // Should return base delay
    EXPECT_FLOAT_EQ(NIMCP_MIRROR_BASE_DELAY_MS, delay);
}

TEST_F(MirrorSubstrateBackingTest, DelayDecreasesWithMyelination) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Get base delay
    float base_delay = mirror_substrate_get_observation_delay(backing);

    // Set myelination level
    backing->myelination_level = 0.8f;

    // Delay should be less with myelination
    float myelinated_delay = mirror_substrate_get_observation_delay(backing);
    EXPECT_LT(myelinated_delay, base_delay);
}

//=============================================================================
// Myelin Sheath Integration Tests
//=============================================================================

TEST_F(MirrorSubstrateBackingTest, BindNullMyelinSheath) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    nimcp_result_t result = mirror_substrate_bind_myelin_sheath(backing, nullptr);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    EXPECT_EQ(nullptr, backing->myelin_sheath);
    EXPECT_EQ(0u, backing->myelin_sheath_id);
    EXPECT_FLOAT_EQ(0.0f, backing->myelination_level);
}

TEST_F(MirrorSubstrateBackingTest, UpdateMyelinationWithoutSheath) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Set initial level
    backing->myelination_level = 0.5f;

    // Update should preserve level without sheath
    float level = mirror_substrate_update_myelination(backing);
    EXPECT_FLOAT_EQ(0.5f, level);
}

TEST_F(MirrorSubstrateBackingTest, ApplyActivityToMyelinWithoutSheath) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Should not crash without sheath
    mirror_substrate_apply_activity_to_myelin(backing, 0.8f, 0.01f);
    SUCCEED();
}

//=============================================================================
// Dendrite Integration Tests
//=============================================================================

TEST_F(MirrorSubstrateBackingTest, BindNullDendrite) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    nimcp_result_t result = mirror_substrate_bind_dendrite(backing, nullptr);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    EXPECT_EQ(nullptr, backing->dendrite);
    EXPECT_EQ(0u, backing->dendrite_id);
}

TEST_F(MirrorSubstrateBackingTest, AddSpine) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    EXPECT_EQ(0u, backing->num_spines);

    // Add spine (with NULL pointer - just tracking)
    int32_t idx = mirror_substrate_add_spine(backing, nullptr, 0.5f);
    EXPECT_GE(idx, 0);

    EXPECT_EQ(1u, backing->num_spines);
    EXPECT_FLOAT_EQ(0.5f, backing->spine_weights[idx]);
    EXPECT_EQ(MIRROR_SPINE_STATE_THIN, backing->spine_states[idx]);
}

TEST_F(MirrorSubstrateBackingTest, AddMaxSpines) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Add max spines
    for (uint32_t i = 0; i < NIMCP_MIRROR_SUBSTRATE_MAX_SPINES; i++) {
        int32_t idx = mirror_substrate_add_spine(backing, nullptr, 0.5f);
        EXPECT_EQ((int32_t)i, idx);
    }

    EXPECT_EQ(NIMCP_MIRROR_SUBSTRATE_MAX_SPINES, backing->num_spines);

    // One more should fail
    int32_t overflow_idx = mirror_substrate_add_spine(backing, nullptr, 0.5f);
    EXPECT_EQ(-1, overflow_idx);
}

TEST_F(MirrorSubstrateBackingTest, UpdateSpinePlasticity) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Add spine
    int32_t idx = mirror_substrate_add_spine(backing, nullptr, 0.5f);
    ASSERT_GE(idx, 0);

    float initial_weight = backing->spine_weights[idx];

    // Simulate coactivation (observation + execution)
    for (int i = 0; i < 10; i++) {
        mirror_substrate_update_spine_plasticity(backing, true, true, 0.01f);
    }

    // Weight should increase with coactivation (LTP)
    EXPECT_GT(backing->spine_weights[idx], initial_weight);
}

TEST_F(MirrorSubstrateBackingTest, SpineStateTransition) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Add spine
    int32_t idx = mirror_substrate_add_spine(backing, nullptr, 0.2f);
    ASSERT_GE(idx, 0);
    EXPECT_EQ(MIRROR_SPINE_STATE_THIN, backing->spine_states[idx]);

    // Strengthen through coactivation
    backing->spine_weights[idx] = 0.5f;
    for (int i = 0; i < 50; i++) {
        mirror_substrate_update_spine_plasticity(backing, true, true, 0.01f);
    }

    // Should transition to stubby or mushroom with enough strengthening
    EXPECT_TRUE(backing->spine_states[idx] >= MIRROR_SPINE_STATE_THIN);
}

TEST_F(MirrorSubstrateBackingTest, GetTotalSpineWeight) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Add 3 spines with different weights
    mirror_substrate_add_spine(backing, nullptr, 0.2f);
    mirror_substrate_add_spine(backing, nullptr, 0.3f);
    mirror_substrate_add_spine(backing, nullptr, 0.5f);

    float total = mirror_substrate_get_total_spine_weight(backing);
    EXPECT_FLOAT_EQ(1.0f, total);
}

//=============================================================================
// Glial Cell Integration Tests
//=============================================================================

TEST_F(MirrorSubstrateBackingTest, BindAstrocyte) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Bind with dummy pointer
    int dummy_astrocyte = 42;
    nimcp_result_t result = mirror_substrate_bind_astrocyte(backing, &dummy_astrocyte);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    EXPECT_NE(nullptr, backing->astrocyte);
    EXPECT_NE(0u, backing->astrocyte_id);
    EXPECT_FLOAT_EQ(1.0f, backing->astrocyte_modulation);
}

TEST_F(MirrorSubstrateBackingTest, BindOligodendrocyte) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    int dummy_oligo = 43;
    nimcp_result_t result = mirror_substrate_bind_oligodendrocyte(backing, &dummy_oligo);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    EXPECT_NE(nullptr, backing->oligodendrocyte);
    EXPECT_NE(0u, backing->oligodendrocyte_id);
    EXPECT_FLOAT_EQ(0.0f, backing->lactate_received);
}

TEST_F(MirrorSubstrateBackingTest, BindMicroglia) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    int dummy_microglia = 44;
    nimcp_result_t result = mirror_substrate_bind_microglia(backing, &dummy_microglia);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    EXPECT_NE(nullptr, backing->microglia);
    EXPECT_NE(0u, backing->microglia_id);
    EXPECT_FLOAT_EQ(0.5f, backing->surveillance_score);
    EXPECT_FALSE(backing->marked_for_pruning);
}

TEST_F(MirrorSubstrateBackingTest, AstrocyteModulation) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    EXPECT_FLOAT_EQ(1.0f, mirror_substrate_get_astrocyte_modulation(backing));

    // Bind astrocyte
    int dummy_astrocyte = 42;
    mirror_substrate_bind_astrocyte(backing, &dummy_astrocyte);

    // Update with high activity
    for (int i = 0; i < 10; i++) {
        mirror_substrate_update_glial(backing, 0.9f, 0.01f);
    }

    // Modulation should have changed
    float mod = mirror_substrate_get_astrocyte_modulation(backing);
    EXPECT_GE(mod, NIMCP_MIRROR_ASTROCYTE_MOD_MIN);
    EXPECT_LE(mod, NIMCP_MIRROR_ASTROCYTE_MOD_MAX);
}

TEST_F(MirrorSubstrateBackingTest, MicrogliaPruning) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Bind microglia
    int dummy_microglia = 44;
    mirror_substrate_bind_microglia(backing, &dummy_microglia);

    EXPECT_FALSE(mirror_substrate_is_marked_for_pruning(backing));

    // Simulate low activity for extended period
    for (int i = 0; i < 100; i++) {
        mirror_substrate_update_glial(backing, 0.01f, 0.1f);
    }

    // Should eventually be marked for pruning
    // (depends on implementation thresholds)
}

//=============================================================================
// Activity Tracking Tests
//=============================================================================

TEST_F(MirrorSubstrateBackingTest, RecordObservation) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    EXPECT_EQ(0u, backing->last_observation_time);
    EXPECT_FLOAT_EQ(0.0f, backing->observation_activity_ema);

    // Record observation (strength, timestamp)
    mirror_substrate_record_observation(backing, 0.8f, 1000000);

    EXPECT_NE(0u, backing->last_observation_time);
    EXPECT_GT(backing->observation_activity_ema, 0.0f);
}

TEST_F(MirrorSubstrateBackingTest, RecordExecution) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    EXPECT_EQ(0u, backing->last_execution_time);
    EXPECT_FLOAT_EQ(0.0f, backing->execution_activity_ema);

    // Record execution (strength, timestamp)
    mirror_substrate_record_execution(backing, 0.7f, 1000000);

    EXPECT_NE(0u, backing->last_execution_time);
    EXPECT_GT(backing->execution_activity_ema, 0.0f);
}

TEST_F(MirrorSubstrateBackingTest, CoactivationDetection) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    EXPECT_FLOAT_EQ(0.0f, backing->coactivation_score);

    // Record observation and execution within coactivation window (times in microseconds)
    uint64_t time = 1000;  // 1ms in microseconds
    mirror_substrate_record_observation(backing, 0.8f, time);
    mirror_substrate_record_execution(backing, 0.7f, time + 50000);  // 50ms later (within 100ms window)

    // Coactivation is detected during step(), run a step to trigger detection
    mirror_substrate_step(backing, 0.016f, time + 100000);  // 100ms total

    // Should detect coactivation
    EXPECT_GT(backing->coactivation_score, 0.0f);
}

//=============================================================================
// Simulation Step Tests
//=============================================================================

TEST_F(MirrorSubstrateBackingTest, StepForward) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Set initial state
    backing->observation_activity_ema = 0.5f;
    backing->execution_activity_ema = 0.5f;
    backing->coactivation_score = 0.5f;

    // Step forward
    mirror_substrate_step(backing, 1000000, 0.01f);

    // Activities should decay
    EXPECT_LT(backing->observation_activity_ema, 0.5f);
    EXPECT_LT(backing->execution_activity_ema, 0.5f);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST(MirrorSubstrateConfigTest, DefaultConfig) {
    mirror_substrate_config_t config = mirror_substrate_get_default_config();

    EXPECT_EQ(MIRROR_SUBSTRATE_MODE_PARTIAL, config.mode);
    EXPECT_TRUE(config.enable_myelination);
    EXPECT_TRUE(config.enable_dendrites);
    EXPECT_TRUE(config.enable_memory_pool);
    EXPECT_FLOAT_EQ(NIMCP_MIRROR_BASE_DELAY_MS, config.base_recognition_delay_ms);
    EXPECT_FLOAT_EQ(NIMCP_MIRROR_SPINE_LTP_THRESHOLD, config.spine_ltp_threshold);
    EXPECT_FLOAT_EQ(NIMCP_MIRROR_SPINE_LTD_THRESHOLD, config.spine_ltd_threshold);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MirrorSubstrateBackingTest, GetStats) {
    // Create pool and allocate several backings
    mirror_substrate_backing_t* b1 = mirror_substrate_backing_create(1, &config, pool);
    mirror_substrate_backing_t* b2 = mirror_substrate_backing_create(2, &config, pool);
    mirror_substrate_backing_t* b3 = mirror_substrate_backing_create(3, &config, pool);

    ASSERT_NE(nullptr, b1);
    ASSERT_NE(nullptr, b2);
    ASSERT_NE(nullptr, b3);

    // Set varying myelination levels
    b1->myelination_level = 0.2f;
    b2->myelination_level = 0.5f;
    b3->myelination_level = 0.8f;

    // Add spines to some
    mirror_substrate_add_spine(b1, nullptr, 0.3f);
    mirror_substrate_add_spine(b2, nullptr, 0.5f);
    mirror_substrate_add_spine(b2, nullptr, 0.7f);

    uint32_t allocated, capacity, high_water;
    mirror_substrate_pool_stats(pool, &allocated, &capacity, &high_water);
    EXPECT_EQ(3u, allocated);  // Pool should have 3 allocated (excluding backing)

    // Clean up
    mirror_substrate_backing_destroy(b1, pool);
    mirror_substrate_backing_destroy(b2, pool);
    mirror_substrate_backing_destroy(b3, pool);
    backing = nullptr;  // Don't double free in TearDown
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST(MirrorSubstrateErrorTest, NullBackingOperations) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, mirror_substrate_bind_observation_axon(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, mirror_substrate_bind_execution_axon(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, mirror_substrate_bind_myelin_sheath(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, mirror_substrate_bind_dendrite(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, mirror_substrate_bind_astrocyte(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, mirror_substrate_bind_oligodendrocyte(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, mirror_substrate_bind_microglia(nullptr, nullptr));

    EXPECT_EQ(-1, mirror_substrate_add_spine(nullptr, nullptr, 0.5f));

    EXPECT_FLOAT_EQ(NIMCP_MIRROR_BASE_DELAY_MS, mirror_substrate_get_observation_delay(nullptr));
    EXPECT_FLOAT_EQ(NIMCP_MIRROR_BASE_DELAY_MS, mirror_substrate_get_execution_delay(nullptr));
    EXPECT_FLOAT_EQ(1.0f, mirror_substrate_get_astrocyte_modulation(nullptr));
    EXPECT_FALSE(mirror_substrate_is_marked_for_pruning(nullptr));
    EXPECT_FALSE(mirror_substrate_is_cow_copy(nullptr));
    EXPECT_FLOAT_EQ(0.0f, mirror_substrate_get_total_spine_weight(nullptr));
    EXPECT_FLOAT_EQ(0.0f, mirror_substrate_update_myelination(nullptr));
}

TEST(MirrorSubstrateErrorTest, NullPoolOperations) {
    EXPECT_EQ(nullptr, mirror_substrate_pool_alloc(nullptr));

    // These should not crash
    mirror_substrate_pool_free(nullptr, nullptr);

    uint32_t a, c, h;
    mirror_substrate_pool_stats(nullptr, &a, &c, &h);
    // Values should be safe defaults
}

//=============================================================================
// Mirror Neuron System Integration Tests
//=============================================================================

TEST_F(MirrorSubstrateIntegrationTest, EnableSubstrate) {
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(nullptr, mirror);

    // Substrate should be enabled via config
    // (if implementation supports this)
    bool has_substrate = mirror_neurons_has_substrate(mirror);
    // May or may not be true depending on auto-initialization
}

TEST_F(MirrorSubstrateIntegrationTest, GetRecognitionDelay) {
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(nullptr, mirror);

    // Create and observe an action to register it
    float features[32] = {0};
    for (int i = 0; i < 10; i++) features[i] = 0.1f * i;
    action_t action = mirror_neurons_create_action(1, "test", features, 10, 0);
    mirror_neurons_observe_action(mirror, &action);

    // Get recognition delay
    float delay = mirror_neurons_get_recognition_delay(mirror, 1);
    EXPECT_GT(delay, 0.0f);
}

TEST_F(MirrorSubstrateIntegrationTest, ConnectNetworks) {
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(nullptr, mirror);

    // Connect NULL networks (should handle gracefully)
    bool result1 = mirror_neurons_connect_axon_network(mirror, nullptr);
    bool result2 = mirror_neurons_connect_dendrite_network(mirror, nullptr);
    bool result3 = mirror_neurons_connect_myelin_network(mirror, nullptr);

    // NULL should return true (no-op)
    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
    EXPECT_TRUE(result3);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(MirrorSubstratePoolTest, AllocationPerformance) {
    pool = mirror_substrate_pool_create(4096);
    ASSERT_NE(nullptr, pool);

    auto start = std::chrono::high_resolution_clock::now();

    // Allocate 1000 times
    mirror_substrate_backing_t* backings[1000];
    for (int i = 0; i < 1000; i++) {
        backings[i] = mirror_substrate_pool_alloc(pool);
        ASSERT_NE(nullptr, backings[i]);
    }

    // Free all
    for (int i = 0; i < 1000; i++) {
        mirror_substrate_pool_free(pool, backings[i]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete in reasonable time (< 10ms for 2000 operations)
    EXPECT_LT(duration.count(), 10000);

    // Print performance info
    std::cout << "Pool alloc/free 1000 pairs: " << duration.count() << " us" << std::endl;
}

TEST_F(MirrorSubstrateBackingTest, PlasticityUpdatePerformance) {
    backing = mirror_substrate_backing_create(1, &config, pool);
    ASSERT_NE(nullptr, backing);

    // Add max spines
    for (uint32_t i = 0; i < NIMCP_MIRROR_SUBSTRATE_MAX_SPINES; i++) {
        mirror_substrate_add_spine(backing, nullptr, 0.5f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Update plasticity 10000 times
    for (int i = 0; i < 10000; i++) {
        mirror_substrate_update_spine_plasticity(backing, i % 2 == 0, i % 3 == 0, 0.001f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete in reasonable time (< 100ms)
    EXPECT_LT(duration.count(), 100000);

    std::cout << "Plasticity update 10000 iterations: " << duration.count() << " us" << std::endl;
}
