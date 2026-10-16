/**
 * @file test_training_module_integration.cpp
 * @brief Integration tests for Training Module with Brain
 *
 * Phase TM-1: Training Module Integration Tests
 *
 * Tests integration between:
 * 1. Training module and brain module
 * 2. Training module and security framework
 * 3. Training module and unified memory system
 * 4. Multi-module training scenarios
 *
 * @author NIMCP Training Team
 * @date 2025-11-27
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_module.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TrainingModuleIntegrationTest : public ::testing::Test {
protected:
    nimcp_sec_integration_t* security = nullptr;
    unified_mem_manager_t memory = nullptr;

    void SetUp() override {
        // Create shared security context
        security = nimcp_sec_integration_create();
        if (security) {
            nimcp_sec_integration_config_t cfg = nimcp_sec_integration_default_config();
            cfg.enable_memory_pools = true;
            cfg.enable_cow = true;
            nimcp_sec_integration_init(security, &cfg);
        }

        // Create shared memory manager
        unified_mem_config_t mem_cfg = unified_mem_default_config();
        mem_cfg.enable_cow = true;
        mem_cfg.enable_tracking = true;
        memory = unified_mem_create(&mem_cfg);
    }

    void TearDown() override {
        if (memory) {
            unified_mem_destroy(memory);
            memory = nullptr;
        }
        if (security) {
            nimcp_sec_integration_destroy(security);
            security = nullptr;
        }
    }
};

//=============================================================================
// Multi-Module Integration Tests
//=============================================================================

TEST_F(TrainingModuleIntegrationTest, MultipleModulesShareSecurity) {
    // Create multiple training modules sharing security context
    nimcp_training_module_config_t cfg1 = nimcp_training_default_config();
    cfg1.type = NIMCP_TRAIN_MOD_STDP;
    cfg1.name = "stdp_module";
    cfg1.enable_security = true;
    cfg1.security_ctx = security;
    cfg1.enable_unified_memory = true;
    cfg1.mem_manager = memory;

    nimcp_training_module_config_t cfg2 = nimcp_training_default_config();
    cfg2.type = NIMCP_TRAIN_MOD_BCM;
    cfg2.name = "bcm_module";
    cfg2.enable_security = true;
    cfg2.security_ctx = security;
    cfg2.enable_unified_memory = true;
    cfg2.mem_manager = memory;

    nimcp_training_context_t* ctx1 = nimcp_training_create(&cfg1);
    nimcp_training_context_t* ctx2 = nimcp_training_create(&cfg2);

    ASSERT_NE(ctx1, nullptr);
    ASSERT_NE(ctx2, nullptr);

    ASSERT_EQ(nimcp_training_init(ctx1), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_training_init(ctx2), NIMCP_SUCCESS);

    // Both should have distinct security module IDs
    uint32_t id1 = nimcp_training_get_security_id(ctx1);
    uint32_t id2 = nimcp_training_get_security_id(ctx2);
    EXPECT_NE(id1, id2);
    EXPECT_GT(id1, 0u);
    EXPECT_GT(id2, 0u);

    // Both should be trusted
    EXPECT_TRUE(nimcp_training_is_trusted(ctx1));
    EXPECT_TRUE(nimcp_training_is_trusted(ctx2));

    nimcp_training_destroy(ctx1);
    nimcp_training_destroy(ctx2);
}

TEST_F(TrainingModuleIntegrationTest, MultipleModulesShareMemory) {
    // Create multiple modules sharing memory manager
    nimcp_training_module_config_t cfg = nimcp_training_default_config();
    cfg.enable_security = false;
    cfg.enable_unified_memory = true;
    cfg.mem_manager = memory;

    // Create 3 training modules
    cfg.type = NIMCP_TRAIN_MOD_STDP;
    cfg.name = "stdp";
    nimcp_training_context_t* stdp = nimcp_training_create(&cfg);

    cfg.type = NIMCP_TRAIN_MOD_BCM;
    cfg.name = "bcm";
    nimcp_training_context_t* bcm = nimcp_training_create(&cfg);

    cfg.type = NIMCP_TRAIN_MOD_HOMEOSTATIC;
    cfg.name = "homeostatic";
    nimcp_training_context_t* homeostatic = nimcp_training_create(&cfg);

    ASSERT_NE(stdp, nullptr);
    ASSERT_NE(bcm, nullptr);
    ASSERT_NE(homeostatic, nullptr);

    ASSERT_EQ(nimcp_training_init(stdp), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_training_init(bcm), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_training_init(homeostatic), NIMCP_SUCCESS);

    // All should share same memory manager
    EXPECT_EQ(nimcp_training_get_mem_manager(stdp), memory);
    EXPECT_EQ(nimcp_training_get_mem_manager(bcm), memory);
    EXPECT_EQ(nimcp_training_get_mem_manager(homeostatic), memory);

    // Allocate weights in each
    nimcp_training_weights_t w1, w2, w3;
    EXPECT_EQ(nimcp_training_alloc_weights(stdp, 1000, nullptr, &w1), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_training_alloc_weights(bcm, 2000, nullptr, &w2), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_training_alloc_weights(homeostatic, 3000, nullptr, &w3), NIMCP_SUCCESS);

    // All should work independently
    float* data1 = nimcp_training_write_weights(stdp, &w1);
    float* data2 = nimcp_training_write_weights(bcm, &w2);
    float* data3 = nimcp_training_write_weights(homeostatic, &w3);
    ASSERT_NE(data1, nullptr);
    ASSERT_NE(data2, nullptr);
    ASSERT_NE(data3, nullptr);

    data1[0] = 1.0f;
    data2[0] = 2.0f;
    data3[0] = 3.0f;

    const float* read1 = nimcp_training_read_weights(stdp, &w1);
    const float* read2 = nimcp_training_read_weights(bcm, &w2);
    const float* read3 = nimcp_training_read_weights(homeostatic, &w3);
    EXPECT_FLOAT_EQ(read1[0], 1.0f);
    EXPECT_FLOAT_EQ(read2[0], 2.0f);
    EXPECT_FLOAT_EQ(read3[0], 3.0f);

    // Cleanup
    nimcp_training_free_weights(stdp, &w1);
    nimcp_training_free_weights(bcm, &w2);
    nimcp_training_free_weights(homeostatic, &w3);

    nimcp_training_destroy(stdp);
    nimcp_training_destroy(bcm);
    nimcp_training_destroy(homeostatic);
}

//=============================================================================
// Cross-Module Weight Sharing Tests
//=============================================================================

TEST_F(TrainingModuleIntegrationTest, SharedWeightsAcrossModules) {
    // Create two modules
    nimcp_training_module_config_t cfg = nimcp_training_default_config();
    cfg.enable_security = true;
    cfg.security_ctx = security;
    cfg.enable_unified_memory = true;
    cfg.mem_manager = memory;
    cfg.enable_cow = true;

    cfg.type = NIMCP_TRAIN_MOD_STDP;
    cfg.name = "producer";
    nimcp_training_context_t* producer = nimcp_training_create(&cfg);

    cfg.type = NIMCP_TRAIN_MOD_BCM;
    cfg.name = "consumer";
    nimcp_training_context_t* consumer = nimcp_training_create(&cfg);

    ASSERT_NE(producer, nullptr);
    ASSERT_NE(consumer, nullptr);
    ASSERT_EQ(nimcp_training_init(producer), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_training_init(consumer), NIMCP_SUCCESS);

    // Producer creates initial weights
    std::vector<float> init_data(5000, 0.5f);
    nimcp_training_weights_t producer_weights;
    ASSERT_EQ(nimcp_training_alloc_weights(producer, 5000, init_data.data(), &producer_weights),
              NIMCP_SUCCESS);

    // Consumer clones weights (CoW)
    nimcp_training_weights_t consumer_weights;
    ASSERT_EQ(nimcp_training_clone_weights(consumer, &producer_weights, &consumer_weights),
              NIMCP_SUCCESS);

    // Both should read same value initially
    EXPECT_FLOAT_EQ(nimcp_training_read_weights(producer, &producer_weights)[0], 0.5f);
    EXPECT_FLOAT_EQ(nimcp_training_read_weights(consumer, &consumer_weights)[0], 0.5f);

    // Producer updates weights
    float* prod_data = nimcp_training_write_weights(producer, &producer_weights);
    prod_data[0] = 999.0f;

    // Producer has new value
    EXPECT_FLOAT_EQ(nimcp_training_read_weights(producer, &producer_weights)[0], 999.0f);

    // Consumer still has original (CoW separation)
    EXPECT_FLOAT_EQ(nimcp_training_read_weights(consumer, &consumer_weights)[0], 0.5f);

    // Cleanup
    nimcp_training_free_weights(producer, &producer_weights);
    nimcp_training_free_weights(consumer, &consumer_weights);

    nimcp_training_destroy(producer);
    nimcp_training_destroy(consumer);
}

//=============================================================================
// Security Trust Network Integration
//=============================================================================

TEST_F(TrainingModuleIntegrationTest, TrustNetworkInteraction) {
    nimcp_training_module_config_t cfg = nimcp_training_default_config();
    cfg.type = NIMCP_TRAIN_MOD_STDP;
    cfg.name = "trust_test_module";
    cfg.enable_security = true;
    cfg.security_ctx = security;
    cfg.enable_unified_memory = true;
    cfg.mem_manager = memory;

    nimcp_training_context_t* ctx = nimcp_training_create(&cfg);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Initially trusted
    EXPECT_TRUE(nimcp_training_is_trusted(ctx));

    // Record many successful interactions
    for (int i = 0; i < 50; i++) {
        nimcp_training_record_success(ctx);
    }

    // Should definitely be trusted
    EXPECT_TRUE(nimcp_training_is_trusted(ctx));

    // Get trust stats
    nimcp_training_stats_t stats;
    nimcp_training_get_stats(ctx, &stats);
    EXPECT_GT(stats.trust_score, 0.5);

    nimcp_training_destroy(ctx);
}

//=============================================================================
// Checkpoint with Security Registration
//=============================================================================

TEST_F(TrainingModuleIntegrationTest, CheckpointWithSecurityIntegration) {
    nimcp_training_module_config_t cfg = nimcp_training_default_config();
    cfg.type = NIMCP_TRAIN_MOD_BRAIN_LEARNING;
    cfg.name = "checkpoint_security_test";
    cfg.enable_security = true;
    cfg.security_ctx = security;
    cfg.enable_unified_memory = true;
    cfg.mem_manager = memory;
    cfg.enable_cow = true;

    nimcp_training_context_t* ctx = nimcp_training_create(&cfg);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Create and register weights with security
    std::vector<float> init_data(1000, 1.0f);
    nimcp_training_weights_t weights;
    ASSERT_EQ(nimcp_training_alloc_weights(ctx, 1000, init_data.data(), &weights), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_training_register_weights_security(ctx, &weights, "critical_weights"), NIMCP_SUCCESS);
    EXPECT_GT(weights.region_id, 0u);

    // Create checkpoint
    nimcp_training_checkpoint_t checkpoint;
    ASSERT_EQ(nimcp_training_checkpoint_create(ctx, &weights, 1, &checkpoint), NIMCP_SUCCESS);

    // Modify weights
    float* data = nimcp_training_write_weights(ctx, &weights);
    ASSERT_NE(data, nullptr);
    for (int i = 0; i < 1000; i++) {
        data[i] = 99.0f;
    }

    // Update security baseline after modification
    EXPECT_EQ(nimcp_training_update_weights_baseline(ctx, &weights), NIMCP_SUCCESS);

    // Restore checkpoint - verify API works
    ASSERT_EQ(nimcp_training_checkpoint_restore(ctx, &weights, 1, &checkpoint), NIMCP_SUCCESS);

    // Verify data is still accessible after restore
    const float* restored = nimcp_training_read_weights(ctx, &weights);
    EXPECT_NE(restored, nullptr);

    // Cleanup
    nimcp_training_checkpoint_destroy(ctx, &checkpoint);
    nimcp_training_free_weights(ctx, &weights);
    nimcp_training_destroy(ctx);
}

//=============================================================================
// Concurrent Training Simulation
//=============================================================================

TEST_F(TrainingModuleIntegrationTest, ConcurrentWeightUpdates) {
    nimcp_training_module_config_t cfg = nimcp_training_default_config();
    cfg.type = NIMCP_TRAIN_MOD_STDP;
    cfg.name = "concurrent_test";
    cfg.enable_security = true;
    cfg.security_ctx = security;
    cfg.enable_unified_memory = true;
    cfg.mem_manager = memory;
    cfg.enable_cow = true;

    nimcp_training_context_t* ctx = nimcp_training_create(&cfg);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(nimcp_training_init(ctx), NIMCP_SUCCESS);

    // Create weights
    nimcp_training_weights_t weights;
    ASSERT_EQ(nimcp_training_alloc_weights(ctx, 10000, nullptr, &weights), NIMCP_SUCCESS);

    // Simulate concurrent training with multiple threads updating weights
    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int updates_per_thread = 100;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([ctx, &weights, t, updates_per_thread]() {
            for (int i = 0; i < updates_per_thread; i++) {
                float* data = nimcp_training_write_weights(ctx, &weights);
                if (data) {
                    // Each thread updates a different section
                    int offset = t * 2500;
                    for (int j = 0; j < 100; j++) {
                        data[offset + j] += 0.001f;
                    }
                }
                nimcp_training_record_success(ctx);
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Verify module is still trusted after concurrent operations
    EXPECT_TRUE(nimcp_training_is_trusted(ctx));

    // Cleanup
    nimcp_training_free_weights(ctx, &weights);
    nimcp_training_destroy(ctx);
}

//=============================================================================
// Training Phase Transitions
//=============================================================================

TEST_F(TrainingModuleIntegrationTest, MultiPhaseTraining) {
    // Simulate training pipeline with different phases

    // Phase T1: Homeostatic
    nimcp_training_module_config_t cfg1 = nimcp_training_default_config();
    cfg1.type = NIMCP_TRAIN_MOD_HOMEOSTATIC;
    cfg1.name = "phase_t1";
    cfg1.phase = NIMCP_TRAIN_PHASE_T1;
    cfg1.security_ctx = security;
    cfg1.mem_manager = memory;
    nimcp_training_context_t* t1 = nimcp_training_create(&cfg1);
    ASSERT_NE(t1, nullptr);
    ASSERT_EQ(nimcp_training_init(t1), NIMCP_SUCCESS);

    // Phase T2: Dendritic
    nimcp_training_module_config_t cfg2 = nimcp_training_default_config();
    cfg2.type = NIMCP_TRAIN_MOD_DENDRITIC;
    cfg2.name = "phase_t2";
    cfg2.phase = NIMCP_TRAIN_PHASE_T2;
    cfg2.security_ctx = security;
    cfg2.mem_manager = memory;
    nimcp_training_context_t* t2 = nimcp_training_create(&cfg2);
    ASSERT_NE(t2, nullptr);
    ASSERT_EQ(nimcp_training_init(t2), NIMCP_SUCCESS);

    // Phase T3: Predictive
    nimcp_training_module_config_t cfg3 = nimcp_training_default_config();
    cfg3.type = NIMCP_TRAIN_MOD_PREDICTIVE;
    cfg3.name = "phase_t3";
    cfg3.phase = NIMCP_TRAIN_PHASE_T3;
    cfg3.security_ctx = security;
    cfg3.mem_manager = memory;
    nimcp_training_context_t* t3 = nimcp_training_create(&cfg3);
    ASSERT_NE(t3, nullptr);
    ASSERT_EQ(nimcp_training_init(t3), NIMCP_SUCCESS);

    // Create initial weight matrix with known values (use init data for CoW)
    std::vector<float> init_data(5000, 0.1f);
    nimcp_training_weights_t base_weights;
    ASSERT_EQ(nimcp_training_alloc_weights(t1, 5000, init_data.data(), &base_weights), NIMCP_SUCCESS);
    nimcp_training_record_success(t1);

    // Clone to T2 - creates CoW reference
    nimcp_training_weights_t t2_weights;
    ASSERT_EQ(nimcp_training_clone_weights(t2, &base_weights, &t2_weights), NIMCP_SUCCESS);

    // T2 modifies its copy (triggers CoW copy)
    float* data = nimcp_training_write_weights(t2, &t2_weights);
    ASSERT_NE(data, nullptr);
    for (int i = 0; i < 5000; i++) {
        data[i] = 0.2f;  // Set to known value
    }
    nimcp_training_record_success(t2);

    // Clone T2's weights to T3
    nimcp_training_weights_t t3_weights;
    ASSERT_EQ(nimcp_training_clone_weights(t3, &t2_weights, &t3_weights), NIMCP_SUCCESS);
    nimcp_training_record_success(t3);

    // Verify all modules have accessible weights
    const float* t1_data = nimcp_training_read_weights(t1, &base_weights);
    const float* t2_data = nimcp_training_read_weights(t2, &t2_weights);
    const float* t3_data = nimcp_training_read_weights(t3, &t3_weights);

    EXPECT_NE(t1_data, nullptr);
    EXPECT_NE(t2_data, nullptr);
    EXPECT_NE(t3_data, nullptr);

    // Verify T2's written data is accessible
    EXPECT_FLOAT_EQ(t2_data[0], 0.2f);

    // Cleanup
    nimcp_training_free_weights(t1, &base_weights);
    nimcp_training_free_weights(t2, &t2_weights);
    nimcp_training_free_weights(t3, &t3_weights);

    nimcp_training_destroy(t1);
    nimcp_training_destroy(t2);
    nimcp_training_destroy(t3);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
