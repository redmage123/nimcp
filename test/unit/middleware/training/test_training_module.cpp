/**
 * @file test_training_module.cpp
 * @brief Unit tests for Training Module Infrastructure
 *
 * Phase TM-1: Training Module Security and Memory Pool Integration Tests
 *
 * Tests:
 * 1. Lifecycle management (create, init, destroy)
 * 2. Weight allocation with unified memory
 * 3. CoW (Copy-on-Write) semantics
 * 4. Security registration and interaction recording
 * 5. Checkpoint/rollback functionality
 * 6. Statistics tracking
 *
 * @author NIMCP Training Team
 * @date 2025-11-27
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_module.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class TrainingModuleTest : public ::testing::Test {
protected:
    nimcp_training_context_t* ctx = nullptr;
    nimcp_sec_integration_t* shared_security = nullptr;
    unified_mem_manager_t shared_memory = nullptr;

    void SetUp() override {
        // Create shared security context for tests that need it
        shared_security = nimcp_sec_integration_create();
        if (shared_security) {
            nimcp_sec_integration_config_t sec_cfg = nimcp_sec_integration_default_config();
            sec_cfg.enable_memory_pools = true;
            sec_cfg.enable_cow = true;
            nimcp_sec_integration_init(shared_security, &sec_cfg);
        }

        // Create shared memory manager
        unified_mem_config_t mem_cfg = unified_mem_default_config();
        mem_cfg.enable_cow = true;
        mem_cfg.enable_tracking = true;
        shared_memory = unified_mem_create(&mem_cfg);
    }

    void TearDown() override {
        if (ctx) {
            nimcp_training_destroy(ctx);
            ctx = nullptr;
        }
        if (shared_memory) {
            unified_mem_destroy(shared_memory);
            shared_memory = nullptr;
        }
        if (shared_security) {
            nimcp_sec_integration_destroy(shared_security);
            shared_security = nullptr;
        }
    }

    // Helper to create a basic training context
    nimcp_training_context_t* createContext(bool with_security = true,
                                            bool with_memory = true,
                                            bool with_cow = true) {
        nimcp_training_module_config_t cfg = nimcp_training_default_config();
        cfg.type = NIMCP_TRAIN_MOD_STDP;
        cfg.name = "test_training_module";
        cfg.enable_security = with_security;
        cfg.enable_unified_memory = with_memory;
        cfg.enable_cow = with_cow;
        cfg.phase = NIMCP_TRAIN_PHASE_T1;
        cfg.learning_rate = 0.001;

        if (with_security && shared_security) {
            cfg.security_ctx = shared_security;
        }
        if (with_memory && shared_memory) {
            cfg.mem_manager = shared_memory;
        }

        return nimcp_training_create(&cfg);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(TrainingModuleTest, CreateWithNullConfig) {
    // Create with NULL config should use defaults
    ctx = nimcp_training_create(nullptr);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(nimcp_training_get_state(ctx), NIMCP_TRAIN_STATE_UNINITIALIZED);
}

TEST_F(TrainingModuleTest, CreateWithConfig) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(nimcp_training_get_state(ctx), NIMCP_TRAIN_STATE_UNINITIALIZED);
    EXPECT_EQ(nimcp_training_get_type(ctx), NIMCP_TRAIN_MOD_STDP);
}

TEST_F(TrainingModuleTest, InitializeContext) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_training_get_state(ctx), NIMCP_TRAIN_STATE_INITIALIZED);
}

TEST_F(TrainingModuleTest, DoubleInitFails) {
    ctx = createContext();
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Second init should fail
    result = nimcp_training_init(ctx);
    EXPECT_EQ(result, NIMCP_ALREADY_EXISTS);
}

TEST_F(TrainingModuleTest, DestroyNullSafe) {
    // Should not crash
    nimcp_training_destroy(nullptr);
}

TEST_F(TrainingModuleTest, DestroyUninitializedContext) {
    ctx = createContext();
    ASSERT_NE(ctx, nullptr);
    // Destroy without init should be safe
    nimcp_training_destroy(ctx);
    ctx = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// Weight Allocation Tests
//=============================================================================

TEST_F(TrainingModuleTest, AllocateWeights) {
    ctx = createContext();
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    nimcp_training_weights_t weights;
    nimcp_result_t result = nimcp_training_alloc_weights(ctx, 1000, nullptr, &weights);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(weights.num_weights, 1000u);
    EXPECT_EQ(weights.num_dims, 1u);
    EXPECT_EQ(weights.dimensions[0], 1000u);

    // Verify can read (should be zero-initialized)
    const float* data = nimcp_training_read_weights(ctx, &weights);
    ASSERT_NE(data, nullptr);
    for (size_t i = 0; i < 100; i++) {  // Check first 100
        EXPECT_FLOAT_EQ(data[i], 0.0f);
    }

    nimcp_training_free_weights(ctx, &weights);
}

TEST_F(TrainingModuleTest, AllocateWeightsWithInitialData) {
    ctx = createContext();
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Create initial data
    std::vector<float> init_data(500);
    for (size_t i = 0; i < init_data.size(); i++) {
        init_data[i] = static_cast<float>(i) * 0.01f;
    }

    nimcp_training_weights_t weights;
    nimcp_result_t result = nimcp_training_alloc_weights(ctx, 500, init_data.data(), &weights);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify data was copied
    const float* data = nimcp_training_read_weights(ctx, &weights);
    ASSERT_NE(data, nullptr);
    for (size_t i = 0; i < 100; i++) {
        EXPECT_FLOAT_EQ(data[i], init_data[i]);
    }

    nimcp_training_free_weights(ctx, &weights);
}

TEST_F(TrainingModuleTest, AllocateWeightsND) {
    ctx = createContext();
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Allocate a 3D tensor (like conv weights)
    size_t dims[] = {64, 32, 3};
    nimcp_training_weights_t weights;
    nimcp_result_t result = nimcp_training_alloc_weights_nd(ctx, dims, 3, nullptr, &weights);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(weights.num_weights, 64u * 32 * 3);
    EXPECT_EQ(weights.num_dims, 3u);
    EXPECT_EQ(weights.dimensions[0], 64u);
    EXPECT_EQ(weights.dimensions[1], 32u);
    EXPECT_EQ(weights.dimensions[2], 3u);

    nimcp_training_free_weights(ctx, &weights);
}

TEST_F(TrainingModuleTest, AllocateWeights4D) {
    ctx = createContext();
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Allocate a 4D tensor (conv kernel: out_channels x in_channels x H x W)
    size_t dims[] = {32, 16, 5, 5};
    nimcp_training_weights_t weights;
    nimcp_result_t result = nimcp_training_alloc_weights_nd(ctx, dims, 4, nullptr, &weights);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(weights.num_weights, 32u * 16 * 5 * 5);
    EXPECT_EQ(weights.num_dims, 4u);

    nimcp_training_free_weights(ctx, &weights);
}

TEST_F(TrainingModuleTest, WriteTriggersCoW) {
    ctx = createContext(true, true, true);  // CoW enabled
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    nimcp_training_weights_t weights;
    ASSERT_EQ(nimcp_training_alloc_weights(ctx, 1000, nullptr, &weights), NIMCP_SUCCESS);

    // Get write access
    float* data = nimcp_training_write_weights(ctx, &weights);
    ASSERT_NE(data, nullptr);

    // Modify data
    data[0] = 1.5f;
    data[999] = 2.5f;

    // Verify reads back correctly
    const float* read_data = nimcp_training_read_weights(ctx, &weights);
    EXPECT_FLOAT_EQ(read_data[0], 1.5f);
    EXPECT_FLOAT_EQ(read_data[999], 2.5f);

    nimcp_training_free_weights(ctx, &weights);
}

//=============================================================================
// Copy-on-Write Tests
//=============================================================================

TEST_F(TrainingModuleTest, CloneWeightsSharesData) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Create original weights
    std::vector<float> init_data(1000, 0.5f);
    nimcp_training_weights_t original;
    ASSERT_EQ(nimcp_training_alloc_weights(ctx, 1000, init_data.data(), &original), NIMCP_SUCCESS);

    // Clone
    nimcp_training_weights_t clone;
    nimcp_result_t result = nimcp_training_clone_weights(ctx, &original, &clone);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(clone.num_weights, original.num_weights);

    // Both should read the same data
    const float* orig_data = nimcp_training_read_weights(ctx, &original);
    const float* clone_data = nimcp_training_read_weights(ctx, &clone);
    EXPECT_FLOAT_EQ(orig_data[0], clone_data[0]);
    EXPECT_FLOAT_EQ(orig_data[500], clone_data[500]);

    nimcp_training_free_weights(ctx, &original);
    nimcp_training_free_weights(ctx, &clone);
}

TEST_F(TrainingModuleTest, WriteToCloneSeparates) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Create original weights
    std::vector<float> init_data(1000, 0.5f);
    nimcp_training_weights_t original;
    ASSERT_EQ(nimcp_training_alloc_weights(ctx, 1000, init_data.data(), &original), NIMCP_SUCCESS);

    // Clone
    nimcp_training_weights_t clone;
    ASSERT_EQ(nimcp_training_clone_weights(ctx, &original, &clone), NIMCP_SUCCESS);

    // Modify clone
    float* clone_write = nimcp_training_write_weights(ctx, &clone);
    ASSERT_NE(clone_write, nullptr);
    clone_write[0] = 999.0f;

    // Original should be unchanged
    const float* orig_data = nimcp_training_read_weights(ctx, &original);
    EXPECT_FLOAT_EQ(orig_data[0], 0.5f);

    // Clone should have new value
    const float* clone_data = nimcp_training_read_weights(ctx, &clone);
    EXPECT_FLOAT_EQ(clone_data[0], 999.0f);

    nimcp_training_free_weights(ctx, &original);
    nimcp_training_free_weights(ctx, &clone);
}

//=============================================================================
// Security Integration Tests
//=============================================================================

TEST_F(TrainingModuleTest, SecurityRegistration) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Should have been registered with security
    uint32_t sec_id = nimcp_training_get_security_id(ctx);
    EXPECT_GT(sec_id, 0u);

    // Should be trusted initially
    EXPECT_TRUE(nimcp_training_is_trusted(ctx));
}

TEST_F(TrainingModuleTest, RecordSuccessAndFailure) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Record several successful interactions
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(nimcp_training_record_success(ctx), NIMCP_SUCCESS);
    }

    // Should still be trusted
    EXPECT_TRUE(nimcp_training_is_trusted(ctx));

    // Record some failures (but not enough to lose trust)
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(nimcp_training_record_failure(ctx), NIMCP_SUCCESS);
    }

    // Still trusted (requires many failures to lose trust)
    EXPECT_TRUE(nimcp_training_is_trusted(ctx));
}

TEST_F(TrainingModuleTest, RegisterWeightsWithSecurity) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    nimcp_training_weights_t weights;
    ASSERT_EQ(nimcp_training_alloc_weights(ctx, 1000, nullptr, &weights), NIMCP_SUCCESS);
    EXPECT_EQ(weights.region_id, 0u);  // Not registered yet

    // Register with security
    nimcp_result_t result = nimcp_training_register_weights_security(ctx, &weights, "test_weights");
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(weights.region_id, 0u);  // Now registered

    // Update baseline after modifications
    float* data = nimcp_training_write_weights(ctx, &weights);
    data[0] = 1.0f;
    result = nimcp_training_update_weights_baseline(ctx, &weights);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    nimcp_training_free_weights(ctx, &weights);
}

//=============================================================================
// Checkpoint Tests
//=============================================================================

TEST_F(TrainingModuleTest, CheckpointAndRestore) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Create weights
    std::vector<float> init_data(1000, 1.0f);
    nimcp_training_weights_t weights;
    ASSERT_EQ(nimcp_training_alloc_weights(ctx, 1000, init_data.data(), &weights), NIMCP_SUCCESS);

    // Create checkpoint
    nimcp_training_checkpoint_t checkpoint;
    nimcp_result_t result = nimcp_training_checkpoint_create(ctx, &weights, 1, &checkpoint);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(checkpoint.num_snapshots, 1u);

    // Modify weights
    float* data = nimcp_training_write_weights(ctx, &weights);
    ASSERT_NE(data, nullptr);
    for (size_t i = 0; i < 1000; i++) {
        data[i] = 99.0f;
    }

    // Verify modification
    const float* read_data = nimcp_training_read_weights(ctx, &weights);
    EXPECT_NE(read_data, nullptr);

    // Restore from checkpoint - verify API works
    result = nimcp_training_checkpoint_restore(ctx, &weights, 1, &checkpoint);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify data is still accessible after restore
    read_data = nimcp_training_read_weights(ctx, &weights);
    EXPECT_NE(read_data, nullptr);

    nimcp_training_checkpoint_destroy(ctx, &checkpoint);
    nimcp_training_free_weights(ctx, &weights);
}

TEST_F(TrainingModuleTest, MultipleWeightsCheckpoint) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Create multiple weight tensors
    const size_t num_layers = 3;
    nimcp_training_weights_t weights[3];
    for (size_t i = 0; i < num_layers; i++) {
        std::vector<float> init((i + 1) * 100, static_cast<float>(i));
        ASSERT_EQ(nimcp_training_alloc_weights(ctx, (i + 1) * 100, init.data(), &weights[i]), NIMCP_SUCCESS);
    }

    // Checkpoint all
    nimcp_training_checkpoint_t checkpoint;
    nimcp_result_t result = nimcp_training_checkpoint_create(ctx, weights, num_layers, &checkpoint);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(checkpoint.num_snapshots, num_layers);

    // Modify all
    for (size_t i = 0; i < num_layers; i++) {
        float* data = nimcp_training_write_weights(ctx, &weights[i]);
        ASSERT_NE(data, nullptr);
        data[0] = 777.0f;
    }

    // Restore all - verify API works
    result = nimcp_training_checkpoint_restore(ctx, weights, num_layers, &checkpoint);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify all data is accessible after restore
    for (size_t i = 0; i < num_layers; i++) {
        const float* data = nimcp_training_read_weights(ctx, &weights[i]);
        EXPECT_NE(data, nullptr);
    }

    // Cleanup
    nimcp_training_checkpoint_destroy(ctx, &checkpoint);
    for (size_t i = 0; i < num_layers; i++) {
        nimcp_training_free_weights(ctx, &weights[i]);
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(TrainingModuleTest, GetStatistics) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    nimcp_training_stats_t stats;
    nimcp_result_t result = nimcp_training_get_stats(ctx, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(stats.security_module_id, 0u);
}

TEST_F(TrainingModuleTest, MemoryStatsTrack) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Get initial stats
    nimcp_training_stats_t stats_before;
    nimcp_training_get_stats(ctx, &stats_before);

    // Allocate some weights
    nimcp_training_weights_t weights;
    ASSERT_EQ(nimcp_training_alloc_weights(ctx, 10000, nullptr, &weights), NIMCP_SUCCESS);

    // Get stats after
    nimcp_training_stats_t stats_after;
    nimcp_training_get_stats(ctx, &stats_after);

    // Should have tracked memory
    EXPECT_GE(stats_after.weights_allocated, sizeof(float) * 10000);

    nimcp_training_free_weights(ctx, &weights);
}

TEST_F(TrainingModuleTest, ResetStatistics) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Do some operations
    nimcp_training_weights_t weights;
    ASSERT_EQ(nimcp_training_alloc_weights(ctx, 1000, nullptr, &weights), NIMCP_SUCCESS);
    nimcp_training_record_success(ctx);
    nimcp_training_free_weights(ctx, &weights);

    // Reset stats
    nimcp_training_reset_stats(ctx);

    // Get stats - should be mostly zero
    nimcp_training_stats_t stats;
    nimcp_training_get_stats(ctx, &stats);
    EXPECT_EQ(stats.training_steps, 0u);
    EXPECT_EQ(stats.weight_updates, 0u);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(TrainingModuleTest, TypeNames) {
    EXPECT_STREQ(nimcp_training_type_name(NIMCP_TRAIN_MOD_STDP), "STDP");
    EXPECT_STREQ(nimcp_training_type_name(NIMCP_TRAIN_MOD_DENDRITIC), "Dendritic");
    EXPECT_STREQ(nimcp_training_type_name(NIMCP_TRAIN_MOD_PREDICTIVE), "Predictive");
    EXPECT_STREQ(nimcp_training_type_name(NIMCP_TRAIN_MOD_BCM), "BCM");
    EXPECT_STREQ(nimcp_training_type_name(NIMCP_TRAIN_MOD_HOMEOSTATIC), "Homeostatic");
    EXPECT_STREQ(nimcp_training_type_name(NIMCP_TRAIN_MOD_BRAIN_LEARNING), "BrainLearning");
}

TEST_F(TrainingModuleTest, PhaseNames) {
    EXPECT_STREQ(nimcp_training_phase_name(NIMCP_TRAIN_PHASE_T1), "T1-Homeostatic");
    EXPECT_STREQ(nimcp_training_phase_name(NIMCP_TRAIN_PHASE_T2), "T2-Dendritic");
    EXPECT_STREQ(nimcp_training_phase_name(NIMCP_TRAIN_PHASE_T3), "T3-Predictive");
    EXPECT_STREQ(nimcp_training_phase_name(NIMCP_TRAIN_PHASE_T4), "T4-MetaLearning");
}

TEST_F(TrainingModuleTest, StateNames) {
    EXPECT_STREQ(nimcp_training_state_name(NIMCP_TRAIN_STATE_UNINITIALIZED), "Uninitialized");
    EXPECT_STREQ(nimcp_training_state_name(NIMCP_TRAIN_STATE_INITIALIZED), "Initialized");
    EXPECT_STREQ(nimcp_training_state_name(NIMCP_TRAIN_STATE_ACTIVE), "Active");
    EXPECT_STREQ(nimcp_training_state_name(NIMCP_TRAIN_STATE_PAUSED), "Paused");
    EXPECT_STREQ(nimcp_training_state_name(NIMCP_TRAIN_STATE_ERROR), "Error");
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(TrainingModuleTest, ZeroSizeWeightsFails) {
    ctx = createContext();
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    nimcp_training_weights_t weights;
    nimcp_result_t result = nimcp_training_alloc_weights(ctx, 0, nullptr, &weights);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(TrainingModuleTest, NullWeightsHandleFails) {
    ctx = createContext();
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    nimcp_result_t result = nimcp_training_alloc_weights(ctx, 1000, nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(TrainingModuleTest, TooManyDimensionsFails) {
    ctx = createContext();
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    size_t dims[] = {1, 2, 3, 4, 5};  // 5 dimensions - max is 4
    nimcp_training_weights_t weights;
    nimcp_result_t result = nimcp_training_alloc_weights_nd(ctx, dims, 5, nullptr, &weights);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(TrainingModuleTest, AllocWithoutInitFails) {
    ctx = createContext();
    ASSERT_NE(ctx, nullptr);
    // Don't call init

    nimcp_training_weights_t weights;
    nimcp_result_t result = nimcp_training_alloc_weights(ctx, 1000, nullptr, &weights);
    EXPECT_EQ(result, NIMCP_NOT_INITIALIZED);
}

//=============================================================================
// Performance Tests (Basic)
//=============================================================================

TEST_F(TrainingModuleTest, LargeWeightAllocation) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Allocate large weight matrix (10 million weights = ~40MB)
    const size_t large_size = 10 * 1024 * 1024;

    auto start = std::chrono::high_resolution_clock::now();
    nimcp_training_weights_t weights;
    nimcp_result_t result = nimcp_training_alloc_weights(ctx, large_size, nullptr, &weights);
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(result, NIMCP_SUCCESS);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // Should complete in reasonable time (< 1 second)
    EXPECT_LT(duration.count(), 1000);

    nimcp_training_free_weights(ctx, &weights);
}

TEST_F(TrainingModuleTest, ManyClones) {
    ctx = createContext(true, true, true);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Create original
    nimcp_training_weights_t original;
    ASSERT_EQ(nimcp_training_alloc_weights(ctx, 100000, nullptr, &original), NIMCP_SUCCESS);

    // Create many clones
    const size_t num_clones = 100;
    std::vector<nimcp_training_weights_t> clones(num_clones);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_clones; i++) {
        EXPECT_EQ(nimcp_training_clone_weights(ctx, &original, &clones[i]), NIMCP_SUCCESS);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // Cloning should be fast with CoW (< 100ms for 100 clones)
    EXPECT_LT(duration.count(), 100);

    // Cleanup
    for (size_t i = 0; i < num_clones; i++) {
        nimcp_training_free_weights(ctx, &clones[i]);
    }
    nimcp_training_free_weights(ctx, &original);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
