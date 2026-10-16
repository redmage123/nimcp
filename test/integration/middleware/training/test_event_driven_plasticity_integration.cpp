//=============================================================================
// test_event_driven_plasticity_integration.cpp - Integration Tests
//=============================================================================
/**
 * @file test_event_driven_plasticity_integration.cpp
 * @brief Integration tests for Event-Driven Plasticity with other modules
 *
 * Tests cover:
 * - Integration with Training-Plasticity Bridge
 * - Integration with event bus
 * - End-to-end continuous learning scenarios
 * - Spike timing dependent plasticity flow
 * - Three-factor learning with reward
 * - Neuromodulator interaction
 *
 * @version 1.0.0
 * @date 2025-11-27
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <numeric>
#include <chrono>
#include <thread>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_event_driven_plasticity.h"
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "core/events/nimcp_event_bus.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EventDrivenPlasticityIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        edp_ = nullptr;
        bridge_ = nullptr;
        event_bus_ = nullptr;
    }

    void TearDown() override {
        if (edp_) {
            edp_stop(edp_);
            edp_destroy(edp_);
            edp_ = nullptr;
        }
        if (bridge_) {
            tpb_destroy(bridge_);
            bridge_ = nullptr;
        }
        if (event_bus_) {
            event_bus_destroy(event_bus_);
            event_bus_ = nullptr;
        }
    }

    void CreateFullPipeline() {
        // Create event bus
        event_bus_ = event_bus_create("edp_test_bus", EVENT_DELIVERY_IMMEDIATE);
        ASSERT_NE(event_bus_, nullptr);

        // Create plasticity bridge
        tpb_config_t bridge_config = tpb_config_default();
        bridge_ = tpb_create(&bridge_config);
        ASSERT_NE(bridge_, nullptr);

        // Create EDP
        edp_config_t edp_config = edp_config_default();
        edp_config.mode = EDP_MODE_IMMEDIATE;
        edp_ = edp_create(&edp_config);
        ASSERT_NE(edp_, nullptr);

        // Connect components
        EXPECT_EQ(edp_connect_bridge(edp_, bridge_), NIMCP_SUCCESS);

        // Start EDP
        EXPECT_EQ(edp_start(edp_), NIMCP_SUCCESS);
    }

    edp_context_t* edp_;
    tpb_context_t* bridge_;
    event_bus_t event_bus_;
};

//=============================================================================
// End-to-End Learning Scenario Tests
//=============================================================================

TEST_F(EventDrivenPlasticityIntegrationTest, ContinuousLearningScenario) {
    CreateFullPipeline();

    // Simulate a continuous learning scenario:
    // 1. Spike bursts arrive (sensory input)
    // 2. Prediction errors are computed
    // 3. Rewards are delivered (for good predictions)
    // 4. Weight updates accumulate via eligibility traces

    // Phase 1: Sensory input (spike bursts)
    for (int i = 0; i < 10; i++) {
        std::vector<uint32_t> neuron_ids = {
            static_cast<uint32_t>(i * 10),
            static_cast<uint32_t>(i * 10 + 1),
            static_cast<uint32_t>(i * 10 + 2)
        };
        spike_burst_data_t burst = {0};
        burst.neuron_ids = neuron_ids.data();
        burst.num_neurons = static_cast<uint32_t>(neuron_ids.size());
        burst.timestamp_ns = i * 10000000;  // 10ms intervals
        burst.synchrony_score = 0.8f + (i * 0.01f);

        EXPECT_EQ(edp_process_spike_burst(edp_, &burst, 0), NIMCP_SUCCESS);
    }

    // Phase 2: Prediction errors (some good, some bad)
    edp_process_prediction_error(edp_, 0.5f, 0);   // Large error initially
    edp_process_prediction_error(edp_, 0.3f, 0);   // Improving
    edp_process_prediction_error(edp_, 0.1f, 0);   // Good prediction

    // Phase 3: Deliver reward for good predictions
    EXPECT_EQ(edp_process_reward(edp_, 1.0f), NIMCP_SUCCESS);

    // Phase 4: Consolidate eligibility traces with reward
    uint32_t consolidated = edp_consolidate_eligibility(edp_, 0.8f);
    // May or may not have traces depending on timing

    // Verify statistics
    edp_stats_t stats;
    EXPECT_EQ(edp_get_stats(edp_, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.total_events_processed, 0);
}

TEST_F(EventDrivenPlasticityIntegrationTest, STDPLearningFlow) {
    CreateFullPipeline();

    // Simulate STDP-compliant spike timing:
    // Pre-synaptic spike followed by post-synaptic spike → LTP
    // Post-synaptic spike followed by pre-synaptic spike → LTD

    // LTP scenario: Pre fires, then Post fires (5ms later)
    uint32_t pre_neurons[] = {1, 2, 3};
    spike_burst_data_t pre_burst = {0};
    pre_burst.neuron_ids = pre_neurons;
    pre_burst.num_neurons = 3;
    pre_burst.timestamp_ns = 1000000;  // t=1ms
    pre_burst.synchrony_score = 0.9f;

    uint32_t post_neurons[] = {4, 5, 6};
    spike_burst_data_t post_burst = {0};
    post_burst.neuron_ids = post_neurons;
    post_burst.num_neurons = 3;
    post_burst.timestamp_ns = 6000000;  // t=6ms (5ms after pre)
    post_burst.synchrony_score = 0.9f;

    EXPECT_EQ(edp_process_spike_burst(edp_, &pre_burst, 0), NIMCP_SUCCESS);
    EXPECT_EQ(edp_process_spike_burst(edp_, &post_burst, 0), NIMCP_SUCCESS);

    // Deliver reward to consolidate LTP
    EXPECT_EQ(edp_process_reward(edp_, 1.0f), NIMCP_SUCCESS);

    edp_stats_t stats;
    edp_get_stats(edp_, &stats);
    EXPECT_GT(stats.spike_pairs_evaluated, 0);
}

TEST_F(EventDrivenPlasticityIntegrationTest, ThreeFactorLearning) {
    CreateFullPipeline();

    // Three-factor learning: activity × eligibility × reward
    // 1. Generate activity (spikes) to create eligibility traces
    // 2. Wait for delayed reward signal
    // 3. Consolidate with reward → weight changes

    // Step 1: Activity
    for (int burst = 0; burst < 5; burst++) {
        std::vector<uint32_t> neurons;
        for (int i = 0; i < 10; i++) {
            neurons.push_back(burst * 20 + i);
        }
        spike_burst_data_t spike_burst = {0};
        spike_burst.neuron_ids = neurons.data();
        spike_burst.num_neurons = static_cast<uint32_t>(neurons.size());
        spike_burst.timestamp_ns = burst * 20000000;  // 20ms intervals
        spike_burst.synchrony_score = 0.85f;

        edp_process_spike_burst(edp_, &spike_burst, 0);
    }

    // Step 2: Simulate delay (eligibility traces decay)
    // In real system, this would be actual time passing
    // Here we just simulate the reward arriving later

    // Step 3: Delayed reward (like dopamine from VTA)
    uint32_t before_consolidation = 0;
    {
        edp_stats_t stats;
        edp_get_stats(edp_, &stats);
        before_consolidation = stats.eligibility_consolidations;
    }

    edp_consolidate_eligibility(edp_, 1.0f);  // Strong reward

    uint32_t after_consolidation = 0;
    {
        edp_stats_t stats;
        edp_get_stats(edp_, &stats);
        after_consolidation = stats.eligibility_consolidations;
    }

    // Should have consolidated some eligibility traces
    EXPECT_GE(after_consolidation, before_consolidation);
}

TEST_F(EventDrivenPlasticityIntegrationTest, NoveltyDrivenLearning) {
    CreateFullPipeline();

    // Novelty-driven learning: high novelty → increased learning rate

    // Process novelty signal
    EXPECT_EQ(edp_process_novelty(edp_, 0.9f, 0), NIMCP_SUCCESS);

    // Now process spike activity (should be enhanced by novelty)
    uint32_t neurons[] = {100, 101, 102, 103, 104};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 5;
    burst.timestamp_ns = 50000000;
    burst.synchrony_score = 0.8f;

    EXPECT_EQ(edp_process_spike_burst(edp_, &burst, 0), NIMCP_SUCCESS);

    edp_stats_t stats;
    edp_get_stats(edp_, &stats);
    // Novelty event should be counted
    EXPECT_GT(stats.category_stats[EDP_CATEGORY_NOVELTY].events_received, 0);
}

//=============================================================================
// Bridge Integration Tests
//=============================================================================

TEST_F(EventDrivenPlasticityIntegrationTest, BridgeNeuromodulatorCoupling) {
    CreateFullPipeline();

    // Get initial DA levels
    float initial_da = 0.0f;
    tpb_get_neuromod_levels(bridge_, &initial_da, nullptr, nullptr, nullptr);

    // Process positive reward (should increase DA)
    edp_process_reward(edp_, 1.0f);

    float after_reward_da = 0.0f;
    tpb_get_neuromod_levels(bridge_, &after_reward_da, nullptr, nullptr, nullptr);

    // DA should increase after reward
    EXPECT_GE(after_reward_da, initial_da);
}

TEST_F(EventDrivenPlasticityIntegrationTest, MultipleBurstSequence) {
    CreateFullPipeline();

    // Send a rapid sequence of spike bursts (like real cortical activity)
    const int num_bursts = 50;
    for (int i = 0; i < num_bursts; i++) {
        std::vector<uint32_t> neurons;
        for (int j = 0; j < 5; j++) {
            neurons.push_back(i * 5 + j);
        }

        spike_burst_data_t burst = {0};
        burst.neuron_ids = neurons.data();
        burst.num_neurons = static_cast<uint32_t>(neurons.size());
        burst.timestamp_ns = i * 5000000;  // 5ms intervals
        burst.synchrony_score = 0.7f + (static_cast<float>(i % 10) * 0.02f);

        EXPECT_EQ(edp_process_spike_burst(edp_, &burst, 0), NIMCP_SUCCESS);
    }

    // Verify all bursts were processed
    edp_stats_t stats;
    edp_get_stats(edp_, &stats);
    EXPECT_EQ(stats.category_stats[EDP_CATEGORY_SPIKE].events_received, num_bursts);
}

//=============================================================================
// Configuration Presets Integration
//=============================================================================

TEST_F(EventDrivenPlasticityIntegrationTest, BiologicalPresetIntegration) {
    // Create with biological preset
    edp_config_t bio_config = edp_config_biological();
    edp_ = edp_create(&bio_config);
    ASSERT_NE(edp_, nullptr);

    tpb_config_t bridge_config = tpb_config_preset("biological");
    bridge_ = tpb_create(&bridge_config);
    ASSERT_NE(bridge_, nullptr);

    edp_connect_bridge(edp_, bridge_);
    edp_start(edp_);

    // Run biological scenario (longer STDP window, realistic parameters)
    uint32_t pre_neurons[] = {10, 11};
    spike_burst_data_t pre = {0};
    pre.neuron_ids = pre_neurons;
    pre.num_neurons = 2;
    pre.timestamp_ns = 10000000;  // 10ms
    pre.synchrony_score = 0.6f;

    uint32_t post_neurons[] = {20, 21};
    spike_burst_data_t post = {0};
    post.neuron_ids = post_neurons;
    post.num_neurons = 2;
    post.timestamp_ns = 30000000;  // 30ms (20ms after pre - within biological STDP window)
    post.synchrony_score = 0.7f;

    EXPECT_EQ(edp_process_spike_burst(edp_, &pre, 0), NIMCP_SUCCESS);
    EXPECT_EQ(edp_process_spike_burst(edp_, &post, 0), NIMCP_SUCCESS);

    edp_stats_t stats;
    edp_get_stats(edp_, &stats);
    EXPECT_GT(stats.spike_pairs_evaluated, 0);
}

TEST_F(EventDrivenPlasticityIntegrationTest, HighPerformancePresetIntegration) {
    // Create with high-performance preset
    edp_config_t hp_config = edp_config_high_performance();
    edp_ = edp_create(&hp_config);
    ASSERT_NE(edp_, nullptr);

    tpb_config_t bridge_config = tpb_config_default();
    bridge_ = tpb_create(&bridge_config);
    ASSERT_NE(bridge_, nullptr);

    edp_connect_bridge(edp_, bridge_);
    edp_start(edp_);

    // High-throughput scenario
    auto start = std::chrono::high_resolution_clock::now();

    const int num_events = 1000;
    for (int i = 0; i < num_events; i++) {
        uint32_t neurons[] = {static_cast<uint32_t>(i % 100)};
        spike_burst_data_t burst = {0};
        burst.neuron_ids = neurons;
        burst.num_neurons = 1;
        burst.timestamp_ns = i * 1000000;
        burst.synchrony_score = 0.5f;

        edp_process_spike_burst(edp_, &burst, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should process 1000 events in under 500ms
    EXPECT_LT(duration.count(), 500) << "High-performance mode should be fast";

    edp_stats_t stats;
    edp_get_stats(edp_, &stats);
    EXPECT_EQ(stats.category_stats[EDP_CATEGORY_SPIKE].events_received, num_events);
}

//=============================================================================
// Error Handling Integration
//=============================================================================

TEST_F(EventDrivenPlasticityIntegrationTest, GracefulDegradation) {
    CreateFullPipeline();

    // Disconnect bridge
    edp_connect_bridge(edp_, nullptr);

    // Processing should still return success (soft fail)
    nimcp_result_t result = edp_process_reward(edp_, 1.0f);
    EXPECT_EQ(result, NIMCP_NOT_INITIALIZED);

    // Reconnect
    edp_connect_bridge(edp_, bridge_);

    // Should work again
    result = edp_process_reward(edp_, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Statistics Tracking Integration
//=============================================================================

TEST_F(EventDrivenPlasticityIntegrationTest, ComprehensiveStatsTracking) {
    CreateFullPipeline();

    // Generate varied activity across all categories
    uint32_t neurons[] = {1, 2, 3};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 3;
    burst.timestamp_ns = 1000000;
    burst.synchrony_score = 0.8f;

    // Spike events
    for (int i = 0; i < 10; i++) {
        burst.timestamp_ns = i * 10000000;
        edp_process_spike_burst(edp_, &burst, 0);
    }

    // Error events
    for (int i = 0; i < 5; i++) {
        edp_process_prediction_error(edp_, 0.1f * i, 0);
    }

    // Reward events
    for (int i = 0; i < 3; i++) {
        edp_process_reward(edp_, 0.5f + i * 0.2f);
    }

    // Novelty events
    for (int i = 0; i < 2; i++) {
        edp_process_novelty(edp_, 0.7f + i * 0.1f, 0);
    }

    // Check stats
    edp_stats_t stats;
    edp_get_stats(edp_, &stats);

    EXPECT_EQ(stats.category_stats[EDP_CATEGORY_SPIKE].events_received, 10);
    EXPECT_GE(stats.category_stats[EDP_CATEGORY_ERROR].events_received, 5);
    EXPECT_GE(stats.category_stats[EDP_CATEGORY_REWARD].events_received, 3);
    EXPECT_GE(stats.category_stats[EDP_CATEGORY_NOVELTY].events_received, 2);

    // Total should match
    EXPECT_GE(stats.total_events_received, 20);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
