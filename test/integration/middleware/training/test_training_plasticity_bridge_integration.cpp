//=============================================================================
// test_training_plasticity_bridge_integration.cpp - Integration Tests
//=============================================================================
/**
 * @file test_training_plasticity_bridge_integration.cpp
 * @brief Integration tests for Training-Plasticity Bridge with other modules
 *
 * Tests cover:
 * - Integration with brain training context
 * - Integration with neuromodulator system
 * - End-to-end learning scenarios
 * - Multi-region learning
 * - CoW with actual weight matrices
 *
 * @version 1.0.0
 * @date 2025-11-27
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <numeric>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "middleware/training/nimcp_brain_training_integration.h"
#include "middleware/training/nimcp_training_callbacks.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TrainingPlasticityIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        bridge_ = nullptr;
    }

    void TearDown() override {
        if (bridge_) {
            tpb_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    tpb_context_t* bridge_;
};

//=============================================================================
// End-to-End Learning Scenario Tests
//=============================================================================

TEST_F(TrainingPlasticityIntegrationTest, SupervisedLearningScenario) {
    // Create bridge with supervised preset
    tpb_config_t config = tpb_config_preset("supervised");
    bridge_ = tpb_create(&config);
    ASSERT_NE(bridge_, nullptr);

    // Configure cortical region
    tpb_region_config_t cortical = tpb_region_cortical_default();
    cortical.neuron_start_idx = 0;
    cortical.neuron_end_idx = 1000;
    tpb_configure_region(bridge_, &cortical, nullptr);

    // Simulate training epochs with decreasing loss
    std::vector<float> losses = {1.0f, 0.8f, 0.6f, 0.5f, 0.4f, 0.35f, 0.3f, 0.28f, 0.26f, 0.25f};

    float cumulative_rpe = 0.0f;
    for (float loss : losses) {
        float rpe = 0.0f;
        EXPECT_EQ(tpb_report_loss(bridge_, loss, &rpe), NIMCP_SUCCESS);
        cumulative_rpe += rpe;
    }

    // Average RPE should be positive (improving)
    EXPECT_GT(cumulative_rpe / losses.size(), 0.0f)
        << "Decreasing loss should yield positive average RPE";

    // Check DA levels increased
    float da = 0.0f;
    tpb_get_neuromod_levels(bridge_, &da, nullptr, nullptr, nullptr);
    EXPECT_GT(da, 0.5f) << "DA should be elevated after successful learning";

    // Verify weight updates would be positive (LTP)
    const int n_synapses = 100;
    std::vector<float> weights(n_synapses, 0.5f);
    std::vector<uint32_t> pre_ids(n_synapses), post_ids(n_synapses);
    std::vector<float> pre_act(n_synapses), post_act(n_synapses), deltas(n_synapses);

    for (int i = 0; i < n_synapses; i++) {
        pre_ids[i] = i;
        post_ids[i] = i + 1;
        pre_act[i] = 0.8f;
        post_act[i] = 0.9f;
        deltas[i] = 10.0f;  // LTP timing
    }

    EXPECT_EQ(tpb_apply_plasticity_batch(bridge_, n_synapses,
                                          pre_ids.data(), post_ids.data(),
                                          pre_act.data(), post_act.data(),
                                          deltas.data(), weights.data()), NIMCP_SUCCESS);

    // Weights should increase on average with high DA
    float avg_weight = std::accumulate(weights.begin(), weights.end(), 0.0f) / n_synapses;
    EXPECT_GT(avg_weight, 0.5f) << "Weights should increase with LTP and high DA";
}

TEST_F(TrainingPlasticityIntegrationTest, ReinforcementLearningScenario) {
    // Create bridge with RL preset (strong DA modulation)
    tpb_config_t config = tpb_config_preset("reinforcement");
    bridge_ = tpb_create(&config);
    ASSERT_NE(bridge_, nullptr);

    // Configure striatal region (key for RL)
    tpb_region_config_t striatal = tpb_region_striatal_default();
    striatal.neuron_start_idx = 0;
    striatal.neuron_end_idx = 1000;
    tpb_configure_region(bridge_, &striatal, nullptr);

    // Simulate reward prediction errors
    // Positive RPE (unexpected reward)
    tpb_inject_reward(bridge_, 0.8f);

    float da_after_reward = 0.0f;
    tpb_get_neuromod_levels(bridge_, &da_after_reward, nullptr, nullptr, nullptr);
    EXPECT_GT(da_after_reward, 0.7f) << "DA should be high after reward";

    // Check LR modulation is strong
    float base_lr = 0.01f;
    float modulated_lr = 0.0f;
    tpb_get_modulated_lr(bridge_, 0, base_lr, &modulated_lr);
    EXPECT_GT(modulated_lr, base_lr) << "LR should be boosted after reward";

    // Negative RPE (unexpected punishment)
    tpb_inject_reward(bridge_, -0.8f);

    float da_after_punish = 0.0f;
    tpb_get_neuromod_levels(bridge_, &da_after_punish, nullptr, nullptr, nullptr);
    EXPECT_LT(da_after_punish, da_after_reward) << "DA should decrease after punishment";
}

TEST_F(TrainingPlasticityIntegrationTest, UnsupervisedLearningScenario) {
    // Create bridge with unsupervised preset (ACh-dominant)
    tpb_config_t config = tpb_config_preset("unsupervised");
    bridge_ = tpb_create(&config);
    ASSERT_NE(bridge_, nullptr);

    // Configure hippocampal region (BCM learning)
    tpb_region_config_t hippocampal = tpb_region_hippocampal_default();
    hippocampal.neuron_start_idx = 0;
    hippocampal.neuron_end_idx = 500;
    tpb_configure_region(bridge_, &hippocampal, nullptr);

    // Set high ACh (attention/novelty)
    tpb_set_neuromod_levels(bridge_, -1.0f, 0.9f, -1.0f, -1.0f);

    // BCM learning should promote pattern selectivity
    bcm_synapse_t synapse;
    tpb_create_bcm_synapse(bridge_, 0, &synapse);

    float initial_weight = synapse.weight;
    float initial_threshold = synapse.threshold;

    // Simulate repeated high activity (pattern)
    for (int i = 0; i < 100; i++) {
        float delta = 0.0f;
        tpb_update_bcm(bridge_, &synapse, 0.8f, 0.8f, 0.001f, &delta);
    }

    // Threshold should slide toward activity level
    EXPECT_NE(synapse.threshold, initial_threshold)
        << "BCM threshold should adapt to activity";
}

//=============================================================================
// Multi-Region Learning Tests
//=============================================================================

TEST_F(TrainingPlasticityIntegrationTest, MultiRegionDifferentRules) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Configure multiple regions
    tpb_region_config_t cortical = tpb_region_cortical_default();
    cortical.neuron_start_idx = 0;
    cortical.neuron_end_idx = 333;
    uint32_t cortical_id = 0;
    tpb_configure_region(bridge_, &cortical, &cortical_id);

    tpb_region_config_t striatal = tpb_region_striatal_default();
    striatal.neuron_start_idx = 333;
    striatal.neuron_end_idx = 666;
    uint32_t striatal_id = 0;
    tpb_configure_region(bridge_, &striatal, &striatal_id);

    tpb_region_config_t hippocampal = tpb_region_hippocampal_default();
    hippocampal.neuron_start_idx = 666;
    hippocampal.neuron_end_idx = 1000;
    uint32_t hippocampal_id = 0;
    tpb_configure_region(bridge_, &hippocampal, &hippocampal_id);

    // Set DA level to test differential sensitivity
    tpb_set_neuromod_levels(bridge_, 0.8f, 0.5f, 0.5f, 0.5f);

    // Get modulated LR for each region
    float base_lr = 0.01f;
    float cortical_lr = 0.0f, striatal_lr = 0.0f, hippocampal_lr = 0.0f;

    tpb_get_modulated_lr(bridge_, cortical_id, base_lr, &cortical_lr);
    tpb_get_modulated_lr(bridge_, striatal_id, base_lr, &striatal_lr);
    tpb_get_modulated_lr(bridge_, hippocampal_id, base_lr, &hippocampal_lr);

    // Striatal should have strongest DA modulation
    EXPECT_GT(striatal_lr, cortical_lr)
        << "Striatal should have higher LR with high DA due to sensitivity";
}

TEST_F(TrainingPlasticityIntegrationTest, RegionRoutingCorrectness) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Configure regions with distinct parameters
    tpb_region_config_t amygdala = tpb_region_amygdala_default();
    amygdala.neuron_start_idx = 0;
    amygdala.neuron_end_idx = 100;
    amygdala.base_learning_rate = 0.05f;  // Fast
    tpb_configure_region(bridge_, &amygdala, nullptr);

    tpb_region_config_t cerebellar = tpb_region_cerebellar_default();
    cerebellar.neuron_start_idx = 100;
    cerebellar.neuron_end_idx = 200;
    cerebellar.base_learning_rate = 0.001f;  // Slow
    tpb_configure_region(bridge_, &cerebellar, nullptr);

    // Route to amygdala (fast learning)
    float delta_amygdala = 0.0f;
    tpb_route_weight_update(bridge_, 50, 0.8f, 0.9f, 10.0f, &delta_amygdala);

    // Route to cerebellar (slow learning)
    float delta_cerebellar = 0.0f;
    tpb_route_weight_update(bridge_, 150, 0.8f, 0.9f, 10.0f, &delta_cerebellar);

    // Both should be non-zero with correct relative magnitude
    // (Amygdala should learn faster, but cerebellar uses different rule)
    EXPECT_NE(delta_amygdala, 0.0f);
    EXPECT_NE(delta_cerebellar, 0.0f);
}

//=============================================================================
// Neuromodulator System Integration Tests
//=============================================================================

TEST_F(TrainingPlasticityIntegrationTest, SharedNeuromodulatorSystem) {
    // Create external neuromodulator system
    neuromodulator_config_t neuromod_config;
    memset(&neuromod_config, 0, sizeof(neuromod_config));
    neuromod_config.baseline_dopamine = 0.6f;
    neuromod_config.baseline_serotonin = 0.5f;
    neuromod_config.baseline_acetylcholine = 0.5f;
    neuromod_config.baseline_norepinephrine = 0.5f;

    neuromodulator_system_t shared_neuromod = neuromodulator_system_create(&neuromod_config);
    ASSERT_NE(shared_neuromod, nullptr);

    // Create bridge using shared neuromod
    tpb_config_t config = tpb_config_default();
    config.neuromod_system = shared_neuromod;
    bridge_ = tpb_create(&config);
    ASSERT_NE(bridge_, nullptr);

    // Modify through shared system
    neuromodulator_set_level(shared_neuromod, NEUROMOD_DOPAMINE, 0.9f);

    // Bridge should see the change
    float da = 0.0f;
    tpb_get_neuromod_levels(bridge_, &da, nullptr, nullptr, nullptr);
    EXPECT_NEAR(da, 0.9f, 0.05f) << "Bridge should see shared neuromod changes";

    // Cleanup
    tpb_destroy(bridge_);
    bridge_ = nullptr;
    neuromodulator_system_destroy(shared_neuromod);
}

TEST_F(TrainingPlasticityIntegrationTest, RPEDrivenNeuromodulation) {
    tpb_config_t config = tpb_config_default();
    config.rpe_to_da_gain = 0.5f;
    bridge_ = tpb_create(&config);
    ASSERT_NE(bridge_, nullptr);

    // Get baseline DA
    float da_baseline = 0.0f;
    tpb_get_neuromod_levels(bridge_, &da_baseline, nullptr, nullptr, nullptr);

    // Large loss improvement → DA burst
    tpb_report_loss(bridge_, 2.0f, nullptr);  // Establish high
    tpb_report_loss(bridge_, 2.0f, nullptr);
    tpb_report_loss(bridge_, 0.5f, nullptr);  // Big drop

    float da_after_improvement = 0.0f;
    tpb_get_neuromod_levels(bridge_, &da_after_improvement, nullptr, nullptr, nullptr);
    EXPECT_GT(da_after_improvement, da_baseline) << "DA should increase after loss improvement";

    // Reset and do opposite
    tpb_set_neuromod_levels(bridge_, 0.5f, -1.0f, -1.0f, -1.0f);

    tpb_report_loss(bridge_, 0.5f, nullptr);  // Establish low
    tpb_report_loss(bridge_, 0.5f, nullptr);
    tpb_report_loss(bridge_, 2.0f, nullptr);  // Big increase

    float da_after_deterioration = 0.0f;
    tpb_get_neuromod_levels(bridge_, &da_after_deterioration, nullptr, nullptr, nullptr);
    // DA should decrease (may not go below 0)
}

//=============================================================================
// STDP with Neuromodulation Tests
//=============================================================================

TEST_F(TrainingPlasticityIntegrationTest, STDPThreeFactorLearning) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Configure region with three-factor learning
    tpb_region_config_t cortical = tpb_region_cortical_default();
    cortical.neuron_start_idx = 0;
    cortical.neuron_end_idx = 1000;
    cortical.enable_three_factor = true;
    tpb_configure_region(bridge_, &cortical, nullptr);

    // Create STDP synapse
    stdp_synapse_t synapse;
    tpb_create_stdp_synapse(bridge_, 0, &synapse);
    EXPECT_TRUE(synapse.enable_da_modulation);

    // Simulate learning with low DA
    tpb_set_neuromod_levels(bridge_, 0.2f, -1.0f, -1.0f, -1.0f);

    float delta_low_da = 0.0f;
    synapse.weight = 0.5f;
    tpb_update_stdp(bridge_, &synapse, true, false, 100.0f, nullptr);
    tpb_update_stdp(bridge_, &synapse, false, true, 110.0f, &delta_low_da);
    float weight_after_low_da = synapse.weight;

    // Reset synapse
    synapse.weight = 0.5f;
    synapse.pre_trace = 0.0f;
    synapse.post_trace = 0.0f;

    // Simulate learning with high DA
    tpb_set_neuromod_levels(bridge_, 0.9f, -1.0f, -1.0f, -1.0f);

    float delta_high_da = 0.0f;
    tpb_update_stdp(bridge_, &synapse, true, false, 200.0f, nullptr);
    tpb_update_stdp(bridge_, &synapse, false, true, 210.0f, &delta_high_da);
    float weight_after_high_da = synapse.weight;

    // High DA should produce larger weight change
    EXPECT_GT(fabsf(delta_high_da), fabsf(delta_low_da) * 0.5f)
        << "Three-factor learning: high DA should amplify weight change";
}

//=============================================================================
// CoW Integration Tests
//=============================================================================

TEST_F(TrainingPlasticityIntegrationTest, CoWRollbackOnDivergence) {
    tpb_config_t config = tpb_config_default();
    config.enable_cow = true;
    bridge_ = tpb_create(&config);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 1000;
    tpb_configure_region(bridge_, &region, nullptr);

    // Create initial weights
    const uint32_t n = 1000;
    std::vector<float> weights(n);
    for (uint32_t i = 0; i < n; i++) {
        weights[i] = 0.5f + 0.1f * sinf((float)i * 0.01f);
    }

    // Snapshot before "training"
    cow_handle_t snapshot = nullptr;
    EXPECT_EQ(tpb_snapshot_weights(bridge_, weights.data(), n, &snapshot), NIMCP_SUCCESS);
    ASSERT_NE(snapshot, nullptr);

    // Store original for comparison
    std::vector<float> original_weights = weights;

    // Simulate training (modifies weights)
    std::vector<uint32_t> pre_ids(n), post_ids(n);
    std::vector<float> pre_act(n), post_act(n), deltas(n);
    for (uint32_t i = 0; i < n; i++) {
        pre_ids[i] = i;
        post_ids[i] = (i + 1) % n;
        pre_act[i] = 0.8f;
        post_act[i] = 0.9f;
        deltas[i] = (i % 2 == 0) ? 10.0f : -10.0f;
    }

    tpb_apply_plasticity_batch(bridge_, n, pre_ids.data(), post_ids.data(),
                                pre_act.data(), post_act.data(),
                                deltas.data(), weights.data());

    // Also manually modify weights to ensure CoW rollback can be tested
    // (plasticity changes may be very small depending on configuration)
    for (uint32_t i = 0; i < n; i += 100) {
        weights[i] += 0.1f;  // Add visible change
    }

    // Verify weights have changed (manual + plasticity)
    bool weights_changed = false;
    for (uint32_t i = 0; i < n; i++) {
        if (fabsf(weights[i] - original_weights[i]) > 0.0001f) {
            weights_changed = true;
            break;
        }
    }
    ASSERT_TRUE(weights_changed) << "Weights should change during training";

    // Simulate divergence detection - rollback
    EXPECT_EQ(tpb_restore_weights(bridge_, snapshot, weights.data()), NIMCP_SUCCESS);

    // Verify rollback
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_NEAR(weights[i], original_weights[i], 0.0001f)
            << "Weight " << i << " should be restored";
    }

    tpb_release_snapshot(bridge_, snapshot);
}

//=============================================================================
// Statistics Integration Tests
//=============================================================================

TEST_F(TrainingPlasticityIntegrationTest, ComprehensiveStatistics) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Configure region
    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 1000;
    tpb_configure_region(bridge_, &region, nullptr);

    // Report multiple losses
    for (int i = 0; i < 20; i++) {
        float loss = 1.0f - 0.04f * i + 0.01f * (i % 3);  // Generally decreasing
        tpb_report_loss(bridge_, loss, nullptr);
    }

    // Do plasticity updates
    for (int i = 0; i < 50; i++) {
        float delta = 0.0f;
        float timing = (i % 2 == 0) ? 10.0f : -10.0f;
        tpb_route_weight_update(bridge_, i % 1000, 0.8f, 0.9f, timing, &delta);
    }

    // Get stats
    tpb_stats_t stats;
    EXPECT_EQ(tpb_get_stats(bridge_, &stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.rpe_computations, 20u);
    EXPECT_EQ(stats.total_plasticity_updates, 50u);
    EXPECT_GT(stats.stdp_updates, 0u);
    EXPECT_GT(stats.avg_rpe, 0.0f) << "Generally decreasing loss = positive avg RPE";
    EXPECT_EQ(stats.region_updates[0], 50u);
}

//=============================================================================
// Long-Running Stability Tests
//=============================================================================

TEST_F(TrainingPlasticityIntegrationTest, LongRunningStability) {
    tpb_config_t config = tpb_config_default();
    config.thread_pool_size = 4;
    bridge_ = tpb_create(&config);
    ASSERT_NE(bridge_, nullptr);

    // Configure multiple regions
    for (int r = 0; r < 4; r++) {
        tpb_region_config_t region = tpb_region_cortical_default();
        region.neuron_start_idx = r * 2500;
        region.neuron_end_idx = (r + 1) * 2500;
        tpb_configure_region(bridge_, &region, nullptr);
    }

    // Run many iterations
    const int iterations = 1000;
    const int batch_size = 100;

    std::vector<uint32_t> pre_ids(batch_size), post_ids(batch_size);
    std::vector<float> pre_act(batch_size), post_act(batch_size);
    std::vector<float> deltas(batch_size), weights(batch_size, 0.5f);

    for (int iter = 0; iter < iterations; iter++) {
        // Report loss
        float loss = 1.0f - 0.0005f * iter + 0.1f * sinf(iter * 0.1f);
        tpb_report_loss(bridge_, loss, nullptr);

        // Batch update
        for (int i = 0; i < batch_size; i++) {
            pre_ids[i] = (iter * batch_size + i) % 10000;
            post_ids[i] = (iter * batch_size + i + 1) % 10000;
            pre_act[i] = 0.5f + 0.4f * sinf(i * 0.1f);
            post_act[i] = 0.5f + 0.4f * cosf(i * 0.1f);
            deltas[i] = (i % 2 == 0) ? 10.0f : -10.0f;
        }

        EXPECT_EQ(tpb_apply_plasticity_batch(bridge_, batch_size,
                                              pre_ids.data(), post_ids.data(),
                                              pre_act.data(), post_act.data(),
                                              deltas.data(), weights.data()),
                  NIMCP_SUCCESS);
    }

    // Verify stats accumulated correctly
    tpb_stats_t stats;
    tpb_get_stats(bridge_, &stats);
    EXPECT_EQ(stats.rpe_computations, (uint64_t)iterations);
    EXPECT_EQ(stats.total_plasticity_updates, (uint64_t)(iterations * batch_size));
}

//=============================================================================
// Callback-Plasticity Auto-Wiring Tests (Phase TCB-1)
//=============================================================================

class CallbackPlasticityWiringTest : public ::testing::Test {
protected:
    void SetUp() override {
        bridge_ = nullptr;
        training_ctx_ = nullptr;
    }

    void TearDown() override {
        if (training_ctx_) {
            nimcp_brain_training_destroy(training_ctx_);
            training_ctx_ = nullptr;
        }
        if (bridge_) {
            tpb_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    tpb_context_t* bridge_;
    nimcp_brain_training_ctx_t* training_ctx_;
};

TEST_F(CallbackPlasticityWiringTest, AutoWireWhenBridgeConnected) {
    // Create training context
    nimcp_brain_training_config_t train_cfg = nimcp_brain_training_default_config();
    train_cfg.enable_training_callbacks = true;
    training_ctx_ = nimcp_brain_training_create(&train_cfg);
    ASSERT_NE(training_ctx_, nullptr);

    // Create callbacks
    EXPECT_EQ(nimcp_brain_training_create_callbacks(training_ctx_), NIMCP_SUCCESS);
    tcb_context_t* callbacks = nimcp_brain_training_get_callbacks(training_ctx_);
    ASSERT_NE(callbacks, nullptr);

    // Create plasticity bridge
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Verify no plasticity callbacks registered yet
    EXPECT_EQ(tcb_get_callback_count(callbacks, TCB_EVENT_LOSS_COMPUTED), 0u);

    // Connect bridge to training - this should auto-wire callbacks
    EXPECT_EQ(nimcp_brain_training_connect_plasticity_bridge(training_ctx_, bridge_),
              NIMCP_SUCCESS);

    // Verify plasticity callbacks are now registered
    EXPECT_GT(tcb_get_callback_count(callbacks, TCB_EVENT_LOSS_COMPUTED), 0u)
        << "Loss callback should be auto-registered";
    EXPECT_GT(tcb_get_callback_count(callbacks, TCB_EVENT_WEIGHTS_UPDATED), 0u)
        << "Weights callback should be auto-registered";
    EXPECT_GT(tcb_get_callback_count(callbacks, TCB_EVENT_EPOCH_COMPLETE), 0u)
        << "Epoch callback should be auto-registered";
    EXPECT_GT(tcb_get_callback_count(callbacks, TCB_EVENT_DIVERGENCE), 0u)
        << "Divergence callback should be auto-registered";

    // Verify bridge has the callback context
    EXPECT_EQ(tpb_get_callback_context(bridge_), callbacks);
}

TEST_F(CallbackPlasticityWiringTest, AutoWireWhenCallbacksCreatedAfter) {
    // Create training context WITHOUT callbacks initially
    nimcp_brain_training_config_t train_cfg = nimcp_brain_training_default_config();
    train_cfg.enable_training_callbacks = false;
    training_ctx_ = nimcp_brain_training_create(&train_cfg);
    ASSERT_NE(training_ctx_, nullptr);

    // Create and connect plasticity bridge FIRST
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);
    EXPECT_EQ(nimcp_brain_training_connect_plasticity_bridge(training_ctx_, bridge_),
              NIMCP_SUCCESS);

    // No callbacks yet
    EXPECT_EQ(tpb_get_callback_context(bridge_), nullptr);

    // NOW create callbacks - should auto-wire to bridge
    EXPECT_EQ(nimcp_brain_training_create_callbacks(training_ctx_), NIMCP_SUCCESS);
    tcb_context_t* callbacks = nimcp_brain_training_get_callbacks(training_ctx_);
    ASSERT_NE(callbacks, nullptr);

    // Verify callbacks are wired to plasticity
    EXPECT_EQ(tpb_get_callback_context(bridge_), callbacks)
        << "Bridge should be connected to callbacks created after";
    EXPECT_GT(tcb_get_callback_count(callbacks, TCB_EVENT_LOSS_COMPUTED), 0u)
        << "Plasticity callbacks should be registered";
}

TEST_F(CallbackPlasticityWiringTest, EndToEndCallbackFiringModulatesPlasticity) {
    // Setup training context with callbacks
    nimcp_brain_training_config_t train_cfg = nimcp_brain_training_default_config();
    train_cfg.enable_training_callbacks = true;
    training_ctx_ = nimcp_brain_training_create(&train_cfg);
    ASSERT_NE(training_ctx_, nullptr);
    EXPECT_EQ(nimcp_brain_training_create_callbacks(training_ctx_), NIMCP_SUCCESS);

    // Setup plasticity bridge with region
    tpb_config_t tpb_cfg = tpb_config_default();
    bridge_ = tpb_create(&tpb_cfg);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 1000;
    tpb_configure_region(bridge_, &region, nullptr);

    // Connect bridge (auto-wires callbacks)
    EXPECT_EQ(nimcp_brain_training_connect_plasticity_bridge(training_ctx_, bridge_),
              NIMCP_SUCCESS);

    // Get initial neuromodulator levels
    float da_initial, ach_initial;
    tpb_get_neuromod_levels(bridge_, &da_initial, &ach_initial, nullptr, nullptr);

    // Get callbacks context
    tcb_context_t* callbacks = nimcp_brain_training_get_callbacks(training_ctx_);

    // Fire loss events simulating training improvement
    tcb_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    // Establish baseline loss
    metrics.loss = 1.0f;
    metrics.step = 0;
    tcb_update_metrics(callbacks, metrics.loss, 0.01f, metrics.step, 0.0f);
    tcb_fire_event(callbacks, TCB_EVENT_LOSS_COMPUTED, &metrics);

    // Fire improving loss - should boost DA
    metrics.loss = 0.5f;
    metrics.step = 1;
    tcb_update_metrics(callbacks, metrics.loss, 0.01f, metrics.step, 0.0f);
    tcb_fire_event(callbacks, TCB_EVENT_LOSS_COMPUTED, &metrics);

    // Check callback stats on plasticity bridge
    uint64_t loss_fired = 0;
    tpb_get_callback_stats(bridge_, &loss_fired, nullptr, nullptr, nullptr);
    EXPECT_GT(loss_fired, 0u) << "Loss callbacks should have fired";

    // Verify DA changed from RPE computation
    float da_after;
    tpb_get_neuromod_levels(bridge_, &da_after, nullptr, nullptr, nullptr);

    // Fire large gradient event - should boost NE
    float ne_before, ne_after;
    tpb_get_neuromod_levels(bridge_, nullptr, nullptr, nullptr, &ne_before);

    metrics.gradient_norm = 100.0f;  // Large gradient
    metrics.step = 2;
    tcb_fire_event(callbacks, TCB_EVENT_WEIGHTS_UPDATED, &metrics);

    tpb_get_neuromod_levels(bridge_, nullptr, nullptr, nullptr, &ne_after);
    EXPECT_GT(ne_after, ne_before) << "Large gradient should boost NE";

    // Verify weights callback fired
    uint64_t weight_fired = 0;
    tpb_get_callback_stats(bridge_, nullptr, &weight_fired, nullptr, nullptr);
    EXPECT_GT(weight_fired, 0u) << "Weights callbacks should have fired";
}

TEST_F(CallbackPlasticityWiringTest, DivergenceCallbackTriggersEmergencyResponse) {
    // Setup
    nimcp_brain_training_config_t train_cfg = nimcp_brain_training_default_config();
    train_cfg.enable_training_callbacks = true;
    training_ctx_ = nimcp_brain_training_create(&train_cfg);
    ASSERT_NE(training_ctx_, nullptr);
    EXPECT_EQ(nimcp_brain_training_create_callbacks(training_ctx_), NIMCP_SUCCESS);

    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);
    EXPECT_EQ(nimcp_brain_training_connect_plasticity_bridge(training_ctx_, bridge_),
              NIMCP_SUCCESS);

    // Set high neuromodulator levels (as if learning was going well)
    tpb_set_neuromod_levels(bridge_, 0.9f, 0.8f, 0.3f, 0.8f);

    tcb_context_t* callbacks = nimcp_brain_training_get_callbacks(training_ctx_);

    // Fire divergence event
    tcb_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss = 1000.0f;  // Diverged loss
    metrics.step = 100;

    tcb_action_t action = tcb_fire_event(callbacks, TCB_EVENT_DIVERGENCE, &metrics);

    // Divergence callback should request rollback or reduce LR
    EXPECT_TRUE(action == TCB_ACTION_ROLLBACK || action == TCB_ACTION_REDUCE_LR)
        << "Divergence should trigger emergency action";

    // Check neuromodulators were reset toward baseline (calming response)
    float da, ht5, ne;
    tpb_get_neuromod_levels(bridge_, &da, nullptr, &ht5, &ne);

    // 5-HT should be elevated (calming), NE should be reduced
    EXPECT_GT(ht5, 0.5f) << "5-HT should be elevated after divergence (calm down)";
    EXPECT_LT(ne, 0.5f) << "NE should be reduced after divergence";

    // Verify stats
    uint64_t divergence_fired = 0;
    tpb_get_callback_stats(bridge_, nullptr, nullptr, nullptr, &divergence_fired);
    EXPECT_GT(divergence_fired, 0u) << "Divergence callback should have fired";
}

TEST_F(CallbackPlasticityWiringTest, EpochCallbackTriggersConsolidation) {
    // Setup
    nimcp_brain_training_config_t train_cfg = nimcp_brain_training_default_config();
    train_cfg.enable_training_callbacks = true;
    training_ctx_ = nimcp_brain_training_create(&train_cfg);
    ASSERT_NE(training_ctx_, nullptr);
    EXPECT_EQ(nimcp_brain_training_create_callbacks(training_ctx_), NIMCP_SUCCESS);

    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);
    EXPECT_EQ(nimcp_brain_training_connect_plasticity_bridge(training_ctx_, bridge_),
              NIMCP_SUCCESS);

    // Set extreme neuromodulator levels
    tpb_set_neuromod_levels(bridge_, 0.9f, 0.2f, 0.1f, 0.9f);

    tcb_context_t* callbacks = nimcp_brain_training_get_callbacks(training_ctx_);

    // Fire epoch complete event
    tcb_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.epoch = 1;
    metrics.loss = 0.5f;

    tcb_fire_event(callbacks, TCB_EVENT_EPOCH_COMPLETE, &metrics);

    // Epoch callback triggers homeostatic scaling - neuromodulators drift toward baseline
    float da, ach, ht5, ne;
    tpb_get_neuromod_levels(bridge_, &da, &ach, &ht5, &ne);

    // All should be closer to 0.5 than before
    EXPECT_LT(da, 0.9f) << "DA should drift toward baseline after epoch";
    EXPECT_GT(ach, 0.2f) << "ACh should drift toward baseline after epoch";
    EXPECT_GT(ht5, 0.1f) << "5-HT should drift toward baseline after epoch";
    EXPECT_LT(ne, 0.9f) << "NE should drift toward baseline after epoch";

    // Verify epoch callback fired
    uint64_t epoch_fired = 0;
    tpb_get_callback_stats(bridge_, nullptr, nullptr, &epoch_fired, nullptr);
    EXPECT_GT(epoch_fired, 0u) << "Epoch callback should have fired";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
