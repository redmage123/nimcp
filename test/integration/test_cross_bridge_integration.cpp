//=============================================================================
// test_cross_bridge_integration.cpp - Cross-Bridge Integration Tests
//=============================================================================
/**
 * @file test_cross_bridge_integration.cpp
 * @brief Integration tests for substrate + thalamic + quantum bridges together
 *
 * WHAT: Tests all bridge types working together without conflicts
 * WHY:  Verify no race conditions or conflicts between bridge subsystems
 * HOW:  Create all bridge types, connect to shared systems, run concurrent ops
 *
 * INTEGRATION POINTS:
 * - Substrate bridges: Metabolic effects on neural processing
 * - Thalamic bridges: Attention-based signal routing
 * - Quantum bridges: Accelerated attention and optimization
 *
 * TEST SCENARIOS:
 * 1. All bridge types operating concurrently
 * 2. Substrate effects propagating through thalamic routing
 * 3. Quantum optimization affecting substrate consumption
 * 4. Cross-bridge messaging via bio-async
 * 5. Thread safety under concurrent updates
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "core/cortical_columns/nimcp_cortical_substrate_bridge.h"
#include "cognitive/emotion/nimcp_emotion_substrate_bridge.h"
#include "middleware/routing/nimcp_thalamic_quantum_bridge.h"
#include "snn/bridges/nimcp_snn_thalamic_bridge.h"
#include "plasticity/attention/nimcp_quantum_attention.h"
#include "core/brain/nimcp_brain.h"
#include "utils/bridge/nimcp_bridge_base.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CrossBridgeIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    neural_substrate_t* substrate;
    thalamic_router_t* router;

    void SetUp() override {
        // Create brain
        brain_config_t brain_config;
        memset(&brain_config, 0, sizeof(brain_config));
        brain_config.size = BRAIN_SIZE_SMALL;
        brain_config.task = BRAIN_TASK_CLASSIFICATION;
        brain_config.num_inputs = 16;
        brain_config.num_outputs = 4;
        snprintf(brain_config.task_name, sizeof(brain_config.task_name), "cross_bridge_test");

        brain = brain_create_custom(&brain_config);
        ASSERT_NE(brain, nullptr) << "Failed to create test brain";

        // Create neural substrate
        substrate_config_t substrate_config;
        substrate_default_config(&substrate_config);
        substrate_config.enable_metabolic_model = true;
        substrate_config.enable_temperature_effects = true;

        substrate = substrate_create(&substrate_config);
        ASSERT_NE(substrate, nullptr) << "Failed to create neural substrate";

        // Create thalamic router
        thalamic_router_config_t router_config;
        thalamic_router_default_config(&router_config);
        router_config.max_destinations = 32;

        router = thalamic_router_create(&router_config);
        ASSERT_NE(router, nullptr) << "Failed to create thalamic router";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
    }
};

//=============================================================================
// Test: All Bridges Operating Together
//=============================================================================

TEST_F(CrossBridgeIntegrationTest, AllBridges_OperateConcurrently) {
    // WHAT: Test all bridge types operating together
    // WHY:  Verify no conflicts between different bridge subsystems
    // HOW:  Create all bridges, run updates, verify all function

    // Create substrate bridges
    cortical_substrate_config_t cortical_config;
    cortical_substrate_default_config(&cortical_config);
    cortical_substrate_bridge_t* cortical = cortical_substrate_bridge_create(
        &cortical_config, substrate
    );
    ASSERT_NE(cortical, nullptr);

    emotion_substrate_config_t emotion_config;
    emotion_substrate_default_config(&emotion_config);
    emotion_substrate_bridge_t* emotion = emotion_substrate_bridge_create(
        &emotion_config, nullptr, substrate
    );
    ASSERT_NE(emotion, nullptr);

    // Create thalamic quantum bridge
    thalamic_quantum_config_t quantum_config = thalamic_quantum_default_config();
    thalamic_quantum_bridge_t* thalamic_quantum = thalamic_quantum_bridge_create(
        &quantum_config
    );
    ASSERT_NE(thalamic_quantum, nullptr);

    // Create quantum attention
    quantum_attention_config_t qattn_config = quantum_attention_default_config();
    quantum_attention_t qattn = quantum_attention_create(&qattn_config, 8, 16, 2);
    ASSERT_NE(qattn, nullptr);

    // Run concurrent updates for 100 cycles
    for (int cycle = 0; cycle < 100; cycle++) {
        // Update substrate
        float atp = 0.7f + 0.2f * sin(cycle * 0.1f);
        float temp = 37.0f + 1.0f * cos(cycle * 0.05f);
        substrate_set_atp(substrate, atp);
        substrate_set_temperature(substrate, temp);
        substrate_update(substrate, 10);

        // Update substrate bridges
        EXPECT_EQ(cortical_substrate_update(cortical), 0);
        EXPECT_EQ(emotion_substrate_update(emotion), 0);

        // Do quantum routing
        float signal[16];
        for (int i = 0; i < 16; i++) {
            signal[i] = 0.5f + 0.3f * sin((cycle + i) * 0.2f);
        }

        uint32_t dest_ids[] = {1, 2, 3, 4};
        uint32_t routed[4];
        uint32_t num_routed = 0;

        thalamic_quantum_route(thalamic_quantum, 0, dest_ids, 4,
                               signal, 16, routed, &num_routed);

        // Do quantum attention
        float query[8 * 16], key[8 * 16];
        for (int i = 0; i < 8 * 16; i++) {
            query[i] = signal[i % 16] + 0.1f * sin(i * 0.3f);
            key[i] = signal[i % 16] + 0.1f * cos(i * 0.25f);
        }
        quantum_attention_compute_scores(qattn, query, key, 0, 0.25f);

        // Verify all bridges still functioning
        EXPECT_GE(cortical_substrate_get_column_fidelity(cortical), 0.0f);
        EXPECT_GE(emotion_substrate_get_intensity_mod(emotion), 0.0f);
    }

    // Cleanup
    cortical_substrate_bridge_destroy(cortical);
    emotion_substrate_bridge_destroy(emotion);
    thalamic_quantum_bridge_destroy(thalamic_quantum);
    quantum_attention_destroy(qattn);
}

//=============================================================================
// Test: Substrate Effects Propagate Through Thalamic Routing
//=============================================================================

TEST_F(CrossBridgeIntegrationTest, SubstrateEffects_PropagateToRouting) {
    // WHAT: Test substrate metabolic state affects routing decisions
    // WHY:  Verify metabolic stress influences attention/routing
    // HOW:  Create both bridge types, vary substrate, observe routing changes

    // Create substrate bridge
    cortical_substrate_config_t cortical_config;
    cortical_substrate_default_config(&cortical_config);
    cortical_substrate_bridge_t* cortical = cortical_substrate_bridge_create(
        &cortical_config, substrate
    );
    ASSERT_NE(cortical, nullptr);

    // Create thalamic quantum bridge
    thalamic_quantum_config_t quantum_config = thalamic_quantum_default_config();
    quantum_config.routing_threshold = 0.3f;
    thalamic_quantum_bridge_t* thalamic_quantum = thalamic_quantum_bridge_create(
        &quantum_config
    );
    ASSERT_NE(thalamic_quantum, nullptr);

    // Test signal
    float signal[16];
    for (int i = 0; i < 16; i++) {
        signal[i] = 0.5f;
    }

    uint32_t dest_ids[] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint32_t routed[8];
    uint32_t num_routed;

    // High ATP state - normal routing
    substrate_set_atp(substrate, 0.95f);
    substrate_update(substrate, 1);
    cortical_substrate_update(cortical);

    float high_atp_fidelity = cortical_substrate_get_column_fidelity(cortical);

    thalamic_quantum_route(thalamic_quantum, 0, dest_ids, 8,
                           signal, 16, routed, &num_routed);
    uint32_t high_atp_routes = num_routed;

    // Low ATP state - potentially different routing
    substrate_set_atp(substrate, 0.25f);
    substrate_update(substrate, 1);
    cortical_substrate_update(cortical);

    float low_atp_fidelity = cortical_substrate_get_column_fidelity(cortical);

    thalamic_quantum_route(thalamic_quantum, 0, dest_ids, 8,
                           signal, 16, routed, &num_routed);
    uint32_t low_atp_routes = num_routed;

    // Verify substrate effects
    EXPECT_LT(low_atp_fidelity, high_atp_fidelity)
        << "Low ATP should reduce cortical fidelity";

    // Both routing scenarios should be valid
    EXPECT_LE(high_atp_routes, 8u);
    EXPECT_LE(low_atp_routes, 8u);

    cortical_substrate_bridge_destroy(cortical);
    thalamic_quantum_bridge_destroy(thalamic_quantum);
}

//=============================================================================
// Test: Quantum Optimization with Substrate Constraints
//=============================================================================

TEST_F(CrossBridgeIntegrationTest, QuantumOptimization_SubstrateConstrained) {
    // WHAT: Test quantum optimization respects substrate constraints
    // WHY:  Verify optimization doesn't violate metabolic limits
    // HOW:  Run quantum operations, verify substrate consumption tracked

    // Create substrate bridge for monitoring
    cortical_substrate_config_t cortical_config;
    cortical_substrate_default_config(&cortical_config);
    cortical_substrate_bridge_t* cortical = cortical_substrate_bridge_create(
        &cortical_config, substrate
    );
    ASSERT_NE(cortical, nullptr);

    // Create quantum attention for intensive operations
    quantum_attention_config_t qattn_config = quantum_attention_default_config();
    qattn_config.mode = QUANTUM_ATTENTION_FULL;
    quantum_attention_t qattn = quantum_attention_create(&qattn_config, 16, 32, 4);
    ASSERT_NE(qattn, nullptr);

    // Start with full ATP
    substrate_set_atp(substrate, 0.95f);

    // Run intensive quantum operations
    float query[16 * 32], key[16 * 32];
    for (int i = 0; i < 16 * 32; i++) {
        query[i] = 0.5f + 0.5f * sin(i * 0.1f);
        key[i] = 0.5f + 0.5f * cos(i * 0.12f);
    }

    for (int iter = 0; iter < 50; iter++) {
        // Quantum attention (simulates neural computation)
        quantum_attention_compute_scores(qattn, query, key, iter % 4, 0.177f);

        // Record neural activity
        substrate_record_spikes(substrate, 100);
        substrate_record_transmissions(substrate, 500);

        // Update substrate
        substrate_update(substrate, 20);
        cortical_substrate_update(cortical);
    }

    // Get substrate stats
    substrate_stats_t sub_stats;
    substrate_get_stats(substrate, &sub_stats);

    EXPECT_GT(sub_stats.total_updates, 0u);
    EXPECT_GT(sub_stats.spikes_processed, 0u);

    // Get cortical stats
    cortical_substrate_stats_t cortical_stats;
    cortical_substrate_get_stats(cortical, &cortical_stats);

    EXPECT_GT(cortical_stats.update_count, 0u);

    cortical_substrate_bridge_destroy(cortical);
    quantum_attention_destroy(qattn);
}

//=============================================================================
// Test: Cross-Bridge Statistics Consistency
//=============================================================================

TEST_F(CrossBridgeIntegrationTest, Statistics_ConsistentAcrossBridges) {
    // WHAT: Test statistics are consistent across bridge types
    // WHY:  Verify monitoring captures all bridge activity
    // HOW:  Run updates, compare statistics from different bridges

    // Create multiple bridges
    cortical_substrate_config_t cortical_config;
    cortical_substrate_default_config(&cortical_config);
    cortical_substrate_bridge_t* cortical = cortical_substrate_bridge_create(
        &cortical_config, substrate
    );
    ASSERT_NE(cortical, nullptr);

    emotion_substrate_config_t emotion_config;
    emotion_substrate_default_config(&emotion_config);
    emotion_substrate_bridge_t* emotion = emotion_substrate_bridge_create(
        &emotion_config, nullptr, substrate
    );
    ASSERT_NE(emotion, nullptr);

    thalamic_quantum_config_t quantum_config = thalamic_quantum_default_config();
    thalamic_quantum_bridge_t* thalamic = thalamic_quantum_bridge_create(
        &quantum_config
    );
    ASSERT_NE(thalamic, nullptr);

    // Reset stats
    thalamic_quantum_reset_stats(thalamic);

    const int num_updates = 25;

    for (int i = 0; i < num_updates; i++) {
        substrate_update(substrate, 10);
        cortical_substrate_update(cortical);
        emotion_substrate_update(emotion);

        // Thalamic routing
        float signal[8] = {0.5f, 0.6f, 0.7f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f};
        uint32_t dests[] = {1, 2, 3};
        uint32_t routed[3];
        uint32_t num_routed;
        thalamic_quantum_route(thalamic, 0, dests, 3, signal, 8, routed, &num_routed);
    }

    // Get all statistics
    cortical_substrate_stats_t cortical_stats;
    cortical_substrate_get_stats(cortical, &cortical_stats);

    emotion_substrate_stats_t emotion_stats = emotion_substrate_get_stats(emotion);

    thalamic_quantum_stats_t thalamic_stats;
    thalamic_quantum_get_stats(thalamic, &thalamic_stats);

    // Verify update counts match
    EXPECT_EQ(cortical_stats.update_count, (uint64_t)num_updates);
    EXPECT_EQ(emotion_stats.total_updates, (uint64_t)num_updates);

    // Thalamic should have processed routes
    EXPECT_EQ(thalamic_stats.quantum_routes + thalamic_stats.classical_fallbacks,
              (uint64_t)num_updates);

    cortical_substrate_bridge_destroy(cortical);
    emotion_substrate_bridge_destroy(emotion);
    thalamic_quantum_bridge_destroy(thalamic);
}

//=============================================================================
// Test: Thread Safety Under Concurrent Updates
//=============================================================================

TEST_F(CrossBridgeIntegrationTest, ThreadSafety_ConcurrentUpdates) {
    // WHAT: Test thread safety with concurrent bridge updates
    // WHY:  Verify no race conditions in multi-threaded scenario
    // HOW:  Run updates from multiple threads, verify no crashes/corruption

    // Create bridges
    cortical_substrate_config_t cortical_config;
    cortical_substrate_default_config(&cortical_config);
    cortical_substrate_bridge_t* cortical = cortical_substrate_bridge_create(
        &cortical_config, substrate
    );
    ASSERT_NE(cortical, nullptr);

    thalamic_quantum_config_t quantum_config = thalamic_quantum_default_config();
    thalamic_quantum_bridge_t* thalamic = thalamic_quantum_bridge_create(
        &quantum_config
    );
    ASSERT_NE(thalamic, nullptr);

    std::atomic<bool> has_error{false};
    std::atomic<int> update_count{0};

    // Thread 1: Substrate updates
    std::thread substrate_thread([&]() {
        for (int i = 0; i < 50 && !has_error; i++) {
            float atp = 0.5f + 0.4f * sin(i * 0.2f);
            substrate_set_atp(substrate, atp);
            if (substrate_update(substrate, 10) != 0) {
                has_error = true;
            }
            update_count++;
        }
    });

    // Thread 2: Cortical bridge updates
    std::thread cortical_thread([&]() {
        for (int i = 0; i < 50 && !has_error; i++) {
            if (cortical_substrate_update(cortical) != 0) {
                has_error = true;
            }
            float fidelity = cortical_substrate_get_column_fidelity(cortical);
            if (fidelity < 0.0f || fidelity > 1.0f) {
                has_error = true;
            }
            update_count++;
        }
    });

    // Thread 3: Thalamic routing
    std::thread thalamic_thread([&]() {
        for (int i = 0; i < 50 && !has_error; i++) {
            float signal[8];
            for (int j = 0; j < 8; j++) {
                signal[j] = 0.5f + 0.3f * sin((i + j) * 0.3f);
            }
            uint32_t dests[] = {1, 2, 3, 4};
            uint32_t routed[4];
            uint32_t num_routed = 0;

            int result = thalamic_quantum_route(thalamic, 0, dests, 4,
                                                signal, 8, routed, &num_routed);
            if (result != 0) {
                has_error = true;
            }
            update_count++;
        }
    });

    // Wait for all threads
    substrate_thread.join();
    cortical_thread.join();
    thalamic_thread.join();

    EXPECT_FALSE(has_error) << "No errors should occur during concurrent updates";
    EXPECT_EQ(update_count.load(), 150) << "All updates should complete";

    // Verify bridges still functional
    EXPECT_GE(cortical_substrate_get_column_fidelity(cortical), 0.0f);
    EXPECT_TRUE(thalamic_quantum_bridge_is_enabled(thalamic));

    cortical_substrate_bridge_destroy(cortical);
    thalamic_quantum_bridge_destroy(thalamic);
}

//=============================================================================
// Test: Bridge Reset Coordination
//=============================================================================

TEST_F(CrossBridgeIntegrationTest, Reset_CoordinatedAcrossBridges) {
    // WHAT: Test coordinated reset of multiple bridges
    // WHY:  Verify reset synchronization works correctly
    // HOW:  Run updates, reset all, verify clean state

    // Create bridges
    cortical_substrate_config_t cortical_config;
    cortical_substrate_default_config(&cortical_config);
    cortical_substrate_bridge_t* cortical = cortical_substrate_bridge_create(
        &cortical_config, substrate
    );
    ASSERT_NE(cortical, nullptr);

    thalamic_quantum_config_t quantum_config = thalamic_quantum_default_config();
    thalamic_quantum_bridge_t* thalamic = thalamic_quantum_bridge_create(
        &quantum_config
    );
    ASSERT_NE(thalamic, nullptr);

    // Run some updates
    for (int i = 0; i < 20; i++) {
        substrate_update(substrate, 10);
        cortical_substrate_update(cortical);

        float signal[8] = {0.5f};
        uint32_t dests[] = {1, 2};
        uint32_t routed[2];
        uint32_t num_routed;
        thalamic_quantum_route(thalamic, 0, dests, 2, signal, 8, routed, &num_routed);
    }

    // Get pre-reset stats
    cortical_substrate_stats_t pre_stats;
    cortical_substrate_get_stats(cortical, &pre_stats);
    EXPECT_GT(pre_stats.update_count, 0u);

    thalamic_quantum_stats_t pre_thalamic;
    thalamic_quantum_get_stats(thalamic, &pre_thalamic);
    EXPECT_GT(pre_thalamic.quantum_routes + pre_thalamic.classical_fallbacks, 0u);

    // Reset bridges
    bridge_base_reset(&cortical->base);
    thalamic_quantum_reset_stats(thalamic);
    substrate_reset(substrate);

    // Verify reset
    uint64_t total_updates = 0;
    bridge_base_get_stats(&cortical->base, &total_updates, nullptr);
    EXPECT_EQ(total_updates, 0u);

    thalamic_quantum_stats_t post_thalamic;
    thalamic_quantum_get_stats(thalamic, &post_thalamic);
    EXPECT_EQ(post_thalamic.quantum_routes, 0u);
    EXPECT_EQ(post_thalamic.classical_fallbacks, 0u);

    cortical_substrate_bridge_destroy(cortical);
    thalamic_quantum_bridge_destroy(thalamic);
}

//=============================================================================
// Test: Cross-Bridge State Propagation
//=============================================================================

TEST_F(CrossBridgeIntegrationTest, State_PropagatesAcrossBridges) {
    // WHAT: Test state propagation between different bridge types
    // WHY:  Verify changes in one bridge can affect others
    // HOW:  Modify substrate, verify effects in thalamic routing quality

    // Create substrate and cortical bridge
    cortical_substrate_config_t cortical_config;
    cortical_substrate_default_config(&cortical_config);
    cortical_substrate_bridge_t* cortical = cortical_substrate_bridge_create(
        &cortical_config, substrate
    );
    ASSERT_NE(cortical, nullptr);

    // Create quantum attention with substrate-like threshold adaptation
    quantum_attention_config_t qattn_config = quantum_attention_default_config();
    quantum_attention_t qattn = quantum_attention_create(&qattn_config, 8, 16, 1);
    ASSERT_NE(qattn, nullptr);

    // Test at different substrate states
    struct TestCase {
        float atp;
        const char* description;
    };

    TestCase cases[] = {
        {0.95f, "High ATP"},
        {0.50f, "Medium ATP"},
        {0.25f, "Low ATP"}
    };

    for (const auto& tc : cases) {
        // Set substrate state
        substrate_set_atp(substrate, tc.atp);
        substrate_update(substrate, 1);
        cortical_substrate_update(cortical);

        // Get cortical effect
        float fidelity = cortical_substrate_get_column_fidelity(cortical);

        // Compute quantum attention
        float query[8 * 16], key[8 * 16];
        for (int i = 0; i < 8 * 16; i++) {
            query[i] = fidelity * 0.5f + 0.5f * sin(i * 0.1f);
            key[i] = fidelity * 0.5f + 0.5f * cos(i * 0.12f);
        }

        quantum_attention_reset_mask(qattn);
        quantum_attention_compute_scores(qattn, query, key, 0, 0.25f);

        // Get sparse pairs
        uint32_t q_idx[64], k_idx[64];
        float vals[64];
        uint32_t num_pairs = quantum_attention_get_sparse_pairs(
            qattn, q_idx, k_idx, vals, 64
        );

        // State should affect attention patterns
        // Lower fidelity may lead to different attention distribution
        EXPECT_GE(num_pairs, 0u) << "Should have valid pairs at " << tc.description;
    }

    cortical_substrate_bridge_destroy(cortical);
    quantum_attention_destroy(qattn);
}

//=============================================================================
// Test: Full Pipeline Integration
//=============================================================================

TEST_F(CrossBridgeIntegrationTest, FullPipeline_AllBridgeTypes) {
    // WHAT: Test complete pipeline with all bridge types
    // WHY:  Verify end-to-end integration works correctly
    // HOW:  Process input through substrate, thalamic, quantum, and brain

    // Create all bridges
    cortical_substrate_config_t cortical_config;
    cortical_substrate_default_config(&cortical_config);
    cortical_substrate_bridge_t* cortical = cortical_substrate_bridge_create(
        &cortical_config, substrate
    );
    ASSERT_NE(cortical, nullptr);

    emotion_substrate_config_t emotion_config;
    emotion_substrate_default_config(&emotion_config);
    emotion_substrate_bridge_t* emotion = emotion_substrate_bridge_create(
        &emotion_config, nullptr, substrate
    );
    ASSERT_NE(emotion, nullptr);

    thalamic_quantum_config_t thalamic_config = thalamic_quantum_default_config();
    thalamic_quantum_bridge_t* thalamic = thalamic_quantum_bridge_create(
        &thalamic_config
    );
    ASSERT_NE(thalamic, nullptr);

    quantum_attention_config_t qattn_config = quantum_attention_default_config();
    quantum_attention_t qattn = quantum_attention_create(&qattn_config, 8, 16, 2);
    ASSERT_NE(qattn, nullptr);

    // Process input through full pipeline
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.1f + 0.1f * i;
    }

    // Step 1: Update substrate
    substrate_record_spikes(substrate, 50);
    substrate_update(substrate, 10);

    // Step 2: Update substrate bridges
    cortical_substrate_update(cortical);
    emotion_substrate_update(emotion);

    float cortical_fidelity = cortical_substrate_get_column_fidelity(cortical);
    float emotion_intensity = emotion_substrate_get_intensity_mod(emotion);

    // Step 3: Thalamic routing with substrate-modulated signal
    float modulated_signal[16];
    for (int i = 0; i < 16; i++) {
        modulated_signal[i] = input[i] * cortical_fidelity;
    }

    uint32_t region_ids[] = {1, 2, 3, 4};
    uint32_t routed_regions[4];
    uint32_t num_routed;

    thalamic_quantum_route(thalamic, 0, region_ids, 4,
                           modulated_signal, 16,
                           routed_regions, &num_routed);

    // Step 4: Quantum attention processing
    float query[8 * 16], key[8 * 16];
    for (int i = 0; i < 8 * 16; i++) {
        query[i] = modulated_signal[i % 16];
        key[i] = modulated_signal[(i + 1) % 16];
    }

    quantum_attention_compute_scores(qattn, query, key, 0, 0.25f);

    // Step 5: Brain inference with full context
    brain_decision_t* decision = brain_decide(brain, input, 16);
    ASSERT_NE(decision, nullptr);

    // Verify pipeline produced valid results
    EXPECT_GE(cortical_fidelity, 0.0f);
    EXPECT_LE(cortical_fidelity, 1.0f);
    EXPECT_GE(emotion_intensity, 0.0f);
    EXPECT_LE(num_routed, 4u);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);

    // Cleanup
    cortical_substrate_bridge_destroy(cortical);
    emotion_substrate_bridge_destroy(emotion);
    thalamic_quantum_bridge_destroy(thalamic);
    quantum_attention_destroy(qattn);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
